const title = process.argv[2] ?? process.env.PR_TITLE ?? "";
const allowedTypes = [
  "build",
  "chore",
  "ci",
  "docs",
  "feat",
  "fix",
  "perf",
  "refactor",
  "revert",
  "style",
  "test",
];
const typePattern = allowedTypes.join("|");
const conventionalTitle = new RegExp(
  `^(?:${typePattern})(?:\\([a-z0-9][a-z0-9._/-]*\\))?!?: [^\\s].+$`,
);

if (!conventionalTitle.test(title)) {
  throw new Error(
    `Invalid Conventional Commit title: "${title}"\n` +
      `Expected: <type>(optional-scope): <description>\n` +
      `Allowed types: ${allowedTypes.join(", ")}`,
  );
}

console.log(`Valid Conventional Commit title: ${title}`);

