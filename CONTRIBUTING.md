# Contributing to InkAds display-device firmware

## Conventional Commits

Use a Conventional Commit title for every pull request and include a ticket
reference at the end. This repository uses squash merging, so the pull-request
title becomes the commit that the main-merge release job evaluates.

```text
<type>(optional-scope): <description> [ABC-123]
```

Common types:

- `feat`: user-visible functionality; normally a minor version bump.
- `fix`: a bug fix; normally a patch version bump.
- `perf`: a user-visible performance improvement; normally a patch bump.
- `refactor` and `revert`: normally a patch bump.
- `docs`, `test`, `build`, `ci`, `chore`: maintenance changes that do not
  trigger a release by themselves.

Examples:

```text
feat(ota): check the stable channel for firmware updates [POC-247]
fix(wifi): recover after an access point disconnects [POC-247]
docs: describe initial device provisioning [POC-247]
```

Use `!` and a `BREAKING CHANGE:` footer when compatibility is intentionally
broken:

```text
feat(manifest)!: require schema version 2 [POC-247]
```

The pull-request title check enforces the shape, accepted types, and required
ticket reference. After merge to `main`, `scripts/release_plan.sh` uses those
commits to choose the next semantic version, and `git-cliff` writes changelog
notes.

For local commits, the repo-managed hooks derive the ticket from a matching
branch name such as `feature/POC-247-conventional-commit-hooks` and append it
to the commit subject automatically. If you type a different ticket manually,
the commit is rejected.

## Local hook setup

Install the repo-managed Git hooks once per clone:

```sh
git config core.hooksPath .githooks
```

After that:

- `prepare-commit-msg` appends the branch ticket to valid Conventional Commit
  subjects on `feature/...` and `hotfix/...` branches
- `commit-msg` validates the final subject and blocks ticket mismatches
- CI stamps `[POC-123]` onto a pull-request title when the branch is
  `feature/POC-123-...` or `hotfix/POC-123-...` and the title has no ticket
  suffix. `gh pr create --title` bypasses local hooks, so this is what keeps
  squash-merge titles valid. CI then runs
  `scripts/check-conventional-title.sh` against the final title. It does not
  overwrite a different ticket that is already present.

## Release process

1. Merge Conventional Commits into `main`. Do not open a version-bump PR.
2. The main-merge release workflow runs `scripts/release_plan.sh`. If there
   are releasable commits since the last tag (or since the last
   `version.json` change when no tags exist), it updates `version.json` and
   `DeviceConfig.h`, prepends `CHANGELOG.md` with `git-cliff`, and pushes a
   `vX.Y.Z` tag.
3. Maintenance commits (`docs`, `test`, `build`, `ci`, `chore`) do not
   create a release by themselves. `feat` is a minor bump, `fix` /
   `perf` / `refactor` / `revert` are patch bumps, and a breaking change is
   a major bump.
4. Pushing the tag triggers the firmware release workflow, which validates
   the version, compiles every row in `targets.json`, writes versioned
   `inkads-{filename}-{suffix}-v{version}.bin` assets plus
   `inkads-manifest.json`, and publishes the GitHub Release.

`version.json` is the canonical firmware version. The C++ constant is
a generated release companion that is validated against it on every build.
Artifact names also take that version. Add another `targets.json` row to
compile a second board or a size-reduced feature cut of the same board.

Catalog tests:

```sh
bash scripts/test/run.sh
```

`jq` is required for the catalog scripts.
