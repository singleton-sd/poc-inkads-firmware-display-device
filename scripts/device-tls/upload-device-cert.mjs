import fs from "node:fs/promises";
import path from "node:path";

function required(name) {
  const value = process.env[name];
  if (!value) throw new Error(`Missing ${name}`);
  return value;
}

async function main() {
  const baseUrl = required("DEVICE_BASE_URL");
  const sessionCookie = required("SESSION_COOKIE");
  const csrfToken = required("CSRF_TOKEN");
  const certPath = process.env.CERT_PATH || path.resolve(process.cwd(), ".output/device-cert.pem");
  const keyPath = process.env.KEY_PATH || path.resolve(process.cwd(), ".output/device-key.pem");

  const certificatePem = await fs.readFile(certPath, "utf8");
  const privateKeyPem = await fs.readFile(keyPath, "utf8");

  const response = await fetch(`${baseUrl.replace(/\/$/, "")}/admin/tls-certificate`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "X-CSRF-Token": csrfToken,
      "Cookie": `inkads_session=${sessionCookie}`
    },
    body: JSON.stringify({
      certificate_pem: certificatePem,
      private_key_pem: privateKeyPem
    })
  });

  const body = await response.text();
  if (!response.ok) {
    throw new Error(`Upload failed (${response.status}): ${body}`);
  }

  console.log(body);
}

main().catch((error) => {
  console.error(error.message);
  process.exit(1);
});
