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
__declspec(dllimport) void *WINAPI GetDlgItem(void *, int);
__declspec(dllimport) void *WINAPI SetFocus(void *);
__declspec(dllimport) u32 WINAPI GetACP(void);
__declspec(dllimport) void *WINAPI LoadLibraryA(const char *);

enum {
    UD_OK = 1, UD_CANCEL = 2, UD_LOD = 100, UD_PATH, UD_BROWSE, UD_PROMPTS, UD_RES, UD_DISPLAY, UD_PRESENT, UD_HEADING,
    UD_INIT = 0x0110, UD_COMMAND = 0x0111, UD_CLOSE = 0x0010,
    UD_ADD = 0x0143, UD_GET_SELECTION = 0x0147, UD_SELECT = 0x014e
};
static const char *const ud_display_values[] = {"windowed", "borderless", "fullscreen"};
typedef struct { int w, h; } UdResolution;
static UdResolution ud_resolutions[16] = {
    {0, 0}, {1280, 720}, {1280, 800}, {1600, 900}, {1680, 1050}, {1920, 1080},
    {1920, 1200}, {2560, 1440}, {2560, 1600}, {3840, 2160},
};
static int ud_resolution_count = 10;
static char *ud_launcher_game;
#include "ud_launcher_art.c"

static int ud_launcher_has_game(void) {
    char eboot[1120];
    path_join(eboot, ud_launcher_game, "eboot.bin");
    return plat_file_size(eboot) > 0;
}

typedef struct { u32 data1; u16 data2, data3; u8 data4[8]; } UdGuid;
typedef struct UdShellItem UdShellItem;
typedef struct UdFileDialog UdFileDialog;

struct UdShellItem {
    const struct {
        void *query_interface, *add_ref;
        u32 (WINAPI *release)(UdShellItem *);
        void *bind_to_handler, *get_parent;
        i32 (WINAPI *get_display_name)(UdShellItem *, u32, u16 **);
    } *api;
};

struct UdFileDialog {
    const struct {
        void *query_interface, *add_ref;
        u32 (WINAPI *release)(UdFileDialog *);
        i32 (WINAPI *show)(UdFileDialog *, void *);
        void *set_file_types, *set_file_type_index, *get_file_type_index, *advise, *unadvise;
        i32 (WINAPI *set_options)(UdFileDialog *, u32);
        i32 (WINAPI *get_options)(UdFileDialog *, u32 *);
        void *set_default_folder;
        i32 (WINAPI *set_folder)(UdFileDialog *, UdShellItem *);
        void *get_folder, *get_current_selection, *set_file_name, *get_file_name;
        i32 (WINAPI *set_title)(UdFileDialog *, const u16 *);
        i32 (WINAPI *set_ok_label)(UdFileDialog *, const u16 *);
        void *set_file_name_label;
        i32 (WINAPI *get_result)(UdFileDialog *, UdShellItem **);
    } *api;
};

