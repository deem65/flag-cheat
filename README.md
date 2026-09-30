# FlagCheat
FlagCheat is a small windows utility application built on WinGDI, C++, Lua and WIC

FlagCheat works on Windows only since it uses windows' libraries. 

Paste or drag in a flag image and FlagCheat compares it against its reference flags, returning the most likely country or territory.

## Why I made it
FlagCheat was made to cheat in .flag command by drek's bot. It's fast, small, and it works. 

## How it works
FlagCheat keeps a collection of reference flag images and compares an input image against them.

When an image is pasted or dropped into the application:

1. The image is decoded.
2. Cmpared against the reference flag images.
3. Calculate similarity.
4. Lua ranks the possible matches.
5. The country or territory name is displayed.
6. If the result is ambiguous, FlagCheat shows the closest candidates instead.

## Built with
FlagCheat is primarily built with:

- **C++**
- **Win32 API** for the native Windows interface
- **WIC** for image decoding
- **Lua** for recognition/scoring policy
- **GDI** for displaying image previews

## Platform
**Windows only.**
