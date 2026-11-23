/* In-memory Xbox prompts for the game's Scaleform textures. The input mappings
   and the game files are untouched. The artwork consists of original letter glyphs. */

enum { LETTER_A, LETTER_B, LETTER_X, LETTER_Y, LETTER_L, LETTER_R, LETTER_T };

static const u8 xbox_letters[][7] = {
    {14, 17, 17, 31, 17, 17, 17},  /* A */
    {30, 17, 17, 30, 17, 17, 30},  /* B */
    {17, 17, 10, 4, 10, 17, 17},   /* X */
    {17, 17, 10, 4, 4, 4, 4},      /* Y */
    {16, 16, 16, 16, 16, 16, 31},  /* L */
    {30, 17, 17, 30, 20, 18, 17},  /* R */
    {31, 4, 4, 4, 4, 4, 4},        /* T */
};

typedef struct {
    u8 first, second, letters, color[3];
} XboxGlyph;

static const XboxGlyph xbox_glyphs[] = {
    {LETTER_A, 0, 1, {86, 220, 126}},
    {LETTER_B, 0, 1, {248, 104, 113}},
    {LETTER_X, 0, 1, {103, 185, 255}},
    {LETTER_Y, 0, 1, {255, 217, 93}},
    {LETTER_L, LETTER_B, 2, {240, 240, 240}},
    {LETTER_R, LETTER_B, 2, {240, 240, 240}},
    {LETTER_R, LETTER_T, 2, {240, 240, 240}},
};

static u8 xbox_glyph_coverage(u32 button, u32 x, u32 y, u32 w, u32 h) {
    const XboxGlyph *glyph = &xbox_glyphs[button];
    u32 cols = glyph->letters == 2 ? 11 : 5;
    u32 cell = h * (glyph->letters == 2 ? 46 : 54) / 700;
    u32 max_cell = w * 76 / (100 * cols);
    if (cell > max_cell) cell = max_cell;
    if (!cell) return 0;
    u32 left = (w - cell * cols) / 2, top = (h - cell * 7) / 2;
    if (x < left || y < top || x >= left + cell * cols || y >= top + cell * 7) return 0;
    u32 col = (x - left) / cell, row = (y - top) / cell;
    if (col < 5) return xbox_letters[glyph->first][row] & (16u >> col) ? 15 : 0;
    if (col == 5) return 0;
    return xbox_letters[glyph->second][row] & (16u >> (col - 6)) ? 15 : 0;
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

static u8 *xbox_make_texture(const u8 *original, u32 size, u32 button) {
    u32 header[12];
    mem_copy(header, original, sizeof header);
    u32 w = (header[6] & 0x3fff) + 1, h = ((header[6] >> 14) & 0x3fff) + 1;
    u32 format = (header[5] >> 20) & 63, mode = (header[7] >> 20) & 31;
    u32 pitch = ((header[8] >> 13) & 0x3fff) + 1;
    if (header[0] != 0x20464e47u || header[1] != 40 || header[3] != size ||
        w < 16 || h < 16 || w > 256 || h > 256 || format != 41 || mode != 13) return 0;
    TileInfo tile;
    tile_info(&tile, mode, 128, pitch / 4, (h + 3) / 4);
    if (tile_size(&tile, 1) + sizeof header != size) return 0;
    u8 *texture = (u8 *)plat_alloc_rw(size);
    if (!texture) return 0;
    mem_copy(texture, header, sizeof header);
    for (u32 y = 0; y < tile.height; y++) {
        for (u32 x = 0; x < tile.pitch; x++) {
            u8 alpha[16];
            for (u32 i = 0; i < 16; i++)
                alpha[i] = xbox_glyph_coverage(button, x * 4 + i % 4, y * 4 + i / 4, w, h);
            xbox_bc7_block(texture + sizeof header + tile_addr(&tile, x, y, 0), xbox_glyphs[button].color, alpha);
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
    {"frontend_ia.gnf", 25648, XBOX_B},
    {"controller_icons_i1cb.gnf", 25648, XBOX_B},
    {"interactions_id.gnf", 25648, XBOX_B},
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
    {"tutorial_i10f.gnf", 9264, XBOX_RT},
    {"small_r2_button.uc.gnf", 9264, XBOX_RT},
};
