/*
 * zzrastan_video.c - ZZRastan P3: INDEX16 compositor (ZZPicoDrive model).
 *
 * Frozen architecture: BG (opaque) + FG (transparent pen 0) + sprites (transparent
 * pen 0) all write PALETTE INDICES into a private INDEX16 framebuffer in BSS (WB/WA, Core1-only,
 * NO flush), then ONE linear pass converts index -> ARGB into the P96 backbuffer (the only VRAM
 * write, 76800 u32 once), followed by dcache_clean_range on the VRAM. This replaces the previous
 * 3 direct-ARGB-to-VRAM passes (BG 10.6ms + FG 9.6ms measured in P2).
 *
 * Vertical framing (rastan.cpp set_visarea(0,319,8,247)): the hardware shows world lines 8..247.
 * Screen row y shows world row y + ZZR_VIS_Y0. Applied uniformly to every layer.
 *
 * PC080SN / PC090OJ facts unchanged (see git history / pc080sn.cpp / pc090oj.cpp / rastan.cpp).
 * Palette: 2048 entries, 16-bit BIG-ENDIAN xBGR555 (R=0-4,G=5-9,B=10-14) -> 0xFFRRGGBB (matches
 * firmware main_XX19.c pixel packing: 0xFF000000 | r<<16 | g<<8 | b).
 *
 * ASCII only.
 */

#include "zzrastan.h"

extern void mmu_set_wbwa(zzr_u32 addr, zzr_u32 len);
extern void dcache_clean_range(void *addr, zzr_u32 len);
extern zzr_u32 mmu_get_desc(zzr_u32 addr);
extern zzr_u32 zzrastan_pmu_cycles(void);
extern const zzr_u8 *g_zzr_obj_rom;
extern int g_zzr_obj_ready;

#define ZZR_BG_DX   (16)   /* horizontal chip offset (calibrated on hardware) */
#define ZZR_VIS_Y0  (8)    /* set_visarea Y start: screen row 0 = world row 8 */

#define ZZR_SPR_DX  (0)
#define ZZR_SPR_DY  (0)

#define TM_MASK 0x1FFu          /* 512x512 tilemap wrap */

/* P4a: per-pixel diagnostics (nz counts, oor/coor checks) cost real cycles x76800/layer.
   Off by default -> measure the RELEASE renderer. Set to 1 only when investigating. */
#define ZZR_DEEP_DIAG 0

/* P2G: FG row-transparency mask. One byte per tile, bit y = "row y has at least one non-zero
   pen". 16384 tiles x 8 rows = 131072 bits = 16 KiB, i.e. ALREADY one bit per row - it cannot be
   shrunk to 2 KiB without falling back to one bit per whole tile, which is far weaker. */
#define ZZR_GFX_TILES (ZZR_GFX_FINAL_SIZE / 32u)
static zzr_u8 g_fg_row_nz[ZZR_GFX_TILES];
static int    g_fg_row_nz_ready;

static void build_fg_row_masks(void)
{
    const zzr_u8 *gfx = g_zzr_gfx_rom;
    zzr_u32 t;
    if (!gfx) return;
    for (t = 0; t < ZZR_GFX_TILES; t++) {
        const zzr_u8 *src = gfx + t * 32u;
        zzr_u8 mask = 0;
        zzr_u32 row;
        for (row = 0; row < 8u; row++) {
            const zzr_u8 *r = src + row * 4u;
            if (r[0] || r[1] || r[2] || r[3]) mask |= (zzr_u8)(1u << row);
        }
        g_fg_row_nz[t] = mask;
    }
    g_fg_row_nz_ready = 1;
}

/* Private INDEX16 framebuffer: 320x240 palette indices, in BSS (0x22000000 pool, WB/WA).
   Core1-only owner -> no cache flush needed. */
static zzr_u16 frame_index[ZZR_IMG_W * ZZR_IMG_H];

static zzr_u32 g_pal[2048];
static zzr_u32 g_frame_seq;
static unsigned long long g_bg_sum, g_fg_sum;
static zzr_u32 g_layer_frames;
static zzr_u32 g_gate_block_run;
static zzr_u32 g_last_render_seq;
static zzr_u32 g_rendered_frames;
static unsigned long long g_sum_video;
static zzr_u32 g_mapped[4];
static int g_nmap;

