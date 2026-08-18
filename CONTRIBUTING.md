# Contributing to InkAds display-device firmware

## Conventional Commits

Use a Conventional Commit title for every pull request and include a ticket
reference at the end. This repository uses squash merging, so the pull-request
title becomes the commit that Release Please evaluates.

```text
<type>(optional-scope): <description> [ABC-123]
```

Common types:

- `feat`: user-visible functionality; normally a minor version bump.
- `fix`: a bug fix; normally a patch version bump.
- `perf`: a user-visible performance improvement; normally a patch bump.
- `docs`, `test`, `refactor`, `build`, `ci`, `chore`: maintenance changes that
  do not normally trigger a release by themselves.

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
ticket reference. Release Please uses the merged Conventional Commits to
prepare release notes and choose the next semantic version.

## Local hook setup

Install the repo-managed Git hooks once per clone:

```powershell
.\scripts\install-git-hooks.ps1
```

This configures:

```powershell
git config core.hooksPath .githooks
```

After that, local commits are validated before Git creates them. The same rule
also applies to pull-request titles in CI.

## Release process

1. Merge Conventional Commits into the default branch.
2. Review and merge the Release Please pull request. It updates
   `version.json`, `DeviceConfig.h`, and `CHANGELOG.md` together.
3. Create and push the matching tag, for example `v0.3.0`.
4. The firmware release workflow validates the tag, builds both images,
   generates the OTA manifest, and publishes the GitHub Release assets.

`version.json` is the canonical firmware version. The C++ constant is
a generated release companion that is validated against it on every build.
