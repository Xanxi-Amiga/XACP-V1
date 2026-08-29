# XACP XX19a firmware source tree

This directory is the source/build tree used for the Xanxi XX19a firmware, implementing the XACP v1.6 public baseline on the MNT ZZ9000.

## Origin

The firmware is derived from the MNT ZZ9000OS source tree and was built in the upstream ZZ9000 firmware 2.0.1 SDK environment used during XACP development. Existing upstream copyright, SPDX and attribution headers are preserved in the individual source files.

XX19a adds and integrates XACP services and fixes on top of that base, including the ZZMIDI TinySoundFont/TinyMidiLoader service and the current Core1/XACP support paths.

## Source layout

- `ZZ9000OS/` - main firmware source and Makefile.
- `ZZ9000FSBL/` - Zynq first-stage bootloader source.
- `zz9000_bsp_new/` - Xilinx BSP used by the build.
- `zz9000_ps_wrapper_hw_platform_0/` - hardware platform support files.
- `../util/make_bootrom_image.c` - host-side boot-ROM image builder required by the ZZ9000OS Makefile.

Generated `ZZ9000OS/build/` output is intentionally not included.

Experimental source files that were present in the development workspace but are not part of the XX19a Makefile (including the abandoned reSID experiment and obsolete transitional XACP memory-map files) are also omitted from this publication tree.

## Third-party components

Third-party code retains its original license and attribution. Notable components include:

- MNT ZZ9000OS: GPL-3.0-or-later where indicated by the original source headers.
- Additional upstream components retain their original copyright and SPDX notices.
- TinySoundFont (`tsf.h`) by Bernhard Schelling: MIT License.
- TinyMidiLoader (`tml.h`) by Bernhard Schelling: zlib License.
- Other bundled/vendor sources retain their own notices and licensing terms.

## Build

Keep `ZZ9000_proto.sdk/` and `util/` as sibling directories. From `ZZ9000OS/`, use the supplied Makefile with a compatible ARM GNU Toolchain and the bundled Xilinx BSP. The resulting ELF is then packaged into the ZZ9000 boot image with the same Zynq boot-image tooling used for the firmware release.

The public XACP memory-map reference is maintained separately in the repository SDK documentation.
