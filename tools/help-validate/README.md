# Help Validate

Validates that the runtime capability registry and the bilingual Help content stay aligned.

Run from the repository root:

```bash
python3 tools/help-validate/run.py
```

The validator reports an error when:

- a registered Dither Algorithm has no `helpFamily`;
- English or Traditional Chinese Help is missing details for a registered algorithm;
- a Help limit template or its capability fact is missing;
- repeated named or positional i18n placeholders do not interpolate correctly.

Details for removed algorithms are not shown in Help and are reported as cleanup warnings.
