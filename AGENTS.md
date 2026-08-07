# Cautious Coding Guidelines

Use these guidelines as project-local behavioral guardrails. Bias toward caution over speed, while using judgment for trivial tasks.

## Skill Routing

Use `.codex/skills/esp32-bringup` for first-time board setup:

Use `.codex/skills/esp32-develop` only after the board already builds, flashes, and prints serial logs:

If unsure, start with `.codex/skills/esp32-bringup`.

## Product Alignment

Before changing Wi-Fi setup behavior, REST API scope, admin authentication, or first-use flow, read `docs/SPEC_BEHAVIOR.md`.

Keep `docs/SPEC_BEHAVIOR.md` aligned with firmware product decisions. For `builtin-web/` behavior use `docs/SPEC_FRONTEND_BEHAVIOR.md`; `docs/SPEC_API_REFERENCE.md` remains the external REST contract.

Ask at most 5 requirement questions per round. Update the corresponding behavior SPEC with settled behavior and open questions; requirement discussion history belongs in ignored `tmp/` only when it remains useful.

Record build, flash, filesystem upload, browser checks, and board smoke-test results under ignored `tmp/verification/`; verification history must not be added to `docs/`.

Before changing the built-in setup UI, read `docs/SPEC_FRONTEND_BEHAVIOR.md` and `docs/SPEC_FRONTEND_TECHNICAL.md`; keep `builtin-web/index.html` as structure only and put styling, state, i18n, API client, views, and feature actions under `builtin-web/assets/`.

For every built-in frontend change, edit only the source under `builtin-web/`. Treat `user-web/` as user-supplied input and `build/latest/web/` as generated output; never implement or repair a frontend requirement by editing generated files, and never use stale build output to judge whether the source change was made.

Before adding or changing REST/serial API behavior, read `docs/SPEC_API_REFERENCE.md` and `docs/SPEC_TECHNICAL.md`; keep API business logic in `ApiRouter` and use `ApiServer`/`ConsoleShell` only as adapters.

## Minimal Prompts

For a new board:

```text
Use the esp32-bringup skill.
```

After smoke-test firmware is proven:

```text
Use the esp32-develop skill.
```

## Safety Boundary

Never guess board-specific pins, flash settings, USB settings, or boot-sensitive GPIO behavior. Let the selected skill guide official spec lookup and hardware checks.
