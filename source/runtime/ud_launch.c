/*
 * ud_launch.c - the player-facing start of UntilDawn.exe (project brief section 5).
 *
 *   UntilDawn_PC\
 *     UntilDawn.exe
 *     Game\      the player's own extracted CUSA02636 or CUSA00359 install, version 01.03 (eboot.bin, all_programs.prx,
 *                sce_sys\param.sfo, localcachepink\, Movies\, audio\)
 *     Config\    settings.ini (created with defaults on the first start)
 *     Logs\      UntilDawn.log, renderer log, captures (developer mode)
 *
 * Double-clicking the exe (no command-line arguments): the settings are read, the game folder is validated, and
 * the game starts. If the game files are missing or not the supported version, a message box says exactly what is
 * wrong and where the files are expected, and nothing else happens. Nothing of the original game is shipped: the
 * manifest (ud_manifest_v103.h) holds only file names and sizes, and the two executables are recognised by their
 * SHA-256.
 *
 * Started with arguments (developer runs: e6_loader-style command lines), none of this applies.
 */
#include "ud_manifest_v103.h"

/* ---------------------------------------------------------------- SHA-256 (FIPS 180-4) */
typedef struct { u32 h[8]; u64 len; u8 buf[64]; u32 n; } Sha256;
static const u32 k_sha[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha256 *s, const u8 *p) {
    u32 w[64];
    for (int i = 0; i < 16; i++) w[i] = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        u32 s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        u32 t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + k_sha[i] + w[i];
        u32 t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(Sha256 *s) {
    static const u32 iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    for (int i = 0; i < 8; i++) s->h[i] = iv[i];
    s->len = 0; s->n = 0;
}
static void sha_add(Sha256 *s, const u8 *p, u64 n) {
    s->len += n;
    while (n) {
        if (!s->n && n >= 64) { sha_block(s, p); p += 64; n -= 64; continue; }
        u32 c = 64 - s->n; if (c > n) c = (u32)n;
        mem_copy(s->buf + s->n, p, c); s->n += c; p += c; n -= c;
        if (s->n == 64) { sha_block(s, s->buf); s->n = 0; }
    }
}
static void sha_hex(Sha256 *s, char out[65]) {
    u64 bits = s->len * 8; u8 pad = 0x80, z = 0;
    sha_add(s, &pad, 1);
    while (s->n != 56) sha_add(s, &z, 1);
    u8 L[8]; for (int i = 0; i < 8; i++) L[i] = (u8)(bits >> (56 - 8 * i));
    sha_add(s, L, 8);
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) for (int k = 0; k < 8; k++) out[i * 8 + k] = hx[(s->h[i] >> (28 - 4 * k)) & 15];
    out[64] = 0;
}
/* SHA-256 of a whole file; 0 if it cannot be read */
static int sha_file(const char *path, char out[65]) {
    void *f = hf_open_rb(path); if (!f) return 0;
    static u8 buf[1 << 20]; Sha256 s; sha_init(&s); u64 n;
    while ((n = hf_read(f, buf, sizeof buf)) > 0) sha_add(&s, buf, n);
    hf_close(f); sha_hex(&s, out); return 1;
}

/* ---------------------------------------------------------------- the supported game version */
/* The two 01.03 eboots share the code layout used by the runtime patches and use the same PRX. */
#define UD_EBOOT_V100_SHA "fdd1e3247e1f931e277336099062aa240c362b48ae0a7d68e08390dfbd743789"
#define UD_PRX_V103_SHA   "b0020e11d8236bfd0c57821b673027ad615670df5175557e74ee0b61a27ca3af"

typedef struct {
    const char *title_id, *eboot_sha;
    const UdFile *files;
    u64 file_count;
    int language;   /* PS4 system language, as reported to the game */
} UdGameProfile;

static const UdGameProfile g_ud_profiles[] = {
    {"CUSA02636", "ca291314cdd5308694eb9243f1ba098e97817087e5356d78d94cf8854abebd26",
        g_ud_eu_files, sizeof g_ud_eu_files / sizeof g_ud_eu_files[0], 1},   /* en-US */
    {"CUSA00359", "f7667bbf415edce1f6ac2a5a7d869accc4a1787990f2c776b23a4a8e69ef6d67",
        g_ud_br_files, sizeof g_ud_br_files / sizeof g_ud_br_files[0], 17},   /* pt-BR */
};
static const UdGameProfile *g_ud_profile;
static const u64 g_ud_common_count = sizeof g_ud_common_files / sizeof g_ud_common_files[0];

static const char *ud_game_title_id(void) { return g_ud_profile ? g_ud_profile->title_id : g_ud_profiles[0].title_id; }
static int ud_game_language(void) { return g_ud_profile ? g_ud_profile->language : g_ud_profiles[0].language; }