static void ud_browse_folder(void *window) {
    static const UdGuid clsid = {0xdc1c5a9c, 0xe88a, 0x4dde, {0xa5,0xa1,0x60,0xf8,0x2a,0x20,0xae,0xf7}};
    static const UdGuid iid = {0xd57c7288, 0xd4ad, 0x4768, {0xbe,0x02,0x9d,0x96,0x95,0x32,0xd9,0x60}};
    static const UdGuid item_iid = {0x43826d1e, 0xe718, 0x42ee, {0xbc,0x55,0xa1,0xe2,0x61,0xc3,0x7b,0xfe}};
    static void *shell, *ole;
    if (!shell) shell = LoadLibraryA("shell32.dll");
    if (!ole) ole = LoadLibraryA("ole32.dll");
    if (!shell || !ole) return;
    i32 (WINAPI *init)(void *, u32) = (void *)GetProcAddress(ole, "CoInitializeEx");
    void (WINAPI *uninit)(void) = (void *)GetProcAddress(ole, "CoUninitialize");
    void (WINAPI *free_name)(void *) = (void *)GetProcAddress(ole, "CoTaskMemFree");
    i32 (WINAPI *create)(const UdGuid *, void *, u32, const UdGuid *, void **) =
        (void *)GetProcAddress(ole, "CoCreateInstance");
    i32 (WINAPI *folder_item)(const u16 *, void *, const UdGuid *, void **) =
        (void *)GetProcAddress(shell, "SHCreateItemFromParsingName");
    if (!init || !uninit || !free_name || !create || init(0, 2 /* COINIT_APARTMENTTHREADED */) < 0) {
        MessageBoxA(window, "Digite o caminho no campo Pasta do jogo.", "Until Dawn", 0x10);
        return;
    }

    UdFileDialog *dialog = 0;
    if (create(&clsid, 0, 1 /* CLSCTX_INPROC_SERVER */, &iid, (void **)&dialog) < 0) {
        uninit();
        MessageBoxA(window, "O seletor de pastas do Windows est\xe1 indispon\xedvel.", "Until Dawn", 0x10);
        return;
    }
    u32 options = 0;
    dialog->api->get_options(dialog, &options);
    enum { PICK_FOLDERS = 0x20, FORCE_FILE_SYSTEM = 0x40, PATH_MUST_EXIST = 0x800, DO_NOT_ADD_TO_RECENT = 0x02000000 };
    dialog->api->set_options(dialog, options | PICK_FOLDERS | FORCE_FILE_SYSTEM | PATH_MUST_EXIST | DO_NOT_ADD_TO_RECENT);
    dialog->api->set_title(dialog, u"Selecione a pasta que contém eboot.bin");
    dialog->api->set_ok_label(dialog, u"Selecionar pasta");

    static u16 current[1024];
    GetDlgItemTextW(window, UD_PATH, current, sizeof current / sizeof current[0]);
    for (u16 *c = current; *c; c++) if (*c == '/') *c = '\\';
    UdShellItem *item = 0;
    if (folder_item && current[0] && folder_item(current, 0, &item_iid, (void **)&item) >= 0) {
        dialog->api->set_folder(dialog, item);
        item->api->release(item);
        item = 0;
    }
    if (dialog->api->show(dialog, window) >= 0 && dialog->api->get_result(dialog, &item) >= 0) {
        u16 *path = 0;
        if (item->api->get_display_name(item, 0x80058000u /* SIGDN_FILESYSPATH */, &path) >= 0) {
            u32 n = 0;
            while (n < 1023 && path[n]) n++;
            if (n < 1023) {
                SetDlgItemTextW(window, UD_PATH, path);
                ud_art_refresh(window);
            } else MessageBoxA(window, "O caminho da pasta selecionada \xe9 longo demais.", "Until Dawn", 0x10);
            free_name(path);
        }
        item->api->release(item);
    }
    dialog->api->release(dialog);
    uninit();
}

static void ud_combo_add(void *window, int id, const u16 *label) {
    SendDlgItemMessageW(window, id, UD_ADD, 0, (i64)label);
}

static int ud_resolution_index(int w, int h) {
    for (int i = 0; i < ud_resolution_count; i++)
        if (ud_resolutions[i].w == w && ud_resolutions[i].h == h) return i;
    if (!w || !h || ud_resolution_count == sizeof ud_resolutions / sizeof ud_resolutions[0]) return 0;
    int i = ud_resolution_count++;
    ud_resolutions[i] = (UdResolution){w, h};
    return i;
}

static char *ud_resolution_value(char *out, UdResolution size) {
    fmt_dec(out, size.w);
    char *p = out + str_len(out);
    *p++ = 'x';
    fmt_dec(p, size.h);
    return p + str_len(p);
}

static void ud_resolution_label(void *window, UdResolution size, int automatic) {
    char text[96], *p = text;
    if (automatic) {
        const char prefix[] = "Automatica";
        mem_copy(p, prefix, sizeof prefix);
        if (!size.w) { ud_combo_add(window, UD_RES, u"Automática"); return; }
        p += sizeof prefix - 1;
        *p++ = ' '; *p++ = '-'; *p++ = ' ';
    }
    p = ud_resolution_value(p, size);
    int w = size.w, h = size.h;
    while (h) { int rest = w % h; w = h; h = rest; }
    int wide = size.w / w, high = size.h / w;
    if (wide == 8 && high == 5) { wide = 16; high = 10; }
    if (wide <= 32 && high <= 32) {
        *p++ = ' '; *p++ = ' '; *p++ = '(';
        fmt_dec(p, wide); p += str_len(p);
        *p++ = ':'; fmt_dec(p, high); p += str_len(p);
        *p++ = ')'; *p = 0;
    }
    u16 label[96];
    MultiByteToWideChar(0, 0, text, -1, label, sizeof label / sizeof label[0]);
    if (automatic) label[5] = 0x00e1;
    ud_combo_add(window, UD_RES, label);
}