void zzrastan_video_reset(void)
{
    zzr_u32 i;
    g_frame_seq = 0;
    g_last_render_seq = 0;
    g_nmap = 0;
    g_zzr_shared->frame_ready = 0;
    g_zzr_shared->rendered_frames = 0;
    g_zzr_shared->gfx_out_of_range = 0;
    g_zzr_shared->color_out_of_range = 0;
    g_zzr_shared->render_core_max = 0;
    g_zzr_shared->render_total_max = 0;
    for (i = 0; i < ZZR_IMG_W * ZZR_IMG_H; i++) frame_index[i] = 0;

    if (g_zzr_gfx_ready) build_fg_row_masks();

    g_zzr_shared->mmu_bss_desc = mmu_get_desc(0x22000000u);
    g_zzr_shared->mmu_heap_desc = mmu_get_desc(0x22800000u);
    g_zzr_shared->mmu_gfx_desc = mmu_get_desc(ZZR_ARM_GFX_FINAL);
}

static void ensure_wbwa(zzr_u32 base, zzr_u32 len)
{
    int k;
    for (k = 0; k < g_nmap; k++)
        if (g_mapped[k] == base)
            return;
    if (g_nmap < 4) {
        mmu_set_wbwa(base, len);
        g_mapped[g_nmap++] = base;
    }
    g_zzr_shared->mmu_p96_desc = mmu_get_desc(base);
}

