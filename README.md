# MSUpdateExtractor

MSUpdateExtractor extracts Microsoft update packages that contain PSF/PSFX delta payloads.

Supported package layouts:

1. `*.psm` + matching `*.psf`
2. `*.cab` + matching `*.psf`
3. `*.wim` + matching `*.psf`
4. single `*.cab`, including CAB sets described by `cabinet.cablist.ini`

For a single CAB that contains `cabinet.cablist.ini`, the tool parses the ordered `Cabinet1`, `Cabinet2`, ... entries, extracts every child CAB in that order, processes each child CAB's own `_express*.xml` description, and merges the final files into the output directory named after the main CAB without its extension.

## v2 changes

- The embedded libmspack CAB decoder sources are now compiled as C++ translation units: `cabd.cpp`, `lzxd.cpp`, `mszipd.cpp`, `qtmd.cpp`, and `system.cpp`.
- CAB decompression is routed through `MspackCabDecompressor`, a C++ RAII class that owns the libmspack decompressor lifecycle and the wide-character file backend.
- The application-facing `CabArchiveExtractor` is now only a small archive abstraction wrapper; it no longer exposes libmspack function-table callbacks in its public/private interface.
- The minimal libmspack system layer was made C++-safe by removing C-only implicit `void*` conversions and deprecated `register` usage.
- CAB list cleanup now removes only the listed child CAB files and `cabinet.cablist.ini`, preserving unrelated `.ini` files extracted from the main CAB.

All application code uses wide-character Windows path APIs. The embedded CAB backend receives UTF-8 bridge names internally, but every real file operation is performed through `CreateFileW` and extended-length wide paths, so non-ASCII input and output paths no longer pass through `wcstombs`/`fopen`.