/* ---------------------------------------------------------------- settings (Config\settings.ini) */
/* Only settings that actually change something are listed. Each one is applied in ud_apply_settings(). */
typedef struct {
    char game_dir[1024];     /* empty: <exe folder>\Game */
    int fps_limit;           /* frame-rate limit: 30, 60, 120, 144, 165 ... ; 0 = uncapped */
    int borderless;          /* 0 = window, 1 = borderless window covering the primary monitor */
    int window_w, window_h;  /* client size of the window (display_mode = windowed) */
    int developer;           /* 1: capture keys (P / O / N), status text in the window, diagnostic reports */
    int renderer_thread;     /* 0.1.4: 1 = the renderer on its own thread (asynchronous submits); 0 = on the game's thread, as in 0.1.3 */
    char gpu[64];            /* t121: "auto" / empty, a number from the log's GPU list, or part of a GPU's name */
    int present_mode;        /* t121: 0 auto, 1 fifo, 2 mailbox, 3 immediate */
    int heap_slot_mb;        /* t130: distance between the game's heap address slots: 512 (default), 64..512, or 2048 = the game's own */
    int heap_slot_reuse;     /* t131: 1 = a released heap slot is reused at once (first fit); 0 = the game's round-robin */
    int shader_prep;         /* t159: 1 = shaders of earlier sessions prepared before the game starts (e6_prep.c); 0 = off */
    int gpu_coherence;       /* t165x: 1 = CPU reads / writes of memory the GPU wrote see the GPU's bytes (default); 0 = off */
    int gpu_coherence_off;   /* t165x: an "off" line was seen (it wins over "on" lines) */
    int halloween_hunt;      /* v0.3.0: 1 = the game's own Halloween Hunt 2015 (only its date checks see 31 October 2015); default 0 */
    /* w6_disp */
    int seen_mode, seen_rr;  /* the file has a display_mode / render_resolution line */
    int disp_auto;           /* display_system = auto: the display system (borderless + automatic resolution) is on */
    int monitor;             /* 0 = auto (primary), else the monitor's number in the log's list */
    int have_pos, win_x, win_y;   /* windowed: where the window was (saved by the program) */
    int rr_mode, rr_w, rr_h; /* render_resolution: 0 / 1 = auto, 2 = explicit */
    /* w16_rel (v0.4.0) */
    int fast_render;         /* optimised_rendering: 1 = Optimised Rendering (Fast Mode) on (default), 0 = off */
    int gpu_opt;             /* gpu_optimisations: 1 = the v0.4.0 renderer / GPU optimisations on (default), 0 = off (troubleshooting) */
    int seen_fast, seen_gpuopt, seen_fps, seen_disp;   /* the file has such a line */
    int fps_asked;           /* frame_rate_limit as written (before the 240 ceiling) */
    int fresh;               /* rc3: the file was written with the defaults at this start (its values are the defaults, not the player's) */
    int xbox_prompts;
    int show_launcher, lod_distance;
    int fullscreen;
} UdSettings;
static UdSettings g_uds = {"", 60, 0, 1280, 720, 0, 1, "", 0, 512, 1, 1, 1, 0, 0};
static int g_ud_release;     /* started as the player-facing program (no command-line arguments) */

static int ud_key_is(const char *a, const char *b) { while (*b) { if ((*a | 0x20) != (*b | 0x20)) return 0; a++; b++; } return *a == 0; }
static void ud_trim(char *s) { int n = str_len(s); while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0; }
static int ud_atoi(const char *s) { int v = 0, neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return neg ? -v : v; }
/* w16_rel: on / off value (1, on, true, yes, enabled / 0, off, false, no, disabled; an inline ; or # comment ignored); other text: def */
static int ud_bool(const char *v, int def) {
    char b[32]; int k = 0; while (*v == ' ' || *v == '\t' || *v == '"' || *v == '\'') v++;
    while (v[k] && v[k] != ';' && v[k] != '#' && v[k] != '"' && v[k] != '\'' && k < 31) { b[k] = v[k]; k++; } b[k] = 0; ud_trim(b);
    if (!b[0]) return def;
    if (ud_key_is(b, "1") || ud_key_is(b, "on") || ud_key_is(b, "true") || ud_key_is(b, "yes") || ud_key_is(b, "enabled") || ud_key_is(b, "enable")) return 1;
    if (ud_key_is(b, "0") || ud_key_is(b, "off") || ud_key_is(b, "false") || ud_key_is(b, "no") || ud_key_is(b, "disabled") || ud_key_is(b, "disable")) return 0;
    return def;
}
enum { UD_FPS_CEILING = 240 };   /* w16_rel: the highest frame-rate limit (w14_speed: physics and cloth keep real time up to 240 fps) */

