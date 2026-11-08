/*
 * ZDSlots - 6 save slots for Gauntlet (practice/survival) in Zuma Deluxe Plus.
 * Loaded by a tiny stub at the exe entry point (LoadLibraryA("zdslots.dll")).
 * All game addresses below are for ZDPlus.exe / ZDPlusO.exe (Zuma Deluxe 1.0 build 101).
 */
#include <windows.h>
#include <string.h>

/* ---------------------------------------------------------------- game types */
typedef struct MStr { void *al; union { char buf[16]; char *ptr; } u; unsigned size, res; } MStr; /* MSVC7 std::string */
typedef struct Color { int r, g, b, a; } Color;
typedef struct Rect { int x, y, w, h; } Rect;

#define APP        (*(char **)0x57ef00)
#define IMG_DIALOG_BACK   (*(void **)0x547ec8)
#define IMG_DIALOG_BUTTON (*(void **)0x54802c)
#define FONT_TITLE  (*(void **)0x547e80)
#define FONT_MAIN12 (*(void **)0x547e08)
#define FONT_MAIN10 (*(void **)0x547e38)
#define SOUND_BUTTON1 (*(int *)0x547fec)
#define SAVE_VERSION  (*(int *)0x52e49c)

typedef void *(__cdecl *NewFn)(unsigned);
typedef void *(__thiscall *WidgetCtorFn)(void *);
typedef void (__thiscall *Fn_p)(void *, void *);
typedef void (__thiscall *Fn_v)(void *);
typedef void (__thiscall *Fn_i)(void *, int);
typedef int  (__thiscall *Fn_b_i)(void *, int);
typedef void (__thiscall *DrawStringFn)(void *g, MStr *s, int x, int y);
typedef void (__thiscall *FillRectFn)(void *g, int x, int y, int w, int h);
typedef void (__thiscall *DrawImageBoxFn)(void *g, Rect *r, void *img);
typedef void (__thiscall *DrawImageSrcFn)(void *g, void *img, int x, int y, Rect *src);
typedef void *(__thiscall *GetImageFn)(void *app, MStr *path);
typedef void *(__cdecl *StrFormatFn)(MStr *out, const char *fmt, ...);
typedef void (__thiscall *StrAssignFn)(MStr *s, const char *p, unsigned n);
typedef int  (__thiscall *SaveBoardFn)(void *board, MStr *path);
typedef int  (__thiscall *ButtonDepressFn)(void *app, int id);
typedef void *(__thiscall *DoDialogFn)(void *app, int id, int modal, MStr *hdr, MStr *lines, MStr *footer, int mode);
typedef void (__thiscall *AddDialogFn)(void *app, int id, void *dlg);
typedef int  (__thiscall *StrWidthFn)(void *font, MStr *s);

#define G_new          ((NewFn)0x4d1915)
#define G_WidgetCtor   ((WidgetCtorFn)0x47ef70)
#define G_WidgetUpdate ((Fn_v)0x4774e0)
#define G_AddWidget    ((Fn_p)0x47f640)
#define G_SetFocus     ((Fn_p)0x479ae0)
#define G_RemoveWidget ((Fn_p)0x47afb0)
#define G_SetFont      ((Fn_p)0x447840)
#define G_SetColor     ((Fn_p)0x447850)
#define G_SetColorize  ((Fn_i)0x4478b0)
#define G_DrawString   ((DrawStringFn)0x447df0)
#define G_FillRect     ((FillRectFn)0x44d0f0)
#define G_DrawImageBox ((DrawImageBoxFn)0x450800)
#define G_DrawImageSrc ((DrawImageSrcFn)0x44d510)
#define G_GetImage     ((GetImageFn)0x4651c0)
#define G_StrFormat    ((StrFormatFn)0x4895b0)
#define G_StrAssign    ((StrAssignFn)0x408ff0)
#define G_SaveBoard    ((SaveBoardFn)0x417160)
#define G_AppButtonDepress ((ButtonDepressFn)0x440980)
#define G_ShowPractice ((Fn_i)0x4402c0)
#define G_ShowMainMenu ((Fn_v)0x43cb00)
#define G_CheckSaved   ((Fn_b_i)0x43ff40)
#define G_LoadSaved    ((Fn_i)0x440440)

#define VCALL(obj, off) (*(void ***)(obj))[(off) / 4]

