/*
 * XACP - eXtended ARM Coprocessor Protocol
 *
 * Copyright (C) 2026 Xanxi
 *
 * SPDX-License-Identifier: 0BSD
 */


#ifndef XACP_MEMORY_MAP_V1_5_H
#define XACP_MEMORY_MAP_V1_5_H

/*

* XACP v1.5 memory map
*
* eXtended ARM Coprocessor Protocol for the MNT ZZ9000.
*
* XACP v1.5 is a protocol / ABI / shared-memory map baseline.
* Firmware build numbers are separate:
*
* XX16c : MP3 / MP2 / ZZMPEG / early Core1 baseline
* XX18c : ZZDoom baseline
* XX18m : first ZZMIDI line + ZZ9000AX cold-boot init fix
* XX19  : public firmware implementing XACP v1.5
*
* All offsets below are framebuffer-relative unless explicitly marked
* as ARM absolute physical addresses.
*
* fb = board + 0x00010000
*
* Do not move ZZMIDI MIDI staging back to 0x07000000.
  */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Version                                                                   */
/* ------------------------------------------------------------------------- */

#define XACP_VERSION_MAJOR              1UL
#define XACP_VERSION_MINOR              5UL
#define XACP_VERSION_PATCH              0UL

#define XACP_VERSION_U32                0x00010500UL

/* ------------------------------------------------------------------------- */
/* Addressing conventions                                                    */
/* ------------------------------------------------------------------------- */

/*

* Amiga side:
*
* board = cd->cd_BoardAddr
* fb    = board + XACP_MNT_FB_BASE
*
* ARM side:
*
* framebuffer physical base is normally 0x00200000.
* fb-relative offset X maps to ARM physical 0x00200000 + X.
  */

#define XACP_MNT_FB_BASE                0x00010000UL
#define XACP_ARM_FB_PHYS_BASE           0x00200000UL

#define XACP_FB_OFFSET_TO_ARM_PHYS(x)   (XACP_ARM_FB_PHYS_BASE + (x))

/* ------------------------------------------------------------------------- */
/* Zorro / XACP registers                                                    */
/* ------------------------------------------------------------------------- */

#define XACP_REG_CMD                    0x0064UL
#define XACP_REG_STATUS                 0x0064UL

#define XACP_REG_ARM_RUN_HI             0x0090UL
#define XACP_REG_ARM_RUN_LO             0x0092UL

/* ------------------------------------------------------------------------- */
/* XACP opcodes                                                              */
/* ------------------------------------------------------------------------- */

#define XACP_OP_MEMCPY                  0x0001UL
#define XACP_OP_MANDELBROT              0x0002UL
#define XACP_OP_MP3_DECODE              0x0003UL
#define XACP_OP_STREAM_OPEN             0x0004UL
#define XACP_OP_STREAM_CLOSE            0x0005UL

#define XACP_OP_MIDI_SF2                0x0120UL

#define XACP_OP_CORE1_RESET             0x0303UL

/* ------------------------------------------------------------------------- */
/* Generic XACP / MP3 / MP2 shared DDR map                                   */
/* ------------------------------------------------------------------------- */

#define XACP_COMMAND_OFFSET             0x04000000UL
#define XACP_STREAM_OFFSET              0x04002000UL

#define XACP_MP3_RING_OFFSET            0x04100000UL
#define XACP_MP3_RING_SIZE              (512UL * 1024UL)
#define XACP_MP3_RING_END               (XACP_MP3_RING_OFFSET + XACP_MP3_RING_SIZE)

#define XACP_PCM_RING_OFFSET            0x04200000UL
#define XACP_PCM_RING_SIZE              (1UL * 1024UL * 1024UL)
#define XACP_PCM_RING_END               (XACP_PCM_RING_OFFSET + XACP_PCM_RING_SIZE)

/* ------------------------------------------------------------------------- */
/* Core1 application area                                                    */
/* ------------------------------------------------------------------------- */

/*

* This area is application-dependent.
*
* It has been used by JuliaV2, ZZMPEG, ZZDoom and benchmark blobs.
* Do not treat it as a generic scratch area without checking the
* active application.
  */

