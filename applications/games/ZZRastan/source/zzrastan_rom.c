#include "zzrastan.h"

const zzr_u8 *g_zzr_gfx_rom = 0;
int g_zzr_gfx_ready = 0;

/*
 * B0b: PC080SN graphics. The launcher writes the 4 files back-to-back at ZZR_ARM_GFX_STAGE
 * as [b04-01][b04-02][b04-03][b04-04] (128K each). They form two ROM_LOAD16_BYTE pairs:
 *   pair 0 -> final 0x00000..0x3ffff : even = file0, odd = file1
 *   pair 1 -> final 0x40000..0x7ffff : even = file2, odd = file3
 * Result is a plain packed-4bpp BYTE stream (NOT 68k code): no byteswap, the A1 endianness
 * model is untouched. Deinterleave is out-of-place (stage -> final), never in place.
 */
const zzr_u8 *g_zzr_obj_rom = 0;
int g_zzr_obj_ready = 0;

/* PC090OJ sprites: 4 files (b04-05..08), two ROM_LOAD16_BYTE pairs, deinterleaved out of place
   (stage ARM 0x04F00000 -> final 0x05000000). Plain packed-4bpp byte stream, no byteswap. */
int zzrastan_obj_bind_and_deinterleave(void)
{
    const zzr_u8 *stage = (const zzr_u8 *)ZZR_ARM_OBJ_STAGE;
    zzr_u8 *final_ = (zzr_u8 *)ZZR_ARM_OBJ_FINAL;
    zzr_u32 pair, i;

    for (pair = 0u; pair < 2u; pair++) {
        const zzr_u8 *ev = stage + (pair * 2u) * ZZR_OBJ_FILE_SIZE;
        const zzr_u8 *od = stage + (pair * 2u + 1u) * ZZR_OBJ_FILE_SIZE;
        zzr_u8 *dst = final_ + pair * (ZZR_OBJ_FILE_SIZE * 2u);
        for (i = 0u; i < ZZR_OBJ_FILE_SIZE; i++) {
            dst[i * 2u] = ev[i];
            dst[i * 2u + 1u] = od[i];
        }
    }
    g_zzr_shared->obj_raw_crc = zzrastan_crc32(stage, ZZR_OBJ_STAGE_SIZE);
    g_zzr_shared->obj_final_crc = zzrastan_crc32(final_, ZZR_OBJ_FINAL_SIZE);
    g_zzr_obj_rom = final_;
    g_zzr_obj_ready = 1;
    return 1;
}

int zzrastan_gfx_bind_and_deinterleave(void)
{
    const zzr_u8 *stage = (const zzr_u8 *)ZZR_ARM_GFX_STAGE;
    zzr_u8 *final_ = (zzr_u8 *)ZZR_ARM_GFX_FINAL;
    zzr_u32 pair, i;

    for (pair = 0u; pair < 2u; pair++) {
        const zzr_u8 *ev = stage + (pair * 2u) * ZZR_GFX_FILE_SIZE;
        const zzr_u8 *od = stage + (pair * 2u + 1u) * ZZR_GFX_FILE_SIZE;
        zzr_u8 *dst = final_ + pair * (ZZR_GFX_FILE_SIZE * 2u);
        for (i = 0u; i < ZZR_GFX_FILE_SIZE; i++) {
            dst[i * 2u] = ev[i];
            dst[i * 2u + 1u] = od[i];
        }
    }

    g_zzr_shared->gfx_raw_crc = zzrastan_crc32(stage, ZZR_GFX_STAGE_SIZE);
    g_zzr_shared->gfx_final_crc = zzrastan_crc32(final_, ZZR_GFX_FINAL_SIZE);
    g_zzr_gfx_rom = final_;
    g_zzr_gfx_ready = 1;
    g_zzr_shared->diag = ZZR_DIAG_B0B_GFX_OK;
    return 1;
}

extern void dcache_inval_range(void *addr, zzr_u32 len);

const zzr_u8 *g_zzr_main_rom = (const zzr_u8 *)ZZR_ARM_MAINCPU;
int g_zzr_rom_ready = 0;

/* Per-file CRCs for every supported maincpu set. The World table was the only one here, which is
   why the Japanese pack was rejected with ZZR_ERR_ROM_RAW_CRC: the launcher had been taught about
   rastsaga but the BLOB had not, so it refused the ROMs and never deinterleaved them - hence
   SSP = PC = 0 and a game that could not start.
   Rows: 0 = rastan (World), 1 = rastsaga (Japan). Both are checked, so a corrupt ROM is still
   caught; only the assumption that there is a single valid set is gone. */
#define ZZR_ROM_SET_COUNT 2
static const zzr_u32 expected_file_crc_sets[ZZR_ROM_SET_COUNT][ZZR_ROM_FILE_COUNT] = {
    { 0x1C91DBB1u, 0xECF20BDDu, 0x0930D4B3u, 0xD95ADE5Eu, 0x1857A7CBu, 0xCA4702FFu },
    { 0x1C91DBB1u, 0x4C62E89Eu, 0x8F54DD19u, 0x810A02A3u, 0x32E286C0u, 0xEE5EC5BCu }
};

zzr_u32 zzrastan_crc32(const zzr_u8 *p, zzr_u32 n)
{
    zzr_u32 c = 0xFFFFFFFFu;
    zzr_u32 i;
    zzr_u32 k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c ^ 0xFFFFFFFFu;
}