/* ---------------------------------------------------------------- tiny libc */


static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, const char *s, int max) { int i = 0; for (; s[i] && i < max - 1; i++) d[i] = s[i]; d[i] = 0; }
static void supper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

static void mstr(MStr *m, const char *s)
{
    int n = slen(s);
    memset(m, 0, sizeof *m);
    m->size = n;
    if (n < 16) { m->res = 15; memcpy(m->u.buf, s, n + 1); }
    else { m->res = n; m->u.ptr = (char *)s; }  /* we own the buffer; game only reads it */
}
static const char *mstr_c(MStr *m) { return m->res >= 16 ? m->u.ptr : m->u.buf; }

static void zlog(const char *fmt, int a, int b);

/* ---------------------------------------------------------------- slot data */
#define NSLOTS 6
#define DLG_ID 77
#define DLG_READY 78
#define G_PauseBoard ((Fn_i)0x43c4b0)    /* app: board->Pause(on, counted) */

typedef struct Slot {
    int used;
    int mode;            /* -1 unknown, 0 practice, 1 survival */
    char level[48];
    char map[96];
    int score;
} Slot;

static Slot g_slots[NSLOTS];
static int g_slot = 1;          /* active slot (1..6) used for prc save path */
static void *g_widget;
static void *g_vtbl[64];
static void *g_slotbox;
static int g_hover = -1;        /* 0..5 slots, 10 back, 11 delete */
static int g_sel = -1;
static int g_downBtn = -1;

static int user_id(void)
{
    char *prof = *(char **)(APP + 0x614);
    return prof ? *(int *)(prof + 0x20) : 0;
}

static void slot_path(char *out, int user, int slot)
{
    if (slot <= 1) wsprintfA(out, "userdata/prc%d.sav", user);
    else wsprintfA(out, "userdata/prc%d_%d.sav", user, slot);
}

static int read_file(const char *path, unsigned char *buf, int max)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    ReadFile(h, buf, max, &n, 0);
    CloseHandle(h);
    return (int)n;
}

static void load_slots(void)
{
    static unsigned char buf[512];
    char path[MAX_PATH], inf[MAX_PATH];
    int u = user_id(), i;
    for (i = 0; i < NSLOTS; i++) {
        Slot *s = &g_slots[i];
        int n, p = 0, len;
        memset(s, 0, sizeof *s);
        slot_path(path, u, i + 1);
        n = read_file(path, buf, sizeof buf);
        if (n < 9) continue;
        if (*(int *)buf != SAVE_VERSION) continue;
        p = 5;                                  /* int version, bool practice */
        len = buf[p] | (buf[p + 1] << 8); p += 2;
        if (p + len > n) continue;
        if (len > 47) len = 47;
        memcpy(s->level, buf + p, len); s->level[len] = 0; p += buf[p - 2] | (buf[p - 1] << 8);
        len = buf[p] | (buf[p + 1] << 8); p += 2;
        if (p + len + 4 > n) continue;
        { int l = len > 95 ? 95 : len; memcpy(s->map, buf + p, l); s->map[l] = 0; }
        p += len;
        s->score = *(int *)(buf + p);
        supper(s->level); supper(s->map);
        s->mode = -1;
        wsprintfA(inf, "%s.inf", path);
        { unsigned char m[4]; if (read_file(inf, m, 4) >= 1) s->mode = m[0] ? 1 : 0; }
        s->used = 1;
    }
}

static void delete_slot(int slot)
{
    char path[MAX_PATH], inf[MAX_PATH];
    slot_path(path, user_id(), slot);
    wsprintfA(inf, "%s.inf", path);
    DeleteFileA(path);
    DeleteFileA(inf);
}

/* ---------------------------------------------------------------- drawing helpers */
static void set_color(void *g, int r, int gg, int b, int a) { Color c = { r, gg, b, a }; G_SetColor(g, &c); }

static int text_w(void *font, const char *s) { MStr m; mstr(&m, s); return ((StrWidthFn)VCALL(font, 0x1c))(font, &m); }
static int font_asc(void *font) { return *(int *)((char *)font + 4); }

static void text(void *g, void *font, const char *s, int x, int top, int r, int gg, int b, int center)
{
    MStr m;
    if (!font) return;
    mstr(&m, s);
    G_SetFont(g, font);
    set_color(g, r, gg, b, 255);
    if (center == 1) x -= text_w(font, s) / 2;
    else if (center == 2) x -= text_w(font, s);
    G_DrawString(g, &m, x, top + font_asc(font));
}

