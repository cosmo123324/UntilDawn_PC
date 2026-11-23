/* Windows player settings dialog. */
#ifdef _WIN32
typedef i64 (WINAPI *UdDialogProc)(void *, u32, u64, i64);
__declspec(dllimport) i64 WINAPI DialogBoxIndirectParamW(void *, const void *, void *, UdDialogProc, i64);
__declspec(dllimport) int WINAPI EndDialog(void *, i64);
__declspec(dllimport) u32 WINAPI GetDlgItemInt(void *, int, int *, int);
__declspec(dllimport) int WINAPI SetDlgItemInt(void *, int, u32, int);
__declspec(dllimport) u32 WINAPI GetDlgItemTextW(void *, int, u16 *, int);
__declspec(dllimport) int WINAPI SetDlgItemTextW(void *, int, const u16 *);
__declspec(dllimport) i64 WINAPI SendDlgItemMessageW(void *, int, u32, u64, i64);
__declspec(dllimport) i64 WINAPI SendMessageW(void *, u32, u64, i64);
__declspec(dllimport) u32 WINAPI GetACP(void);
__declspec(dllimport) void *WINAPI LoadLibraryA(const char *);

enum {
    UD_OK = 1, UD_CANCEL = 2, UD_LOD = 100, UD_PATH, UD_BROWSE, UD_PROMPTS, UD_RES, UD_DISPLAY,
    UD_INIT = 0x0110, UD_COMMAND = 0x0111, UD_CLOSE = 0x0010,
    UD_ADD = 0x0143, UD_GET_SELECTION = 0x0147, UD_SELECT = 0x014e
};
static const char *const ud_res_values[] = {"auto", "1080p", "1440p", "2160p"};
static const int ud_res_heights[] = {0, 1080, 1440, 2160};
static const char *const ud_display_values[] = {"windowed", "borderless", "fullscreen"};
static char ud_custom_res[32];
static char *ud_launcher_game;

typedef int (WINAPI *UdBrowseProc)(void *, u32, i64, i64);
static int WINAPI ud_browse_start(void *window, u32 msg, i64 ignored, i64 path) {
    (void)ignored;
    if (msg == 1 && path) SendMessageW(window, 0x467 /* BFFM_SETSELECTIONW */, 1, path);
    return 0;
}

static void ud_browse_folder(void *window) {
    typedef struct {
        void *owner;
        const void *root;
        u16 *name;
        const u16 *title;
        u32 flags;
        UdBrowseProc callback;
        i64 param;
        int image;
    } BrowseInfo;
    static void *shell, *ole;
    if (!shell) shell = LoadLibraryA("shell32.dll");
    if (!ole) ole = LoadLibraryA("ole32.dll");
    if (!shell || !ole) return;
    void *(WINAPI *browse)(BrowseInfo *) = (void *)GetProcAddress(shell, "SHBrowseForFolderW");
    int (WINAPI *get_path)(const void *, u16 *, u32, u32) = (void *)GetProcAddress(shell, "SHGetPathFromIDListEx");
    i32 (WINAPI *init)(void *, u32) = (void *)GetProcAddress(ole, "CoInitializeEx");
    void (WINAPI *uninit)(void) = (void *)GetProcAddress(ole, "CoUninitialize");
    void (WINAPI *release)(void *) = (void *)GetProcAddress(ole, "CoTaskMemFree");
    if (!browse || !get_path || !init || !uninit || !release || init(0, 2) < 0) {
        MessageBoxA(window, "Digite o caminho no campo Pasta do jogo.", "Until Dawn", 0x10);
        return;
    }
    u16 name[260];
    static u16 path[1024], current[1024];
    GetDlgItemTextW(window, UD_PATH, current, sizeof current / sizeof current[0]);
    for (u16 *c = current; *c; c++) if (*c == '/') *c = '\\';
    BrowseInfo info = {window, 0, name, u"Selecione a pasta que contém eboot.bin.", 0x41,
                       ud_browse_start, (i64)current, 0};
    void *item = browse(&info);
    if (item) {
        if (get_path(item, path, sizeof path / sizeof path[0], 0)) SetDlgItemTextW(window, UD_PATH, path);
        else MessageBoxA(window, "O caminho da pasta selecionada \xe9 longo demais.", "Until Dawn", 0x10);
        release(item);
    }
    uninit();
}

static void ud_combo_add(void *window, int id, const u16 *label) {
    SendDlgItemMessageW(window, id, UD_ADD, 0, (i64)label);
}

