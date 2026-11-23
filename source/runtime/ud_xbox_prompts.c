/* In-memory Xbox prompts for the game's Scaleform textures. The input mappings
   and the game files are untouched. The artwork consists of original letter glyphs. */

enum { LETTER_A, LETTER_B, LETTER_X, LETTER_Y, LETTER_T };

static const u8 xbox_letters[][7] = {
    {14, 17, 17, 31, 17, 17, 17},  /* A */
    {30, 17, 17, 30, 17, 17, 30},  /* B */
    {17, 17, 10, 4, 10, 17, 17},   /* X */
    {17, 17, 10, 4, 4, 4, 4},      /* Y */
    {31, 4, 4, 4, 4, 4, 4},        /* T */
};

static const u8 xbox_face_colors[][3] = {
    {86, 220, 126}, {248, 104, 113}, {103, 185, 255}, {255, 217, 93},
};

static u8 xbox_glyph_coverage(u32 button, u32 x, u32 y, u32 w, u32 h) {
    u32 cell = h * 54 / 700;
    u32 max_cell = w * 76 / 500;
    if (cell > max_cell) cell = max_cell;
    if (!cell) return 0;
    u32 left = (w - cell * 5) / 2, top = (h - cell * 7) / 2;
    if (x < left || y < top || x >= left + cell * 5 || y >= top + cell * 7) return 0;
    u32 col = (x - left) / cell, row = (y - top) / cell;
    return xbox_letters[button][row] & (16u >> col) ? 15 : 0;
}

/* BC7 mode 6 uses one RGBA endpoint pair and four-bit indices. Keep RGB
   constant and select alpha for each pixel. Pixel zero is the anchor. */
static void xbox_bits(u8 *dst, u32 *pos, u32 value, u32 count) {
    for (u32 i = 0; i < count; i++, (*pos)++)
        dst[*pos / 8] |= (u8)(((value >> i) & 1) << (*pos % 8));
}

static void xbox_bc7_block(u8 *dst, const u8 color[3], const u8 alpha[16]) {
    u32 pos = 0, reverse = alpha[0] > 7;
    xbox_bits(dst, &pos, 64, 7);
    for (u32 c = 0; c < 3; c++) {
        xbox_bits(dst, &pos, color[c] >> 1, 7);
        xbox_bits(dst, &pos, color[c] >> 1, 7);
    }
    xbox_bits(dst, &pos, reverse ? 127 : 0, 7);
    xbox_bits(dst, &pos, reverse ? 0 : 127, 7);
    xbox_bits(dst, &pos, reverse, 1);
    xbox_bits(dst, &pos, !reverse, 1);
    for (u32 i = 0; i < 16; i++) xbox_bits(dst, &pos, reverse ? 15 - alpha[i] : alpha[i], i ? 4 : 3);
}

/* Only the number in L1/R1/R2 changes. The rest of the GNF, including
   the original silhouette and the first letter, comes from the game. */
static void xbox_bc7_opaque(u8 *dst, u8 pixels[16][4]) {
    u32 lo = 0, hi = 0;
    int low = 766, high = -1;
    for (u32 i = 0; i < 16; i++) {
        int light = pixels[i][0] + pixels[i][1] + pixels[i][2];
        if (light < low) { lo = i; low = light; }
        if (light > high) { hi = i; high = light; }
    }
    u8 indexes[16];
    for (u32 i = 0; i < 16; i++) {
        int best = 0x7fffffff;
        for (u32 j = 0; j < 16; j++) {
            int error = 0;
            for (u32 c = 0; c < 3; c++) {
                int delta = pixels[i][c] - bc7_interp(pixels[lo][c] | 1, pixels[hi][c] | 1, j, 4);
                error += delta * delta;
            }
            if (error < best) { indexes[i] = j; best = error; }
        }
    }
    int reverse = indexes[0] >= 8;
    u32 first = reverse ? hi : lo, second = reverse ? lo : hi, pos = 0;
    for (u32 i = 0; i < 16; i++) dst[i] = 0;
    xbox_bits(dst, &pos, 64, 7);
    for (u32 c = 0; c < 3; c++) {
        xbox_bits(dst, &pos, pixels[first][c] >> 1, 7);
        xbox_bits(dst, &pos, pixels[second][c] >> 1, 7);
    }
    xbox_bits(dst, &pos, 0xffff, 16);  /* Opaque alpha at both endpoints. */
    for (u32 i = 0; i < 16; i++) xbox_bits(dst, &pos, reverse ? 15 - indexes[i] : indexes[i], i ? 4 : 3);
}

typedef struct {
    u8 left, top, right, bottom;
    u8 glyph_x, glyph_y, glyph_w, glyph_h, letter, ink;
} XboxLabel;