static void fmt_num(char *out, int v)
{
    char t[16]; int n, i, j = 0, neg = v < 0;
    unsigned u = neg ? -v : v;
    wsprintfA(t, "%u", u); n = slen(t);
    if (neg) out[j++] = '-';
    for (i = 0; i < n; i++) { if (i && (n - i) % 3 == 0) out[j++] = ','; out[j++] = t[i]; }
    out[j] = 0;
}

/* word-wrap s into up to maxl lines of width maxw; returns line count */
static int wrap(void *font, const char *s, int maxw, char lines[][64], int maxl)
{
    char cur[64] = "", word[64], test[130];
    int n = 0, i = 0;
    while (s[i]) {
        int k = 0;
        while (s[i] == ' ') i++;
        while (s[i] && s[i] != ' ' && k < 63) word[k++] = s[i++];
        word[k] = 0;
        if (!k) break;
        if (cur[0]) wsprintfA(test, "%s %s", cur, word); else scpy(test, word, sizeof test);
        if (!cur[0] || text_w(font, test) <= maxw) scpy(cur, test, 64);
        else { if (n < 16) scpy(lines[n++], cur, 64); scpy(cur, word, 64); }
        if (n >= 16) break;
    }
    if (cur[0] && n < 16) scpy(lines[n++], cur, 64);
    if (n > maxl) {
        char last[200]; int j;
        scpy(last, lines[maxl - 1], 64);
        for (j = maxl; j < n; j++) { int l = slen(last); if (l < 190) { last[l] = ' '; scpy(last + l + 1, lines[j], 190 - l); } }
        for (;;) {
            char t2[210]; int l = slen(last);
            wsprintfA(t2, "%s...", last);
            if (l == 0 || text_w(font, t2) <= maxw) { scpy(lines[maxl - 1], t2, 64); break; }
            last[l - 1] = 0;
            while (slen(last) && last[slen(last) - 1] == ' ') last[slen(last) - 1] = 0;
        }
        n = maxl;
    }
    return n;
}

/* ---------------------------------------------------------------- layout */
#define SW 172
#define SH 136
#define GX 17
#define GY 14
#define X0 (320 - (3 * SW + 2 * GX) / 2)
#define Y0 80
#define BTN_Y 398
#define BACK_X 60
#define DEL_X 460

static void slot_rect(int i, Rect *r) { r->x = X0 + (i % 3) * (SW + GX); r->y = Y0 + (i / 3) * (SH + GY); r->w = SW; r->h = SH; }

static int btn_w(void) { void *b = IMG_DIALOG_BUTTON; return b ? *(int *)((char *)b + 4) / 3 : 120; }
static int btn_h(void) { void *b = IMG_DIALOG_BUTTON; return b ? *(int *)((char *)b + 8) : 48; }

static int hit(int x, int y)
{
    int i;
    for (i = 0; i < NSLOTS; i++) { Rect r; slot_rect(i, &r); if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i; }
    if (y >= BTN_Y && y < BTN_Y + btn_h()) {
        if (x >= BACK_X && x < BACK_X + btn_w()) return 10;
        if (x >= DEL_X && x < DEL_X + btn_w()) return 11;
    }
    return -1;
}

static void draw_button(void *g, int x, int y, const char *label, int cel, int dim)
{
    void *img = IMG_DIALOG_BUTTON;
    if (img) {
        Rect src = { cel * btn_w(), 0, btn_w(), btn_h() };
        if (dim) { G_SetColorize(g, 1); set_color(g, 140, 140, 140, 255); }
        G_DrawImageSrc(g, img, x, y, &src);
        G_SetColorize(g, 0);
    }
    text(g, FONT_MAIN12, label, x + btn_w() / 2, y + (btn_h() - 14) / 2, dim ? 170 : 255, dim ? 170 : 255, dim ? 170 : 255, 1);
}

