#pragma once

// Copy this file to TlsCredentials.local.h and replace both values with the
// certificate and private key issued uniquely for this device. The local file
// is ignored by git. The certificate SAN must contain the device hostname,
// for example DNS:inkads-a1b2c3.local.
namespace InkAdsTlsLocal {
inline constexpr char certificatePem[] = R"pem(-----BEGIN CERTIFICATE-----
REPLACE WITH THIS DEVICE'S PEM CERTIFICATE
-----END CERTIFICATE-----
)pem";

inline constexpr char privateKeyPem[] = R"pem(-----BEGIN PRIVATE KEY-----
REPLACE WITH THIS DEVICE'S PEM PRIVATE KEY
-----END PRIVATE KEY-----
)pem";
}  // namespace InkAdsTlsLocal
