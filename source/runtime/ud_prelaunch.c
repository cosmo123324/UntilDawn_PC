/* Optional Windows dialog for the initial viewport LOD setting. */

#ifdef _WIN32
typedef i64 (WINAPI *UdDialogProc)(void *, u32, u64, i64);
__declspec(dllimport) i64 WINAPI DialogBoxIndirectParamW(void *, const void *, void *, UdDialogProc, i64);
__declspec(dllimport) int WINAPI EndDialog(void *, i64);
__declspec(dllimport) u32 WINAPI GetDlgItemInt(void *, int, int *, int);
__declspec(dllimport) int WINAPI SetDlgItemInt(void *, int, u32, int);

enum { UD_LOD_EDIT = 100, UD_WIN_OK = 1, UD_WIN_CANCEL = 2 };

static i64 WINAPI ud_prelaunch_event(void *window, u32 msg, u64 wp, i64 value) {
    if (msg == 0x0110) { /* WM_INITDIALOG */
        SetDlgItemInt(window, UD_LOD_EDIT, (u32)value, 0);
        return 1;
    }
    if (msg == 0x0010 || (msg == 0x0111 && (u16)wp == UD_WIN_CANCEL)) {
        EndDialog(window, 0);
        return 1;
    }
    if (msg == 0x0111 && (u16)wp == UD_WIN_OK) {
        int valid = 0;
        u32 pct = GetDlgItemInt(window, UD_LOD_EDIT, &valid, 0);
        if (!valid || pct < 100 || pct > 150) {
            MessageBoxA(window, "Digite um valor entre 100 e 150.", "Until Dawn", 0x10);
        } else EndDialog(window, pct);
        return 1;
    }
    return 0;
}

static u16 *ud_dialog_dword(u16 *p, u32 value) {
    *p++ = (u16)value;
    *p++ = (u16)(value >> 16);
    return p;
}

static u16 *ud_dialog_text(u16 *p, const char *text) {
    while (*text) *p++ = (u8)*text++;
    *p++ = 0;
    return p;
}
#endif

/* Returns zero on cancellation. An unavailable dialog falls back to the saved setting. */
static int ud_prelaunch(int current) {
#ifdef _WIN32
    static const struct {
        u32 style;
        u16 x, y, w, h, id, class_id;
        const char *label;
    } controls[] = {
        {0x50000000, 12, 12, 153, 11, 0xffff, 0x82, "Dist\xe2ncia de detalhe (%)"},
        {0x50812000, 169, 9, 55, 15, UD_LOD_EDIT, 0x81, ""},
        {0x50000000, 12, 31, 212, 11, 0xffff, 0x82, "100 \xe9 o padr\xe3o. Ajuste experimental at\xe9 150."},
        {0x50000000, 12, 46, 212, 11, 0xffff, 0x82, "Uma dist\xe2ncia maior pode reduzir o FPS."},
        {0x50010000, 118, 69, 50, 15, UD_WIN_CANCEL, 0x80, "Cancelar"},
        {0x50010001, 174, 69, 50, 15, UD_WIN_OK, 0x80, "Jogar"},
    };
    /* Win32's DLGTEMPLATE and DLGITEMTEMPLATE records with UTF-16 strings. */
    u16 data[384] __attribute__((aligned(4)));
    u16 *p = ud_dialog_dword(data, 0x80c808c0);
    p = ud_dialog_dword(p, 0);
    *p++ = sizeof controls / sizeof controls[0];
    *p++ = 0; *p++ = 0; *p++ = 236; *p++ = 95;
    *p++ = 0; *p++ = 0; /* No menu or custom class. */
    p = ud_dialog_text(p, "Until Dawn");
    *p++ = 9;
    p = ud_dialog_text(p, "Segoe UI");
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
    i64 picked = DialogBoxIndirectParamW(GetModuleHandleA(0), data, 0, ud_prelaunch_event, current);
    if (picked >= 0) return (int)picked;
#endif
    return current;
}