static void ud_launcher_init(void *window) {
    u16 path[1024];
    if (MultiByteToWideChar(0, 0, ud_launcher_game, -1, path, sizeof path / sizeof path[0]))
        SetDlgItemTextW(window, UD_PATH, path);
    ud_combo_add(window, UD_PROMPTS, u"PlayStation 4");
    ud_combo_add(window, UD_PROMPTS, u"Xbox");
    SendDlgItemMessageW(window, UD_PROMPTS, UD_SELECT, g_uds.xbox_prompts, 0);

    ud_combo_add(window, UD_RES, u"Automática");
    ud_combo_add(window, UD_RES, u"1920 × 1080 (1080p)");
    ud_combo_add(window, UD_RES, u"2560 × 1440 (1440p)");
    ud_combo_add(window, UD_RES, u"3840 × 2160 (4K)");
    int resolution = 0;
    if (g_uds.rr_mode == 2) {
        for (int i = 1; i < 4; i++)
            if (g_uds.rr_h == ud_res_heights[i] && g_uds.rr_w == ud_res_heights[i] * 16 / 9) resolution = i;
        if (!resolution) {
            fmt_dec(ud_custom_res, g_uds.rr_w);
            char *height = ud_custom_res + str_len(ud_custom_res);
            *height++ = 'x';
            fmt_dec(height, g_uds.rr_h);
            u16 label[32];
            MultiByteToWideChar(0, 0, ud_custom_res, -1, label, sizeof label / sizeof label[0]);
            ud_combo_add(window, UD_RES, label);
            resolution = 4;
        }
    }
    SendDlgItemMessageW(window, UD_RES, UD_SELECT, resolution, 0);

    ud_combo_add(window, UD_DISPLAY, u"Janela");
    ud_combo_add(window, UD_DISPLAY, u"Janela sem bordas");
    ud_combo_add(window, UD_DISPLAY, u"Tela cheia");
    int display = g_uds.fullscreen ? 2 : g_uds.seen_mode ? g_uds.borderless : 1;
    SendDlgItemMessageW(window, UD_DISPLAY, UD_SELECT, display, 0);
    SetDlgItemInt(window, UD_LOD, g_uds.lod_distance, 0);
}

static int ud_launcher_apply(void *window) {
    int valid = 0;
    u32 lod = GetDlgItemInt(window, UD_LOD, &valid, 0);
    if (!valid || lod < 100 || lod > 150) {
        MessageBoxA(window, "Digite uma dist\xe2ncia de detalhe entre 100 e 150.", "Until Dawn", 0x10);
        return 0;
    }
    int pad = (int)SendDlgItemMessageW(window, UD_PROMPTS, UD_GET_SELECTION, 0, 0);
    int res = (int)SendDlgItemMessageW(window, UD_RES, UD_GET_SELECTION, 0, 0);
    int display = (int)SendDlgItemMessageW(window, UD_DISPLAY, UD_GET_SELECTION, 0, 0);
    if (pad < 0 || pad > 1 || res < 0 || res > 4 || display < 0 || display > 2) return 0;
    static u16 path[1024];
    char game[sizeof g_uds.game_dir - 16];
    u32 count = GetDlgItemTextW(window, UD_PATH, path, sizeof path / sizeof path[0]);
    int replaced = 0, utf8 = GetACP() == 65001;
    if (!count || count == sizeof path / sizeof path[0] - 1 ||
        !WideCharToMultiByte(0, utf8 ? 0 : 0x400 /* WC_NO_BEST_FIT_CHARS */,
                             path, -1, game, sizeof game, 0, utf8 ? 0 : &replaced) || replaced) {
        MessageBoxA(window, "Selecione uma pasta do jogo com um caminho v\xe1lido.", "Until Dawn", 0x10);
        return 0;
    }
    ud_trim(game);
    static char error[4096];
    error[0] = 0;
    UdMsg msg = {error, 0, sizeof error};
    if (ud_check_game(game, &msg)) {
        MessageBoxA(window, error, "Until Dawn", 0x10);
        return 0;
    }
    char pct[24];
    fmt_dec(pct, lod);
    const char *resolution = res == 4 ? ud_custom_res : ud_res_values[res];
    const char *saved_game = !g_uds.game_dir[0] && str_eq(ud_launcher_game, game) ? "" : game;
    const UdIniEdit edits[] = {
        {"game_dir", saved_game, 0}, {"button_prompts", pad ? "xbox" : "playstation", 0},
        {"render_resolution", resolution, 0}, {"display_mode", ud_display_values[display], 0},
        {"lod_distance", pct, 0},
    };
    if (!ud_ini_update(edits, sizeof edits / sizeof edits[0])) {
        MessageBoxA(window, "N\xe3o foi poss\xedvel salvar Config\\settings.ini.", "Until Dawn", 0x10);
        return 0;
    }
    mem_copy(g_uds.game_dir, saved_game, str_len(saved_game) + 1);
    mem_copy(ud_launcher_game, game, str_len(game) + 1);
    g_uds.xbox_prompts = pad;
    g_uds.fullscreen = display == 2;
    g_uds.borderless = display != 0;
    g_uds.seen_mode = g_uds.seen_rr = 1;
    g_uds.lod_distance = lod;
    ud_parse_res(resolution, &g_uds.rr_mode, &g_uds.rr_w, &g_uds.rr_h);
    return 1;
}