static const char *k_ud_ini_default =   /* w16_rel: the v0.4.0 defaults (written on the first start; earlier files are kept and only get the new lines) */
    "; Until Dawn (native PC build) - settings\r\n"
    "; Delete this file to restore the defaults; it is recreated on the next start.\r\n"
    "\r\n"
    "; Folder with the original game files (your own CUSA02636 or CUSA00359 install with the 01.03 update).\r\n"
    "; Empty = the Game folder next to UntilDawn.exe.\r\n"
    "game_dir =\r\n"
    "\r\n"
    "; button_prompts = xbox displays A/B/X/Y, LB/RB and RT in supported prompts. Restart after changing.\r\n"
    "button_prompts = playstation\r\n"
    "\r\n"
    "; Launcher with game folder, controller icons, display settings and experimental LOD distance.\r\n"
    "show_launcher = 1\r\n"
    "lod_distance = 100\r\n"
    "\r\n"
    "; Frame-rate limit: 30, 60, 120, 240 or uncapped (uncapped can also be written as 0). 60 is the default.\r\n"
    "; uncapped still stops at 240 frames a second, so that physics and cloth keep real time; a number above 240\r\n"
    "; counts as 240. This setting is independent of optimised_rendering below.\r\n"
    "frame_rate_limit = 60\r\n"
    "\r\n"
    "; Optimised Rendering (Fast Mode): 1 = on (default), 0 = off.\r\n"
    "; The game goes on with its next frame while the graphics card finishes the previous one, with safety checks\r\n"
    "; that keep the game's memory consistent. Clearly faster, the same picture. It does not change the frame-rate\r\n"
    "; limit. If you see a problem, set 0 to check whether it is related.\r\n"
    "optimised_rendering = 1\r\n"
    "\r\n"
    "; borderless fills the monitor at the desktop resolution; windowed has a frame and can be resized.\r\n"
    "; fullscreen requests a temporary monitor mode while focused, using render_resolution when supported.\r\n"
    "; Alt+Enter switches between a window and the chosen full-screen mode. The picture keeps its 16:9 shape.\r\n"
    "display_mode = borderless\r\n"
    "\r\n"
    "; Monitor: auto (the primary monitor) or its number in the monitor list of Logs\\UntilDawn.log (1, 2, ...).\r\n"
    "monitor = auto\r\n"
    "\r\n"
    "; Window size for display_mode = windowed\r\n"
    "window_width = 1280\r\n"
    "window_height = 720\r\n"
    "\r\n"
    "; Rendering resolution: auto (default) = the resolution of the monitor (or of the window): 1920x1080 on a 1080p\r\n"
    "; monitor, 2560x1440 on a 1440p monitor, 3840x2160 on a 4K monitor - never below the game's own 1920x1080, and\r\n"
    "; lowered by itself if the graphics card has too little memory (see Logs\\UntilDawn.log). Or a fixed value: 1080p,\r\n"
    "; 1440p, 2160p or WIDTHxHEIGHT, e.g. 1080p if the game runs too slowly at the monitor's resolution; the picture is\r\n"
    "; then scaled to the monitor. Restart after changing.\r\n"
    "render_resolution = auto\r\n"
    "\r\n"
    "; 1 = the renderer runs on its own thread, alongside the game (faster in most scenes).\r\n"
    "; 0 = the renderer runs on the game's thread, as in version 0.1.3 - try this if the game crashes or stutters.\r\n"
    "renderer_thread = 1\r\n"
    "\r\n"
    "; Graphics card: auto (normally the dedicated one), or a number or part of the name from the GPU list in\r\n"
    "; Logs\\UntilDawn.log (\"GPU 0: ...\", \"GPU 1: ...\"), e.g. gpu = 1 or gpu = Radeon\r\n"
    "gpu = auto\r\n"
    "\r\n"
    "; How frames are shown: auto, fifo (in step with the display), mailbox or immediate. auto uses mailbox and\r\n"
    "; switches to fifo by itself if frames take very long to reach the display (some laptops with two GPUs).\r\n"
    "present_mode = auto\r\n"
    "\r\n"
    "; 1 = the graphics optimisations of version 0.4.0 (cheaper depth and copy work on the graphics card, faster shader\r\n"
    "; handling). Same picture. 0 = off, only for troubleshooting (some shaders may be prepared again after a change).\r\n"
    "gpu_optimisations = 1\r\n"
    "\r\n"
    "; Distance between the game's heap address slots in MB: 512 (default; keeps all of the game's memory where the\r\n"
    "; GPU can read it), or 2048 = the game's own layout (the GPU loses sight of memory mapped after about 30 minutes).\r\n"
    "heap_slot_mb = 512\r\n"
    "\r\n"
    "; 1 = a heap address slot the game has released is reused at once (keeps memory use level); 0 = the game's own\r\n"
    "; order (each new heap takes the next slot; memory use grows for about two hours of play).\r\n"
    "heap_slot_reuse = 1\r\n"
    "\r\n"
    "; 1 = shaders are prepared before the game starts: on the first start those listed in pipelines.udm (read\r\n"
    "; from your own game files), later also those the game needed in earlier sessions. A progress screen appears\r\n"
    "; when that takes a while (first start, new version, graphics driver update); Esc or Circle skips it. 0 = off.\r\n"
    "shader_preparation = 1\r\n"
    "\r\n"
    "; 1 = memory the graphics card wrote is brought back before the game's code reads or writes it (fixes the stretched\r\n"
    "; geometry and the missing lake water). 0 = off, only for troubleshooting.\r\n"
    "gpu_coherence = 1\r\n"
    "\r\n"
    "; 1 = Halloween Hunt 2015: the game's original Halloween screen and hidden pumpkins (an event that was only active\r\n"
    "; from 28 to 31 October 2015; your Windows date is not changed). 0 = off, the normal game. Restart after changing.\r\n"
    "halloween_hunt = 0\r\n"
    "\r\n"
    "; 1 = developer mode: capture keys (Ctrl+P, Ctrl+O, Ctrl+N, F6, F11), status text, diagnostic files in Logs\\captures\r\n"
    "developer = 0\r\n";

/* w6_disp: "auto", "1080p" / "1440p" / "2160p" / "4k", "WIDTHxHEIGHT" -> mode 1 (auto) or 2 (w, h); anything else: auto */
/* w6_disp: the display lines of the settings file once the display system is the default (UD_DISPLAY_DEFAULT_ON); until
   then it is on with  display_system = auto  in settings.ini (or --display-auto) and the template is the old one */
#if 0
    "; borderless (default) = the game fills the whole monitor, at the monitor's own resolution; windowed = a window.\r\n"
    "; Alt+Enter switches between the two while the game runs. The picture always keeps its 16:9 shape (black bars\r\n"
    "; on other monitor shapes, never stretched).\r\n"
    "display_mode = borderless\r\n"
    "\r\n"
    "; Monitor: auto (the primary monitor) or its number in the monitor list of Logs\\UntilDawn.log (1, 2, ...).\r\n"
    "monitor = auto\r\n"
    "\r\n"
    "; Window size for display_mode = windowed\r\n"
    "window_width = 1280\r\n"
    "window_height = 720\r\n"
    "\r\n"
    "; Rendering resolution: auto = the resolution of the monitor (or of the window): 1920x1080 on a 1080p monitor,\r\n"
    "; 2560x1440 on a 1440p monitor, 3840x2160 on a 4K monitor - never below the game's own 1920x1080, and lowered by\r\n"
    "; itself if the graphics card has too little memory (see Logs\\UntilDawn.log). Or a fixed value: 1080p, 1440p,\r\n"
    "; 2160p or WIDTHxHEIGHT, e.g. 1080p if the game runs too slowly at the monitor's resolution; the picture is then\r\n"
    "; scaled to the monitor. Restart after changing.\r\n"
    "render_resolution = auto\r\n"
    "\r\n"
#endif
static void ud_parse_res(const char *v, int *mode, int *w, int *h) {
    *mode = 1; *w = *h = 0;
    while (*v == ' ') v++;
    if (ud_key_is(v, "4k")) { *mode = 2; *w = 3840; *h = 2160; return; }
    int a = 0, n = 0; while (v[n] >= '0' && v[n] <= '9') { a = a * 10 + (v[n] - '0'); n++; if (a > 100000) return; }
    if (!n) return;
    if ((v[n] | 0x20) == 'p' && !v[n + 1]) { if (a >= 240 && a <= 8640) { *mode = 2; *h = a; *w = (a * 16 + 8) / 9; } return; }
    if ((v[n] | 0x20) == 'x') { int b = ud_atoi(v + n + 1); if (a >= 320 && a <= 16384 && b >= 240 && b <= 16384) { *mode = 2; *w = a; *h = b; } }
}
static char g_ud_ini_path[1100];
typedef struct { const char *key, *value, *comment; } UdIniEdit;

