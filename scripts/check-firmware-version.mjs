import fs from "node:fs";

const metadata = JSON.parse(fs.readFileSync("version.json", "utf8"));
const config = fs.readFileSync("src/config/DeviceConfig.h", "utf8");
const match = config.match(/firmwareVersion\[\]\s*=\s*"([^"]+)"/);

if (!match) {
  throw new Error("Could not find DeviceConfig::firmwareVersion");
}

if (match[1] !== metadata.version) {
  throw new Error(
    `Firmware version mismatch: DeviceConfig=${match[1]}, version.json=${metadata.version}`,
  );
}

const requestedTag = process.argv[2];
if (requestedTag && requestedTag !== `v${metadata.version}`) {
  throw new Error(
    `Release tag ${requestedTag} does not match firmware version v${metadata.version}`,
  );
}

console.log(`Firmware version ${metadata.version} is consistent.`);