static i64 WINAPI ud_prelaunch_event(void *window, u32 msg, u64 wp, i64 value) {
    (void)value;
    if (msg == UD_INIT) { ud_launcher_init(window); return 1; }
    if (msg == UD_CLOSE || (msg == UD_COMMAND && (u16)wp == UD_CANCEL)) {
        EndDialog(window, 0);
        return 1;
    }
    if (msg == UD_COMMAND && (u16)wp == UD_BROWSE) { ud_browse_folder(window); return 1; }
    if (msg == UD_COMMAND && (u16)wp == UD_OK) {
        if (ud_launcher_apply(window)) EndDialog(window, 1);
        return 1;
    }
    return 0;
}

static u16 *ud_dialog_dword(u16 *p, u32 value) {
    *p++ = (u16)value;
    *p++ = (u16)(value >> 16);
    return p;
}

static u16 *ud_dialog_text(u16 *p, const u16 *label) {
    while (*label) *p++ = *label++;
    *p++ = 0;
    return p;
}
#endif

/* 1: settings saved and folder validated; 0: cancelled; -1: no dialog. */
static int ud_prelaunch(char *game) {
#ifdef _WIN32
    static const struct {
        u32 style;
        u16 x, y, w, h, id, class_id;
        const u16 *label;
    } controls[] = {
        {0x50000000, 12, 12, 280, 10, 0xffff, 0x82, u"Pasta do jogo"},
        {0x50810080, 12, 25, 224, 15, UD_PATH, 0x81, u""},
        {0x50010000, 242, 25, 58, 15, UD_BROWSE, 0x80, u"Procurar…"},
        {0x50000000, 12, 54, 110, 11, 0xffff, 0x82, u"Ícones do controle"},
        {0x50210003, 145, 51, 155, 70, UD_PROMPTS, 0x85, u""},
        {0x50000000, 12, 78, 110, 11, 0xffff, 0x82, u"Resolução de renderização"},
        {0x50210003, 145, 75, 155, 84, UD_RES, 0x85, u""},
        {0x50000000, 12, 102, 110, 11, 0xffff, 0x82, u"Modo de tela"},
        {0x50210003, 145, 99, 155, 70, UD_DISPLAY, 0x85, u""},
        {0x50000000, 12, 126, 135, 11, 0xffff, 0x82, u"Distância de detalhe (%)"},
        {0x50812000, 145, 123, 44, 15, UD_LOD, 0x81, u""},
        {0x50000000, 12, 143, 288, 11, 0xffff, 0x82, u"100% é o padrão. Aumentar até 150% pode reduzir o FPS."},
        {0x50010000, 190, 166, 52, 16, UD_CANCEL, 0x80, u"Cancelar"},
        {0x50010001, 248, 166, 52, 16, UD_OK, 0x80, u"Jogar"},
    };
    u16 data[768] __attribute__((aligned(4)));
    u16 *p = ud_dialog_dword(data, 0x80c808c0);
    p = ud_dialog_dword(p, 0);
    *p++ = sizeof controls / sizeof controls[0];
    *p++ = 0; *p++ = 0; *p++ = 312; *p++ = 194;
    *p++ = 0; *p++ = 0;
    p = ud_dialog_text(p, u"Until Dawn");
    *p++ = 9;
    p = ud_dialog_text(p, u"Segoe UI");
    for (u64 i = 0; i < sizeof controls / sizeof controls[0]; i++) {
        if ((p - data) & 1) *p++ = 0;
        p = ud_dialog_dword(p, controls[i].style);
        p = ud_dialog_dword(p, 0);
        *p++ = controls[i].x; *p++ = controls[i].y;
        *p++ = controls[i].w; *p++ = controls[i].h; *p++ = controls[i].id;
        *p++ = 0xffff; *p++ = controls[i].class_id;
        p = ud_dialog_text(p, controls[i].label);
        *p++ = 0;
    }
    ud_launcher_game = game;
    plat_dpi_init();
    return (int)DialogBoxIndirectParamW(GetModuleHandleA(0), data, 0, ud_prelaunch_event, 0);
#else
    (void)game;
    return -1;
#endif
}
