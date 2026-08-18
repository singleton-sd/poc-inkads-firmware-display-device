#pragma once

#include <Arduino.h>

class TlsCertificateStore {
 public:
  bool begin();
  bool loadActive(String& error);
  bool stage(const String& certificatePem, const String& privateKeyPem,
             const String& expectedHostname, String& error);

  const char* certificatePem() const { return activeCertificate_.c_str(); }
  const char* privateKeyPem() const { return activePrivateKey_.c_str(); }
  bool configured() const {
    return !activeCertificate_.isEmpty() && !activePrivateKey_.isEmpty();
  }

 private:
  bool writeFile(const char* path, const String& contents, String& error);
  bool readFile(const char* path, String& out, String& error);
  bool removeIfPresent(const char* path);
  bool validateBundle(const String& certificatePem, const String& privateKeyPem,
                      const String& expectedHostname, String& error) const;
  bool certificateMentionsHost(const String& subjectName,
                               const String& expectedHostname) const;

  String activeCertificate_;
  String activePrivateKey_;
};
