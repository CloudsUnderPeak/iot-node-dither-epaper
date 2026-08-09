---
name: staged-commit
description: Create a commit from staged changes. Use when the user invokes `/commit`, `/commit all`, `$staged-commit`, or asks Codex to inspect the staging area, generate a `[Add]`, `[Modify]`, or `[Fix]` commit title with numbered details, and commit staged changes. `/commit all` first stages all current changes.
---

# Staged Commit

Use this skill to commit the current staging area. `/commit all` is the only variant that stages files before committing.

## Rules

- For plain `/commit`, never run `git add`.
- For `/commit all`, run `git add --all` before inspecting the staged diff.
- Never include unstaged changes in a plain `/commit` commit.
- Inspect staged changes with `git diff --cached`.
- If there are no staged changes, stop and tell the user to stage files first.
- If unstaged changes exist, mention them after the commit plan or final commit summary, but do not modify them.
- If the staged diff mixes unrelated intentions, ask the user to split the staging area before committing.

## Message Format

Use this format:

```text
[type] summary
1. Concise change detail
2. Concise change detail
```

Allowed types:

- `[Add]`: new capability, new documentation, new config, new structure, new asset, or intentional expansion.
- `[Modify]`: change existing behavior, UI, documentation, config, structure, or implementation without primarily adding a new capability or fixing a defect.
- `[Fix]`: bug fix, broken link fix, typo correction, behavior correction, or correction to existing documentation/config.

Title rules:

- Keep the first line concise.
- Do not end the summary with a period.
- Prefer short English summaries.
- The summary does not need to start with an uppercase word.

Body rules:

- Start numbered details on the second line, immediately after the title line.
- Use numbered list items in `1. `, `2. `, `3. ` order.
- Write only the numbered items needed to describe the staged change.
- Keep details brief and avoid explaining obvious implementation steps.
- Keep each numbered item short and implementation-oriented.
- Do not add a blank line between the title and the first numbered item.

Examples:

```text
[Add] preview upload entry
1. Add centered empty-state upload dropzone
2. Route New Image through the file picker
3. Update behavior and technical specs

[Modify] crop control layout
1. Rework existing crop controls into an aligned grid
2. Replace text actions with icon controls
3. Update related specs

[Fix] crop overlay rotation
1. Keep the crop frame centered during rotation
2. Prevent result controls from showing in crop mode
```


## Workflow

1. Run `git status --short`.
2. If the user invoked `/commit all`, run `git add --all`.
3. Run `git status --short`.
4. Run `git diff --cached --stat`.
5. Run `git diff --cached` and infer the main intent.
6. Choose `[Add]`, `[Modify]`, or `[Fix]` by the staged diff's primary intent.
   - Prefer `[Modify]` for changes to existing features, UI, docs, config, or structure that are neither a new capability nor a defect correction.
7. Write concise numbered detail lines that summarize the staged diff.
8. Show the full planned multiline commit message if the user did not explicitly ask to skip confirmation.
9. Commit with exactly the staged changes using the exact multiline message.
   - Prefer a temporary message file plus `git commit -F <message-file>` when needed to preserve the no-blank-line format.
   - Do not use multiple `-m` flags if that would insert a blank line between the title and numbered items.
10. Report the commit hash and mention any unstaged changes left behind.