static void draw_plus(void *g, int cx, int cy, int lit)
{
    /* stone-coloured "+" built from rectangles, with a dark outline */
    int a = 26, t = 9;
    set_color(g, 30, 30, 20, 200);
    G_FillRect(g, cx - a - 2, cy - t / 2 - 2, 2 * a + 4, t + 4);
    G_FillRect(g, cx - t / 2 - 2, cy - a - 2, t + 4, 2 * a + 4);
    if (lit) set_color(g, 225, 222, 196, 255); else set_color(g, 150, 150, 132, 255);
    G_FillRect(g, cx - a, cy - t / 2, 2 * a, t);
    G_FillRect(g, cx - t / 2, cy - a, t, 2 * a);
    if (lit) set_color(g, 255, 255, 235, 255); else set_color(g, 178, 178, 160, 255);
    G_FillRect(g, cx - a, cy - t / 2, 2 * a, 2);
    G_FillRect(g, cx - t / 2, cy - a, 2, 2 * a);
}

static void __thiscall W_Draw(void *w, void *g)
{
    int i;
    (void)w;
    set_color(g, 0, 0, 0, 140);
    G_FillRect(g, 0, 0, 640, 480);
    if (IMG_DIALOG_BACK) { Rect r = { 15, 10, 610, 460 }; G_DrawImageBox(g, &r, IMG_DIALOG_BACK); }
    text(g, FONT_TITLE, "Save Game", 320, 26, 255, 255, 255, 1);

    for (i = 0; i < NSLOTS; i++) {
        Slot *s = &g_slots[i];
        Rect r; char buf[64];
        int cx;
        slot_rect(i, &r); cx = r.x + SW / 2;
        if (g_slotbox) {
            if (!s->used) { int k = g_hover == i ? 205 : 150; G_SetColorize(g, 1); set_color(g, k, k, k, 255); }
            G_DrawImageBox(g, &r, g_slotbox);
            G_SetColorize(g, 0);
        }
        if (s->used) {
            char lines[16][64]; int n, k, top;
            const char *mode = s->mode == 1 ? "SURVIVAL" : s->mode == 0 ? "PRACTICE" : "";
            wsprintfA(buf, "SLOT %d", i + 1);
            text(g, FONT_MAIN10, buf, r.x + 22, r.y + 15, 255, 222, 110, 0);
            if (mode[0]) text(g, FONT_MAIN10, mode, r.x + SW - 22, r.y + 15, s->mode ? 255 : 150, s->mode ? 150 : 255, s->mode ? 70 : 120, 2);
            text(g, FONT_MAIN12, s->level, cx, r.y + 32, 255, 255, 255, 1);
            n = wrap(FONT_MAIN10, s->map, SW - 40, lines, 3);
            top = r.y + 51 + (3 - n) * 13 / 2;
            for (k = 0; k < n; k++) text(g, FONT_MAIN10, lines[k], cx, top + k * 13, 255, 255, 255, 1);
            fmt_num(buf, s->score);
            text(g, FONT_MAIN12, buf, cx, r.y + 97, 255, 222, 110, 1);
            if (g_sel == i || g_hover == i) {
                int t = g_sel == i ? 3 : 2;
                if (g_sel == i) set_color(g, 255, 230, 90, 255); else set_color(g, 255, 240, 170, 150);
                G_FillRect(g, r.x + 2, r.y + 2, SW - 4, t);
                G_FillRect(g, r.x + 2, r.y + SH - 2 - t, SW - 4, t);
                G_FillRect(g, r.x + 2, r.y + 2, t, SH - 4);
                G_FillRect(g, r.x + SW - 2 - t, r.y + 2, t, SH - 4);
            }
        } else {
            int lit = g_hover == i;
            wsprintfA(buf, "SLOT %d", i + 1);
            text(g, FONT_MAIN10, buf, cx, r.y + 15, lit ? 255 : 190, lit ? 222 : 170, lit ? 110 : 90, 1);
            draw_plus(g, cx, r.y + 68, lit);
            text(g, FONT_MAIN10, "EMPTY", cx, r.y + 101, lit ? 255 : 170, lit ? 222 : 190, lit ? 110 : 170, 1);
        }
    }

    draw_button(g, BACK_X, BTN_Y, "BACK", g_hover == 10 ? (g_downBtn == 10 ? 2 : 1) : 0, 0);
    draw_button(g, DEL_X, BTN_Y, "DELETE", g_sel >= 0 && g_hover == 11 ? (g_downBtn == 11 ? 2 : 1) : 0, g_sel < 0);
    if (g_sel >= 0) {
        char buf[32]; wsprintfA(buf, "SLOT %d SELECTED", g_sel + 1);
        text(g, FONT_MAIN10, buf, 320, BTN_Y + 17, 255, 222, 110, 1);
    } else {
        text(g, FONT_MAIN10, "DOUBLE-CLICK A SLOT TO PLAY", 320, BTN_Y + 17, 200, 200, 170, 1);
    }
}