#define XACP_CORE1_APP_AREA_START        0x04300000UL
#define XACP_CORE1_CODE_OFFSET           0x04300000UL
#define XACP_CORE1_SHARED_OFFSET         0x04500000UL
#define XACP_CORE1_APP_AREA_END          0x04600000UL

/*

* ZZMPEG program-stream ring, used by the MPEG-1 / MP2 player path.
* It sits before the ZZMIDI SF2 staging area.
  */

#define XACP_ZZMPEG_PS_RING_OFFSET       0x05000000UL
#define XACP_ZZMPEG_PS_RING_SIZE         (4UL * 1024UL * 1024UL)
#define XACP_ZZMPEG_PS_RING_END          (XACP_ZZMPEG_PS_RING_OFFSET + XACP_ZZMPEG_PS_RING_SIZE)

/* ------------------------------------------------------------------------- */
/* ZZMIDI XACP v1.5 map                                                      */
/* ------------------------------------------------------------------------- */

#define XMID_CTRL_OFFSET                 0x04010000UL
#define XMID_CTRL_STRUCT_SIZE            0x000000D0UL

#define XMID_FIFO_OFFSET                 0x04600000UL

#define XMID_PCM_RING_OFFSET             0x04800000UL
#define XMID_PCM_RING_SIZE               (1UL * 1024UL * 1024UL)
#define XMID_PCM_RING_END                (XMID_PCM_RING_OFFSET + XMID_PCM_RING_SIZE)

#define XMID_SF2_OFFSET                  0x05800000UL
#define XMID_SF2_STAGING_SIZE            (32UL * 1024UL * 1024UL)
#define XMID_SF2_END                     (XMID_SF2_OFFSET + XMID_SF2_STAGING_SIZE)

#define XMID_MIDI_OFFSET                 0x07800000UL
#define XMID_MIDI_STAGING_SIZE           (6UL * 1024UL * 1024UL)
#define XMID_MIDI_END                    (XMID_MIDI_OFFSET + XMID_MIDI_STAGING_SIZE)

/*

* Hard end of the ZZMIDI staging area.
* The firmware C heap starts here in the XACP v1.5 layout.
  */

#define XMID_STAGING_END                 0x07E00000UL
#define XMID_HEAP_C_FB_LIMIT             0x07E00000UL

/*

* Historical forbidden value:
*
* old MIDI staging = 0x07000000
*
* This overlaps the last 8 MB of the 32 MB SF2 staging area and must not
* be reused.
  */

#define XMID_OLD_BAD_MIDI_OFFSET         0x07000000UL

/* ------------------------------------------------------------------------- */
/* ZZMIDI control-block field offsets                                        */
/* ------------------------------------------------------------------------- */

#define XMID_OFF_MAGIC                   0x00000000UL
#define XMID_OFF_VERSION                 0x00000004UL
#define XMID_OFF_STRUCT_SIZE             0x00000008UL
#define XMID_OFF_CMD_SEQ                 0x0000000CUL
#define XMID_OFF_DONE_SEQ                0x00000010UL
#define XMID_OFF_SUBCMD                  0x00000014UL
#define XMID_OFF_STATE                   0x00000018UL
#define XMID_OFF_ERROR                   0x0000001CUL
#define XMID_OFF_FLAGS                   0x00000020UL

#define XMID_OFF_SAMPLE_RATE             0x00000024UL
#define XMID_OFF_CHANNELS                0x00000028UL
#define XMID_OFF_PCM_FORMAT              0x0000002CUL

#define XMID_OFF_SF2_OFFSET              0x00000030UL
#define XMID_OFF_SF2_SIZE                0x00000034UL
#define XMID_OFF_MIDI_OFFSET             0x00000038UL
#define XMID_OFF_MIDI_SIZE               0x0000003CUL

#define XMID_OFF_PCM_RING_OFFSET         0x00000040UL
#define XMID_OFF_PCM_RING_SIZE           0x00000044UL
#define XMID_OFF_PCM_WRITE_TOTAL         0x00000048UL
#define XMID_OFF_PCM_READ_TOTAL          0x0000004CUL

#define XMID_OFF_TOTAL_MS                0x00000058UL
#define XMID_OFF_UNDERRUNS               0x00000068UL

