# XX19a / XACP v1.6 firmware source package

This package contains the publication source tree for the XX19a firmware.

Directory layout:

- `ZZ9000_proto.sdk/` - firmware, FSBL, BSP and hardware-platform source tree.
- `util/make_bootrom_image.c` - host-side source required by the ZZ9000OS Makefile.

The source was prepared from the development tree used for XX19a. Editorial cleanup removes development-history comments and neutralizes internal
diagnostic labels without changing firmware algorithms, memory maps or protocol
behavior.

Generated build objects and obsolete development-only files are intentionally
not included. See `ZZ9000_proto.sdk/README_XACP_XX19a.md` for details.

## Publication cleanup v3

Source comments were cleaned for publication while preserving compiled
strings and executable behavior. Upstream copyright, SPDX and license notices
remain intact.