static zzr_u32 read_be32(const zzr_u8 *p)
{
    return ((zzr_u32)p[0] << 24) | ((zzr_u32)p[1] << 16) |
           ((zzr_u32)p[2] << 8) | (zzr_u32)p[3];
}

int zzrastan_rom_bind_and_deinterleave(void)
{
    const zzr_u8 *stage;
    zzr_u8 *mainrom;
    zzr_u32 pair;
    zzr_u32 i;
    zzr_u32 raw_crc;
    zzr_u32 main_crc;

    g_zzr_rom_ready = 0;
    if (g_zzr_shared->rom_stage_addr == 0u) {
        g_zzr_shared->error = ZZR_ERR_ROM_NULL;
        return 0;
    }
    if (g_zzr_shared->rom_stage_size != ZZR_ROM_STAGE_SIZE) {
        g_zzr_shared->error = ZZR_ERR_ROM_SIZE;
        return 0;
    }

    stage = (const zzr_u8 *)(unsigned long)g_zzr_shared->rom_stage_addr;
    mainrom = (zzr_u8 *)ZZR_ARM_MAINCPU;
    dcache_inval_range((void *)stage, ZZR_ROM_STAGE_SIZE + ZZR_ROM_PAD);

    raw_crc = zzrastan_crc32(stage, ZZR_ROM_STAGE_SIZE);
    g_zzr_shared->rom_raw_crc_arm = raw_crc;
    /* The whole-pack constant only ever described the World set. What matters is that the 68k and
       the ARM read the SAME bytes, so keep that comparison and drop the fixed value. */
    if (g_zzr_shared->rom_raw_crc_68k != 0u &&
        g_zzr_shared->rom_raw_crc_68k != raw_crc) {
        g_zzr_shared->error = ZZR_ERR_ROM_RAW_CRC;
        return 0;
    }

    /* Identify which known set this is: every file must match one row. */
    {
        zzr_u32 set;
        int found = -1;
        for (set = 0; set < ZZR_ROM_SET_COUNT && found < 0; set++) {
            int all = 1;
            for (i = 0; i < ZZR_ROM_FILE_COUNT; i++) {
                zzr_u32 c = zzrastan_crc32(stage + i * ZZR_ROM_FILE_SIZE, ZZR_ROM_FILE_SIZE);
                if (c != expected_file_crc_sets[set][i]) { all = 0; break; }
            }
            if (all) found = (int)set;
        }
        if (found < 0) {
            g_zzr_shared->error = ZZR_ERR_ROM_FILE_CRC;
            g_zzr_shared->diag = 0xFFu;      /* no set matched at all */
            return 0;
        }
        g_zzr_shared->rom_set = (zzr_u32)found;
    }

    g_zzr_shared->diag = ZZR_DIAG_A0_ROM_OK;

    for (pair = 0; pair < 3u; pair++) {
        const zzr_u8 *even = stage + (pair * 2u) * ZZR_ROM_FILE_SIZE;
        const zzr_u8 *odd = even + ZZR_ROM_FILE_SIZE;
        zzr_u8 *dst = mainrom + pair * 0x20000u;
        for (i = 0; i < ZZR_ROM_FILE_SIZE; i++) {
            dst[i * 2u] = even[i];
            dst[i * 2u + 1u] = odd[i];
        }
    }
    g_zzr_shared->diag = ZZR_DIAG_A0_DEINTERLEAVE_OK;

    main_crc = zzrastan_crc32(mainrom, ZZR_MAINCPU_SIZE);
    g_zzr_shared->rom_main_crc_arm = main_crc;
    /* THIRD and last hard-coded CRC: the deinterleaved image. Like the other two it only ever
       described the World set. The per-file table above already proves the ROMs are a known good
       set, and deinterleaving is deterministic, so re-checking a fixed value here adds nothing
       except a second set being rejected. The value is still published for diagnostics. */
    if (0) {
        g_zzr_shared->error = ZZR_ERR_ROM_MAIN_CRC;
        return 0;
    }

    g_zzr_shared->reset_ssp = read_be32(mainrom + 0);
    g_zzr_shared->reset_pc = read_be32(mainrom + 4);
    if (g_zzr_shared->reset_ssp < ZZR_WORK_RAM_BASE ||
        g_zzr_shared->reset_ssp >= ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE ||
        g_zzr_shared->reset_pc >= ZZR_MAINCPU_SIZE ||
        (g_zzr_shared->reset_pc & 1u) != 0u) {
        g_zzr_shared->error = ZZR_ERR_BAD_VECTOR;
        return 0;
    }

    g_zzr_shared->diag = ZZR_DIAG_A0_VECTOR_OK;

    /* Cyclone (little-endian ARM) fetches opcodes with a native ldrh, so the 68000
     * program ROM must be stored byteswapped within each 16-bit word. The CRC and
     * reset vectors above are computed on the big-endian layout first (A0 references
     * unchanged), then we byteswap in place for the CPU fetch path. The ROM read
     * handlers (read8/read16) use the matching swapped convention. Work RAM, palette,
     * tilemap and sprite RAM stay big-endian; Cyclone never fetches from them in A1. */
    {
        zzr_u32 j;
        for (j = 0; j < ZZR_MAINCPU_SIZE; j += 2u) {
            zzr_u8 t = mainrom[j];
            mainrom[j] = mainrom[j + 1u];
            mainrom[j + 1u] = t;
        }
    }

    g_zzr_rom_ready = 1;
    return 1;
}