static int ud_ini_matches(const char *line, const char *end, const char *key) {
    while (line < end && (*line == ' ' || *line == '\t')) line++;
    while (*key && line < end && (*line | 0x20) == (*key | 0x20)) { line++; key++; }
    if (*key) return 0;
    while (line < end && (*line == ' ' || *line == '\t')) line++;
    return line < end && *line == '=';
}

static u64 ud_ini_line(char *out, const UdIniEdit *edit) {
    u64 k = str_len(edit->key), v = str_len(edit->value);
    mem_copy(out, edit->key, k);
    mem_copy(out + k, " = ", 3);
    mem_copy(out + k + 3, edit->value, v);
    mem_copy(out + k + 3 + v, "\r\n", 2);
    return k + v + 5;
}

/* Rewrite selected keys together, retaining unrelated settings and comments. */
static int ud_ini_update(const UdIniEdit *edits, int count) {
    if (!g_ud_ini_path[0] || count < 1 || count > 16) return 0;
    u64 n = 0;
    char *old = (char *)plat_read_file(g_ud_ini_path, &n);
    if (!old) return 0;
    u64 cap = n;
    for (int i = 0; i < count; i++)
        cap += str_len(edits[i].key) + str_len(edits[i].value) +
               (edits[i].comment ? str_len(edits[i].comment) : 0) + 10;
    char *out = cap <= 0xffffffffu ? (char *)malloc(cap) : 0;
    u64 written = 0, pos = 0;
    u32 seen = 0;
    if (out) while (pos < n) {
        u64 start = pos;
        while (pos < n && old[pos] != '\n') pos++;
        u64 end = pos;
        if (pos < n) pos++;
        int i = 0;
        while (i < count && !ud_ini_matches(old + start, old + end, edits[i].key)) i++;
        if (i == count) {
            mem_copy(out + written, old + start, pos - start);
            written += pos - start;
        } else if (!(seen & (1u << i))) {
            written += ud_ini_line(out + written, edits + i);
            seen |= 1u << i;
        }
    }
#ifdef _WIN32
    plat_free_pages(old);
#else
    free(old);
#endif
    if (!out) return 0;
    for (int i = 0; i < count; i++) if (!(seen & (1u << i))) {
        if (written && out[written - 1] != '\n') { out[written++] = '\r'; out[written++] = '\n'; }
        if (edits[i].comment) {
            out[written++] = '\r'; out[written++] = '\n';
            u64 length = str_len(edits[i].comment);
            mem_copy(out + written, edits[i].comment, length);
            written += length;
        }
        written += ud_ini_line(out + written, edits + i);
    }
    char tmp[sizeof g_ud_ini_path + 32];
    int k = str_len(g_ud_ini_path);
    mem_copy(tmp, g_ud_ini_path, k);
    mem_copy(tmp + k, ".tmp-", 5);
#ifdef _WIN32
    fmt_dec(tmp + k + 5, GetCurrentProcessId());
#else
    fmt_dec(tmp + k + 5, getpid());
#endif
    void *file = plat_open_write(tmp);
    int ok = 0;
    if (file) {
#ifdef _WIN32
        u32 bytes = 0;
        ok = WriteFile(file, out, (u32)written, &bytes, 0) && bytes == written;
#else
        ok = fwrite(out, 1, written, (FILE *)file) == written && fflush((FILE *)file) == 0;
#endif
        plat_close(file);
        if (ok) ok = plat_replace_file(tmp, g_ud_ini_path);
        if (!ok) plat_unlink(tmp);
    }
    free(out);
    return ok;
}