static void build_palette(void)
{
    const zzr_u8 *p = zzrastan_palette_ram();
    zzr_u32 i, nz = 0;
    for (i = 0; i < 2048u; i++) {
        zzr_u32 v = ((zzr_u32)p[i * 2u] << 8) | (zzr_u32)p[i * 2u + 1u];
        zzr_u32 r5 = v & 0x1Fu, g5 = (v >> 5) & 0x1Fu, b5 = (v >> 10) & 0x1Fu;
        zzr_u32 r = (r5 << 3) | (r5 >> 2);
        zzr_u32 g = (g5 << 3) | (g5 >> 2);
        zzr_u32 b = (b5 << 3) | (b5 >> 2);
        g_pal[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        if ((v & 0x7FFFu) != 0u) nz++;
    }
    g_zzr_shared->palette_nonzero_entries = nz;
}

/* One PC080SN layer -> INDEX16. transp=0 (BG): write every pixel's palette index. transp=1 (FG):
   write only where pen != 0. Index written = color*16 + pen, matching the ARGB path exactly. */
/* P2G: BG renderer, SPECIALISED. This is the P2C code with transp folded to 0. It contains no
   transp argument, no reference to g_fg_row_nz, no skip branch, no counter - nothing P2F
   added - so its hot path cannot be perturbed by the FG work (register allocation, spills,
   scheduling, code layout). Pen 0 stays opaque exactly as in P2C. */
static __attribute__((noinline)) void render_bg_opaque(zzr_u32 tm_base, int scrollx, int scrolly,
                               zzr_u32 *out_nz_tiles, zzr_u32 *out_oor, zzr_u32 *out_coor,
                               zzr_u32 *out_nz_px, zzr_u32 *out_first_code, zzr_u32 *out_first_attr)
{
    const zzr_u8 *tm = zzrastan_tilemap_ram() + tm_base;
    const zzr_u8 *gfx = g_zzr_gfx_rom;
    zzr_u32 y, nz_tiles = 0, oor = 0, coor = 0, nz_px = 0;
    int first = 1;

    for (y = 0; y < ZZR_IMG_H; y++) {
        zzr_u32 sy = ((zzr_u32)((int)y + ZZR_VIS_Y0 + scrolly)) & TM_MASK;
        zzr_u32 trow = sy >> 3, py = sy & 7u;
        zzr_u16 *drow = frame_index + y * ZZR_IMG_W;
        zzr_u32 x = 0;

        while (x < ZZR_IMG_W) {
            zzr_u32 sx = ((zzr_u32)((int)x + scrollx)) & TM_MASK;
            zzr_u32 tcol = sx >> 3, px = sx & 7u, n = 8u - px;
            zzr_u32 ti, wo, attr, code, color, base, sy2, k;
            const zzr_u8 *src;
            int flipx, flipy;

            if (x + n > ZZR_IMG_W) n = ZZR_IMG_W - x;

            ti = trow * 64u + tcol;
            wo = ti * 4u;
            attr = ((zzr_u32)tm[wo] << 8) | (zzr_u32)tm[wo + 1u];
            code = (((zzr_u32)tm[wo + 2u] << 8) | (zzr_u32)tm[wo + 3u]) & 0x3FFFu;
            color = attr & 0x1FFu;
            flipx = (attr & 0x4000u) ? 1 : 0;
            flipy = (attr & 0x8000u) ? 1 : 0;
            base = code * 32u;

            if (first) {
                if (out_first_code) *out_first_code = code;
                if (out_first_attr) *out_first_attr = attr;
                first = 0;
            }
            if (base + 32u > ZZR_GFX_FINAL_SIZE) { base = 0u;
#if ZZR_DEEP_DIAG
                oor++;
#endif
            }
#if ZZR_DEEP_DIAG
            if (y == 0u && code != 0u) nz_tiles++;
            if (color > 127u) coor++;
#endif

            sy2 = flipy ? (7u - py) : py;
            src = gfx + base + sy2 * 4u;

            for (k = 0; k < n; k++) {
                zzr_u32 tx = px + k;
                zzr_u32 sxp = flipx ? (7u - tx) : tx;
                zzr_u32 b = src[sxp >> 1];
                zzr_u32 pen = (sxp & 1u) ? (b & 0x0Fu) : (b >> 4);
                drow[x + k] = (zzr_u16)(((color * 16u) + pen) & 0x7FFu);
#if ZZR_DEEP_DIAG
                nz_px++;
#endif
            }
            x += n;
        }
    }
    if (out_nz_tiles) *out_nz_tiles = nz_tiles;
    if (out_oor) *out_oor = oor;
    if (out_coor) *out_coor = coor;
    if (out_nz_px) *out_nz_px = nz_px;
}

/* P2G: FG renderer, SPECIALISED to transparent, carrying the P2F row-skip that measured
   -2.15 ms (31 pct of runs skipped, 32 pct of pixels avoided). */
static __attribute__((noinline)) void render_fg_transparent(zzr_u32 tm_base, int scrollx, int scrolly,
                               zzr_u32 *out_nz_tiles, zzr_u32 *out_oor, zzr_u32 *out_coor,
                               zzr_u32 *out_nz_px, zzr_u32 *out_first_code, zzr_u32 *out_first_attr)
{
    const zzr_u8 *tm = zzrastan_tilemap_ram() + tm_base;
    const zzr_u8 *gfx = g_zzr_gfx_rom;
    zzr_u32 y, nz_tiles = 0, oor = 0, coor = 0, nz_px = 0;
    int first = 1;

    for (y = 0; y < ZZR_IMG_H; y++) {
        zzr_u32 sy = ((zzr_u32)((int)y + ZZR_VIS_Y0 + scrolly)) & TM_MASK;
        zzr_u32 trow = sy >> 3, py = sy & 7u;
        zzr_u16 *drow = frame_index + y * ZZR_IMG_W;
        zzr_u32 x = 0;

        while (x < ZZR_IMG_W) {
            zzr_u32 sx = ((zzr_u32)((int)x + scrollx)) & TM_MASK;
            zzr_u32 tcol = sx >> 3, px = sx & 7u, n = 8u - px;
            zzr_u32 ti, wo, attr, code, color, base, sy2, k;
            const zzr_u8 *src;
            int flipx, flipy;

            if (x + n > ZZR_IMG_W) n = ZZR_IMG_W - x;

            ti = trow * 64u + tcol;
            wo = ti * 4u;
            attr = ((zzr_u32)tm[wo] << 8) | (zzr_u32)tm[wo + 1u];
            code = (((zzr_u32)tm[wo + 2u] << 8) | (zzr_u32)tm[wo + 3u]) & 0x3FFFu;
            color = attr & 0x1FFu;
            flipx = (attr & 0x4000u) ? 1 : 0;
            flipy = (attr & 0x8000u) ? 1 : 0;
            base = code * 32u;

            if (first) {
                if (out_first_code) *out_first_code = code;
                if (out_first_attr) *out_first_attr = attr;
                first = 0;
            }
            if (base + 32u > ZZR_GFX_FINAL_SIZE) { base = 0u;
#if ZZR_DEEP_DIAG
                oor++;
#endif
            }
#if ZZR_DEEP_DIAG
            if (y == 0u && code != 0u) nz_tiles++;
            if (color > 127u) coor++;
#endif

            sy2 = flipy ? (7u - py) : py;

            /* P2F mechanism, kept: if this tile row is entirely transparent, skip the whole run -
               no GFX access, no per-pixel flipX, no pen test, no store. flipY already selected the
               source row; flipX cannot change whether a row is empty. */
            if ((g_fg_row_nz[base >> 5] & (1u << sy2)) == 0u) { x += n; continue; }

            src = gfx + base + sy2 * 4u;

            for (k = 0; k < n; k++) {
                zzr_u32 tx = px + k;
                zzr_u32 sxp = flipx ? (7u - tx) : tx;
                zzr_u32 b = src[sxp >> 1];
                zzr_u32 pen = (sxp & 1u) ? (b & 0x0Fu) : (b >> 4);
                if (pen == 0u) continue;
                drow[x + k] = (zzr_u16)(((color * 16u) + pen) & 0x7FFu);
#if ZZR_DEEP_DIAG
                nz_px++;
#endif
            }
            x += n;
        }
    }
    if (out_nz_tiles) *out_nz_tiles = nz_tiles;
    if (out_oor) *out_oor = oor;
    if (out_coor) *out_coor = coor;
    if (out_nz_px) *out_nz_px = nz_px;
}

/* PC090OJ sprites -> INDEX16, over BG+FG. pen 0 transparent. Same format decisions as the ARGB
   version (16x16 row-major, signed coords, colbank = (sprite_ctrl & 0xe0) >> 1). */
static void render_sprites_index(void)
{
    const zzr_u8 *sr = zzrastan_sprite_ram();
    const zzr_u8 *gfx = g_zzr_obj_rom;
    zzr_u16 sctrl = zzrastan_sprite_ctrl();
    zzr_u32 colbank = (zzr_u32)(sctrl & 0x00E0u) >> 1;
    zzr_u32 active = 0, nz_px = 0, oor = 0, first_code = 0, first_xy = 0;
    int first = 1, offs;

    if (!g_zzr_obj_ready) return;

    for (offs = (256 - 1) * 8; offs >= 0; offs -= 8) {
        zzr_u32 w0 = ((zzr_u32)sr[offs] << 8) | sr[offs + 1];
        zzr_u32 w1 = ((zzr_u32)sr[offs + 2] << 8) | sr[offs + 3];
        zzr_u32 w2 = ((zzr_u32)sr[offs + 4] << 8) | sr[offs + 5];
        zzr_u32 w3 = ((zzr_u32)sr[offs + 6] << 8) | sr[offs + 7];
        int flipy = (w0 & 0x8000u) ? 1 : 0;
        int flipx = (w0 & 0x4000u) ? 1 : 0;
        zzr_u32 color = (w0 & 0x000Fu) | colbank;
        zzr_u32 code = w2 & 0x1FFFu;
        int x = (int)(w3 & 0x1FFu), y = (int)(w1 & 0x1FFu);

        if (w0 == 0 && w1 == 0 && w2 == 0 && w3 == 0) continue;
        if (x > 0x140) x -= 0x200;
        if (y > 0x140) y -= 0x200;
        x += ZZR_SPR_DX;
        y += ZZR_SPR_DY - ZZR_VIS_Y0;   /* world->screen vertical framing */

        active++;
        if (first) { first_code = code; first_xy = ((zzr_u32)(x & 0xFFFF) << 16) | (y & 0xFFFF); first = 0; }
        if ((code * 128u) + 128u > ZZR_OBJ_FINAL_SIZE) { oor++; continue; }

        {
            const zzr_u8 *tile = gfx + code * 128u;
            zzr_u32 ry, rx;
            for (ry = 0; ry < 16u; ry++) {
                zzr_u32 syr = flipy ? (15u - ry) : ry;
                const zzr_u8 *row = tile + syr * 8u;
                int py = y + (int)ry;
                zzr_u16 *drow;
                if (py < 0 || py >= (int)ZZR_IMG_H) continue;
                drow = frame_index + (zzr_u32)py * ZZR_IMG_W;
                for (rx = 0; rx < 16u; rx++) {
                    zzr_u32 sxr = flipx ? (15u - rx) : rx;
                    zzr_u32 b = row[sxr >> 1];
                    zzr_u32 pen = (sxr & 1u) ? (b & 0x0Fu) : (b >> 4);
                    int px;
                    if (pen == 0u) continue;
                    px = x + (int)rx;
                    if (px < 0 || px >= (int)ZZR_IMG_W) continue;
                    drow[px] = (zzr_u16)(((color * 16u) + pen) & 0x7FFu);
#if ZZR_DEEP_DIAG
                    nz_px++;
#endif
                }
            }
        }
    }
    g_zzr_shared->spr_active = active;
    g_zzr_shared->spr_nonzero_px = nz_px;
    g_zzr_shared->spr_ctrl = sctrl;
    g_zzr_shared->spr_first_code = first_code;
    g_zzr_shared->spr_first_xy = first_xy;
    g_zzr_shared->spr_oor = oor;
}

/* Compose all layers into frame_index, publish stats. Per-stage PMU kept. */
static void compose_index(void)
{
    const zzr_u16 *xs = zzrastan_xscroll_regs();
    const zzr_u16 *ys = zzrastan_yscroll_regs();
    int bgx = -(int)xs[0] + ZZR_BG_DX;
    int bgy = -(int)ys[0];
    int fgx = -(int)xs[1] + ZZR_BG_DX;
    int fgy = -(int)ys[1];
    zzr_u32 bg_tiles = 0, bg_oor = 0, bg_coor = 0, bg_px = 0, bg_code = 0, bg_attr = 0;
    zzr_u32 fg_tiles = 0, fg_oor = 0, fg_coor = 0, fg_px = 0;
    zzr_u32 pa, pb, pc, pd;

    pa = zzrastan_pmu_cycles();
    render_bg_opaque(0x0000u, bgx, bgy, &bg_tiles, &bg_oor, &bg_coor, &bg_px, &bg_code, &bg_attr);
    pb = zzrastan_pmu_cycles();
    render_fg_transparent(0x8000u, fgx, fgy, &fg_tiles, &fg_oor, &fg_coor, &fg_px, 0, 0);
    pc = zzrastan_pmu_cycles();
    render_sprites_index();
    pd = zzrastan_pmu_cycles();

    /* P2G: proper 64-bit averages taken ONLY at the function boundaries - no internal timers,
       no counters in runs or pixels. This is what finally makes BG/FG comparable across builds. */
    g_bg_sum += (unsigned long long)(pb - pa);
    g_fg_sum += (unsigned long long)(pc - pb);
    g_layer_frames++;
    g_zzr_shared->prof_bg = (zzr_u32)(g_bg_sum / g_layer_frames);
    g_zzr_shared->prof_fg = (zzr_u32)(g_fg_sum / g_layer_frames);
    g_zzr_shared->prof_bg_avg_us = (zzr_u32)(g_bg_sum / g_layer_frames / 666ULL);
    g_zzr_shared->prof_fg_avg_us = (zzr_u32)(g_fg_sum / g_layer_frames / 666ULL);
    g_zzr_shared->prof_sprites = pd - pc;

    g_zzr_shared->bg_nonzero_tiles = bg_tiles;
    g_zzr_shared->gfx_out_of_range = bg_oor + fg_oor;
    g_zzr_shared->color_out_of_range = bg_coor + fg_coor;
    g_zzr_shared->bg_first_code = bg_code;
    g_zzr_shared->bg_first_attr = bg_attr;
    g_zzr_shared->fg_nonzero_tiles = fg_tiles;
    g_zzr_shared->fg_nonzero_px = fg_px;
    g_zzr_shared->bg_scrollx = xs[0];
    g_zzr_shared->bg_scrolly = ys[0];
    g_zzr_shared->fg_scrollx = xs[1];
    g_zzr_shared->fg_scrolly = ys[1];
    g_zzr_shared->pc080sn_ctrl = zzrastan_ctrl_regs()[0];
}

/* Single linear conversion INDEX16 -> ARGB into the P96 backbuffer. Only VRAM write per frame. */
static void convert_to_p96(zzr_u32 *fb, zzr_u32 stride_px)
{
    zzr_u32 y, x, nz = 0;
    (void)nz;
    for (y = 0; y < ZZR_IMG_H; y++) {
        const zzr_u16 *si = frame_index + y * ZZR_IMG_W;
        zzr_u32 *dst = fb + y * stride_px;
        for (x = 0; x < ZZR_IMG_W; x++) {
            zzr_u32 argb = g_pal[si[x] & 0x7FFu];
            dst[x] = argb;
#if ZZR_DEEP_DIAG
            if ((argb & 0x00FFFFFFu) != 0u) nz++;
#endif
        }
    }
    g_zzr_shared->framebuffer_nonzero_pixels = nz;
}

void zzrastan_video_present(void)
{
    zzr_u32 base, pitch, stride, t0, t1, t2, tc, core, clean, total, i;

    if (!g_zzr_gfx_ready) return;
    if (!g_fg_row_nz_ready) build_fg_row_masks();
    if (!g_zzr_shared->p96_enable) return;

    /* P1 probe: measure the gate, do not change it. The question is whether Core1 is blocked here
       because the 68k has not released it yet, and for how many consecutive calls. */
    g_zzr_shared->video_present_calls++;
    g_zzr_shared->last_gate_flip_seq = g_zzr_shared->flip_seq;
    g_zzr_shared->last_gate_frame_seq = g_frame_seq;
    /* P2A gate: render whenever the 68k has AUTHORISED a new frame, regardless of whether a PAN
       is still in flight. The 68k guarantees SH_P96_BASE points at a buffer that is neither FRONT
       nor PENDING, so this can never draw into a visible or committed buffer. */
    if (g_zzr_shared->render_seq == g_last_render_seq) {
        g_zzr_shared->video_gate_block++;
        g_gate_block_run++;
        if (g_gate_block_run > g_zzr_shared->video_gate_block_cons_max)
            g_zzr_shared->video_gate_block_cons_max = g_gate_block_run;
        return;
    }
    g_gate_block_run = 0;
    g_zzr_shared->video_gate_pass++;
    g_zzr_shared->render_start_cpu_frame = g_zzr_shared->frame_counter;
    g_last_render_seq = g_zzr_shared->render_seq;

    base = g_zzr_shared->p96_base;
    if (base == 0u) return;
    pitch = g_zzr_shared->p96_pitch;
    stride = pitch >> 2;
    if (stride == 0u) return;

    ensure_wbwa(base, ZZR_IMG_H * pitch);
    __asm__ volatile("dsb" ::: "memory");

    t0 = zzrastan_pmu_cycles();
    build_palette();
    { zzr_u32 tp = zzrastan_pmu_cycles(); g_zzr_shared->prof_palette = tp - t0; }

    /* P2B: the per-frame clear of frame_index is REMOVED. It wrote 76800 u16 (153600 bytes) in a
       scalar loop every single frame, while the BG layer is OPAQUE and covers the whole screen -
       render_layer_index() with transp=0 writes every pixel of every row before anything reads
       it. The clear was therefore pure waste. frame_index is still zeroed once in
       zzrastan_video_reset(), so the very first frame is well defined. */
    { zzr_u32 t_clr = zzrastan_pmu_cycles(); g_zzr_shared->prof_clear = zzrastan_pmu_cycles() - t_clr; }

    compose_index();
    t1 = zzrastan_pmu_cycles();
    convert_to_p96((zzr_u32 *)base, stride);
    tc = zzrastan_pmu_cycles();
    dcache_clean_range((void *)base, ZZR_IMG_H * pitch);
    t2 = zzrastan_pmu_cycles();

    core = t1 - t0;                 /* palette + compose index */
    g_zzr_shared->prof_convert = tc - t1;   /* INDEX16 -> ARGB, mandatory work, not a diagnostic */
    g_zzr_shared->prof_crc = tc - t1;       /* legacy slot kept so old logs stay comparable */
    clean = t2 - tc;
    total = t2 - t0;
    g_zzr_shared->prof_clean = clean;
    g_zzr_shared->render_core_cyc = core;
    g_zzr_shared->render_clean_cyc = clean;
    g_zzr_shared->render_total_cyc = total;
    g_zzr_shared->render_cycles = total;
    if (core > g_zzr_shared->render_core_max) g_zzr_shared->render_core_max = core;
    if (total > g_zzr_shared->render_total_max) g_zzr_shared->render_total_max = total;
    if (total > g_zzr_shared->prof_total_max) g_zzr_shared->prof_total_max = total;
    /* averages over RENDERED frames only - the P1 mistake was averaging video cost over all
       logic frames when only a third of them carried a render */
    g_rendered_frames++;
    g_sum_video += (unsigned long long)total;
    g_zzr_shared->prof_video_per_render = (zzr_u32)(g_sum_video / g_rendered_frames / 666ULL);
    g_zzr_shared->prof_rendered_frames = g_rendered_frames;

    g_frame_seq++;
    g_zzr_shared->rendered_frames = g_frame_seq;
    __asm__ volatile("dsb" ::: "memory");
    g_zzr_shared->render_ready_cpu_frame = g_zzr_shared->frame_counter;
    g_zzr_shared->render_done_seq = g_last_render_seq;
    __asm__ volatile("dsb" ::: "memory");
    g_zzr_shared->frame_ready = g_last_render_seq;   /* publish the authorisation we completed */
    __asm__ volatile("dsb" ::: "memory");
}
