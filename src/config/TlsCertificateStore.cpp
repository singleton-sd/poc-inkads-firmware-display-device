#include "TlsCertificateStore.h"

#include <SPIFFS.h>
#include <esp_random.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509.h>

namespace {
constexpr char kActiveCertPath[] = "/tls_active_cert.pem";
constexpr char kActiveKeyPath[] = "/tls_active_key.pem";
constexpr char kStagedCertPath[] = "/tls_staged_cert.pem";
constexpr char kStagedKeyPath[] = "/tls_staged_key.pem";
constexpr size_t kMaxPemBytes = 8192;

int fillRandom(void* /*context*/, unsigned char* output, size_t length) {
  esp_fill_random(output, length);
  return 0;
}
}  // namespace

bool TlsCertificateStore::begin() { return SPIFFS.begin(true); }

bool TlsCertificateStore::loadActive(String& error) {
  activeCertificate_ = "";
  activePrivateKey_ = "";
  if (!readFile(kActiveCertPath, activeCertificate_, error)) return false;
  if (!readFile(kActiveKeyPath, activePrivateKey_, error)) return false;
  return true;
}

bool TlsCertificateStore::stage(const String& certificatePem,
                                const String& privateKeyPem,
                                const String& expectedHostname,
                                String& error) {
  if (!validateBundle(certificatePem, privateKeyPem, expectedHostname, error)) {
    return false;
  }
  if (!writeFile(kStagedCertPath, certificatePem, error)) return false;
  if (!writeFile(kStagedKeyPath, privateKeyPem, error)) {
    removeIfPresent(kStagedCertPath);
    return false;
  }
  String previousCert;
  String previousKey;
  String ignoredError;
  const bool hadPrevious =
      readFile(kActiveCertPath, previousCert, ignoredError) &&
      readFile(kActiveKeyPath, previousKey, ignoredError);

  if (!writeFile(kActiveCertPath, certificatePem, error)) {
    return false;
  }
  if (!writeFile(kActiveKeyPath, privateKeyPem, error)) {
    if (hadPrevious) {
      String rollbackError;
      writeFile(kActiveCertPath, previousCert, rollbackError);
      writeFile(kActiveKeyPath, previousKey, rollbackError);
    }
    return false;
  }

  removeIfPresent(kStagedCertPath);
  removeIfPresent(kStagedKeyPath);
  return loadActive(error);
}

bool TlsCertificateStore::writeFile(const char* path, const String& contents,
                                    String& error) {
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    error = String("failed to open ") + path + " for write";
    return false;
  }
  const size_t written = file.print(contents);
  file.close();
  if (written != static_cast<size_t>(contents.length())) {
    error = String("failed to write ") + path;
    return false;
  }
  return true;
}

bool TlsCertificateStore::readFile(const char* path, String& out, String& error) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    error = String("missing TLS file: ") + path;
    return false;
  }
  out = file.readString();
  file.close();
  if (out.isEmpty()) {
    error = String("empty TLS file: ") + path;
    return false;
  }
  return true;
}

bool TlsCertificateStore::removeIfPresent(const char* path) {
  if (!SPIFFS.exists(path)) return true;
  return SPIFFS.remove(path);
}

bool TlsCertificateStore::validateBundle(const String& certificatePem,
                                         const String& privateKeyPem,
                                         const String& expectedHostname,
                                         String& error) const {
  if (certificatePem.isEmpty() || privateKeyPem.isEmpty()) {
    error = "certificate and private key are required";
    return false;
  }
  if (certificatePem.length() > kMaxPemBytes ||
      privateKeyPem.length() > kMaxPemBytes) {
    error = "certificate bundle is unexpectedly large";
    return false;
  }

  mbedtls_x509_crt cert;
  mbedtls_x509_crt_init(&cert);
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);

  const int certResult = mbedtls_x509_crt_parse(
      &cert, reinterpret_cast<const unsigned char*>(certificatePem.c_str()),
      certificatePem.length() + 1);
  if (certResult != 0) {
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    error = "certificate PEM is invalid";
    return false;
  }

  const int keyResult = mbedtls_pk_parse_key(
      &key, reinterpret_cast<const unsigned char*>(privateKeyPem.c_str()),
      privateKeyPem.length() + 1, nullptr, 0, fillRandom, nullptr);
  if (keyResult != 0) {
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    error = "private key PEM is invalid";
    return false;
  }

  const int pairResult =
      mbedtls_pk_check_pair(&cert.pk, &key, fillRandom, nullptr);
  if (pairResult != 0) {
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    error = "certificate does not match private key";
    return false;
  }

  if (mbedtls_x509_time_is_past(&cert.valid_to) != 0) {
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    error = "certificate is already expired";
    return false;
  }

  char certInfo[1024] = {};
  mbedtls_x509_crt_info(certInfo, sizeof(certInfo) - 1, "", &cert);
  char subject[512] = {};
  mbedtls_x509_dn_gets(subject, sizeof(subject), &cert.subject);
  if (!certificateMentionsHost(String(subject) + certInfo, expectedHostname)) {
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&cert);
    error = "certificate subject does not mention this device hostname";
    return false;
  }

  mbedtls_pk_free(&key);
  mbedtls_x509_crt_free(&cert);
  return true;
}

bool TlsCertificateStore::certificateMentionsHost(
    const String& subjectName, const String& expectedHostname) const {
  return subjectName.indexOf(expectedHostname) >= 0;
}
