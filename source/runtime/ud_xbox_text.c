/* Localized tutorial records in the game use the same two-letter button names
   in each language. Relabel only records known to contain those instructions. */
static const u32 xbox_tutorial_ids[] = {
    56961, 61743, 61744, 61745, 61747, 61748, 61750, 61751, 61782,
    61787, 61790, 64417, 64420, 64446, 64448, 64732, 65148, 65277,
    65286, 65287, 65326, 65328, 65329, 65959, 65966, 65973,
};
static u8 xbox_text_seen[sizeof xbox_tutorial_ids / sizeof xbox_tutorial_ids[0]];
static u8 (SYSV *xbox_lookup_original)(void *table, const void *key, void *output);
static void (SYSV *xbox_wstring_unique)(void *output, int length, int capacity);

static int xbox_text_word(u16 c) {
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

static int xbox_text_buttons(u16 *text, u32 n) {
    int changed = 0;
    for (u32 i = 0; i + 1 < n; i++) {
        if ((text[i] != 'L' && text[i] != 'R') || (text[i + 1] != '1' && text[i + 1] != '2') ||
            (i && xbox_text_word(text[i - 1])) || (i + 2 < n && xbox_text_word(text[i + 2]))) continue;
        text[i + 1] = text[i + 1] == '1' ? 'B' : 'T';
        changed = 1;
    }
    return changed;
}

static u8 SYSV xbox_lookup(void *table, const void *key, void *output) {
    u8 found = xbox_lookup_original(table, key, output);
    u64 string = 0, wide = 0;
    u32 length = 0;
    char name[13];
    if (!found || !atm_rd((u64)key, &string, 8) || !vis_ptr_ok(string) ||
        !atm_rd(string + 8, &length, 4) || length != 12 || !atm_rd(string + 16, name, sizeof name)) return found;
    static const char prefix[] = "SMG013_";
    for (int i = 0; i < 7; i++) if (name[i] != prefix[i]) return found;
    u32 id = 0;
    for (int i = 7; i < 12; i++) {
        if (name[i] < '0' || name[i] > '9') return found;
        id = id * 10 + (name[i] - '0');
    }
    u32 entry = 0, count = sizeof xbox_tutorial_ids / sizeof xbox_tutorial_ids[0];
    while (entry < count && xbox_tutorial_ids[entry] < id) entry++;
    if (entry == count || xbox_tutorial_ids[entry] != id) return found;
    u16 text[384];
    if (!atm_rd((u64)output, &wide, 8) || !vis_ptr_ok(wide) || !atm_rd(wide + 8, &length, 4) ||
        length >= sizeof text / sizeof text[0] || !atm_rd(wide + 16, text, 2ull * length) ||
        !xbox_text_buttons(text, length)) return found;

    /* The original lookup returns a shared string. The engine makes this
       caller's copy unique before the replacement is applied. */
    xbox_wstring_unique(output, (int)length, -1);
    u32 capacity = 0;
    if (!atm_rd((u64)output, &wide, 8) || !vis_ptr_ok(wide) ||
        !atm_rd(wide + 12, &capacity, 4) || capacity < length) return found;
    mem_copy((void *)(wide + 16), text, 2ull * length);
    if (!__atomic_exchange_n(xbox_text_seen + entry, 1, __ATOMIC_RELAXED)) {
        name[12] = 0;
        char record[64]; Sb s = {record, 0, sizeof record};
        sb_s(&s, "  Xbox tutorial text: "); sb_s(&s, name); sb_s(&s, "\n");
        say(record);
    }
    return found;
}

static void patch_xbox_text(Module *m) {
    static const u8 lookup_start[] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56};
    static const u8 unique_start[] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x50};
    if (!g_stub_w || m->size < 0xe65000) return;
    u8 *site = m->base + 0xe64fd0, *unique = m->base + 0x979770;
    for (u32 i = 0; i < sizeof lookup_start; i++) if (site[i] != lookup_start[i]) return;
    for (u32 i = 0; i < sizeof unique_start; i++) if (unique[i] != unique_start[i]) return;

    xbox_wstring_unique = (void *)unique;
    u8 *original = g_stub_w;
    mem_copy(original, lookup_start, sizeof lookup_start);
    u8 *jump = original + sizeof lookup_start;
    jump[0] = 0xe9;
    *(i32 *)(jump + 1) = (i32)((site + sizeof lookup_start) - (jump + 5));
    xbox_lookup_original = (void *)original;

    u8 *replacement = jump + 5;
    replacement[0] = 0x48; replacement[1] = 0xb8;   /* mov rax, xbox_lookup; jmp rax */
    *(u64 *)(replacement + 2) = (u64)&xbox_lookup;
    replacement[10] = 0xff; replacement[11] = 0xe0;
    g_stub_w = (u8 *)(((u64)replacement + 12 + 15) & ~15ull);
    site[0] = 0xe9;
    *(i32 *)(site + 1) = (i32)(replacement - (site + 5));
    for (u32 i = 5; i < sizeof lookup_start; i++) site[i] = 0x90;
    say("  Xbox localized tutorial labels enabled\n");
}