static void ud_ini_set(const char *key, const char *value, const char *comment) {
    const UdIniEdit edit = {key, value, comment};
    ud_ini_update(&edit, 1);
}
static void ud_parse_ini(char *t) {
    while (*t) {
        char *line = t; while (*t && *t != '\n') t++; if (*t) *t++ = 0;
        ud_trim(line); while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == ';' || *line == '#' || *line == '[') continue;
        char *eq = line; while (*eq && *eq != '=') eq++; if (!*eq) continue;
        *eq = 0; char *v = eq + 1; ud_trim(line); while (*v == ' ' || *v == '\t') v++; ud_trim(v);
        if (ud_key_is(line, "game_dir")) { int n = str_len(v); if (n >= (int)sizeof g_uds.game_dir) n = sizeof g_uds.game_dir - 1; mem_copy(g_uds.game_dir, v, (u64)n); g_uds.game_dir[n] = 0; }
        else if (ud_key_is(line, "button_prompts")) g_uds.xbox_prompts = ud_key_is(v, "xbox");
        else if (ud_key_is(line, "show_launcher")) g_uds.show_launcher = ud_bool(v, 1);
        else if (ud_key_is(line, "lod_distance")) {
            int pct = str_len(v) == 3 ? ud_atoi(v) : 100;
            g_uds.lod_distance = pct >= 100 && pct <= 150 ? pct : 100;
        }
        else if (ud_key_is(line, "frame_rate_limit")) {   /* w16_rel: 30 / 60 / 120 / 240 / uncapped (= 0); any other number up to 240 still works, above 240 counts as 240 */
            int x = ud_atoi(v); if (ud_key_is(v, "uncapped") || ud_key_is(v, "unlimited") || ud_key_is(v, "none") || ud_key_is(v, "off")) x = 0;
            else if (v[0] == '-') x = 0;   /* (a negative number: uncapped, as before) */
            else if (!(v[0] >= '0' && v[0] <= '9')) x = 60;   /* (unreadable: the default) */
            if (x < 0) x = 0; if (x > 100000) x = 100000;
            g_uds.fps_asked = x; g_uds.fps_limit = x > UD_FPS_CEILING ? UD_FPS_CEILING : x; g_uds.seen_fps = 1; }
        else if (ud_key_is(line, "optimised_rendering") || ud_key_is(line, "optimized_rendering") || ud_key_is(line, "fast_mode")) { g_uds.fast_render = ud_bool(v, 1); g_uds.seen_fast = 1; }   /* w16_rel */
        else if (ud_key_is(line, "gpu_optimisations") || ud_key_is(line, "gpu_optimizations")) { g_uds.gpu_opt = ud_bool(v, 1); g_uds.seen_gpuopt = 1; }   /* w16_rel */
        else if (ud_key_is(line, "display_mode")) { g_uds.fullscreen = ud_key_is(v, "fullscreen"); g_uds.borderless = g_uds.fullscreen || ud_key_is(v, "borderless"); g_uds.seen_mode = 1; }
        else if (ud_key_is(line, "display_system")) { g_uds.disp_auto = !(ud_key_is(v, "off") || ud_key_is(v, "legacy") || ud_key_is(v, "old") || (v[0] == '0' && !v[1])); g_uds.seen_disp = 1; }   /* w6_disp; w16_rel: on unless off / legacy / 0 (v0.4.0: the default) */
        else if (ud_key_is(line, "monitor")) { int x = ud_atoi(v); g_uds.monitor = x >= 1 && x <= 8 ? x : 0; }   /* w6_disp */
        else if (ud_key_is(line, "window_x")) { g_uds.win_x = ud_atoi(v); g_uds.have_pos |= 1; }
        else if (ud_key_is(line, "window_y")) { g_uds.win_y = ud_atoi(v); g_uds.have_pos |= 2; }
        else if (ud_key_is(line, "render_resolution")) { g_uds.seen_rr = 1; ud_parse_res(v, &g_uds.rr_mode, &g_uds.rr_w, &g_uds.rr_h); }
        else if (ud_key_is(line, "window_width")) { int x = ud_atoi(v); if (x >= 320 && x <= 16384) g_uds.window_w = x; }
        else if (ud_key_is(line, "window_height")) { int x = ud_atoi(v); if (x >= 240 && x <= 16384) g_uds.window_h = x; }
        else if (ud_key_is(line, "developer")) g_uds.developer = ud_atoi(v) != 0;
        else if (ud_key_is(line, "renderer_thread")) g_uds.renderer_thread = ud_atoi(v) != 0;
        else if (ud_key_is(line, "gpu")) { int n = str_len(v); if (n >= (int)sizeof g_uds.gpu) n = sizeof g_uds.gpu - 1; mem_copy(g_uds.gpu, v, (u64)n); g_uds.gpu[n] = 0; }
        else if (ud_key_is(line, "present_mode")) g_uds.present_mode = ud_key_is(v, "fifo") ? 1 : ud_key_is(v, "mailbox") ? 2 : ud_key_is(v, "immediate") ? 3 : 0;
        else if (ud_key_is(line, "halloween_hunt")) g_uds.halloween_hunt = ud_atoi(v) == 1 || ud_key_is(v, "on") || ud_key_is(v, "true") || ud_key_is(v, "yes");   /* t167 */
        else if (ud_key_is(line, "heap_slot_reuse")) g_uds.heap_slot_reuse = ud_atoi(v) != 0;   /* t131 */
        else if (ud_key_is(line, "gpu_coherence")) {   /* t165x: on / true / yes / 1 = on; 0 / off / false / no = off (an inline ; or # comment
                                                           ignored). Review 6 X2-2: an "off" line anywhere wins - the switch must work when added above or below the default line */
            char vb[32]; int k = 0; while (v[k] && v[k] != ';' && v[k] != '#' && !(v[k] == '/' && v[k + 1] == '/') && k < 31) { vb[k] = v[k]; k++; } vb[k] = 0; ud_trim(vb);
            char *q = vb; if (*q == '"' || *q == '\'') { q++; int m = str_len(q); if (m && (q[m - 1] == '"' || q[m - 1] == '\'')) q[m - 1] = 0; ud_trim(q); while (*q == ' ' || *q == '\t') q++; }   /* (review 7 N4: quotes, // comments, 00, n, disable) */
            int dz = *q != 0; for (char *c = q; *c; c++) if (*c != '0') dz = 0;
            int off = dz || ud_key_is(q, "off") || ud_key_is(q, "false") || ud_key_is(q, "no") || ud_key_is(q, "n") || ud_key_is(q, "disable") || ud_key_is(q, "disabled");
            if (off) g_uds.gpu_coherence_off = 1;
            g_uds.gpu_coherence = !g_uds.gpu_coherence_off; }
        else if (ud_key_is(line, "shader_preparation")) { int pv = ud_atoi(v); g_uds.shader_prep = pv == 2 ? 2 : pv != 0; }   /* t159; t164: 2 = background */
        else if (ud_key_is(line, "heap_slot_mb")) { int x = ud_atoi(v); g_uds.heap_slot_mb = x == 2048 ? 2048 : x < 64 ? 64 : x > 512 ? 512 : x; }   /* t130 */
    }
}
static void ud_load_settings(const char *cfg_dir) {
    char p[1100]; path_join(p, cfg_dir, "settings.ini"); { int k = 0; while (p[k] && k < (int)sizeof g_ud_ini_path - 1) { g_ud_ini_path[k] = p[k]; k++; } g_ud_ini_path[k] = 0; }
    g_uds.disp_auto = 1; g_uds.fast_render = 1; g_uds.gpu_opt = 1; g_uds.fps_asked = 60;   /* w16_rel: the v0.4.0 defaults (a line in the file wins) */
    g_uds.lod_distance = 100;
    g_uds.show_launcher = 1;
    u64 n = 0; char *t = (char *)plat_read_file(p, &n);
    if (!t) {   /* first start: write the defaults */
        void *f = plat_open_write(p);
        if (f) { plat_write(f, k_ud_ini_default); plat_close(f); }
        say("settings: no Config\\settings.ini - written with the defaults (first start)\n");
        n = (u64)str_len(k_ud_ini_default); t = (char *)k_ud_ini_default; g_uds.fresh = 1;   /* w16_rel: read like any other file (its display lines are the new ones: no migration) */
    }
    char *z = (char *)malloc(n + 1); if (!z) return;
    mem_copy(z, t, n); z[n] = 0; ud_parse_ini(z); free(z);
}

/* ---------------------------------------------------------------- validation of the game folder */
typedef struct { char *b; int n, cap; } UdMsg;
static void um(UdMsg *m, const char *s) { while (*s && m->n + 1 < m->cap) m->b[m->n++] = *s++; m->b[m->n] = 0; }
static void um_d(UdMsg *m, i64 v) { char t[24]; fmt_dec(t, v); um(m, t); }

