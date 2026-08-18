import { ChangeResourceRecordSetsCommand, Route53Client } from "@aws-sdk/client-route-53";

function required(name) {
  const value = process.env[name];
  if (!value) throw new Error(`Missing ${name}`);
  return value;
}

async function main() {
  const hostedZoneId = required("HOSTED_ZONE_ID");
  const hostname = required("DEVICE_HOSTNAME");
  const ipAddress = required("DEVICE_IP");
  const ttl = Number(process.env.DNS_TTL_SECONDS || "60");

  const client = new Route53Client({});
  await client.send(
    new ChangeResourceRecordSetsCommand({
      HostedZoneId: hostedZoneId,
      ChangeBatch: {
        Comment: "InkAds device DNS record",
        Changes: [
          {
            Action: "UPSERT",
            ResourceRecordSet: {
              Name: hostname.endsWith(".") ? hostname : `${hostname}.`,
              Type: "A",
              TTL: ttl,
              ResourceRecords: [{ Value: ipAddress }]
            }
          }
        ]
      }
    })
  );

  console.log(`Published DNS A record: ${hostname} -> ${ipAddress}`);
}

main().catch((error) => {
  console.error(error.message);
  process.exit(1);
});
