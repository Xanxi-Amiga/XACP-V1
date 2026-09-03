# ZZ9000 AmigaAMP Engine - licensing and provenance

The XACP `zz9000.engine` work is published with the agreement of Thomas Wenzel under the **GNU Lesser General Public License v2.1 or later** (`LGPL-2.1-or-later`) for the EngineLibrary/XACP code controlled by Thomas Wenzel and Xanxi.

## Credits

- **Thomas Wenzel** - original AmigaAMP EngineLibrary framework and original ZZ9000 backend stub.
- **Xanxi** - ZZ9000/XACP backend implementation, integration and real-hardware validation, 2026.

The original attribution comments contained in the source are preserved.

## Two source trees

The repository deliberately retains two source trees:

- `zz9000-engine-source/current/` - the clean XACP-only build intended for future builds.
- `zz9000-engine-source/legacy-2026-06/` - the source corresponding to the historical June 2026 development build that produced the originally distributed XACP engine.

The historical build linked additional `amp`, `equalizer` and `fifo` objects from the original EngineLibrary development tree. These objects are not required by the XACP backend, because MPEG audio decoding is performed by the ZZ9000 ARM.

Those legacy third-party sources carry their own original notices and licensing terms. For that reason, the historical binary is preserved for reproducibility but is **not represented as an LGPL-only binary**.

## Startup framework attribution

`StartUp.c` and `LibInit.c` contain their original statement that they are based on **CLib37x by Andreas R. Kleinert**. That attribution is retained unchanged. No additional licensing claim is made here for third-party material beyond the notices present in the source itself.
