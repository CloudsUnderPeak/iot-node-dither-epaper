---
name: docs
description: Maintain Embedded Web Dithering documentation. Use when the user invokes `/docs`, `$docs`, or asks to summarize changes, update project documentation, synchronize README language variants, or update docs/SPEC_INDEX.md, docs/SPEC_BEHAVIOR.md, and docs/SPEC_TECHNICAL.md after product or technical changes.
---

# Docs

Use this skill to keep project documentation coherent after changes.

## Required Inputs

Read these files before editing documentation:

- `docs/SPEC_INDEX.md`
- `docs/SPEC_BEHAVIOR.md`
- `docs/SPEC_TECHNICAL.md`
- `README.md`
- `README.zh-TW.md`

If any file is missing, recreate only when the user asked for it or the existing docs clearly require it.

## Workflow

1. Inspect the current change set with `git status --short`.
2. Review relevant diffs before editing docs:
   - Use `git diff -- README.md README.zh-TW.md docs`.
   - Use targeted `git diff -- <path>` for changed source files that affect docs.
3. Classify the change:
   - User-visible behavior, flows, UI behavior, milestones, or acceptance criteria -> update `docs/SPEC_BEHAVIOR.md`.
   - Architecture, module boundaries, state, pipeline, storage, implementation constraints -> update `docs/SPEC_TECHNICAL.md`.
   - File map, document roles, or agent/skill maintenance rules -> update `docs/SPEC_INDEX.md`.
   - User-facing project overview, setup, scope, or docs map -> update both README language files.
4. Keep `README.md` as the English entry and `README.zh-TW.md` as the Traditional Chinese version.
5. Keep README language variants structurally equivalent:
   - Same major sections.
   - Same links.
   - Same project facts.
   - Natural wording per language; do not write both languages in one README.
6. Preserve the spec split:
   - Do not put long behavior or technical requirements in `docs/SPEC_INDEX.md`.
   - Do not duplicate detailed implementation rules into `README.md`.
7. After edits, verify:
   - No stale docs filenames remain.
   - Markdown links resolve.
   - English and Traditional Chinese README files still cross-link.

## Output Summary

When done, report:

- Which docs were updated.
- Which product or technical changes were reflected.
- Any docs gaps or assumptions that remain.