static int ud_read_game_metadata(const char *dir, char id[32], char version[16]) {
    char path[1200];
    path_join(path, dir, "sce_sys/param.sfo");
    void *file = hf_open_rb(path);
    if (!file) return 0;
    u8 data[4096];
    u64 size = hf_read(file, data, sizeof data);
    hf_close(file);
    sfo_str(data, size, "TITLE_ID", id, 32);
    sfo_str(data, size, "APP_VER", version, 16);
    return 1;
}

/* Direct developer runs still accept arbitrary dumps. Use the local title for language and add-ons. */
static void ud_detect_game_profile(const char *dir) {
    char id[32] = "", version[16] = "";
    if (!ud_read_game_metadata(dir, id, version) || !str_eq(version, "01.03")) return;
    for (u64 i = 0; i < sizeof g_ud_profiles / sizeof g_ud_profiles[0]; i++) {
        if (str_eq(id, g_ud_profiles[i].title_id)) {
            g_ud_profile = &g_ud_profiles[i];
            return;
        }
    }
}

/* 0 = the folder holds the supported game files; otherwise msg says what is wrong */
static int ud_validate(const char *dir, UdMsg *msg) {
    char p[1200], h[65];
    if (!plat_is_dir(dir)) {
        um(msg, "The game folder was not found:\n    "); um(msg, dir);
        um(msg, "\n\nUntil Dawn needs the original game files from your own copy of the PS4 game (CUSA02636 or CUSA00359, "
                "updated to version 01.03), extracted into that folder. See README.txt next to UntilDawn.exe.");
        return 1;
    }
    path_join(p, dir, "eboot.bin");
    if (!sha_file(p, h)) {
        um(msg, "eboot.bin is missing from the game folder:\n    "); um(msg, dir);
        um(msg, "\n\nThe folder must contain the extracted PS4 game (CUSA02636 or CUSA00359) with the 01.03 update applied: "
                "eboot.bin, all_programs.prx, sce_sys\\param.sfo, localcachepink\\, Movies\\ and audio\\.");
        return 2;
    }
    const UdGameProfile *profile = 0;
    for (u64 i = 0; i < sizeof g_ud_profiles / sizeof g_ud_profiles[0]; i++)
        if (str_eq(h, g_ud_profiles[i].eboot_sha)) { profile = &g_ud_profiles[i]; break; }
    if (!profile) {
        if (str_eq(h, UD_EBOOT_V100_SHA))
            um(msg, "The game folder holds version 01.00 of Until Dawn. This program needs version 01.03: "
                    "apply the 01.03 update to the extracted game files (the update replaces eboot.bin, "
                    "all_programs.prx and 151 data files).");
        else { um(msg, "eboot.bin does not match Until Dawn CUSA02636 or CUSA00359, version 01.03.\n\nFound SHA-256:\n    "); um(msg, h);
               um(msg, "\n\nThe file may be damaged, from another region or another version, or modified."); }
        um(msg, "\n\nGame folder: "); um(msg, dir);
        return 3;
    }
    path_join(p, dir, "all_programs.prx");
    if (!sha_file(p, h) || !str_eq(h, UD_PRX_V103_SHA)) {
        um(msg, "all_programs.prx in the game folder is missing or is not the version that belongs to eboot.bin 01.03.\n\nGame folder: "); um(msg, dir);
        return 4;
    }
    char id[32] = "", version[16] = "";
    int has_sfo = ud_read_game_metadata(dir, id, version);
    if (!has_sfo || !str_eq(id, profile->title_id) || !str_eq(version, "01.03")) {
        um(msg, "sce_sys\\param.sfo is missing or does not match the eboot.bin for ");
        um(msg, profile->title_id); um(msg, " version 01.03");
        if (has_sfo) { um(msg, " (found title "); um(msg, id[0] ? id : "?"); um(msg, ", version "); um(msg, version[0] ? version : "?"); um(msg, ")"); }
        um(msg, ".\n\nGame folder: "); um(msg, dir);
        return 5;
    }
    /* every data file: present with the right size */
    int nbad = 0, nmiss = 0;
    const UdFile *groups[] = {g_ud_common_files, profile->files};
    const u64 counts[] = {g_ud_common_count, profile->file_count};
    UdMsg list = {0}; static char lb[2048]; list.b = lb; list.cap = sizeof lb; lb[0] = 0;
    for (int group = 0; group < 2; group++) {
        for (u64 i = 0; i < counts[group]; i++) {
            const UdFile *file = &groups[group][i];
            path_join(p, dir, file->path);
            i64 sz = plat_file_size(p);
            if (sz == (i64)file->size) continue;
            if (sz < 0) nmiss++; else nbad++;
            if (nmiss + nbad <= 8) { um(&list, "    "); um(&list, file->path); um(&list, sz < 0 ? "  (missing)\n" : "  (wrong size)\n"); }
        }
    }
    if (nmiss || nbad) {
        um(msg, "Some of the original game files are missing or incomplete (");
        um_d(msg, nmiss); um(msg, " missing, "); um_d(msg, nbad); um(msg, " with the wrong size, of ");
        um_d(msg, (i64)(g_ud_common_count + profile->file_count)); um(msg, " checked):\n\n"); um(msg, lb);
        if (nmiss + nbad > 8) um(msg, "    ...\n");
        um(msg, "\nCopy the complete extracted game (with the 01.03 update) into:\n    "); um(msg, dir);
        return 6;
    }
    g_ud_profile = profile;
    return 0;
}

static int ud_check_game(const char *game, UdMsg *msg) {
    say("game folder: "); say(game); say("\n");
    u64 t0 = plat_ticks();
    int bad = ud_validate(game, msg);
    say("game files checked in "); say_dec((i64)((plat_ticks() - t0) * 1000 / plat_ticks_per_sec())); say(" ms: ");
    if (bad) {
        say("NOT USABLE\n"); say(msg->b); say("\n");
    } else {
        say("ok ("); say(ud_game_title_id()); say(" 01.03, ");
        say_dec((i64)(g_ud_common_count + g_ud_profile->file_count)); say(" files)\n");
    }
    return bad;
}

