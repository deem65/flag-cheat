# Validation record

## What ran

The actual C++ `Image`, `Matcher`, `FileIO`, and `LuaPolicy` implementations and
the bundled Lua 5.4.8 interpreter were compiled for a **Linux-only test runner**
using GCC/G++ and optimization `-O2`. The Windows desktop application was not
compiled. PNG test inputs were decoded with Pillow, composited on white, and fed
to the same matching code as BGRA pixels. This does not exercise Windows WIC.

| Check | Observed result |
| --- | --- |
| Supplied Tonga image, 640 x 320 | `tonga`, unambiguous; about 4.7 ms |
| Supplied Albania image, 640 x 457 | `albania`, unambiguous; about 5.0 ms |
| Smaller, JPEG-quality-70 variants of both examples | Correct, unambiguous; about 3.5 ms each |
| All 254 original reference images | 250 correct first candidates; all 254 correct codes included |
| The same 254 flags reduced to 80 pixels wide | 250 correct first candidates; all 254 correct codes included |
| Ambiguity in each catalog sweep | 7 inputs marked ambiguous, covering the identical-artwork groups |
| Norway/Bouvet duplicate check | Both retained; result marked ambiguous |
| Solid white, red, and black images | Marked uncertain |
| Fully transparent pixel | Composited on white |
| Excessive decoded dimensions | Rejected before buffer allocation |
| Portable test compile | No GCC/G++ warnings from the application test code |
| Solution/project structure | All project XML parsed; every listed source/reference path exists |
| Reference archive | All 254 cached PNGs agree with the recorded SHA-256 hashes |

The four non-first answers belong to groups sharing identical artwork. The other
member of each group can sort first. These are not uniquely solvable from pixels.

The 80-pixel sweep's matching median was **3.7 ms**, with **4.9 ms at the 95th
percentile** in this environment. Timing covers feature calculation plus native
comparison and Lua policy. It excludes clipboard access, file/PNG decoding,
startup cache loading, UI painting, and Windows scheduling. It is not a Windows
end-to-end performance guarantee.

The catalog sweeps are synthetic checks against the source artwork, not an
independent real-world accuracy benchmark. The only supplied real bot inputs
tested were Tonga and Albania. Old bot artwork, different designs, or substantial
cropping can still cause wrong or uncertain results.

The optional PowerShell refresh script was reviewed but was not executed in a
Windows PowerShell session. It is not needed to use the bundled reference cache.

## Windows checks still required

1. Open the solution in Visual Studio 2022. Build **Release | x64** and run drek_flag_cheat.
2. Copy `tests/fixtures/tonga.png` from an image viewer and paste. Confirm `tonga`.
3. Copy a flag directly from Discord and paste. Confirm image preview and answer.
4. Copy the answer and paste into a text editor. Check that it is lowercase and
   contains no code, prefix, or newline. Test an accented name such as Åland Islands.
5. Paste with focus in the result field, then with focus on a button. Test Ctrl+C.
6. Paste several images quickly, then press Esc during recognition. Old results
   should never replace the latest result or reappear after clearing.
7. Paste plain text/a link. The previous answer should clear and an image-copy
   message should appear. Try a clipboard bitmap from a screenshot tool as well.
8. Use a shared-artwork flag such as Norway. Confirm the ambiguity list and copy
   a selected alternative.
9. Resize the window and move it between monitors at 100%, 150%, and 200% scaling.
   Confirm the preview stays proportional and buttons/text remain usable.
10. Build and run the separate **drek_flag_cheat.Tests** project to repeat the core tests
    using the actual Windows WIC decoder.

## Test fixtures

`tonga.png` and `albania.png` are the supplied image copies. The `-jpeg.png`
fixtures contain pixels after resizing to fit 160 x 120, encoding at JPEG quality
70, and decoding again to PNG. Saving those decoded pixels as PNG keeps the
test files reproducible without requiring JPEG encoding during the test run.
