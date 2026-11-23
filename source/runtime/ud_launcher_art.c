/* Native launcher painting. Artwork is read from the selected game folder. */
__declspec(dllimport) void *WINAPI CreateSolidBrush(u32);
__declspec(dllimport) int WINAPI DeleteObject(void *);
__declspec(dllimport) void *WINAPI SelectObject(void *, void *);
__declspec(dllimport) void *WINAPI CreateFontW(int, int, int, int, int, u32, u32, u32, u32, u32, u32, u32, u32, const u16 *);
__declspec(dllimport) int WINAPI FillRect(void *, const i32 *, void *);
__declspec(dllimport) int WINAPI DrawTextW(void *, const u16 *, int, i32 *, u32);
__declspec(dllimport) int WINAPI DrawFocusRect(void *, const i32 *);
__declspec(dllimport) int WINAPI GetWindowTextW(void *, u16 *, int);
__declspec(dllimport) int WINAPI MapDialogRect(void *, i32 *);
__declspec(dllimport) int WINAPI InvalidateRect(void *, const i32 *, int);

enum { UD_ART_WIDTH = 190, UD_ART_HEIGHT = 207 };
static const u32 ud_panel = 0x201914, ud_ink = 0xf5f0e9, ud_field = 0x382e26;
static void *ud_panel_brush, *ud_field_brush, *ud_heading_font, *ud_title_font;
static struct {
    u64 token;
    void *image;
    u32 w, h;
    int (WINAPI *load)(const u16 *, void **);
    int (WINAPI *dispose)(void *);
    int (WINAPI *width)(void *, u32 *);
    int (WINAPI *height)(void *, u32 *);
    int (WINAPI *graphics)(void *, void **);
    int (WINAPI *draw)(void *, void *, int, int, int, int, int, int, int, int, int, void *, void *, void *);
    int (WINAPI *delete_graphics)(void *);
    void (WINAPI *shutdown)(u64);
} ud_art;