/* ---------------------------------------------------------------- widget behaviour */
static void mark_dirty(void *w) { ((Fn_v)VCALL(w, 0xa8))(w); }

static void close_widget(void)
{
    void *w = g_widget;
    void *wm;
    if (!w) return;
    g_widget = 0;
    wm = *(void **)((char *)w + 0x24);
    if (wm) G_RemoveWidget(wm, w);
    ((Fn_p)VCALL(APP, 0x54))(APP, w);     /* SafeDeleteWidget */
}

static void play_click(void) { ((Fn_i)VCALL(APP, 0x9c))(APP, SOUND_BUTTON1); }

static void do_back(void)
{
    play_click();
    close_widget();
    G_ShowMainMenu(APP);
}

/* after a slot is loaded: freeze the board and wait for CONFIRM */
static int g_readyPaused;

static void show_ready(Slot *s, int slot)
{
    char lines[400], num[24], hdr[32];
    MStr mh, ml, mf;
    void *dlg;
    char *board = *(char **)(APP + 0x5e4);
    if (!board) return;
    G_PauseBoard(APP, 1);
    g_readyPaused = 1;
    fmt_num(num, s->score);
    wsprintfA(hdr, "SLOT %d", slot);
    wsprintfA(lines, "%s%s\r\n%s\r\nScore: %s\r\n\r\nGet ready!",
              s->level, s->mode == 1 ? " - SURVIVAL" : s->mode == 0 ? " - PRACTICE" : "", s->map, num);
    mstr(&mh, hdr); mstr(&ml, lines); mstr(&mf, "CONFIRM");
    dlg = ((DoDialogFn)VCALL(APP, 0x4c))(APP, DLG_READY, 1, &mh, &ml, &mf, 3);   /* 3 = one footer button */
    if (!dlg) { G_PauseBoard(APP, 0); g_readyPaused = 0; return; }
    ((AddDialogFn)VCALL(APP, 0xdc))(APP, DLG_READY, dlg);
}

static void do_load(int i)
{
    Slot s = g_slots[i];
    play_click();
    g_slot = i + 1;
    zlog("load slot %d", g_slot, 0);
    close_widget();
    *(unsigned char *)(APP + 0x664) = 1;   /* practice mode */
    if (G_CheckSaved(APP, 0) & 0xff) {
        G_LoadSaved(APP, 1);
        show_ready(&s, g_slot);
    }
}

static void do_new(int i)
{
    play_click();
    g_slot = i + 1;
    zlog("new game in slot %d", g_slot, 0);
    delete_slot(g_slot);                   /* start clean */
    close_widget();
}

static void do_delete_prompt(void)
{
    Slot *s;
    char lines[400], num[24], hdr[32];
    MStr mh, ml, mf;
    void *dlg;
    if (g_sel < 0 || !g_slots[g_sel].used) return;
    s = &g_slots[g_sel];
    play_click();
    fmt_num(num, s->score);
    wsprintfA(hdr, "DELETE SLOT %d?", g_sel + 1);
    wsprintfA(lines, "%s%s%s\r\n%s\r\nScore: %s\r\n\r\nThis save will be lost forever.",
              s->level, s->mode == 1 ? " - SURVIVAL" : s->mode == 0 ? " - PRACTICE" : "", "", s->map, num);
    mstr(&mh, hdr); mstr(&ml, lines); mstr(&mf, "");
    dlg = ((DoDialogFn)VCALL(APP, 0x4c))(APP, DLG_ID, 1, &mh, &ml, &mf, 2);
    if (!dlg) return;
    {
        MStr *yes = (MStr *)(*(char **)((char *)dlg + 0x60) + 0x5c);
        MStr *no = (MStr *)(*(char **)((char *)dlg + 0x64) + 0x5c);
        G_StrAssign(yes, "Delete", 6);
        G_StrAssign(no, "Cancel", 6);
    }
    ((AddDialogFn)VCALL(APP, 0xdc))(APP, DLG_ID, dlg);
}

static void __thiscall W_Update(void *w) { G_WidgetUpdate(w); mark_dirty(w); }

