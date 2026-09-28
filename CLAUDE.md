@AGENTS.md

## Always on

- Run `/ponytail:ponytail` (full) at the start of every session and keep it active: smallest working
  change, reuse existing code first, short explanations.

## Personal fork

- This copy of mod-playerbots is a git working copy of the user's personal fork,
  https://github.com/Yero0/mod-playerbots-personal-fork (branch `master`). Improve bots directly in `src/`;
  still mark every change `// Local change` (it makes upstream merges easy). Commit or push only when asked.
- Record every change, and every upstream PR applied, in
  `.claude/handoff/playerbots-fork-handoff-2026-09-26.md` §3 (local only, gitignored).
- Never build and never run `../AzerothCore/Compile-AzerothCore-Playerbots.ps1`; the user copies files
  to the server and rebuilds manually.
- Nothing can be compiled here: review every C++ change with the `code-reviewer` agent before handing it
  over, always spawned with the Agent tool's `model: "sonnet"`
  parameter (the frontmatter `model:` is ignored here; without the parameter they ran on the session model).
  Not the `code-review` skill: it runs on the session model.
