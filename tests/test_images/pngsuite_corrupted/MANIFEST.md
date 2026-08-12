# Malformed PNG files for slp_png error-handling tests

Source: https://github.com/lunapaint/pngsuite.git ("x"-prefixed files — PngSuite's
intentionally-invalid set). Every file here should make `slp_png_read` return a
non-zero error code, never crash and never succeed. Verified by inspecting each
file's bytes directly (not by running slp_png):

| File | What's actually wrong | Expected failure point in slp_png |
|---|---|---|
| `xs1n0g01.png` | Signature byte 0 altered | `read_ihdr`: `PNG_SIGNATURE` mismatch |
| `xs2n0g01.png` | Signature byte 1 altered | same |
| `xs4n0g01.png` | Signature byte 3 altered | same |
| `xs7n0g01.png` | Signature byte 6 altered | same |
| `xcrn0g04.png` | Signature bytes 5 & 7 altered (simulated CR-corruption from a text-mode transfer) | same |
| `xlfn0g04.png` | Signature byte 4 altered **and** IHDR chunk length field wrong (10 instead of 13) | signature check, or `big_edian_u32(ihdr+8) != 13` |
| `xhdn0g08.png` | IHDR chunk CRC is wrong (IDAT's CRC is fine) | `read_ihdr`: CRC mismatch |
| `xcsn0g01.png` | IDAT chunk CRC is wrong (IHDR's CRC is fine) | CRC check inside `idat_decode` |
| `xc1n0g08.png` | color type = 1 (not a valid PNG color type) | `get_channels()` returns -1 → `INVALID_PNG` |
| `xc9n2c08.png` | color type = 9 (not a valid PNG color type) | same |
| `xd0n2c08.png` | bit depth = 0 (invalid for color type 2) | same |
| `xd3n2c08.png` | bit depth = 3 (invalid for color type 2) | same |
| `xd9n2c08.png` | bit depth = 99 (invalid for color type 2) | same |
| `xdtn0g01.png` | IDAT chunk is missing entirely | `IEND` handler: `!idat_check` → `INVALID_PNG` |

All 14 have well-formed IEND chunks and otherwise-plausible structure, so they
specifically exercise validation logic rather than just crashing on truncated input.
