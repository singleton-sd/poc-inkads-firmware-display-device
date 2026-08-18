# Skills for InkAds display-device

Do not copy the Singleton SD skill library into this repository. This Cursor
workspace already mounts `ai-plattform-skills`.

Read these before implementing:

| Skill | Path in the skills repo |
| --- | --- |
| Git conventions | `engineering/git-conventions/SKILL.md` |
| Implement feature | `engineering/implement-feature/SKILL.md` |
| Code review | `engineering/code-review/SKILL.md` |
| Task-driven development | `operations/task-driven-development/SKILL.md` |

Local checkouts:

- Workspace folder: `ai-plattform-skills/main`
- Absolute: `C:\00Personal\singleton-sd\ai-plattform-skills\main`

Then apply the firmware overlays in [`AGENTS.md`](../../AGENTS.md): ClickUp
`POC-###` branches, bash scripts, Arduino CLI via `scripts/compile.sh`, and
worktrees under `firmware/worktrees/`.

Do not use poc-plattform-kit GitHub-issue branch names, `pnpm worktree:add`,
preview-scenario PR lines, or Exclusive Claim.