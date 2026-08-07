---
name: docs
description: Maintain IOT-Node-Bedrock project documentation. Use when the user invokes `/docs` or `$docs`, asks to summarize implemented changes, update or review project specifications, synchronize the English and Traditional Chinese README files, or align documentation after firmware, frontend, REST API, serial console, build, or deployment changes.
---

# Docs

Keep the tracked documentation aligned with the current product and implementation without turning specifications into verification logs.

## Establish context

1. Read `AGENTS.md` and follow its product-alignment and source-of-truth rules.
2. Inspect `git status --short`, staged and unstaged diffs, and relevant untracked files.
3. Read `docs/SPEC_INDEX.md` to identify the authoritative document for each changed concern.
4. Read only the relevant specifications, source files, and build-tool documentation needed to understand the change. Do not infer current behavior from generated `build/` output.
5. Distinguish settled behavior from proposals, open questions, local board facts, and dated verification results. If product intent remains ambiguous, ask no more than five requirement questions in one round before documenting it as settled.

## Route changes

- Update `docs/SPEC_BEHAVIOR.md` for product positioning, firmware-visible behavior, Wi-Fi lifecycle, authentication, storage behavior, system behavior, security boundaries, and firmware product questions.
- Update `docs/SPEC_FRONTEND_BEHAVIOR.md` for `builtin-web/` user flows, information architecture, page behavior, copy, responsive behavior, accessibility, preview experience, acceptance criteria, and frontend product questions.
- Update `docs/SPEC_TECHNICAL.md` for firmware architecture, module responsibilities, state ownership, persistence, REST/serial dispatch, coding constraints, and embedded limitations outside `builtin-web/`.
- Update `docs/SPEC_FRONTEND_TECHNICAL.md` for `builtin-web/` and `user-web/` structure, frontend layers, state and routing, REST client boundaries, preview/mock behavior, web outputs, deployment, and frontend verification requirements.
- Update `docs/SPEC_API_REFERENCE.md` whenever the external REST or serial API contract changes, including methods, paths, authentication, payloads, responses, status codes, error codes, and transport support.
- Update `docs/SPEC_CONSOLE_REFERENCE.md` for serial connection instructions, human commands, parser behavior, token usage, and console examples. Keep shared API payload and response details in `SPEC_API_REFERENCE.md`.
- Update `docs/SPEC_INDEX.md` only when document roles, reading order, tracked-document policy, or the documentation map changes.
- Update both `README.md` and `README.zh-TW.md` when the public overview, audience, features, setup, build commands, demo, deployment, limitations, or documentation entry points change.

Apply every relevant route when a change crosses boundaries. Keep product behavior, public contracts, and implementation details in their respective documents instead of duplicating them.

## Edit documentation

- Treat the current specifications as authoritative product decisions and reconcile them with the actual source change. Do not silently choose between conflicting code, requirements, and specifications when the intended result is unclear.
- Document observable behavior in behavior specifications, exact integration contracts in reference documents, and implementation constraints in technical specifications.
- Keep `builtin-web/` as the built-in frontend source of truth. Treat `user-web/` as user-supplied input and describe `build/latest/web/` only as generated verification or publication output.
- Keep `ApiRouter` business behavior separate from the `ApiServer` and `ConsoleShell` adapter descriptions.
- Keep README files concise and newcomer-oriented. Preserve equivalent major sections, links, commands, project facts, and warnings while using natural language in each locale.
- Record settled requirements and explicit open questions in the appropriate behavior specification. Put superseded plans and useful requirement-discussion history under ignored `tmp/`.
- Put build, flash, filesystem upload, browser, board smoke-test, dated measurement, and pass/fail records under ignored `tmp/verification/`; never present them as durable specifications.
- Keep local board observations in ignored `board-profile.local.md`; never link tracked documentation to that file.
- Do not claim that a feature, command, workflow, test, or deployment path exists unless the repository or supplied verification evidence supports it.

## Verify

1. Review the documentation diff for accidental scope expansion and stale project terminology.
2. Confirm Markdown links and referenced paths resolve.
3. Confirm `README.md` and `README.zh-TW.md` cross-link and remain structurally and factually equivalent.
4. Confirm API and console examples match the documented method, path, authentication, payload, response envelope, and transport rules.
5. Confirm documentation does not treat generated output, local board state, or dated verification results as a source of truth.
6. Run only documentation-specific checks that are available and relevant. Do not rerun hardware or release verification merely to produce a documentation summary.

## Report

Summarize the files updated, the product or technical decisions reflected, the checks performed, and any unresolved documentation gaps or assumptions. Mention verification records separately from tracked specifications.