/* ---------------------------------------------------------------- w6_disp: display mode and automatic resolution
   The player no longer chooses a percentage. The display system (a player start, or --display-auto / any --display-* /
   --monitor / --window / --sim-monitor option in a developer run) decides:
     display mode   borderless (default: a window covering the chosen monitor at its own resolution) | windowed
     monitor        auto = the primary one, or a number from the list in the log; a number that does not exist: primary
     picture area   the largest 16:9 rectangle of the window (pillarbox on 21:9 / 32:9, letterbox on 16:10 / 4:3; the
                    presentation never stretches)
     internal resolution (3D, interface, text, movies' target): auto = the picture area's, or render_resolution /
                    --render-res. The renderer's scaled targets support the ratios 1, 5/4, 4/3, 3/2, 7/4, 2 ... 4 of
                    1920x1080 (the pixel mapping of gcn2spv.c is verified for these): the smallest of them that is not
                    below the wanted height is used and the picture is filtered down to the area when it is larger
                    (2304x1296 -> 2400x1350). Never below 1 (1920x1080): a smaller window or monitor shows the 1080p
                    picture filtered down, as every version did. The ratio is fixed for the session (shaders are
                    translated per ratio and the interface's viewport is patched into the game's code at start): moving
                    the window to another monitor or Alt+Enter changes the presentation only.
     video memory   the screen-sized targets take about 950 MB x ratio^2 (measured: cave sessions 60_05 / res133_03 /
                    res200_02); a ratio is used only if that plus 1.5 GB of textures fits the image budget the card
                    will get (cards under 12 GB: the --vram-fit budget, else half the card) - otherwise the next lower
                    ratio, said in the log. --no-vram-step: off.
   --render-scale N stays the developer switch: given, it decides the ratio and nothing here changes it.
   Saving (player start, or --display-save): display_mode on Alt+Enter, the window's size and place when the user changed
   them, the monitor when a borderless window was moved to another one. */