static void __thiscall W_MouseMove(void *w, int x, int y)
{
    int h = hit(x, y);
    if (h != g_hover) {
        g_hover = h;
        ((Fn_i)VCALL(w, 0x2c))(w, h >= 0 && !(h == 11 && g_sel < 0));  /* ShowFinger */
        mark_dirty(w);
    }
}

static void __thiscall W_MouseLeave(void *w) { g_hover = -1; ((Fn_i)VCALL(w, 0x2c))(w, 0); mark_dirty(w); }

static void __thiscall W_MouseDown(void *w, int x, int y, int cc)
{
    int h = hit(x, y);
    if (cc < 0) return;                     /* right button */
    if (h >= 0 && h < NSLOTS) {
        if (!g_slots[h].used) { do_new(h); return; }
        if (cc >= 2 && g_sel == h) { do_load(h); return; }
        if (g_sel != h) play_click();
        g_sel = h;
    } else if (h == 10) { g_downBtn = 10; }
    else if (h == 11 && g_sel >= 0) { g_downBtn = 11; }
    mark_dirty(w);
}

static void __thiscall W_MouseUp(void *w, int x, int y)
{
    int h = hit(x, y), d = g_downBtn;
    g_downBtn = -1;
    if (d >= 0 && d == h) {
        if (d == 10) { do_back(); return; }
        if (d == 11) { do_delete_prompt(); }
    }
    mark_dirty(w);
}

static void __thiscall W_KeyDown(void *w, int key)
{
    (void)w;
    if (key == VK_ESCAPE) do_back();
    else if (key == VK_RETURN && g_sel >= 0 && g_slots[g_sel].used) do_load(g_sel);
    else if (key == VK_DELETE) do_delete_prompt();
}

static void open_widget(void)
{
    char *app = APP;
    void *w, *wm;
    if (g_widget) return;
    if (!g_slotbox) { MStr p; mstr(&p, "images/slotbox"); g_slotbox = G_GetImage(app, &p); }
    if (!g_vtbl[0]) {
        int i; void **base = (void **)0x50553c;
        for (i = 0; i < 44; i++) g_vtbl[i] = base[i];
        g_vtbl[0x40 / 4] = (void *)W_Draw;
        g_vtbl[0x44 / 4] = (void *)W_Update;
        g_vtbl[0x54 / 4] = (void *)W_KeyDown;
        g_vtbl[0x60 / 4] = (void *)W_MouseLeave;
        g_vtbl[0x64 / 4] = (void *)W_MouseMove;
        g_vtbl[0x68 / 4] = (void *)W_MouseDown;
        g_vtbl[0x70 / 4] = (void *)W_MouseUp;
    }
    load_slots();
    g_hover = -1; g_sel = -1; g_downBtn = -1;
    w = G_new(0x60);
    if (!w) return;
    memset(w, 0, 0x60);
    G_WidgetCtor(w);
    *(void ***)w = g_vtbl;
    ((void (__thiscall *)(void *, int, int, int, int))VCALL(w, 0x34))(w, 0, 0, 640, 480);
    wm = *(void **)(app + 0x2dc);
    G_AddWidget(wm, w);
    G_SetFocus(wm, w);
    g_widget = w;
    zlog("slot panel opened (slotbox image %s)", (int)(g_slotbox ? "ok" : "MISSING"), 0);
}

/* ---------------------------------------------------------------- hooks */
/* main menu "Gauntlet" button: show practice screen without the CONTINUE? dialog, then our slot panel */
static void __fastcall H_OnGauntlet(void *app)
{
    zlog("gauntlet clicked, user %d", user_id(), 0);
    G_ShowPractice(app, 0);
    open_widget();
}

/* replaces GetSaveGameName(std::string *out, bool practice, int userId) */
static void *__cdecl H_SaveName(MStr *out, int practice, int user)
{
    practice &= 0xff;
    if (__builtin_return_address(0) == (void *)0x40700b) {   /* deleting a user: drop slots 2..6 too */
        int i; char p[MAX_PATH], inf[MAX_PATH];
        for (i = 1; i <= NSLOTS; i++) {
            slot_path(p, user, i); wsprintfA(inf, "%s.inf", p);
            DeleteFileA(inf);
            if (i > 1) DeleteFileA(p);
        }
        return G_StrFormat(out, "userdata/%s%d.sav", "prc", user);
    }
    if (practice && g_slot > 1) return G_StrFormat(out, "userdata/prc%d_%d.sav", user, g_slot);
    return G_StrFormat(out, "userdata/%s%d.sav", practice ? "prc" : "adv", user);
}

