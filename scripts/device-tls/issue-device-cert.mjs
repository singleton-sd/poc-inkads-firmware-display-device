import fs from "node:fs/promises";
import path from "node:path";
import { createHash } from "node:crypto";

import * as acme from "acme-client";
import { ChangeResourceRecordSetsCommand, Route53Client } from "@aws-sdk/client-route-53";

function required(name) {
  const value = process.env[name];
  if (!value) throw new Error(`Missing ${name}`);
  return value;
}

async function main() {
  const email = required("ACME_EMAIL");
  const hostname = required("DEVICE_HOSTNAME");
  const hostedZoneId = required("HOSTED_ZONE_ID");
  const outputDir = process.env.OUTPUT_DIR || path.resolve(process.cwd(), ".output");
  const directoryUrl =
    process.env.ACME_DIRECTORY_URL || acme.directory.letsencrypt.production;

  const accountKey = await acme.crypto.createPrivateKey();
  const client = new acme.Client({ directoryUrl, accountKey });
  const [key, csr] = await acme.crypto.createCsr({
    commonName: hostname,
    altNames: [hostname]
  });
  const route53 = new Route53Client({});

  const certificate = await client.auto({
    csr,
    email,
    termsOfServiceAgreed: true,
    challengeCreateFn: async (_authz, challenge, keyAuthorization) => {
      const recordName = `_acme-challenge.${hostname}`;
      const recordValue = createHash("sha256")
        .update(keyAuthorization)
        .digest("base64url");
      await route53.send(
        new ChangeResourceRecordSetsCommand({
          HostedZoneId: hostedZoneId,
          ChangeBatch: {
            Comment: "InkAds ACME DNS challenge",
            Changes: [
              {
                Action: "UPSERT",
                ResourceRecordSet: {
                  Name: `${recordName}.`,
                  Type: "TXT",
                  TTL: 30,
                  ResourceRecords: [{ Value: `"${recordValue}"` }]
                }
              }
            ]
          }
        })
      );
    },
    challengeRemoveFn: async (_authz, challenge, keyAuthorization) => {
      const recordName = `_acme-challenge.${hostname}`;
      const recordValue = createHash("sha256")
        .update(keyAuthorization)
        .digest("base64url");
      await route53.send(
        new ChangeResourceRecordSetsCommand({
          HostedZoneId: hostedZoneId,
          ChangeBatch: {
            Comment: "InkAds ACME DNS challenge cleanup",
            Changes: [
              {
                Action: "DELETE",
                ResourceRecordSet: {
                  Name: `${recordName}.`,
                  Type: "TXT",
                  TTL: 30,
                  ResourceRecords: [{ Value: `"${recordValue}"` }]
                }
              }
            ]
          }
        })
      ).catch(() => {
        // Cleanup is best-effort.
      });
    }
  });

  await fs.mkdir(outputDir, { recursive: true });
  await fs.writeFile(path.join(outputDir, "device-cert.pem"), certificate, "utf8");
  await fs.writeFile(path.join(outputDir, "device-key.pem"), key.toString(), "utf8");

  console.log(`Issued certificate for ${hostname}`);
  console.log(`Wrote ${path.join(outputDir, "device-cert.pem")}`);
  console.log(`Wrote ${path.join(outputDir, "device-key.pem")}`);
}

main().catch((error) => {
  console.error(error.message);
  process.exit(1);
});
