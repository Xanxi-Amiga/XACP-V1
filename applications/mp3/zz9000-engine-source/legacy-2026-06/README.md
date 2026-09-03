# Legacy June 2026 `zz9000.engine` source

This directory preserves the source corresponding to the original XACP AmigaAMP engine development build distributed in June 2026.

It is retained to document exactly how the historical binary was built. **Do not use this tree as the basis for new releases.** Use `../current/` instead.

## Historical binary

```text
Filename: zz9000.engine
Size:     99660 bytes
MD5:      265d0bb4b49b114e3504c24b94ded271
SHA-256:  8e44ef257933a214d34b7b255a0584d0acaef6bdec1a8ec7724bff6416d14334
```

## Why this tree contains extra decoder code

The historical `makefile-xacp` links:

```text
StartUp.o LibInit.o Funcs.o EngineAccess.o iEngine_ZZ9000.o MyStdlib.o
fifo/fifo.o
equalizer/equalizer.o
amp/*.o
```

The XACP backend itself performs MPEG decoding on the ZZ9000 ARM and does not need the local `amp`, `equalizer` or `fifo` decoder path. Those objects remained linked during the original development build as a conservative way to avoid unresolved references.

The clean current build removes them.

## Licensing

This legacy tree is **not a single-license LGPL tree**.

Thomas Wenzel and Xanxi have agreed to distribute the EngineLibrary/XACP work under **GNU LGPL v2.1 or later**, but the historical tree also contains third-party material carrying its own original notices and licensing terms, including:

- the `amp/` MPEG decoder by Tomislav Uzelac and contributors; see `amp/amp.readme` for its original legal terms;
- `equalizer/equalizer.c`, carrying its original GNU GPL notice and copyright/attribution;
- `amp/rtbuf.h`, carrying its original GNU GPL v2-or-later notice from Thomas Sailer;
- startup code retaining the statement that it is based on CLib37x by Andreas R. Kleinert.

All original notices are retained in the files themselves. No attempt is made here to relicense third-party material.

## Case-sensitive filesystems

The historical tree contains `compiler.h`, while some EngineLibrary files include it as `Compiler.h`. The original build environment was case-insensitive. An identical `Compiler.h` convenience copy is included here so the source is easier to inspect/build on case-sensitive hosts; the historical `compiler.h` is preserved unchanged.