static void ud_launcher_init(void *window) {
    u16 path[1024];
    if (g_uds.game_dir[0] && MultiByteToWideChar(0, 0, ud_launcher_game, -1, path, sizeof path / sizeof path[0]))
        SetDlgItemTextW(window, UD_PATH, path);
    SendDlgItemMessageW(window, UD_PATH, 0x1501 /* EM_SETCUEBANNER */, 0, (i64)u"Escolha a pasta do jogo");
    ud_combo_add(window, UD_PROMPTS, u"PlayStation 4");
    ud_combo_add(window, UD_PROMPTS, u"Xbox");
    SendDlgItemMessageW(window, UD_PROMPTS, UD_SELECT, g_uds.xbox_prompts, 0);

    plat_mon_enum();
    UdResolution desktop = {0};
    if (g_nmon) {
        int mon = g_uds.monitor > 0 && g_uds.monitor <= g_nmon ? g_uds.monitor - 1 : 0;
        desktop = (UdResolution){g_mon[mon].w, g_mon[mon].ht};
        ud_resolution_index(desktop.w, desktop.h);
    }
    int resolution = g_uds.rr_mode == 2 ? ud_resolution_index(g_uds.rr_w, g_uds.rr_h) : 0;
    ud_resolution_label(window, desktop, 1);
    for (int i = 1; i < ud_resolution_count; i++) ud_resolution_label(window, ud_resolutions[i], 0);
    SendDlgItemMessageW(window, UD_RES, UD_SELECT, resolution, 0);

    ud_combo_add(window, UD_DISPLAY, u"Janela");
    ud_combo_add(window, UD_DISPLAY, u"Janela sem bordas");
    ud_combo_add(window, UD_DISPLAY, u"Tela cheia");
    int display = g_uds.fullscreen ? 2 : g_uds.seen_mode ? g_uds.borderless : 1;
    SendDlgItemMessageW(window, UD_DISPLAY, UD_SELECT, display, 0);
    ud_combo_add(window, UD_PRESENT, u"Imagem inteira (pode ter barras)");
    ud_combo_add(window, UD_PRESENT, u"Preencher (interface ajustada)");
    SendDlgItemMessageW(window, UD_PRESENT, UD_SELECT, g_uds.fill, 0);
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
    int fill = (int)SendDlgItemMessageW(window, UD_PRESENT, UD_GET_SELECTION, 0, 0);
    if (pad < 0 || pad > 1 || res < 0 || res >= ud_resolution_count || display < 0 || display > 2 ||
        fill < 0 || fill > 1) return 0;
    static u16 path[1024];
    char game[sizeof g_uds.game_dir - 16];
    u32 count = GetDlgItemTextW(window, UD_PATH, path, sizeof path / sizeof path[0]);
    int replaced = 0, utf8 = GetACP() == 65001;
    if (!count && !g_uds.game_dir[0] && ud_launcher_has_game() &&
        str_len(ud_launcher_game) < (int)sizeof game) {
        mem_copy(game, ud_launcher_game, str_len(ud_launcher_game) + 1);
    } else if (!count) {
        ud_browse_folder(window);
        return 0;
    } else if (count == sizeof path / sizeof path[0] - 1 ||
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
    char pct[24], size[24], window_w[24], window_h[24];
    fmt_dec(pct, lod);
    UdResolution selected = ud_resolutions[res];
    if (res) ud_resolution_value(size, selected);
    const char *resolution = res ? size : "auto";
    const char *saved_game = !g_uds.game_dir[0] && str_eq(ud_launcher_game, game) ? "" : game;
    const UdIniEdit edits[] = {
        {"game_dir", saved_game, 0}, {"button_prompts", pad ? "xbox" : "playstation", 0},
        {"render_resolution", resolution, 0}, {"display_mode", ud_display_values[display], 0},
        {"lod_distance", pct, 0}, {"presentation", fill ? "fill" : "fit", 0},
        {"window_width", window_w, 0}, {"window_height", window_h, 0},
    };
    int edit_count = 6, resize = display == 0 && res &&
        (g_uds.borderless || g_uds.rr_mode != 2 || g_uds.rr_w != selected.w || g_uds.rr_h != selected.h);
    if (resize) {
        fmt_dec(window_w, selected.w); fmt_dec(window_h, selected.h);
        edit_count += 2;
    }
    if (!ud_ini_update(edits, edit_count)) {
        MessageBoxA(window, "N\xe3o foi poss\xedvel salvar Config\\settings.ini.", "Until Dawn", 0x10);
        return 0;
    }
    mem_copy(g_uds.game_dir, saved_game, str_len(saved_game) + 1);
    mem_copy(ud_launcher_game, game, str_len(game) + 1);
    g_uds.xbox_prompts = pad;
    g_uds.fullscreen = display == 2;
    g_uds.borderless = display != 0;
    g_uds.fill = fill;
    if (resize) { g_uds.window_w = selected.w; g_uds.window_h = selected.h; }
    g_uds.seen_mode = g_uds.seen_rr = 1;
    g_uds.lod_distance = lod;
    ud_parse_res(resolution, &g_uds.rr_mode, &g_uds.rr_w, &g_uds.rr_h);
    return 1;
}

static i64 WINAPI ud_prelaunch_event(void *window, u32 msg, u64 wp, i64 value) {
    if (msg == UD_INIT) {
        ud_art_setup(window);
        ud_launcher_init(window);
        ud_art_refresh(window);
        SendMessageW(window, 0x401 /* DM_SETDEFID */, UD_OK, 0);
        int control = ud_launcher_has_game() ? UD_OK : UD_BROWSE;
        SetFocus(GetDlgItem(window, control));
        return 0;  /* Focus was set explicitly. */
    }
    if (msg == 0x0014 /* WM_ERASEBKGND */ && ud_panel_brush) {
        ud_art_background(window, (void *)wp);
        return 1;
    }
    if ((msg == 0x0133 || msg == 0x0134 || msg == 0x0138) && ud_panel_brush) {
        int field = msg != 0x0138;
        SetTextColor((void *)wp, ud_ink);
        SetBkColor((void *)wp, field ? ud_field : ud_panel);
        SetBkMode((void *)wp, 2);
        return (i64)(field ? ud_field_brush : ud_panel_brush);
    }
    if (msg == 0x002b /* WM_DRAWITEM */ && (wp == UD_OK || wp == UD_CANCEL || wp == UD_BROWSE)) {
        ud_art_button((const void *)value);
        return 1;
    }
    if (msg == UD_CLOSE || (msg == UD_COMMAND && (u16)wp == UD_CANCEL)) {
        EndDialog(window, 0);
        return 1;
    }
    if (msg == UD_COMMAND && (u16)wp == UD_BROWSE) { ud_browse_folder(window); return 1; }
    if (msg == UD_COMMAND && (u16)wp == UD_PATH && (u16)(wp >> 16) == 0x0200 /* EN_KILLFOCUS */)
        ud_art_refresh(window);
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
        {0x50000000, 211, 13, 230, 20, UD_HEADING, 0x82, u"Configurações"},
        {0x50000000, 211, 42, 230, 11, 0xffff, 0x82, u"Pasta do jogo"},
        {0x50810080, 211, 55, 164, 16, UD_PATH, 0x81, u""},
        {0x5001000b, 381, 54, 59, 18, UD_BROWSE, 0x80, u"Procurar…"},
        {0x50000000, 211, 86, 93, 11, 0xffff, 0x82, u"Ícones do controle"},
        {0x50210003, 307, 83, 133, 64, UD_PROMPTS, 0x85, u""},
        {0x50000000, 211, 109, 93, 11, 0xffff, 0x82, u"Resolução"},
        {0x50210003, 307, 106, 133, 126, UD_RES, 0x85, u""},
        {0x50000000, 211, 132, 93, 11, 0xffff, 0x82, u"Modo de tela"},
        {0x50210003, 307, 129, 133, 69, UD_DISPLAY, 0x85, u""},
        {0x50000000, 211, 155, 93, 11, 0xffff, 0x82, u"Enquadramento"},
        {0x50210003, 307, 152, 133, 69, UD_PRESENT, 0x85, u""},
        {0x50000000, 211, 178, 93, 11, 0xffff, 0x82, u"Distância de detalhe (%)"},
        {0x50812000, 307, 175, 44, 16, UD_LOD, 0x81, u""},
        {0x50000000, 211, 195, 234, 18, 0xffff, 0x82, u"100% é o padrão. Aumentar até 150% pode reduzir o FPS."},
        {0x5001000b, 312, 219, 53, 20, UD_CANCEL, 0x80, u"Cancelar"},
        {0x5001000b, 372, 219, 68, 20, UD_OK, 0x80, u"Jogar"},
    };
    u16 data[768] __attribute__((aligned(4)));
    u16 *p = ud_dialog_dword(data, 0x80c808c0);
    p = ud_dialog_dword(p, 0);
    *p++ = sizeof controls / sizeof controls[0];
    *p++ = 0; *p++ = 0; *p++ = 457; *p++ = 252;
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
    int result = (int)DialogBoxIndirectParamW(GetModuleHandleA(0), data, 0, ud_prelaunch_event, 0);
    ud_art_close();
    return result;
#else
    (void)game;
    return -1;
#endif
}
