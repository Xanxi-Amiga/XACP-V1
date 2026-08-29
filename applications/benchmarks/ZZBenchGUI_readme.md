# ZZBench GUI

**SysInfo-style dual benchmark for the ZZ9000 RTG baord : your Amiga's 68k CPU vs the card's ARM Cortex-A9, side by side.**

ZZBench runs the same integer (Dhrystone 2.1) and floating-point (Whetstone)
kernels on both processors of a ZZ9000-equipped Amiga - the 68k host and the
ARM Core1 on the card's Xilinx Zynq - and shows MIPS / MWIPS for each, with the
ARM/68k speed ratios.

---

## Highlights

- **One window, both CPUs.** Comparison bars for integer (MIPS) and floating
  point (MWIPS), with raw Dhrystones/sec and the ARM/68k ratios.
- **Single universal binary.** `ZZBenchGUI` is built for `-m68020 -m68881` and
  runs on **68020 / 68030 / 68040 / 68060**. The real CPU and FPU are detected
  at runtime.
- **Self-contained.** The ARM benchmark blob is embedded in the executable -
  nothing else to copy.
- **Honest numbers.** Peak-performance measurement, not a CPU-usage meter.
  Best-of-3 passes, task switching disabled during each 68k pass.

---

## Requirements

- An **MNT ZZ9000** (Zynq Z-7020). The **ZZ9000AX add-on is not required**.
- **AmigaOS 2.04+** (Kickstart 37 or later).
- A **68020 or newer** CPU. An FPU (68881/68882, or the integrated 68040/68060
  FPU) is used for the floating-point test; **without an FPU that test is
  skipped** and shown as `no FPU`, the integer test still runs.
- ZZ9000 firmware that exposes the Core1 launch path used by the ARM side.
  Developed and tested against the author's ZZ9000 firmware.

---

## Usage

Double-click the icon, or run `ZZBenchGUI` from a Shell. The benchmark starts
immediately.

- **The machine freezes for a few seconds during the 68k passes - this is
  normal.** Task switching is disabled while measuring so other tasks can't
  steal cycles.
- **Test** re-runs the benchmark. **Quit** closes the window.

No data files are needed - the ARM blob is embedded.

---

## Reading the results

**Integer (Dhrystone 2.1)** - reported in MIPS plus raw Dhrystones/sec:

```
MIPS = Dhrystones per second / 1757
```

1757 Dhrystones/sec = the VAX-11/780, the historical "1 MIPS" reference. A
68060@50 landing around 45-55 MIPS is normal - that is VAX-normalized
Dhrystone, not marketing MHz.

**Floating point (Whetstone)** - reported in MWIPS.

Example on a reference **Amiga A4000 / 68060@50** with a ZZ9000 (ARM ~666 MHz):

| Test                | 68k (68060) | ARM Core1   | ARM / 68k |
|---------------------|-------------|-------------|-----------|
| Integer (MIPS)      | ~52         | ~1059       | ~x20      |
| Float (MWIPS)       | ~47         | ~917        | ~x19      |

The ARM figures are identical on every run; the 68k figures vary by ~1-2% (see
below).

---

## How it measures

**68k side.** Unmodified Dhrystone 2.1 (Weicker) and Whetstone kernels, timed
with the CIA E-Clock via `ReadEClock()`. The run length is auto-calibrated to a
~3-second target, each test is run **best-of-3** (the fastest pass = the peak),
and `Forbid()/Permit()` brackets each pass to remove task-switch jitter. All
timing and scoring use 64-bit integers; the only floating point is inside the
Whetstone kernel itself.

**ARM side.** The same kernels run **bare-metal on Core1**, timed by the
Cortex-A9 cycle counter. No OS, no multitasking - so the ARM result is
deterministic and repeatable to the cycle. That is why the ARM numbers never
change between runs while the 68k numbers wobble slightly: under AmigaOS,
hardware interrupts still steal a variable handful of cycles even with task
switching disabled.

---

## Notes on the numbers (honest)

**The floating-point path.** A 68060 has **no hardware** `sin`/`cos`/`exp`/`log`
- those trap to `68060.library` (Motorola FPSP). To keep the MWIPS figure
**consistent across CPUs and build targets**, ZZBench emits the FPU
transcendental instructions directly, so the path is always *"FPU + the
system's math support"*: the FPSP emulation on a 68060, real hardware on a
68881/68882/68040. On a 68060 it is therefore neither pure software nor pure
hardware - that is a property of the chip, not of the benchmark.

**The ARM "softfp".** The header shows `softfp` - that is the **ABI**, not
soft-float. The Cortex-A9 VFP is fully used for the maths; softfp vs hard only
changes how float arguments are passed across function calls.

---

## License

ZZBench is closed-source freeware.

Copyright (C) 2026 Xanxi.

Redistribution is permitted only in unmodified form and with this
documentation included.

Modification, reverse engineering, repackaging, or redistribution of
modified binaries is not permitted without prior authorization.

The source code is not included or published.

Dhrystone 2.1 and Whetstone remain subject to their respective original
authorship and notices.

---

## Credits

- **Xanxi**, 2026. Part of the **XACP** (Xanxi ARM Coprocessor Protocol) work
  for the MNT ZZ9000.
- Dhrystone 2.1 by Reinhold P. Weicker; Whetstone by Curnow & Wichmann - both
  long-standing public benchmarks.
- MNT ZZ9000 hardware by MNT Research.



---

## Credits

- **Xanxi**, 2026. Part of the **XACP** (Xanxi ARM Coprocessor Protocol) work
  for the MNT ZZ9000.
- Dhrystone 2.1 by Reinhold P. Weicker; Whetstone by Curnow & Wichmann - both
  long-standing public benchmarks.
- MNT ZZ9000 hardware by MNT Research.