/* Board::SaveGame call in CleanupBoard: also write the survival flag next to the save */
static int __thiscall H_SaveBoard(void *board, MStr *path)
{
    int r = G_SaveBoard(board, path);
    zlog("saved board, slot %d, survival %d", g_slot, *((unsigned char *)board + 0x15b));
    if (*(int *)((char *)board + 0x17c)) {
        char inf[MAX_PATH]; HANDLE h; DWORD n;
        wsprintfA(inf, "%s.inf", mstr_c(path));
        h = CreateFileA(inf, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (h != INVALID_HANDLE_VALUE) {
            unsigned char m = *((unsigned char *)board + 0x15b) ? 1 : 0;
            WriteFile(h, &m, 1, &n, 0);
            CloseHandle(h);
        }
    }
    return r;
}

/* app ButtonDepress (dialog buttons): handle our delete-confirm dialog */
static int __thiscall H_AppButton(void *app, int id)
{
    int base = id % 10000;
    if (base % 1000 == DLG_READY && base >= 1000 && base < 4000) {
        ((Fn_i)VCALL(app, 0xe0))(app, DLG_READY);   /* KillDialog */
        if (g_readyPaused) { g_readyPaused = 0; G_PauseBoard(app, 0); }
        play_click();
        zlog("ready confirmed, slot %d", g_slot, 0);
        return 1;
    }
    if (base == 2000 + DLG_ID || base == 3000 + DLG_ID) {
        ((Fn_i)VCALL(app, 0xe0))(app, DLG_ID);      /* KillDialog */
        if (base == 2000 + DLG_ID && g_sel >= 0) {
            zlog("deleted slot %d", g_sel + 1, 0);
            delete_slot(g_sel + 1);
            g_sel = -1;
            load_slots();
        }
        play_click();
        if (g_widget) { G_SetFocus(*(void **)(APP + 0x2dc), g_widget); mark_dirty(g_widget); }
        return 1;
    }
    return G_AppButtonDepress(app, id);
}

static void zlog(const char *fmt, int a, int b)
{
    char line[256]; DWORD n; HANDLE h;
    SYSTEMTIME t; GetLocalTime(&t);
    wsprintfA(line, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    wsprintfA(line + slen(line), fmt, a, b);
    lstrcatA(line, "\r\n");
    h = CreateFileA("zdslots.log", FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, line, slen(line), &n, 0);
    CloseHandle(h);
}

static void patch(void *addr, const void *bytes, int n)
{
    DWORD old;
    VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(addr, bytes, n);
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
}

static void put_rel(unsigned char op, unsigned at, void *target, int pad)
{
    unsigned char b[8];
    int rel = (int)((unsigned)target - (at + 5)), i;
    b[0] = op; memcpy(b + 1, &rel, 4);
    for (i = 0; i < pad; i++) b[5 + i] = 0x90;
    patch((void *)at, b, 5 + pad);
}

static int memcmp_(const void *a, const void *b, int n) { const unsigned char *p = a, *q = b; while (n--) { if (*p != *q) return *p - *q; p++; q++; } return 0; }

static int expect(unsigned at, const char *bytes, int n)
{
    return memcmp_((const void *)at, bytes, n) == 0;
}

static void install(void)
{
    /* sanity: make sure this is the exe we were built for */
    if (!expect(0x430e35, "\x6a\x01\xe8", 3) || !expect(0x42acc0, "\x55\x8b\xec\x51", 4) ||
        *(unsigned *)0x502cac != 0x440980 || !expect(0x43c8a3, "\xe8", 1))
        return;
    put_rel(0xE8, 0x430e35, (void *)H_OnGauntlet, 2);   /* push 1; call ShowPracticeScreen -> call ours */
    put_rel(0xE9, 0x42acc0, (void *)H_SaveName, 0);
    put_rel(0xE8, 0x43c8a3, (void *)H_SaveBoard, 0);
    { void *p = (void *)H_AppButton; patch((void *)0x502cac, &p, 4); }
    zlog("install: hooks installed", 0, 0);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) install();
    return TRUE;
}