static void ud_art_setup(void *window) {
    ud_panel_brush = CreateSolidBrush(ud_panel);
    ud_field_brush = CreateSolidBrush(ud_field);
    void *dwm = LoadLibraryA("dwmapi.dll");
    if (dwm) {
        int (WINAPI *set_attribute)(void *, u32, const void *, u32) =
            (void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
        const int dark = 1;
        if (set_attribute) set_attribute(window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    }
    i32 units[4] = {0, 0, 16, 17};
    MapDialogRect(window, units);
    ud_heading_font = CreateFontW(-units[3], 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 5, 0, u"Segoe UI");
    ud_title_font = CreateFontW(-units[3], 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, u"Georgia");
    if (ud_heading_font) SendDlgItemMessageW(window, UD_HEADING, 0x0030 /* WM_SETFONT */, (u64)ud_heading_font, 1);
    void *dll = LoadLibraryA("gdiplus.dll");
    if (!dll) return;
    struct { u32 version; void *callback; int no_background, no_codecs; } startup_data = {1, 0, 0, 1};
    int (WINAPI *startup)(u64 *, const void *, void *) = (void *)GetProcAddress(dll, "GdiplusStartup");
    ud_art.shutdown = (void *)GetProcAddress(dll, "GdiplusShutdown");
    ud_art.load = (void *)GetProcAddress(dll, "GdipLoadImageFromFile");
    ud_art.dispose = (void *)GetProcAddress(dll, "GdipDisposeImage");
    ud_art.width = (void *)GetProcAddress(dll, "GdipGetImageWidth");
    ud_art.height = (void *)GetProcAddress(dll, "GdipGetImageHeight");
    ud_art.graphics = (void *)GetProcAddress(dll, "GdipCreateFromHDC");
    ud_art.draw = (void *)GetProcAddress(dll, "GdipDrawImageRectRectI");
    ud_art.delete_graphics = (void *)GetProcAddress(dll, "GdipDeleteGraphics");
    if (startup && ud_art.shutdown && ud_art.load && ud_art.dispose && ud_art.width && ud_art.height &&
        ud_art.graphics && ud_art.draw && ud_art.delete_graphics)
        startup(&ud_art.token, &startup_data, 0);
}

static void ud_art_refresh(void *window) {
    if (!ud_art.token) return;
    static u16 last[1024], path[1050];
    u32 n = GetDlgItemTextW(window, UD_PATH, path, 1024);
    u32 diff = 0;
    while (diff <= n && last[diff] == path[diff]) diff++;
    if (diff > n) return;
    mem_copy(last, path, ((u64)n + 1) * sizeof *path);
    if (ud_art.image) { ud_art.dispose(ud_art.image); ud_art.image = 0; }
    if (n && n < 1023) {
        for (u32 i = 0; i < n; i++) if (path[i] == '/') path[i] = '\\';
        static const u16 suffix[] = u"\\sce_sys\\icon0.png";
        mem_copy(path + n, suffix, sizeof suffix);
        if (ud_art.load(path, &ud_art.image) == 0) {
            if (ud_art.width(ud_art.image, &ud_art.w) || ud_art.height(ud_art.image, &ud_art.h) ||
                !ud_art.w || !ud_art.h) { ud_art.dispose(ud_art.image); ud_art.image = 0; }
        } else ud_art.image = 0;
    }
    InvalidateRect(window, 0, 1);
}

static void ud_paint_color(void *dc, const i32 *rect, u32 color) {
    void *brush = CreateSolidBrush(color);
    if (brush) { FillRect(dc, rect, brush); DeleteObject(brush); }
}

static void ud_art_background(void *window, void *dc) {
    i32 client[4], cover[4] = {0, 0, UD_ART_WIDTH, UD_ART_HEIGHT};
    GetClientRect(window, client);
    FillRect(dc, client, ud_panel_brush);
    MapDialogRect(window, cover);
    int w = cover[2], h = cover[3];
    ud_paint_color(dc, cover, 0x30231a);
    if (ud_art.image && w > 0 && h > 0) {
        int sw = ud_art.w, sh = ud_art.h;
        if ((u64)w * sh < (u64)h * sw) sw = (int)((u64)w * sh / h);
        else sh = (int)((u64)h * sw / w);
        void *graphics = 0;
        if (!ud_art.graphics(dc, &graphics)) {
            ud_art.draw(graphics, ud_art.image, 0, 0, w, h, (ud_art.w - sw) / 2, (ud_art.h - sh) / 2,
                        sw, sh, 2 /* UnitPixel */, 0, 0, 0);
            ud_art.delete_graphics(graphics);
        }
    }
    i32 title[4] = {0, h, w, client[3]};
    ud_paint_color(dc, title, 0x15100e);
    i32 line[4] = {w, 0, w + 1, client[3]};
    ud_paint_color(dc, line, 0x544337);
    void *old_font = ud_title_font ? SelectObject(dc, ud_title_font) : 0;
    SetBkMode(dc, 1);
    SetTextColor(dc, ud_ink);
    title[0] += w / 20;
    title[2] -= w / 20;
    DrawTextW(dc, u"UNTIL DAWN", -1, title, 0x25 /* CENTER | VCENTER | SINGLELINE */);
    if (old_font) SelectObject(dc, old_font);
}

static void ud_art_button(const void *lparam) {
    typedef struct { u32 type, id, item, action, state; void *window, *dc; i32 rect[4]; u64 data; } UdDrawItem;
    const UdDrawItem *item = lparam;
    u32 color = item->id == UD_OK ? 0xd8cf88 : ud_field;
    if (item->state & 1) color = item->id == UD_OK ? 0xb1a869 : 0x504239;
    ud_paint_color(item->dc, item->rect, color);
    u16 label[64] = {0};
    GetWindowTextW(item->window, label, sizeof label / sizeof label[0]);
    SetBkMode(item->dc, 1);
    SetTextColor(item->dc, item->id == UD_OK ? 0x1c1710 : ud_ink);
    i32 box[4];
    mem_copy(box, item->rect, sizeof box);
    DrawTextW(item->dc, label, -1, box, 0x25);
    if (item->state & 0x10) {
        box[0] += 3; box[1] += 3; box[2] -= 3; box[3] -= 3;
        DrawFocusRect(item->dc, box);
    }
}

static void ud_art_close(void) {
    if (ud_art.image) ud_art.dispose(ud_art.image);
    if (ud_art.token) ud_art.shutdown(ud_art.token);
    ud_art.image = 0; ud_art.token = 0;
    if (ud_title_font) DeleteObject(ud_title_font);
    if (ud_heading_font) DeleteObject(ud_heading_font);
    if (ud_field_brush) DeleteObject(ud_field_brush);
    if (ud_panel_brush) DeleteObject(ud_panel_brush);
}
