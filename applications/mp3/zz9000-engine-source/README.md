# AmigaAMP `zz9000.engine` source for XACP

This directory contains the source history for the AmigaAMP external `zz9000.engine` used with the MNT ZZ9000 and XACP.

## Layout

```text
current/
legacy-2026-06/
```

### `current/`

Clean XACP-only source intended for future builds.

The build links only the AmigaAMP EngineLibrary framework and the ZZ9000/XACP backend. MPEG audio decoding is performed by the XACP service on the ZZ9000 ARM, so the old local software MPEG decoder, equalizer and FIFO implementation are not linked.

The EngineLibrary/XACP work is distributed under **GNU LGPL v2.1 or later** (`LGPL-2.1-or-later`), subject to any third-party notices retained in individual source files.

### `legacy-2026-06/`

Historical source corresponding to the June 2026 development build of `zz9000.engine` that was distributed in early XACP releases.

That Makefile linked the original EngineLibrary `amp`, `equalizer` and `fifo` objects in addition to the XACP backend. They were kept in the link during development even though the XACP backend does not call the local MPEG decoder.

Those additional sources retain their original notices and licensing terms. This directory is retained for source correspondence, provenance and reproducibility. It should not be treated as a single-license LGPL source tree.

## Historical binary identification

The historical binary in the June 2026 development directory was:

```text
Filename: zz9000.engine
Size:     99660 bytes
MD5:      265d0bb4b49b114e3504c24b94ded271
SHA-256:  8e44ef257933a214d34b7b255a0584d0acaef6bdec1a8ec7724bff6416d14334
```

The historical GitHub releases should remain unchanged. Future binaries should be built from `current/` and released as a new version after real-hardware validation.

## Credits

- Thomas Wenzel - original AmigaAMP EngineLibrary framework and original ZZ9000 backend stub.
- Xanxi - ZZ9000/XACP backend implementation, integration and real-hardware validation.

See the parent `NOTICE_ZZ9000_ENGINE.md` and `LICENSE_ZZ9000_ENGINE.txt`.