#define XMID_OFF_VOLUME_Q16              0x00000074UL
#define XMID_OFF_MAX_VOICES              0x00000078UL

/*

* Debug / diagnostic fields may extend up to 0xCC.
* The full control block size for XACP v1.5 is 0xD0.
  */

/* ------------------------------------------------------------------------- */
/* ZZMIDI state values                                                       */
/* ------------------------------------------------------------------------- */

#define XMID_STATE_READY                 0x00000001UL
#define XMID_STATE_PLAYING               0x00000002UL
#define XMID_STATE_DONE                  0x00000004UL
#define XMID_STATE_ERROR                 0x80000000UL

/* ------------------------------------------------------------------------- */
/* Private ARM absolute pool                                                 */
/* ------------------------------------------------------------------------- */

/*

* These are ARM absolute physical addresses, not fb-relative offsets.
  */

#define XACP_ARM_Z3_VISIBLE_BASE         0x20000000UL
#define XACP_ARM_Z3_VISIBLE_END          0x22000000UL

#define XACP_ARM_PRIVATE_RAW_BASE        0x22000000UL
#define XACP_ARM_PRIVATE_RAW_SIZE        (64UL * 1024UL * 1024UL)
#define XACP_ARM_PRIVATE_RAW_END         (XACP_ARM_PRIVATE_RAW_BASE + XACP_ARM_PRIVATE_RAW_SIZE)

#define XACP_ARM_PRIVATE_HEAP_BASE       0x26000000UL
#define XACP_ARM_PRIVATE_HEAP_SIZE       (160UL * 1024UL * 1024UL)
#define XACP_ARM_PRIVATE_HEAP_END        (XACP_ARM_PRIVATE_HEAP_BASE + XACP_ARM_PRIVATE_HEAP_SIZE)

/* ------------------------------------------------------------------------- */
/* Compile-time sanity checks                                                */
/* ------------------------------------------------------------------------- */

#define XACP_STATIC_ASSERT(name, cond) typedef char xacp_static_assert_##name[(cond) ? 1 : -1]

XACP_STATIC_ASSERT(mp3_before_pcm,
XACP_MP3_RING_END <= XACP_PCM_RING_OFFSET);

XACP_STATIC_ASSERT(pcm_before_core1_area,
XACP_PCM_RING_END <= XACP_CORE1_APP_AREA_START);

XACP_STATIC_ASSERT(core1_area_before_xmid_fifo,
XACP_CORE1_APP_AREA_END <= XMID_FIFO_OFFSET);

XACP_STATIC_ASSERT(xmid_pcm_before_sf2,
XMID_PCM_RING_END <= XMID_SF2_OFFSET);

XACP_STATIC_ASSERT(sf2_is_32mb,
XMID_SF2_STAGING_SIZE == (32UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(sf2_ends_at_midi_start,
XMID_SF2_END == XMID_MIDI_OFFSET);

XACP_STATIC_ASSERT(midi_is_6mb,
XMID_MIDI_STAGING_SIZE == (6UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(midi_ends_at_heap_limit,
XMID_MIDI_END == XMID_HEAP_C_FB_LIMIT);

XACP_STATIC_ASSERT(staging_end_matches_heap_limit,
XMID_STAGING_END == XMID_HEAP_C_FB_LIMIT);

XACP_STATIC_ASSERT(bad_old_midi_offset_overlaps_sf2,
XMID_OLD_BAD_MIDI_OFFSET < XMID_SF2_END);

XACP_STATIC_ASSERT(private_raw_after_z3_visible,
XACP_ARM_PRIVATE_RAW_BASE >= XACP_ARM_Z3_VISIBLE_END);

XACP_STATIC_ASSERT(private_heap_after_raw,
XACP_ARM_PRIVATE_HEAP_BASE >= XACP_ARM_PRIVATE_RAW_END);

XACP_STATIC_ASSERT(private_heap_ends_at_0x30000000,
XACP_ARM_PRIVATE_HEAP_END == 0x30000000UL);

#ifdef __cplusplus
}
#endif

#endif /* XACP_MEMORY_MAP_V1_5_H */
