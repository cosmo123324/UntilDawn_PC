/* Add the desktop quit action in the stock menu slot shared by title and pause. */
#include "ud_exit_opcodes.h"

static u8 *ud_exit_frontend;
static u64 ud_exit_owner, ud_exit_bounds;
static int ud_exit_requested;

static int ud_exit_menu_open(void) {
    return __atomic_load_n(&ud_exit_owner, __ATOMIC_ACQUIRE) != 0;
}

static const u8 *ud_exit_prepare(const u8 *source, u32 length, u32 *new_length) {
    if (length != UD_EXIT_FRONTEND_SIZE || skp_fnv(source, length) != UD_EXIT_FRONTEND_HASH) return 0;
    *new_length = length + UD_EXIT_EXTRA_BYTES;
    u8 *existing = __atomic_load_n(&ud_exit_frontend, __ATOMIC_ACQUIRE);
    if (existing) return existing;
    u8 *movie = plat_alloc_rw(*new_length);
    if (!movie) return 0;
    u32 read_at = 0, write_at = 0;
    for (u32 i = 0; i < sizeof ud_exit_edits / sizeof ud_exit_edits[0]; i++) {
        u32 unchanged = ud_exit_edits[i].offset - read_at;
        mem_copy(movie + write_at, source + read_at, unchanged);
        write_at += unchanged;
        mem_copy(movie + write_at, ud_exit_edits[i].code, ud_exit_edits[i].added);
        if (ud_exit_edits[i].code == ud_exit_strings && ud_game_language() != 17)
            mem_copy(movie + write_at + 2, "Quit", 4);   /* Keep the game's literal-text marker (~). */
        write_at += ud_exit_edits[i].added;
        read_at = ud_exit_edits[i].offset + ud_exit_edits[i].removed;
    }
    mem_copy(movie + write_at, source + read_at, length - read_at);
    u32 tag_length;
    mem_copy(&tag_length, source + UD_EXIT_TAG_LENGTH_AT, 4);
    tag_length += UD_EXIT_EXTRA_BYTES;
    mem_copy(movie + UD_EXIT_TAG_LENGTH_AT, &tag_length, 4);
    mem_copy(movie + 4, new_length, 4);
    if (!__atomic_compare_exchange_n(&ud_exit_frontend, &existing, movie, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        plat_release_rw(movie, *new_length);
        return existing;
    }
    say("  desktop quit item added to the title and pause menu\n");
    return movie;
}

static void ud_exit_hide(u64 owner) {
    if (owner != __atomic_load_n(&ud_exit_owner, __ATOMIC_ACQUIRE)) return;
    __atomic_store_n(&ud_exit_owner, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&ud_exit_bounds, 0, __ATOMIC_RELEASE);
}

static void ud_exit_request(void) {
    if (__atomic_exchange_n(&ud_exit_requested, 1, __ATOMIC_ACQ_REL)) return;
    say("  exit requested from the game menu\n");
    if (!plat_request_window_close()) __atomic_store_n(&ud_exit_requested, 0, __ATOMIC_RELEASE);
}

static void ud_exit_show(u64 owner, const char *text) {
    int pos[4] = {0};
    for (int i = 0; i < 4; i++) {
        if (i && *text++ != ' ') return;
        int negative = *text == '-';
        if (negative) text++;
        if (*text < '0' || *text > '9') return;
        while (*text >= '0' && *text <= '9' && pos[i] < 10000) pos[i] = pos[i] * 10 + *text++ - '0';
        if (negative) pos[i] = -pos[i];
        int limit = i & 1 ? 1080 : 1920;
        if (pos[i] < 0) pos[i] = 0;
        if (pos[i] > limit) pos[i] = limit;
    }
    if (*text) return;
    u64 bounds = 0;
    if (pos[2] > pos[0] && pos[3] > pos[1])
        bounds = (u64)pos[0] | ((u64)pos[1] << 16) | ((u64)pos[2] << 32) | ((u64)pos[3] << 48);
    __atomic_store_n(&ud_exit_bounds, bounds, __ATOMIC_RELEASE);
    __atomic_store_n(&ud_exit_owner, owner, __ATOMIC_RELEASE);
    if (g_opt_trace_movies) {
        say("  desktop quit button bounds: "); say_dec(pos[0]); say(", "); say_dec(pos[1]);
        say(" to "); say_dec(pos[2]); say(", "); say_dec(pos[3]); say("\n");
    }
}

#ifdef _WIN32
static int ud_exit_click(void *window, int x, int y, int released) {
    static int pressing;
    u64 owner = __atomic_load_n(&ud_exit_owner, __ATOMIC_ACQUIRE);
    u64 bounds = owner ? __atomic_load_n(&ud_exit_bounds, __ATOMIC_ACQUIRE) : 0;
    i32 client[4];
    int hit = 0;
    if (bounds && GetClientRect(window, client) && client[2] > 0 && client[3] > 0) {
        int w = client[2], h = client[3], dw = w, dh = h, sw = 1920, sh = 1080;
        if ((i64)w * sh > (i64)h * sw) {
            if (g_disp.fill) sh = (int)((i64)h * sw / w);
            else dw = (int)((i64)h * sw / sh);
        } else if (!g_disp.fill) dh = (int)((i64)w * sh / sw);
        /* In narrow fill mode the Scaleform viewport maps its full width to the client. */
        x -= (w - dw) / 2;
        y -= (h - dh) / 2;
        if (dw > 0 && dh > 0 && x >= 0 && x < dw && y >= 0 && y < dh) {
            x = (1920 - sw) / 2 + (int)((i64)x * sw / dw);
            y = (1080 - sh) / 2 + (int)((i64)y * sh / dh);
            int left = (u16)bounds, top = (u16)(bounds >> 16);
            /* The clip includes invisible padding. Keep the hit area near the label. */
            hit = x >= left && x < left + 330 && x < (int)(u16)(bounds >> 32) &&
                  y >= top && y < top + 48 && y < (int)(u16)(bounds >> 48);
        }
    }
    if (!released) return pressing = hit;
    int clicked = pressing && hit && owner == __atomic_load_n(&ud_exit_owner, __ATOMIC_ACQUIRE);
    pressing = 0;
    if (clicked) ud_exit_request();
    return clicked;
}
#endif
