# AGENTS.md — InkAds display-device firmware

Cross-agent working agreements for `singleton-sd/poc-inkads-firmware-display-device`.
Product detail lives in [README.md](README.md). Git/release process lives in
[CONTRIBUTING.md](CONTRIBUTING.md).

## Skills

This Cursor workspace already mounts the skills repo. **Read those files; do
not copy them into this repository.**

From `C:\00Personal\singleton-sd\ai-plattform-skills\main` (or the `ai-plattform-skills` workspace folder):

- `engineering/git-conventions/SKILL.md`
- `engineering/implement-feature/SKILL.md`
- `engineering/code-review/SKILL.md`
- `operations/task-driven-development/SKILL.md`

See [.cursor/skills/README.md](.cursor/skills/README.md). Do not apply
poc-plattform-kit GitHub-issue branch names, `pnpm worktree:add`, preview
scenarios, or Exclusive Claim. This firmware repo tracks work in ClickUp
`POC-###`.

## Worktrees (mandatory)

Never edit or commit on the shared clone. It stays on `main`.

```text
InkAds/                              <-- open this workspace
  firmware/display-device/           <-- git clone; stays on main
  firmware/worktrees/
    POC-247-kebab-slug/              <-- one worktree per ticket
```

```bash
./scripts/add-worktree.sh --task-id POC-247 --slug conventional-commit-hooks
./scripts/add-worktree.sh --task-id POC-247 --slug broken-wifi --hotfix
./scripts/add-worktree.sh --type docs --slug agent-working-agreements
```

- Branch: `feature/POC-###-kebab-title` or `hotfix/POC-###-kebab-title`
- Folder name = branch without `feature/` / `hotfix/`
- Create from `origin/main` only. Each implementer subagent gets its own worktree.
- After merge or abandon: `git worktree remove ../worktrees/<POC-###>-<slug>` then `git worktree prune`
- On this Windows machine, git may need `git -c safe.directory=<abs-path>` (the helper sets it)

Do not invent sibling `firmware/display-device-*` folders or in-repo `.worktrees/`.

## Scripts

New automation is POSIX `scripts/*.sh` with `set -euo pipefail`. Do not add
Node, `package.json`, or Python for CI or release helpers.

Legacy Node on `main` (`check-firmware-version.mjs`, `generate-ota-manifest.mjs`):
use until replaced; do not extend.

## Compile

Arduino CLI requires the sketch folder name to match `display-device.ino`.
Worktree folders are not named `display-device`. Always compile with:

```bash
bash scripts/compile.sh
```

Toolchain (same as CI):

- Board manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Core: `esp32:esp32@3.3.11`
- FQBNs and feature cuts: `targets.json` (compile with
  `bash scripts/compile.sh`, or `--target <id>`)

No third-party Arduino libraries. Do not commit `dist/`, `build/`, or
`src/config/TlsCredentials.local.h`. Upload is optional and port-specific;
compile is the default check.

## Feature flags (mandatory for new features)

Arduino compiles every `.cpp` in the sketch. New optional behaviour must be a
catalog feature, not a runtime `if`, so size-reduced targets can leave it out.

When developing a feature:

1. Add the name to `targets.json` `features` (lowercase, underscores allowed).
2. Gate code with `#if INKADS_FEATURE_<NAME>` (`https_admin` →
   `INKADS_FEATURE_HTTPS_ADMIN`).
3. Enable it on every target with `"suffix": "full"`. That is the shipping cut.
4. Omit it from smaller suffixes only when flash size requires it.
5. Update committed `src/config/InkAdsFeatures.h` so Arduino IDE matches
   `full` (use `1`). Leave a stub at `0` only while the feature is not
   implemented.

Do not ship a developed feature that is missing from `full`.

## Pull requests

This repo squash-merges. The **PR title becomes the only commit** the release
job sees. CI validates the title with `scripts/check-conventional-title.sh`.
Humans merge. Agents never approve or merge.

Branch: `feature/POC-###-kebab-title` (or `hotfix/…`).

Title:

```text
<type>(optional-scope): <description> [POC-###]
```

Types: `feat` `fix` `perf` `refactor` `revert` `docs` `test` `build` `ci`
`chore` `style`. Breaking: `feat(scope)!: …` plus a `BREAKING CHANGE:` footer.

```text
feat(ota): check the stable channel for firmware updates [POC-247]
fix(wifi): recover after an access point disconnects [POC-246]
docs: add agent working agreements [POC-247]
```

`feat` → minor. `fix` / `perf` / `refactor` / `revert` → patch.
`docs` / `test` / `build` / `ci` / `chore` / `style` → no release by themselves.

Local hooks (`git config core.hooksPath .githooks`) append `[POC-###]` from
`feature/` and `hotfix/` branch names. CI still checks the PR title.

Open with `gh pr create --base main`. Watch `gh pr checks --watch`. Return the
URL. Use the pull-request template. Do not force-push `main`. After a rebase,
`git push --force-with-lease` only.

Releases: Conventional Commits on `main` drive `scripts/release_plan.sh` +
`git-cliff`. Do not open a version-bump PR. Do not add Node release tooling.
`version.json` is canonical and must match `DeviceConfig::firmwareVersion`.

## Security

- Do not commit PEM keys or `TlsCredentials.local.h`. Empty certs still compile.
- Entra IDs in `src/config/EntraConfig.h` are public. No client secret. Device
  Code Flow, `InkAds.Admin`, tenant-specific authority (never `/common`).
- Do not log passwords, device codes, tokens, cookies, or token subjects.
- HTML/CSS/JS/tokens are compiled into firmware. No internet-hosted assets.

## ClickUp

Product folder: [InkAds](https://app.clickup.com/90161394355/v/f/901610926351/90165834867)
(list 03 — Device Firmware).

## Do not

- Edit or push the shared `main` checkout
- Add Node or Python for new repo scripts
- Invent PlatformIO or extra Arduino libraries
- Print or commit secrets
