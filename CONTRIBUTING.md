# Contributing

Thank you for helping improve Gatito Dtb Pacher.

## Development rules

1. Inspect the current file before changing it.
2. Do not make blind DTB transformations.
3. Never overwrite the original donor or receptor.
4. Prefer structural/semantic operations over text substitution.
5. Keep the native C++ implementation as the single active implementation.
6. Every change that affects DTB generation should include a test or a clearly documented validation procedure.
7. Keep third-party source and license notices intact.
8. Do not commit private DTBs, firmware dumps, proprietary game files, or personal data.

## Native build

The build environment may use MSYS2, GCC, Flex and Bison, but these are build-time tools. They are not intended to be required by the end user.

## Testing priorities

When changing the DTB engine, validate at least:

- ordinary properties;
- strings and string lists;
- byte arrays;
- cell arrays;
- phandles;
- GPIO and pinctrl;
- audio-routing;
- display and backlight;
- nodes with unit addresses;
- labels and path references;
- repeated patch generation.

## Pull requests

Explain what changed, why it changed, how it was tested, which DTB structures were exercised, and whether the generated DTB was compared against the expected result.

Small, focused pull requests are preferred.