static int g_dopt_on, g_dopt_save, g_dopt_mode = -1, g_dopt_mon = -1, g_dopt_cw, g_dopt_ch, g_dopt_simw, g_dopt_simh, g_dopt_no_vram;
static int g_rr_mode, g_rr_w, g_rr_h;   /* --render-res: 0 not given, 1 auto, 2 explicit */
static int g_disp_start_mon = -1, g_disp_start_mode;
static const int k_disp_pct[] = {100, 125, 133, 150, 175, 200, 225, 250, 275, 300, 325, 350, 375, 400};
enum { DISP_NPCT = sizeof k_disp_pct / sizeof k_disp_pct[0] };
static int disp_pct_h(int pct) { return pct == 133 ? 1440 : (1080 * pct + 99) / 100; }
static int disp_pct_w(int pct) { return pct == 133 ? 2560 : (1920 * pct + 99) / 100; }
static int disp_pick(int th) { for (int i = 0; i < DISP_NPCT; i++) if (disp_pct_h(k_disp_pct[i]) >= th) return i; return DISP_NPCT - 1; }
static u64 disp_need_mb(int pct) { u64 n = pct == 133 ? 16 : (u64)pct * pct, d = pct == 133 ? 9 : 10000; return 950 * n / d; }
static void disp_fmt(char *o, int v) { char t[24]; fmt_dec(t, v); int k = 0; while (t[k]) { o[k] = t[k]; k++; } o[k] = 0; }
static void disp_save(void) {   /* window thread: something worth keeping changed */
    say("display: "); say(g_disp.borderless ? "borderless on monitor " : "windowed "); if (g_disp.borderless) say_dec(g_disp.mon + 1); else { say_dec(g_disp.w); say("x"); say_dec(g_disp.h); say(" at "); say_dec(g_disp.x); say(","); say_dec(g_disp.y); }
    say(g_dopt_save ? " (saved to Config\\settings.ini)\n" : " (not saved: not a player start)\n");
    if (!g_dopt_save) return;
    char v[24];
    if (g_disp.borderless != g_disp_start_mode) { ud_ini_set("display_mode", g_disp.borderless ? (g_disp.fullscreen ? "fullscreen" : "borderless") : "windowed", 0); g_disp_start_mode = g_disp.borderless; }
    if (g_disp.borderless) { if (g_disp.mon != g_disp_start_mon) { disp_fmt(v, g_disp.mon + 1); ud_ini_set("monitor", v, 0); g_disp_start_mon = g_disp.mon; } }
    else { disp_fmt(v, g_disp.w); ud_ini_set("window_width", v, 0); disp_fmt(v, g_disp.h); ud_ini_set("window_height", v, 0);
           disp_fmt(v, g_disp.x); ud_ini_set("window_x", v, "; Where the window was (written by the program)\r\n"); disp_fmt(v, g_disp.y); ud_ini_set("window_y", v, 0); }
}
/* after the options are parsed, before the game's code is patched and the window is made */
static void disp_setup(int replay) {
    int rr_mode = g_rr_mode ? g_rr_mode : g_uds.rr_mode, rr_w = g_rr_mode ? g_rr_w : g_uds.rr_w, rr_h = g_rr_mode ? g_rr_h : g_uds.rr_h;
    int forced = g_rs_pct > 100;   /* --render-scale: the developer switch decides */
    if (replay || !g_dopt_on) {   /* no display system: only an explicit internal resolution, as --render-scale would give it */
        if (g_rr_mode == 2 && !forced) { int th = rr_h < (rr_w * 9 + 8) / 16 ? rr_h : (rr_w * 9 + 8) / 16; g_rs_pct = k_disp_pct[disp_pick(th)];
            say("display: internal resolution "); say_dec(disp_pct_w(g_rs_pct)); say("x"); say_dec(disp_pct_h(g_rs_pct)); say(" (--render-res "); say_dec(rr_w); say("x"); say_dec(rr_h); say(")\n"); }
        return;
    }
    g_disp.borderless = g_dopt_mode >= 0 ? g_dopt_mode : g_ud_release ? (g_uds.seen_mode ? g_uds.borderless : 1) : 1;
    g_disp.fullscreen = g_dopt_mode < 0 && g_ud_release && g_uds.fullscreen;
    if (rr_mode == 2) { g_disp.fullscreen_w = rr_w; g_disp.fullscreen_h = rr_h; }
    g_disp.mon_req = g_dopt_mon >= 0 ? g_dopt_mon : g_ud_release ? g_uds.monitor : 0;
    g_disp.cw_req = g_dopt_cw ? g_dopt_cw : g_ud_release ? g_uds.window_w : 1280; g_disp.ch_req = g_dopt_ch ? g_dopt_ch : g_ud_release ? g_uds.window_h : 720;
    if (g_ud_release && g_uds.have_pos == 3 && !g_dopt_cw && g_dopt_mon < 0) { g_disp.have_pos = 1; g_disp.px = g_uds.win_x; g_disp.py = g_uds.win_y; }
    g_disp.sim_w = g_dopt_simw; g_disp.sim_h = g_dopt_simh;
    if (!plat_disp_resolve()) { say("display: no monitor information from Windows - the window as the settings say, internal resolution 1920x1080\n"); g_win_borderless = g_disp.borderless; g_win_cw = g_disp.cw_req; g_win_ch = g_disp.ch_req; return; }
    g_disp.on = 1; g_disp_start_mon = g_disp.mon_req ? g_disp.mon : -1; g_disp_start_mode = g_disp.borderless; g_disp_changed_cb = disp_save;
    g_win_borderless = g_disp.borderless; g_win_cw = g_disp.w; g_win_ch = g_disp.h; g_win_rs_mon_w = g_mon[g_disp.mon].w; g_win_rs_mon_h = g_mon[g_disp.mon].ht;
    say("display: "); say_dec(g_nmon); say(g_nmon == 1 ? " monitor" : " monitors"); say(g_disp.dpi_mode == 2 ? " (DPI-aware per monitor)" : g_disp.dpi_mode == 1 ? " (DPI-aware, system)" : " (NOT DPI-aware)"); say(":");
    for (int i = 0; i < g_nmon; i++) { say(i ? "; " : " "); say_dec(i + 1); say(": "); say_dec(g_mon[i].w); say("x"); say_dec(g_mon[i].ht); say(" at "); say_dec(g_mon[i].x); say(","); say_dec(g_mon[i].y); say(g_mon[i].primary ? " primary" : ""); }
    say("\n");
    if (g_disp.mon_req > g_nmon) { say("display: monitor "); say_dec(g_disp.mon_req); say(" does not exist - the primary monitor is used\n"); }
    if (g_disp.sim_w) { say("display: TEST --sim-monitor: as if the monitor were "); say_dec(g_disp.sim_w); say("x"); say_dec(g_disp.sim_h); say("\n"); }
    int ow = g_disp.out_w, oh = g_disp.out_h, pw, ph;
    if ((i64)ow * 9 > (i64)oh * 16) { ph = oh; pw = (int)(((i64)oh * 16 + 4) / 9); } else { pw = ow; ph = (int)(((i64)ow * 9 + 8) / 16); }
    say("display: "); say(g_disp.fullscreen ? "fullscreen on monitor " : g_disp.borderless ? "borderless on monitor " : "windowed on monitor "); say_dec(g_disp.mon + 1); say(", "); say_dec(ow); say("x"); say_dec(oh);
    say(", picture area "); say_dec(pw); say("x"); say_dec(ph); say(pw < ow ? " (pillarbox: black bars left and right)" : ph < oh ? " (letterbox: black bars above and below)" : " (the whole of it)"); say("\n");
    if (forced) { say("display: internal resolution from --render-scale "); say_dec(g_rs_pct); say(" (developer switch)\n"); return; }
    int th = ph; if (rr_mode == 2) th = rr_h < (rr_w * 9 + 8) / 16 ? rr_h : (rr_w * 9 + 8) / 16;
    int ci = disp_pick(th), c0 = ci;
    if (ci > 0 && !g_dopt_no_vram) {
        char nm[96]; u64 vram = vk_early_vram(nm, sizeof nm); if (g_sim_vram) vram = g_sim_vram;
        if (vram) {
            u64 usable = vram > (4ull << 30) ? vram - (2ull << 30) : vram / 2, cap = vram < (12ull << 30) ? ((usable * 9 / 16) & ~((256ull << 20) - 1)) : vram / 2;
            if (!g_opt_no_adaptive_mem || g_opt_vram_fit || vram >= (12ull << 30)) { } else cap = vram / 2;   /* (--no-adaptive-mem without --vram-fit: no fitted budget) */
            while (ci > 0 && (disp_need_mb(k_disp_pct[ci]) + 1500) > (cap >> 20)) ci--;
            say("display: video memory "); say_dec((i64)(vram >> 20)); say(" MB"); if (nm[0]) { say(" ("); say(nm); say(")"); } say(", image budget "); say_dec((i64)(cap >> 20)); say(" MB; screen-sized targets need about "); say_dec((i64)disp_need_mb(k_disp_pct[c0])); say(" MB at ");
            say_dec(disp_pct_w(k_disp_pct[c0])); say("x"); say_dec(disp_pct_h(k_disp_pct[c0]));
            if (ci != c0) { say(" - TOO LITTLE VIDEO MEMORY: internal resolution lowered to "); say_dec(disp_pct_w(k_disp_pct[ci])); say("x"); say_dec(disp_pct_h(k_disp_pct[ci])); say(" (about "); say_dec((i64)disp_need_mb(k_disp_pct[ci])); say(" MB); the picture is scaled to the window"); }
            say("\n");
        } else say("display: the graphics card's memory could not be read before the start - no check of the internal resolution against it\n");
    }
    g_rs_pct = k_disp_pct[ci];
    int iw = disp_pct_w(g_rs_pct), ih = disp_pct_h(g_rs_pct);
    say("display: internal resolution "); say_dec(iw); say("x"); say_dec(ih); say(rr_mode == 2 ? " (render_resolution " : " (auto: the picture area's; wanted "); if (rr_mode == 2) { say_dec(rr_w); say("x"); say_dec(rr_h); } else { say_dec(pw); say("x"); say_dec(ph); }
    say(iw == pw && ih == ph ? "), shown pixel for pixel" : iw > pw ? "), filtered down to the picture area" : "), enlarged to the picture area"); say(g_rs_pct == 100 ? "; the game's own resolution\n" : "\n");
}
