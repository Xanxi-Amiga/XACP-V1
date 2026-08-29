#ifndef XACP_MEMORY_MAP_V1_6_H
#define XACP_MEMORY_MAP_V1_6_H

/*
 * XACP v1.6 memory map
 *
 * eXtended ARM Coprocessor Protocol for the MNT ZZ9000.
 *
 * Current public baseline:
 *
 *   Firmware : XX19a
 *   Protocol : XACP v1.6
 *
 * Firmware build numbers and XACP protocol versions are separate.
 *
 * Shared offsets below are framebuffer-relative unless explicitly marked
 * as ARM absolute physical addresses.
 *
 * Amiga side:
 *
 *   board = cd->cd_BoardAddr
 *   fb    = board + XACP_MNT_FB_BASE
 *
 * ARM side:
 *
 *   framebuffer physical base = 0x00200000
 *
 * Therefore:
 *
 *   ARM physical = XACP_ARM_FB_PHYS_BASE + fb-relative offset
 *
 * XACP v1.6 rule:
 *
 *   Do not create undocumented shared-DDR allocations.
 *   All persistent Core0 and dynamic Core1 allocations must remain inside
 *   their assigned regions.
 */

#ifdef __cplusplus
extern "C" {
#endif


/* ------------------------------------------------------------------------- */
/* Version                                                                   */
/* ------------------------------------------------------------------------- */

#define XACP_VERSION_MAJOR                  1UL
#define XACP_VERSION_MINOR                  6UL
#define XACP_VERSION_PATCH                  0UL

#define XACP_VERSION_U32                    0x00010600UL


/* ------------------------------------------------------------------------- */
/* Addressing conventions                                                    */
/* ------------------------------------------------------------------------- */

#define XACP_MNT_FB_BASE                    0x00010000UL
#define XACP_ARM_FB_PHYS_BASE               0x00200000UL

#define XACP_FB_OFFSET_TO_ARM_PHYS(x) \
    (XACP_ARM_FB_PHYS_BASE + (x))


/* ------------------------------------------------------------------------- */
/* Zorro / XACP registers                                                    */
/* ------------------------------------------------------------------------- */

#define XACP_REG_CMD                        0x0064UL
#define XACP_REG_STATUS                     0x0064UL

#define XACP_REG_ARM_RUN_HI                 0x0090UL
#define XACP_REG_ARM_RUN_LO                 0x0092UL


/* ------------------------------------------------------------------------- */
/* Established XACP opcodes                                                  */
/* ------------------------------------------------------------------------- */

/*
 * These constants are retained here because they are part of the established
 * public XACP interface used together with the memory map.
 *
 * Future SDK revisions may move opcode definitions to xacp_opcodes.h.
 */

#define XACP_OP_MEMCPY                      0x0001UL
#define XACP_OP_MANDELBROT                  0x0002UL
#define XACP_OP_MP3_DECODE                  0x0003UL
#define XACP_OP_STREAM_OPEN                 0x0004UL
#define XACP_OP_STREAM_CLOSE                0x0005UL

#define XACP_OP_MIDI_SF2                    0x0120UL

#define XACP_OP_CORE1_RESET                 0x0303UL


/* ------------------------------------------------------------------------- */
/* Generic XACP / MP3 / MP2 shared DDR map                                   */
/* ------------------------------------------------------------------------- */

#define XACP_COMMAND_OFFSET                 0x04000000UL
#define XACP_STREAM_OFFSET                  0x04002000UL

#define XACP_MP3_RING_OFFSET                0x04100000UL
#define XACP_MP3_RING_SIZE                  (512UL * 1024UL)
#define XACP_MP3_RING_END \
    (XACP_MP3_RING_OFFSET + XACP_MP3_RING_SIZE)

#define XACP_PCM_RING_OFFSET                0x04200000UL
#define XACP_PCM_RING_SIZE                  (1UL * 1024UL * 1024UL)
#define XACP_PCM_RING_END \
    (XACP_PCM_RING_OFFSET + XACP_PCM_RING_SIZE)


/* ------------------------------------------------------------------------- */
/* Legacy Core1 shared application area                                      */
/* ------------------------------------------------------------------------- */

/*
 * Established framebuffer-relative Core1 application area.
 *
 * Historically used by JuliaV2, ZZMPEG, ZZDoom and benchmark/Core1 paths.
 *
 * XACP v1.6 preserves this existing allocation. It is not generic scratch
 * memory: an application must obey the layout defined for its Core1 runtime.
 */

#define XACP_CORE1_APP_AREA_START           0x04300000UL
#define XACP_CORE1_CODE_OFFSET              0x04300000UL
#define XACP_CORE1_SHARED_OFFSET            0x04500000UL
#define XACP_CORE1_APP_AREA_END             0x04600000UL


/* ------------------------------------------------------------------------- */
/* ZZMPEG Program Stream ring                                                */
/* ------------------------------------------------------------------------- */

#define XACP_ZZMPEG_PS_RING_OFFSET          0x05000000UL
#define XACP_ZZMPEG_PS_RING_SIZE            (4UL * 1024UL * 1024UL)
#define XACP_ZZMPEG_PS_RING_END \
    (XACP_ZZMPEG_PS_RING_OFFSET + XACP_ZZMPEG_PS_RING_SIZE)


/* ------------------------------------------------------------------------- */
/* XACP v1.6 ZZMIDI shared region                                            */
/* ------------------------------------------------------------------------- */

/*
 * XACP v1.6 replaces the former large Zorro-visible ZZMIDI SF2/MIDI staging
 * model with a compact shared service region plus ARM-private storage.
 *
 * Shared / framebuffer-relative allocation:
 *
 *   fb+0x06000000 - fb+0x06010000  control allocation
 *   fb+0x06010000 - fb+0x06100000  realtime FIFO allocation
 *   fb+0x06100000 - fb+0x06200000  PCM ring
 *   fb+0x06200000 - fb+0x06300000  upload window
 *
 * Total: 3 MB.
 *
 * ARM physical view:
 *
 *   0x06200000 - 0x06500000
 *
 * The FIFO allocation is the reserved DDR extent, not necessarily the
 * logical FIFO payload capacity.
 */

#define XMID_V16_SHARED_START               0x06000000UL


/* Control allocation: 64 KB */

#define XMID_V16_CTRL_OFFSET                0x06000000UL
#define XMID_V16_CTRL_REGION_SIZE           0x00010000UL
#define XMID_V16_CTRL_REGION_END \
    (XMID_V16_CTRL_OFFSET + XMID_V16_CTRL_REGION_SIZE)


/* Realtime FIFO allocation: 960 KB */

#define XMID_V16_FIFO_OFFSET                0x06010000UL
#define XMID_V16_FIFO_REGION_SIZE           0x000F0000UL
#define XMID_V16_FIFO_REGION_END \
    (XMID_V16_FIFO_OFFSET + XMID_V16_FIFO_REGION_SIZE)


/* PCM output ring: 1 MB */

#define XMID_V16_PCM_RING_OFFSET            0x06100000UL
#define XMID_V16_PCM_RING_SIZE              (1UL * 1024UL * 1024UL)
#define XMID_V16_PCM_RING_END \
    (XMID_V16_PCM_RING_OFFSET + XMID_V16_PCM_RING_SIZE)


/* Chunked host -> ARM upload window: 1 MB */

#define XMID_V16_UPLOAD_OFFSET              0x06200000UL
#define XMID_V16_UPLOAD_SIZE                (1UL * 1024UL * 1024UL)
#define XMID_V16_UPLOAD_END \
    (XMID_V16_UPLOAD_OFFSET + XMID_V16_UPLOAD_SIZE)


#define XMID_V16_SHARED_END                 0x06300000UL
#define XMID_V16_SHARED_SIZE \
    (XMID_V16_SHARED_END - XMID_V16_SHARED_START)


/* ARM physical representation of the same shared region */

#define XACP_ARM_XMID_SHARED_BASE \
    XACP_FB_OFFSET_TO_ARM_PHYS(XMID_V16_SHARED_START)

#define XACP_ARM_XMID_SHARED_END \
    XACP_FB_OFFSET_TO_ARM_PHYS(XMID_V16_SHARED_END)


/* ------------------------------------------------------------------------- */
/* ARM address-space boundaries                                              */
/* ------------------------------------------------------------------------- */

/*
 * Absolute ARM physical addresses below this point.
 *
 * These are NOT framebuffer-relative offsets.
 */

/*
 * Existing Zorro-visible ARM range.
 *
 * Do not allocate private ARM buffers here.
 */

#define XACP_ARM_Z3_VISIBLE_BASE            0x20000000UL
#define XACP_ARM_Z3_VISIBLE_END             0x22000000UL


/* ------------------------------------------------------------------------- */
/* ARM-private XACP v1.6 map                                                 */
/* ------------------------------------------------------------------------- */

/*
 * 0x22000000 - 0x23000000
 *
 * Reserved for the established Core1 private / legacy application
 * environment.
 */

#define XACP_ARM_CORE1_LEGACY_BASE          0x22000000UL
#define XACP_ARM_CORE1_LEGACY_SIZE          (16UL * 1024UL * 1024UL)
#define XACP_ARM_CORE1_LEGACY_END \
    (XACP_ARM_CORE1_LEGACY_BASE + XACP_ARM_CORE1_LEGACY_SIZE)


/*
 * 0x23000000 - 0x25000000
 *
 * ZZMIDI ARM-private SoundFont storage.
 */

#define XACP_ARM_XMID_SF2_BASE              0x23000000UL
#define XACP_ARM_XMID_SF2_SIZE              (32UL * 1024UL * 1024UL)
#define XACP_ARM_XMID_SF2_END \
    (XACP_ARM_XMID_SF2_BASE + XACP_ARM_XMID_SF2_SIZE)


/*
 * 0x25000000 - 0x25600000
 *
 * ZZMIDI ARM-private MIDI / parsed-data storage.
 */

#define XACP_ARM_XMID_MIDI_BASE             0x25000000UL
#define XACP_ARM_XMID_MIDI_SIZE             (6UL * 1024UL * 1024UL)
#define XACP_ARM_XMID_MIDI_END \
    (XACP_ARM_XMID_MIDI_BASE + XACP_ARM_XMID_MIDI_SIZE)


/*
 * 0x25600000 - 0x2F600000
 *
 * ZZMIDI / firmware service heap.
 *
 * Intended for TinySoundFont, TinyMidiLoader and related persistent
 * service/runtime allocations.
 */

#define XACP_ARM_SERVICE_HEAP_BASE          0x25600000UL
#define XACP_ARM_SERVICE_HEAP_SIZE          (160UL * 1024UL * 1024UL)
#define XACP_ARM_SERVICE_HEAP_END \
    (XACP_ARM_SERVICE_HEAP_BASE + XACP_ARM_SERVICE_HEAP_SIZE)


/*
 * 0x2F600000 - 0x30000000
 *
 * XACP v1.6 guard area.
 *
 * Intentionally not available as generic application scratch memory.
 */

#define XACP_ARM_V16_GUARD_BASE             0x2F600000UL
#define XACP_ARM_V16_GUARD_SIZE             (10UL * 1024UL * 1024UL)
#define XACP_ARM_V16_GUARD_END \
    (XACP_ARM_V16_GUARD_BASE + XACP_ARM_V16_GUARD_SIZE)


#define XACP_ARM_V16_PRIVATE_BASE           0x22000000UL
#define XACP_ARM_V16_PRIVATE_END            0x30000000UL


/* ------------------------------------------------------------------------- */
/* Convenience sizes                                                         */
/* ------------------------------------------------------------------------- */

#define XACP_KIB                            1024UL
#define XACP_MIB                            (1024UL * 1024UL)


/* ------------------------------------------------------------------------- */
/* Compile-time sanity checks                                                */
/* ------------------------------------------------------------------------- */

#define XACP_STATIC_ASSERT(name, cond) \
    typedef char xacp_static_assert_##name[(cond) ? 1 : -1]


/* Generic XACP layout */

XACP_STATIC_ASSERT(mp3_before_pcm,
    XACP_MP3_RING_END <= XACP_PCM_RING_OFFSET);

XACP_STATIC_ASSERT(pcm_before_core1_area,
    XACP_PCM_RING_END <= XACP_CORE1_APP_AREA_START);

XACP_STATIC_ASSERT(core1_before_zzmpeg,
    XACP_CORE1_APP_AREA_END <= XACP_ZZMPEG_PS_RING_OFFSET);


/* v1.6 ZZMIDI shared layout */

XACP_STATIC_ASSERT(xmid_ctrl_starts_shared,
    XMID_V16_CTRL_OFFSET == XMID_V16_SHARED_START);

XACP_STATIC_ASSERT(xmid_ctrl_ends_at_fifo,
    XMID_V16_CTRL_REGION_END == XMID_V16_FIFO_OFFSET);

XACP_STATIC_ASSERT(xmid_fifo_ends_at_pcm,
    XMID_V16_FIFO_REGION_END == XMID_V16_PCM_RING_OFFSET);

XACP_STATIC_ASSERT(xmid_pcm_is_1mb,
    XMID_V16_PCM_RING_SIZE == (1UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_pcm_ends_at_upload,
    XMID_V16_PCM_RING_END == XMID_V16_UPLOAD_OFFSET);

XACP_STATIC_ASSERT(xmid_upload_is_1mb,
    XMID_V16_UPLOAD_SIZE == (1UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_upload_ends_shared,
    XMID_V16_UPLOAD_END == XMID_V16_SHARED_END);

XACP_STATIC_ASSERT(xmid_shared_is_3mb,
    XMID_V16_SHARED_SIZE == (3UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_shared_after_zzmpeg,
    XACP_ZZMPEG_PS_RING_END <= XMID_V16_SHARED_START);


/* Shared fb-relative -> ARM physical translation */

XACP_STATIC_ASSERT(xmid_arm_shared_base,
    XACP_ARM_XMID_SHARED_BASE == 0x06200000UL);

XACP_STATIC_ASSERT(xmid_arm_shared_end,
    XACP_ARM_XMID_SHARED_END == 0x06500000UL);


/* ARM-private v1.6 layout */

XACP_STATIC_ASSERT(core1_private_after_z3_visible,
    XACP_ARM_CORE1_LEGACY_BASE == XACP_ARM_Z3_VISIBLE_END);

XACP_STATIC_ASSERT(core1_private_is_16mb,
    XACP_ARM_CORE1_LEGACY_SIZE == (16UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(core1_private_ends_at_sf2,
    XACP_ARM_CORE1_LEGACY_END == XACP_ARM_XMID_SF2_BASE);

XACP_STATIC_ASSERT(sf2_is_32mb,
    XACP_ARM_XMID_SF2_SIZE == (32UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(sf2_ends_at_midi,
    XACP_ARM_XMID_SF2_END == XACP_ARM_XMID_MIDI_BASE);

XACP_STATIC_ASSERT(midi_is_6mb,
    XACP_ARM_XMID_MIDI_SIZE == (6UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(midi_ends_at_service_heap,
    XACP_ARM_XMID_MIDI_END == XACP_ARM_SERVICE_HEAP_BASE);

XACP_STATIC_ASSERT(service_heap_is_160mb,
    XACP_ARM_SERVICE_HEAP_SIZE == (160UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(service_heap_ends_at_guard,
    XACP_ARM_SERVICE_HEAP_END == XACP_ARM_V16_GUARD_BASE);

XACP_STATIC_ASSERT(v16_guard_is_10mb,
    XACP_ARM_V16_GUARD_SIZE == (10UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(v16_guard_ends_at_0x30000000,
    XACP_ARM_V16_GUARD_END == 0x30000000UL);

XACP_STATIC_ASSERT(v16_private_base_is_0x22000000,
    XACP_ARM_V16_PRIVATE_BASE == 0x22000000UL);

XACP_STATIC_ASSERT(v16_private_end_is_0x30000000,
    XACP_ARM_V16_PRIVATE_END == 0x30000000UL);


#ifdef __cplusplus
}
#endif

#endif /* XACP_MEMORY_MAP_V1_6_H */
