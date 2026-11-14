import fs from "node:fs";

const metadata = JSON.parse(fs.readFileSync("version.json", "utf8"));
const config = fs.readFileSync("src/config/DeviceConfig.h", "utf8");
const defineMatch = config.match(/#define\s+INKADS_FIRMWARE_VERSION\s+"([^"]+)"/);
const assignMatch = config.match(/firmwareVersion\[\s*\]\s*=\s*"([^"]+)"/);
const headerVersion = defineMatch?.[1] ?? assignMatch?.[1];

if (!headerVersion) {
  throw new Error("Could not find INKADS_FIRMWARE_VERSION or firmwareVersion[]");
}

if (headerVersion !== metadata.version) {
  throw new Error(
    `Firmware version mismatch: DeviceConfig=${headerVersion}, version.json=${metadata.version}`,
  );
}

const requestedTag = process.argv[2];
if (requestedTag && requestedTag !== `v${metadata.version}`) {
  throw new Error(
    `Release tag ${requestedTag} does not match firmware version v${metadata.version}`,
  );
}

console.log(`Firmware version ${metadata.version} is consistent.`);
