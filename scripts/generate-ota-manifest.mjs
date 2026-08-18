import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const [binaryPath, outputPath] = process.argv.slice(2);
const repository = process.env.GITHUB_REPOSITORY;
const tag = process.env.RELEASE_TAG;

if (!binaryPath || !outputPath || !repository || !tag) {
  throw new Error(
    "Usage: GITHUB_REPOSITORY=owner/repo RELEASE_TAG=v1.2.3 node " +
      "scripts/generate-ota-manifest.mjs <binary> <output>",
  );
}

const metadata = JSON.parse(fs.readFileSync("version.json", "utf8"));
const binary = fs.readFileSync(binaryPath);
const assetName = path.basename(binaryPath);
const manifest = {
  schemaVersion: 1,
  version: metadata.version,
  channel: metadata.channel,
  url: `https://github.com/${repository}/releases/download/${tag}/${assetName}`,
  sha256: crypto.createHash("sha256").update(binary).digest("hex"),
  size: binary.length,
  publishedAt: new Date().toISOString(),
};

fs.writeFileSync(outputPath, `${JSON.stringify(manifest, null, 2)}\n`);
console.log(`Wrote OTA manifest to ${outputPath}`);
