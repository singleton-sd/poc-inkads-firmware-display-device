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
const ticketPattern = "\\[(?:[A-Z][A-Z0-9]+-\\d+)\\]";
const conventionalTitle = new RegExp(
  `^(?:${typePattern})(?:\\([a-z0-9][a-z0-9._/-]*\\))?!?: [^\\s].* ${ticketPattern}$`,
);

if (!conventionalTitle.test(title)) {
  throw new Error(
    `Invalid Conventional Commit title: "${title}"\n` +
      `Expected: <type>(optional-scope): <description> [ABC-123]\n` +
      `Allowed types: ${allowedTypes.join(", ")}`,
  );
}

console.log(`Valid Conventional Commit title: ${title}`);