static void xbox_replace_number(u8 *texture, const u8 *source, const TileInfo *tile, const XboxLabel *label) {
    u8 samples[16][4], top[16][4], bottom[16][4];
    for (u32 bx = label->left / 4; bx <= (label->right - 1u) / 4; bx++) {
        bc7_decode_block(source + tile_addr(tile, bx, (label->top - 1u) / 4, 0), top[0]);
        bc7_decode_block(source + tile_addr(tile, bx, label->bottom / 4, 0), bottom[0]);
        for (u32 by = label->top / 4; by <= (label->bottom - 1u) / 4; by++) {
            u64 offset = tile_addr(tile, bx, by, 0);
            bc7_decode_block(source + offset, samples[0]);
            for (u32 i = 0; i < 16; i++) {
                int x = 4 * bx + i % 4, y = 4 * by + i / 4;
                if (x < label->left || x >= label->right || y < label->top || y >= label->bottom) continue;
                int gx = x - label->glyph_x, gy = y - label->glyph_y, coverage = 0;
                if (gx >= 0 && gx < label->glyph_w && gy >= 0 && gy < label->glyph_h) {
                    for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
                        int row = (8 * gy + 2 * sy + 1) * 7 / (8 * label->glyph_h);
                        int col = (8 * gx + 2 * sx + 1) * 5 / (8 * label->glyph_w);
                        coverage += !!(xbox_letters[label->letter][row] & (16u >> col));
                    }
                }
                int dist = y - label->top + 1, span = label->bottom - label->top + 1;
                for (u32 c = 0; c < 3; c++) {
                    int upper = top[((label->top - 1) % 4) * 4 + i % 4][c];
                    int lower = bottom[(label->bottom % 4) * 4 + i % 4][c];
                    int bg = (upper * (span - dist) + lower * dist) / span;
                    if (samples[i][c] < bg + 13) bg = samples[i][c];
                    samples[i][c] = (u8)((bg * (16 - coverage) + label->ink * coverage + 8) / 16);
                }
            }
            xbox_bc7_opaque(texture + offset, samples);
        }
    }
}

static u8 *xbox_make_texture(const u8 *original, u32 size, u32 button) {
    u32 header[12];
    mem_copy(header, original, sizeof header);
    u32 w = (header[6] & 0x3fff) + 1, h = ((header[6] >> 14) & 0x3fff) + 1;
    u32 format = (header[5] >> 20) & 63, mode = (header[7] >> 20) & 31;
    u32 pitch = ((header[8] >> 13) & 0x3fff) + 1;
    if (header[0] != 0x20464e47u || header[1] != 40 || header[3] != size ||
        w < 16 || h < 16 || w > 256 || h > 256 || pitch < w || pitch % 4 || format != 41 || mode != 13) return 0;
    if ((button == XBOX_LB || button == XBOX_RB) && (w != 78 || h != 47)) return 0;
    if (button == XBOX_RT && (w != 85 || h != 85)) return 0;
    TileInfo tile;
    tile_info(&tile, mode, 128, pitch / 4, (h + 3) / 4);
    if (tile_size(&tile, 1) + sizeof header != size) return 0;
    u8 *texture = (u8 *)plat_alloc_rw(size);
    if (!texture) return 0;
    if (button >= XBOX_LB) {
        static const XboxLabel labels[] = {
            {41, 12, 54, 31, 42, 13, 10, 17, LETTER_B, 232},
            {41, 12, 54, 31, 43, 13, 10, 17, LETTER_B, 232},
            {42, 31, 56, 54, 43, 32, 11, 21, LETTER_T, 137},
        };
        mem_copy(texture, original, size);
        xbox_replace_number(texture + sizeof header, original + sizeof header, &tile, &labels[button - XBOX_LB]);
        return texture;
    }
    mem_copy(texture, header, sizeof header);
    for (u32 y = 0; y < tile.height; y++) {
        for (u32 x = 0; x < tile.pitch; x++) {
            u8 alpha[16];
            for (u32 i = 0; i < 16; i++)
                alpha[i] = xbox_glyph_coverage(button, x * 4 + i % 4, y * 4 + i / 4, w, h);
            xbox_bc7_block(texture + sizeof header + tile_addr(&tile, x, y, 0), xbox_face_colors[button], alpha);
        }
    }
    return texture;
}

typedef struct { const char *name; u32 size, button; u8 *texture; } XboxPrompt;
static XboxPrompt g_xbox_textures[] = {
    {"frontend_if9.gnf", 25648, XBOX_A},
    {"credits_i44.gnf", 25648, XBOX_A},
    {"controller_icons_i1cf.gnf", 25648, XBOX_A},
    {"interactions_ia.gnf", 25648, XBOX_A},
    {"characterintros_i17.gnf", 25648, XBOX_A},
    {"startendscreens_i10.gnf", 9264, XBOX_A},
    {"frontend_ia.gnf", 25648, XBOX_B},
    {"controller_icons_i1cb.gnf", 25648, XBOX_B},
    {"interactions_id.gnf", 25648, XBOX_B},
    {"characterintros_i3.gnf", 25648, XBOX_B},
    {"startendscreens_id.gnf", 25648, XBOX_B},
    {"interactions_i10.gnf", 20528, XBOX_X},
    {"frontend_i38e.gnf", 25648, XBOX_Y},
    {"interactions_i13.gnf", 25648, XBOX_Y},
    {"icon_choices_withtext_i38.gnf", 25648, XBOX_A},
    {"icon_choices_withtext_i35.gnf", 25648, XBOX_B},
    {"icon_choices_withtext_i3b.gnf", 20528, XBOX_X},
    {"icon_choices_withtext_i3e.gnf", 25648, XBOX_Y},
    {"icon_choices_i5f.gnf", 25648, XBOX_A},
    {"icon_choices_i5c.gnf", 25648, XBOX_B},
    {"icon_choices_i62.gnf", 20528, XBOX_X},
    {"icon_choices_i65.gnf", 25648, XBOX_Y},
    {"frontend_i504.gnf", 6192, XBOX_LB},
    {"tutorial_i133.gnf", 6192, XBOX_LB},
    {"frontend_i508.gnf", 6192, XBOX_RB},
    {"tutorial_i131.gnf", 6192, XBOX_RB},
    {"cluescreens_id.gnf", 6192, XBOX_RB},
    {"tutorial_i10f.gnf", 9264, XBOX_RT},
    {"small_r2_button.uc.gnf", 9264, XBOX_RT},
};
