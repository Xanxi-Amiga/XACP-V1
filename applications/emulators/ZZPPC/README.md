# ZZPPC

**Experimental PowerPC execution on the ARM Cortex-A9 Core1 of the MNT ZZ9000.**

## Proof of concept

On 6 September 2026, ZZPPC reached its first hardware proof of concept: the original and unmodified `FlashMandelWOS_No68k_FPU` application successfully rendered a Mandelbrot fractal on a real Amiga 4000 without any physical PowerPC accelerator.

Test configuration:

- Amiga 4000
- Phase 5 Cyberstorm MK2 with Motorola 68060 at 50 MHz
- MNT ZZ9000
- XACP firmware
- no BlizzardPPC or CyberStormPPC
- original FlashMandelWOS application from Aminet
- experimental ZZPPC interpreter running on ZZ9000 Core1

This is not an ARM-native port of FlashMandel. The original application and its original PowerPC/FPU workload are used. An experimental replacement `powerpc.library` transfers the PowerPC execution request through XACP to the second ARM core of the ZZ9000.

The ZZPPC interpreter executes the required big-endian PPC32 integer and floating-point instructions. The result is returned to the original Amiga application, which displays the completed fractal through the ZZ9000 RTG framebuffer.

## First results

During the initial 320x240 test:

- ZZPPC interpreted PPC/FPU calculation: approximately 33 seconds
- native 68060/50 FPU calculation: approximately 8 seconds
- one completed PPC run reported approximately 117 million interpreted instructions
- FlashMandel reported approximately 14,455 pixels per second for the PPC path

These are preliminary measurements from a deliberately simple interpreter. They are not estimates of final ZZPPC performance and must not be interpreted as an equivalence with a particular PowerPC processor.

## Demonstration

[Watch the first raw hardware demonstration](https://youtu.be/lP0kV6TtkcY)

The recording shows the original FlashMandelWOS application calculating and displaying its fractal on the Amiga while its PowerPC/FPU workload is executed by ZZPPC on the ZZ9000.

## Architecture

```text
Original Amiga 68k/PPC application
                |
    experimental powerpc.library
                |
               XACP
                |
       ZZ9000 ARM Core1
                |
     custom PPC32/FPU interpreter
```

The interpreter was written specifically for this experiment. It does not incorporate code from QEMU, PearPC, Dolphin, Emu68 or another PowerPC emulator.

## Current limitations

ZZPPC is currently an application-specific proof of concept. It is not yet:

- a complete PowerPC 603/604 emulator;
- a complete WarpOS implementation;
- compatible with arbitrary WarpOS or PowerUP applications;
- a replacement for a physical PowerPC accelerator;
- optimised for performance;
- ready for general public testing.

Only the PPC32 integer and FPU instruction subset required by the current tests is implemented. Unsupported PowerPC instructions still terminate an execution request.

The current prototype has no clean Core1 shutdown mechanism. It must not be initialized while another conflicting XACP/Core1 application is active, and a complete power cycle is recommended after testing.

## Availability

There is currently no public binary release. A testable alpha may follow after startup, shutdown, diagnostics and XACP resource coordination have been made sufficiently safe.

The source code is not public at this stage. The publication policy and final licence may change as development progresses.

## Project status

This first experiment establishes that the second ARM core of the ZZ9000 can execute original Amiga PowerPC/FPU workloads and return their results to a real Amiga application without a physical PowerPC processor.

Development will proceed cautiously. The immediate priorities are reliability, clean shutdown, clearer diagnostics and broader instruction coverage before performance optimisation or support for additional applications.
