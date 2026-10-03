@AGENTS.md

## Always on

- Run `/ponytail:ponytail` (full) at the start of every session and keep it active: smallest working
  change, reuse existing code first, short explanations.

## Personal fork

- This copy of mod-playerbots is a git working copy of the user's personal fork,
  https://github.com/Yero0/mod-playerbots-personal-fork (branch `master`). Improve bots directly in `src/`;
  still mark every change `// Local change` (it makes upstream merges easy). Commit or push only when asked.
- Record every change, and every upstream PR applied, in
  `.claude/handoff/playerbots-fork-handoff-2026-10-03.md` §3 (local only, gitignored).
- Never build and never run `../AzerothCore/Compile-AzerothCore-Playerbots.ps1`; the user copies files
  to the server and rebuilds manually.
- Nothing can be compiled here: review every C++ change with the `code-reviewer` agent before handing it
  over, always spawned with the Agent tool's `model: "sonnet"`
  parameter (the frontmatter `model:` is ignored here; without the parameter they ran on the session model).
  Not the `code-review` skill: it runs on the session model.

## Model routing

The session runs on Sonnet (`/model sonnet`) and routes work to tier agents in `.claude/agents/`.
Always pass the Agent tool's `model:` parameter (frontmatter is ignored here).

| Task | Agent | `model:` |
|---|---|---|
| Lookups, grep sweeps, reading logs/pmon for values, dictated text edits | `tier-haiku` | `"haiku"` |
| Single-feature fix (1-2 files), porting PR lines, a config option like an existing one, SQL texts, gh PR reading | `tier-sonnet` | `"sonnet"` |
| New features (3+ files), design decisions, root-causing behaviour bugs, cross-thread/core-API work, merges with conflicts, pmon analysis | `tier-opus` | `"opus"` |

- Handle inline: answers, git status/commit/push, handoff updates, relaying results.
- Bump one tier for ambiguity, map-thread or core-API risk, or data loss; unsure → the higher tier.
- `ESCALATE:` reply → re-delegate one tier up with its findings. Brief agents fully: they don't see
  the conversation.
- C++ changes still go through `code-reviewer` (`model: "sonnet"`) before hand-over.
- Don't route when the user names a model or says to do it directly.
