/*
 * ZDSlots - 6 save slots for Gauntlet (practice/survival) in Zuma Deluxe Plus.
 * Loaded by a tiny stub at the exe entry point (LoadLibraryA("zdslots.dll")).
 * All game addresses below are for ZDPlus.exe / ZDPlusO.exe (Zuma Deluxe 1.0 build 101).
 */
#include <windows.h>
#include <string.h>
#include <math.h>

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
static int memcmp_(const void *a, const void *b, int n);

/* ---------------------------------------------------------------- slot data */
#define NSLOTS 6
#define DLG_ID 77
#define DLG_READY 78
#define G_PauseBoard ((Fn_i)0x43c4b0)    /* app: board->Pause(on, counted) */

typedef struct Slot {
    int used;
    int mode;            /* -1 unknown, 0 practice, 1 survival, 2 timeless */
    int timeLeft;        /* timeless: frames left when saved */
    char level[48];
    char map[96];
    int score;
} Slot;

static Slot g_slots[NSLOTS];
static void timeless_set(int on);
static void timeless_resume(int frames);
static int timeless_active(char *board);
static int timeless_left(void);
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
        { unsigned char m[8]; int k = read_file(inf, m, 8); if (k >= 1) s->mode = m[0] > 2 ? 1 : m[0]; if (k >= 5) memcpy(&s->timeLeft, m + 1, 4); }
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
            const char *mode = s->mode == 2 ? "TIMELESS" : s->mode == 1 ? "SURVIVAL" : s->mode == 0 ? "PRACTICE" : "";
            wsprintfA(buf, "SLOT %d", i + 1);
            text(g, FONT_MAIN10, buf, r.x + 22, r.y + 15, 255, 222, 110, 0);
            if (mode[0]) text(g, FONT_MAIN10, mode, r.x + SW - 22, r.y + 15, s->mode == 2 ? 120 : s->mode ? 255 : 150, s->mode == 2 ? 210 : s->mode ? 150 : 255, s->mode == 2 ? 255 : s->mode ? 70 : 120, 2);
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
              s->level, s->mode == 2 ? " - TIMELESS" : s->mode == 1 ? " - SURVIVAL" : s->mode == 0 ? " - PRACTICE" : "", s->map, num);
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
    timeless_set(s.mode == 2);
    if (s.mode == 2) timeless_resume(s.timeLeft);
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
              s->level, s->mode == 2 ? " - TIMELESS" : s->mode == 1 ? " - SURVIVAL" : s->mode == 0 ? " - PRACTICE" : "", "", s->map, num);
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
            unsigned char m[5];
            int left = timeless_left();
            m[0] = *((unsigned char *)board + 0x15b) ? 1 : timeless_active((char *)board) ? 2 : 0;
            memcpy(m + 1, &left, 4);
            WriteFile(h, m, 5, &n, 0);
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

/* ---------------------------------------------------------------- frog with two positions
 * levels.xml: <Graphics ... dispname="X" gx="a" gy="b" gx2="c" gy2="d">
 * A / Left arrow -> left position, D / Right arrow -> right position.
 * Maps without gx2/gy2 are untouched. */
#define G_GunSetPos ((void (__thiscall *)(void *, int, int))0x404720)
#define G_BoardUpdate ((Fn_v)0x421ce0)
#define G_BoardKeyDown ((Fn_i)0x401b10)
#define MAXFROG 64
#define SLIDE_FRAMES 14

typedef struct FrogMap { char disp[96]; int x[2], y[2]; } FrogMap;
static FrogMap g_frog[MAXFROG];
static int g_nfrog = -1;

static struct {
    void *board, *level;
    FrogMap *map;
    int cur, target, step;      /* step 0 = idle */
    int fx, fy;                 /* slide start */
    int lx, ly;                 /* last position we set */
} g_fs;

static int attr(const char *tag, int tlen, const char *name, char *out, int max)
{
    int nl = slen(name), i;
    for (i = 0; i + nl + 2 < tlen; i++) {
        if ((i == 0 || tag[i - 1] == ' ' || tag[i - 1] == '\t' || tag[i - 1] == '\n' || tag[i - 1] == '\r') &&
            memcmp_(tag + i, name, nl) == 0) {
            int j = i + nl, k = 0;
            while (j < tlen && (tag[j] == ' ' || tag[j] == '\t')) j++;
            if (j >= tlen || tag[j] != '=') continue;
            j++;
            while (j < tlen && (tag[j] == ' ' || tag[j] == '\t')) j++;
            if (j >= tlen || tag[j] != '"') continue;
            j++;
            while (j < tlen && tag[j] != '"' && k < max - 1) out[k++] = tag[j++];
            out[k] = 0;
            return 1;
        }
    }
    return 0;
}

static int atoi_(const char *s) { int v = 0, neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return neg ? -v : v; }

static void load_frog_config(void)
{
    HANDLE h; DWORD size, n = 0; char *buf; DWORD i;
    g_nfrog = 0;
    h = CreateFileA("levels/levels.xml", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, 0);
    buf = (char *)HeapAlloc(GetProcessHeap(), 0, size + 1);
    if (!buf) { CloseHandle(h); return; }
    ReadFile(h, buf, size, &n, 0); CloseHandle(h);
    buf[n] = 0;
    for (i = 0; i + 9 < n && g_nfrog < MAXFROG; i++) {
        if (memcmp_(buf + i, "<Graphics", 9) == 0) {
            DWORD e = i; char v[96];
            FrogMap *f = &g_frog[g_nfrog];
            while (e < n && buf[e] != '>') e++;
            if (attr(buf + i, e - i, "gx2", v, sizeof v)) {
                f->x[1] = atoi_(v);
                if (!attr(buf + i, e - i, "gy2", v, sizeof v)) continue;
                f->y[1] = atoi_(v);
                if (!attr(buf + i, e - i, "gx", v, sizeof v)) continue;
                f->x[0] = atoi_(v);
                if (!attr(buf + i, e - i, "gy", v, sizeof v)) continue;
                f->y[0] = atoi_(v);
                if (!attr(buf + i, e - i, "dispname", f->disp, sizeof f->disp)) continue;
                zlog("frog: two positions for a map, x %d / %d", f->x[0], f->x[1]);
                g_nfrog++;
            }
            i = e;
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
}

static char *board_level(char *board) { return *(char **)(board + (*(board + 0x15a) ? 0xfc : 0xf8)); }

static FrogMap *frog_map_for(char *level)
{
    int i;
    if (!level) return 0;
    if (g_nfrog < 0) load_frog_config();
    for (i = 0; i < g_nfrog; i++) {
        FrogMap *f = &g_frog[i];
        if (*(int *)(level + 0x60) == f->x[0] && *(int *)(level + 0x64) == f->y[0] &&
            lstrcmpA(mstr_c((MStr *)(level + 0x20)), f->disp) == 0)
            return f;
    }
    return 0;
}

static void frog_sync(char *board)
{
    char *level = board_level(board);
    char *gun = *(char **)(board + 0x268);
    if (board != g_fs.board || level != g_fs.level) {
        g_fs.board = board; g_fs.level = level;
        g_fs.map = frog_map_for(level);
        g_fs.cur = g_fs.target = 0; g_fs.step = 0;
        if (gun) { g_fs.lx = *(int *)(gun + 0x10); g_fs.ly = *(int *)(gun + 0x14); }
    }
    /* the game moved the frog itself (level start/restart): back to position 0 */
    if (gun && g_fs.map && !g_fs.step && (*(int *)(gun + 0x10) != g_fs.lx || *(int *)(gun + 0x14) != g_fs.ly)) {
        g_fs.cur = g_fs.target = 0;
        g_fs.lx = *(int *)(gun + 0x10); g_fs.ly = *(int *)(gun + 0x14);
    }
}

static void balls_update(char *board);

static void __thiscall H_BoardUpdate(void *board)
{
    char *gun;
    G_BoardUpdate(board);
    balls_update((char *)board);
    frog_sync((char *)board);
    gun = *(char **)((char *)board + 0x268);
    if (!gun || !g_fs.map || !g_fs.step) return;
    if (*(int *)((char *)board + 0xcc)) return;          /* paused */
    {
        FrogMap *f = g_fs.map;
        int t = g_fs.target, s = ++g_fs.step;
        /* ease-out */
        int k = SLIDE_FRAMES - s, num = SLIDE_FRAMES * SLIDE_FRAMES - k * k, den = SLIDE_FRAMES * SLIDE_FRAMES;
        int x = g_fs.fx + (f->x[t] - g_fs.fx) * num / den;
        int y = g_fs.fy + (f->y[t] - g_fs.fy) * num / den;
        if (s >= SLIDE_FRAMES) { x = f->x[t]; y = f->y[t]; g_fs.step = 0; g_fs.cur = t; }
        {
            /* +0x10/+0x14 = home position, +8/+0xc = drawn position (recoil), +0x18/+0x1c = recoil point */
            int dx = x - *(int *)(gun + 0x10), dy = y - *(int *)(gun + 0x14);
            int rx = *(int *)(gun + 0x18) + dx, ry = *(int *)(gun + 0x1c) + dy;
            int ox = *(int *)(gun + 8) + dx, oy = *(int *)(gun + 0xc) + dy;
            G_GunSetPos(gun, x, y);
            *(int *)(gun + 0x18) = rx; *(int *)(gun + 0x1c) = ry;
            *(int *)(gun + 8) = ox; *(int *)(gun + 0xc) = oy;   /* keep any recoil offset */
        }
        g_fs.lx = x; g_fs.ly = y;
    }
}

static void __thiscall H_BoardKeyDown(void *board, int key)
{
    int want = -1;
    if (key == 'A' || key == VK_LEFT) want = 0;
    else if (key == 'D' || key == VK_RIGHT) want = 1;
    if (want >= 0) {
        char *gun;
        frog_sync((char *)board);
        gun = *(char **)((char *)board + 0x268);
        if (gun && g_fs.map && !*(int *)((char *)board + 0xcc)) {
            FrogMap *f = g_fs.map;
            /* "left" = the position with the smaller x */
            int left = f->x[0] <= f->x[1] ? 0 : 1;
            int t = want == 0 ? left : 1 - left;
            if (t != (g_fs.step ? g_fs.target : g_fs.cur)) {
                g_fs.fx = *(int *)(gun + 0x10); g_fs.fy = *(int *)(gun + 0x14);
                g_fs.target = t; g_fs.step = 1;
            }
        }
    }
    G_BoardKeyDown(board, key);
}

/* ---------------------------------------------------------------- special balls
 * Stored in Ball::mPowerType (+0x60) next to the stock powers 0..3 (4 = none), so they are
 * saved/loaded with the board like any power:
 *   5 rainbow  matches any colour (CountGroup replacement)
 *   6 skull    when it explodes a little frog runs out of the skull hole and eats the
 *              front of the chain
 *   7 frog     when it explodes every ball of its colour on the board explodes
 * Tuning: zdballs.ini in the game folder (section [Balls]). */
#define PW_RAINBOW 5
#define PW_SKULL   6
#define PW_FROG    7

#define G_Rand          ((int (__cdecl *)(void))0x4298d0)
#define G_BallNext      ((char *(__thiscall *)(void *, int))0x4054c0)
#define G_BallPrev      ((char *(__thiscall *)(void *, int))0x405470)
#define G_BallSetPower  ((void (__thiscall *)(void *, int, int))0x401320)
#define G_BallDraw      ((void (__thiscall *)(void *, void *))0x403f00)
#define G_CurveExplode  ((void (__thiscall *)(void *, void *))0x417540)
#define G_ActivatePower ((void (__thiscall *)(void *, void *))0x4172f0)
#define G_IncScore      ((void (__thiscall *)(void *, int, int))0x413a40)
#define G_BoardDraw     ((void (__thiscall *)(void *, void *))0x40d330)
#define G_DrawImageRotF ((void (__thiscall *)(void *, void *, float, float, double, Rect *))0x450740)
#define SOUND_POP      (*(int *)0x547fb8)
#define SOUND_FROGLAND (*(int *)0x547f2c)
#define SOUND_EXPLODE  (*(int *)0x547fdc)
#define SOUND_JEWEL_APPEAR (*(int *)0x548000)

#define B_I(p, o) (*(int *)((char *)(p) + (o)))
#define B_F(p, o) (*(float *)((char *)(p) + (o)))
#define B_P(p, o) (*(char **)((char *)(p) + (o)))

static struct {
    int loaded, enabled;
    int freq[3];          /* rainbow, skull, frog: mean frames between spawns per curve (0 = off) */
    int maxPer;           /* max balls of one kind on a curve at once */
    int frogPts, skullEat, skullPts;
    int tlSeconds;
} cfg;

static void load_cfg(void)
{
    const char *f = ".\\zdballs.ini";
    cfg.loaded = 1;
    cfg.enabled  = GetPrivateProfileIntA("Balls", "Enabled", 1, f);
    cfg.freq[0]  = GetPrivateProfileIntA("Balls", "RainbowFreq", 1500, f);
    cfg.freq[1]  = GetPrivateProfileIntA("Balls", "SkullFreq", 2500, f);
    cfg.freq[2]  = GetPrivateProfileIntA("Balls", "FrogFreq", 2000, f);
    cfg.maxPer   = GetPrivateProfileIntA("Balls", "MaxPerCurve", 1, f);
    cfg.frogPts  = GetPrivateProfileIntA("Balls", "FrogPoints", 10, f);
    cfg.skullEat = GetPrivateProfileIntA("Balls", "SkullEat", 10, f);
    cfg.skullPts = GetPrivateProfileIntA("Balls", "SkullPoints", 10, f);
    cfg.tlSeconds = GetPrivateProfileIntA("Timeless", "Seconds", 120, f);
    if (cfg.tlSeconds < 1) cfg.tlSeconds = 1;
    zlog("balls: enabled %d, rainbow freq %d", cfg.enabled, cfg.freq[0]);
}

static void *g_img[4];    /* rainbow, skull icon, frog icon, mini frog */
static void *img(int i)
{
    static const char *names[4] = { "images/zdx_rainbow", "images/zdx_skull", "images/zdx_frog", "images/zdx_minifrog" };
    static int tried[4];
    if (!g_img[i] && !tried[i]) {
        MStr p; mstr(&p, names[i]);
        tried[i] = 1;
        g_img[i] = G_GetImage(APP, &p);
        if (!g_img[i]) zlog("balls: missing image %d", i, 0);
    }
    return g_img[i];
}

static void play(int snd) { if (snd) ((Fn_i)VCALL(APP, 0x9c))(APP, snd); }

static int ball_power(char *b) { int t = B_I(b, 0x60); return t == 4 ? B_I(b, 0x64) : t; }
static int is_rainbow(char *b) { return B_I(b, 0x60) == PW_RAINBOW; }

/* board curves */
static int ncurves(char *board) { int n = B_I(board, 0x8c); return n < 0 ? 0 : n > 8 ? 8 : n; }
static char *curve_at(char *board, int i) { return B_P(board, 0x5c + 4 * i); }
static char *curve_of(char *board, char *ball)
{
    int i;
    for (i = 0; i < ncurves(board); i++) { char *c = curve_at(board, i); if (c && B_P(ball, 0x24) == c + 0x2c) return c; }
    return 0;
}
/* std::list<Ball*> at curve+0x2c: head node at +0x30; node = {next, prev, Ball*} */
#define FOR_BALLS(curve, b) for (char **n_ = B_P(curve, 0x30) ? *(char ***)B_P(curve, 0x30) : 0, *b; \
                                 n_ && n_ != (char **)B_P(curve, 0x30) && ((b = n_[2]), 1); n_ = (char **)n_[0])

static int wp_count(char *curve)
{
    char *w = B_P(curve, 0xc), *b, *e;
    if (!w) return 0;
    b = B_P(w, 8); e = B_P(w, 0xc);
    return b ? (int)(e - b) / 36 : 0;
}
static float *wp_at(char *curve, int i)
{
    int n = wp_count(curve);
    if (i >= n) i = n - 1;
    if (i < 0) i = 0;
    return (float *)(B_P(B_P(curve, 0xc), 8) + 36 * i);
}

/* same steps the bomb power uses for every ball it hits */
static int explode(char *curve, char *ball)
{
    char *board = B_P(curve, 8);
    if (B_I(ball, 0x34)) return 0;
    B_I(ball, 0x48) = B_I(board, 0x240);
    B_I(ball, 0x44) = B_I(board, 0x23c);
    G_CurveExplode(curve, ball);
    return 1;
}

/* ---- floating "+N" texts */
typedef struct Pop { int t; float x, y; char s[16]; } Pop;
#define NPOP 12
static Pop g_pop[NPOP];
static void add_pop(float x, float y, int pts)
{
    int i, best = 0;
    for (i = 0; i < NPOP; i++) if (g_pop[i].t <= 0) { best = i; break; } else if (g_pop[i].t < g_pop[best].t) best = i;
    g_pop[best].t = 110; g_pop[best].x = x; g_pop[best].y = y;
    wsprintfA(g_pop[best].s, "+%d", pts);
}

/* ---- the little frog from the skull hole */
typedef struct Mini { int on; char *curve; float pos; int eaten, cool, state, t, pts; float x, y, ang; int pri; } Mini;
#define NMINI 4
#define MINI_SPEED 5.0f
#define MINI_REACH 24.0f
#define MINI_EAT_EVERY 9
static Mini g_mini[NMINI];
static char *g_bboard;
static int g_lastSpawn[8][3];
static int g_depth;

static void mini_spawn(char *curve)
{
    int i, n = wp_count(curve);
    if (n < 2) return;
    for (i = 0; i < NMINI; i++) if (!g_mini[i].on) break;
    if (i == NMINI) return;
    memset(&g_mini[i], 0, sizeof g_mini[i]);
    g_mini[i].on = 1; g_mini[i].curve = curve; g_mini[i].pos = (float)(n - 1);
    play(SOUND_FROGLAND);
    zlog("balls: mini frog out of the skull hole (path %d points)", n, 0);
}

static char *front_ball(char *curve)
{
    char *best = 0;
    FOR_BALLS(curve, b) if (!B_I(b, 0x34) && (!best || B_F(b, 0xc) > B_F(best, 0xc))) best = b;
    return best;
}

static void mini_update(Mini *m, char *board)
{
    int i, ok = 0;
    float *p, *q, dx, dy;
    for (i = 0; i < ncurves(board); i++) if (curve_at(board, i) == m->curve) ok = 1;
    if (!ok || wp_count(m->curve) < 2) { m->on = 0; return; }
    m->t++;
    if (m->state == 0) {
        char *f = front_ball(m->curve);
        if (m->cool > 0) m->cool--;
        if (!f || m->eaten >= cfg.skullEat || m->pos <= 0) {
            m->state = 1; m->t = 0;
            if (m->pts) add_pop(m->x, m->y - 20, m->pts);
        } else {
            float fp = B_F(f, 0xc);
            if (m->pos > fp + MINI_REACH) {
                m->pos -= MINI_SPEED;
                if (m->pos < fp + MINI_REACH) m->pos = fp + MINI_REACH;
            } else {
                m->pos = fp + MINI_REACH;          /* the chain pushes the frog back */
                if (!m->cool) {
                    g_depth++;
                    if (explode(m->curve, f)) {
                        m->eaten++;
                        if (cfg.skullPts) { G_IncScore(board, cfg.skullPts, 1); m->pts += cfg.skullPts; }
                        play(SOUND_POP);
                    }
                    g_depth--;
                    m->cool = MINI_EAT_EVERY;
                }
            }
        }
    } else if (m->t > 45) { m->on = 0; return; }
    p = wp_at(m->curve, (int)m->pos);
    q = wp_at(m->curve, (int)m->pos - 12);
    m->x = p[0]; m->y = p[1];
    m->pri = *((unsigned char *)p + 0x21);
    dx = q[0] - p[0]; dy = q[1] - p[1];
    if (dx * dx + dy * dy > 0.5f) m->ang = atan2f(-dx, -dy);   /* sprite faces up; game rotates counter-clockwise */
}

static void spawn_specials(char *board)
{
    int frame = B_I(board, 0xc4), ci, k;
    for (ci = 0; ci < ncurves(board) && ci < 8; ci++) {
        char *c = curve_at(board, ci);
        int have[3] = { 0, 0, 0 }, n = 0;
        if (!c || B_I(c, 0x34) < 8) continue;
        FOR_BALLS(c, b) {
            int t = B_I(b, 0x60);
            if (t >= PW_RAINBOW && t <= PW_FROG) have[t - PW_RAINBOW]++;
        }
        for (k = 0; k < 3; k++) {
            int f = cfg.freq[k];
            if (f <= 0 || have[k] >= cfg.maxPer) continue;
            if (frame < g_lastSpawn[ci][k]) g_lastSpawn[ci][k] = frame;
            if (frame - g_lastSpawn[ci][k] <= f || G_Rand() % f) continue;
            n = 0;
            FOR_BALLS(c, b) if (B_I(b, 0x60) == 4 && B_I(b, 0x64) == 4 && !B_I(b, 0x34) && B_F(b, 0xc) > 0) n++;
            if (!n) continue;
            n = G_Rand() % n;
            FOR_BALLS(c, b) if (B_I(b, 0x60) == 4 && B_I(b, 0x64) == 4 && !B_I(b, 0x34) && B_F(b, 0xc) > 0 && n-- == 0) {
                G_BallSetPower(b, PW_RAINBOW + k, 0);
                break;
            }
            g_lastSpawn[ci][k] = frame;
        }
    }
}


/* ---------------------------------------------------------------- Timeless mode
 * A Gauntlet mode next to Standard and Survival: the level is played like Practice, but a
 * countdown runs (zdballs.ini [Timeless] Seconds). At 0.0 the game's own "a ball reached the
 * hole" routine runs: every ball rushes into the hole and a life is lost. The clock restarts
 * with each try and each level. The board itself is an ordinary Practice board. */
#define G_BoardLose ((Fn_v)0x421430)
#define G_ScrAdded    ((Fn_p)0x43bfd0)
#define G_ScrRemoved  ((Fn_p)0x435250)
#define G_ScrResize   ((void (__thiscall *)(void *, int, int, int, int))0x435170)
#define G_ScrButton   ((Fn_i)0x43bf10)
#define G_ScrRefresh  ((Fn_v)0x43b680)
#define G_SaveScores  ((Fn_v)0x4212c0)
#define G_MakeButton  ((void *(__cdecl *)(int, void *, MStr *, int, void *, int))0x42b940)
#define ID_TIMELESS 7
#define ID_PRACTICE 5
#define ID_SURVIVAL 6

/* read from the HUD asm below */
int g_hudOn;
float g_hudSecs;
const char g_hudFmt[] = "%.1f";

static int g_timeless;           /* mode picked on the Gauntlet screen */
static int g_tm;                 /* frames left */
static int g_tmKeep;             /* resuming a save: keep g_tm on the next start */
static int g_tmPlaying;
static char *g_tmBoard, *g_tmLevel;
static void *g_tlBtn;            /* our button while the Gauntlet screen is up */
static int g_tlW, g_tlH;         /* its size */
static char g_lblStd[16] = "Standard";
static char g_prefix[4] = "e_";  /* high score table prefix: e_ survival, t_ timeless */

static void timeless_set(int on)
{
    g_timeless = on;
    scpy(g_lblStd, on ? "Timeless" : "Standard", sizeof g_lblStd);
}
static void timeless_resume(int frames) { g_tm = frames; g_tmKeep = frames > 0; }
static int timeless_left(void) { return g_tm; }
static int timeless_active(char *board)
{
    return g_timeless && *(unsigned char *)(APP + 0x664) && !board[0x15b];
}

static void timeless_update(char *board)
{
    char *lvl;
    int active = timeless_active(board), i, balls = 0;
    g_hudOn = active;
    if (!active) { g_tmPlaying = 0; return; }
    if (B_I(board, 0xc8) != 1) { g_tmPlaying = 0; return; }   /* not in play */
    lvl = board_level(board);
    if (!g_tmPlaying || board != g_tmBoard || lvl != g_tmLevel) {
        if (g_tmKeep) g_tmKeep = 0; else g_tm = cfg.tlSeconds * 100;
        g_tmPlaying = 1; g_tmBoard = board; g_tmLevel = lvl;
    }
    if (!B_I(board, 0xcc) && g_tm > 0) g_tm--;
    g_hudSecs = g_tm / 100.0f;
    if (g_tm > 0 || B_I(board, 0xcc)) return;
    for (i = 0; i < ncurves(board); i++) { char *c = curve_at(board, i); if (c && B_I(c, 0x34)) balls = 1; }
    if (balls) {
        zlog("timeless: time up", 0, 0);
        G_BoardLose(board);
    }
}

/* the HUD's "SPEED x.xx" branch for Survival, extended: Timeless prints the clock the same way */
__attribute__((naked)) static void H_HudCave(void)
{
    __asm__(
        "cmpb $0, 0x15b(%edi)\n"
        "jne 1f\n"
        "cmpl $0, _g_hudOn\n"
        "jne 2f\n"
        "pushl $0x40d1ec\n"
        "ret\n"
        "1:\n"
        "pushl $0x4fde9e\n"
        "ret\n"
        "2:\n"
        "flds _g_hudSecs\n"
        "subl $8, %esp\n"
        "fstpl (%esp)\n"
        "pushl $_g_hudFmt\n"
        "leal -0x8c(%ebp), %eax\n"
        "pushl %eax\n"
        "movl $0x4895b0, %eax\n"
        "call *%eax\n"
        "addl $0x10, %esp\n"
        "movl $0x10, %edi\n"
        "pushl $0x4fde00\n"
        "ret\n");
}

/* ---- Gauntlet screen */
static void btn_select(char *b, int on)
{
    if (!b) return;
    b[0xbc] = (char)on;
    ((Fn_i)VCALL(b, 0x28))(b, on);
}

static void tl_place(char *screen)
{
    char *surv = B_P(screen, 0x70), *b = (char *)g_tlBtn;
    if (!b || !surv) return;
    /* on the stone ledge between the statue's feet and the Survival slot; the image is one row of 3 cells */
    ((void (__thiscall *)(void *, int, int, int, int))VCALL(b, 0x34))(b, B_I(surv, 8) - 2, B_I(surv, 0xc) - 21, g_tlW, g_tlH);
}

/* the screen's score list: Timeless keeps its own tables */
static void __thiscall H_ScrRefresh(char *screen)
{
    char saved = screen[0x80];
    if (g_timeless) { screen[0x80] = 1; scpy(g_prefix, "t_", sizeof g_prefix); }
    G_ScrRefresh(screen);
    screen[0x80] = saved;
    scpy(g_prefix, "e_", sizeof g_prefix);
}

static void __thiscall H_SaveScores(char *board)
{
    if (timeless_active(board)) {
        board[0x15b] = 1;
        scpy(g_prefix, "t_", sizeof g_prefix);
        G_SaveScores(board);
        board[0x15b] = 0;
        scpy(g_prefix, "e_", sizeof g_prefix);
    } else G_SaveScores(board);
}

static void show_timeless_selected(char *screen)
{
    btn_select(B_P(screen, 0x6c), 0);
    btn_select(B_P(screen, 0x70), 0);
    btn_select((char *)g_tlBtn, 1);
    screen[0x80] = 0;
}

static void __thiscall H_ScrAdded(char *screen, void *wm)
{
    MStr label, p;
    void *im;
    G_ScrAdded(screen, wm);
    mstr(&p, "images/gaunttimelessbutton");
    im = G_GetImage(APP, &p);
    if (!im) { zlog("timeless: button image missing", 0, 0); return; }
    mstr(&label, "");
    g_tlW = B_I(im, 4) / 3; g_tlH = B_I(im, 8);
    g_tlBtn = G_MakeButton(ID_TIMELESS, screen + 0x58, &label, 1, im, 3);
    if (!g_tlBtn) return;
    tl_place(screen);
    G_AddWidget(wm, g_tlBtn);
    if (g_timeless) { show_timeless_selected(screen); H_ScrRefresh(screen); }
}

static void __thiscall H_ScrRemoved(char *screen, void *wm)
{
    if (g_tlBtn) {
        void *b = g_tlBtn;
        g_tlBtn = 0;
        G_RemoveWidget(wm, b);
        ((Fn_p)VCALL(APP, 0x54))(APP, b);     /* SafeDeleteWidget */
    }
    G_ScrRemoved(screen, wm);
}

static void __thiscall H_ScrResize(char *screen, int x, int y, int w, int h)
{
    G_ScrResize(screen, x, y, w, h);
    tl_place(screen);
}

/* ButtonDepress on the screen's button-listener part (screen + 0x58) */
static void __thiscall H_ScrButton(char *listener, int id)
{
    char *screen = listener - 0x58;
    if (id == ID_TIMELESS) {
        timeless_set(1);
        show_timeless_selected(screen);
        H_ScrRefresh(screen);
        return;
    }
    if (id == ID_PRACTICE || id == ID_SURVIVAL) {
        timeless_set(0);
        btn_select((char *)g_tlBtn, 0);
    }
    G_ScrButton(listener, id);
}

static void balls_update(char *board)
{
    int i;
    if (!cfg.loaded) load_cfg();
    if (board != g_bboard) {
        g_bboard = board;
        memset(g_mini, 0, sizeof g_mini); memset(g_pop, 0, sizeof g_pop); memset(g_lastSpawn, 0, sizeof g_lastSpawn);
    }
    timeless_update(board);
    if (B_I(board, 0xcc)) return;          /* paused */
    for (i = 0; i < NPOP; i++) if (g_pop[i].t > 0) { g_pop[i].t--; g_pop[i].y -= 0.35f; }
    if (B_I(board, 0xc8) != 1) {           /* life lost / level over: the little frogs go home */
        memset(g_mini, 0, sizeof g_mini);
        return;
    }
    for (i = 0; i < NMINI; i++) if (g_mini[i].on) mini_update(&g_mini[i], board);
    if (cfg.enabled) spawn_specials(board);
}

/* Board::ActivatePower(ball), called when a ball with a power explodes */
static void __thiscall H_ActivatePower(void *board, char *ball)
{
    int t = ball_power(ball);
    char *curve;
    if (t < PW_RAINBOW || t > PW_FROG) { G_ActivatePower(board, ball); return; }
    if (g_depth > 4) return;
    curve = curve_of((char *)board, ball);
    if (t == PW_SKULL) { if (curve) mini_spawn(curve); return; }
    if (t == PW_FROG) {
        int color = B_I(ball, 8), n = 0, i;
        g_depth++;
        for (i = 0; i < ncurves((char *)board); i++) {
            char *c = curve_at((char *)board, i);
            if (!c) continue;
            FOR_BALLS(c, b) if (b != ball && B_I(b, 8) == color && explode(c, b)) n++;
        }
        g_depth--;
        zlog("balls: frog ball cleared %d balls of colour %d", n, color);
        if (n) {
            G_IncScore(board, n * cfg.frogPts, 1);
            add_pop(B_F(ball, 0x1c), B_F(ball, 0x20) - 20, n * cfg.frogPts);
            play(SOUND_EXPLODE);
        }
    }
}

/* CountGroup(ball, colour, &lastNext, &lastPrev) -> size of the same-coloured run through ball.
 * Rainbow balls match any colour. */
static int count_run(char *b, int color, char **outNext, char **outPrev)
{
    char *x, *last = b, *first = b;
    int n = 1;
    if (B_I(b, 8) != color && !is_rainbow(b)) return 0;
    for (x = G_BallNext(b, 1); x && (B_I(x, 8) == color || is_rainbow(x)); x = G_BallNext(x, 1)) { last = x; n++; }
    for (x = G_BallPrev(b, 1); x && (B_I(x, 8) == color || is_rainbow(x)); x = G_BallPrev(x, 1)) { first = x; n++; }
    if (outNext) *outNext = last;
    if (outPrev) *outPrev = first;
    return n;
}

static int __stdcall H_CountGroup(char *b, int color, char **outNext, char **outPrev)
{
    if (is_rainbow(b)) {
        /* try the colour of the nearest real ball on each side, keep the bigger run */
        char *x, *n1 = 0, *p1 = 0, *n2 = 0, *p2 = 0;
        int c1 = -1, c2 = -1, k1 = 0, k2 = 0;
        for (x = G_BallNext(b, 1); x && is_rainbow(x); x = G_BallNext(x, 1)) ;
        if (x) c1 = B_I(x, 8);
        for (x = G_BallPrev(b, 1); x && is_rainbow(x); x = G_BallPrev(x, 1)) ;
        if (x) c2 = B_I(x, 8);
        if (c1 < 0 && c2 < 0) c1 = B_I(b, 8);
        if (c1 >= 0) k1 = count_run(b, c1, &n1, &p1);
        if (c2 >= 0 && c2 != c1) k2 = count_run(b, c2, &n2, &p2);
        if (k2 > k1) { k1 = k2; n1 = n2; p1 = p2; c1 = c2; }
        if (outNext) *outNext = n1;
        if (outPrev) *outPrev = p1;
        return k1;
    }
    return count_run(b, color, outNext, outPrev);
}

/* Ball::Draw (vtable): draw the plain ball, then our overlay rotated with it */
static void __thiscall H_BallDraw(char *ball, void *g)
{
    int t = B_I(ball, 0x60);
    void *im;
    if (t < PW_RAINBOW || t > PW_FROG) { G_BallDraw(ball, g); return; }
    B_I(ball, 0x60) = 4;
    G_BallDraw(ball, g);
    B_I(ball, 0x60) = t;
    if (B_I(ball, 0x34)) return;           /* exploding */
    im = img(t == PW_RAINBOW ? 0 : t == PW_SKULL ? 1 : 2);
    if (im) {
        int w = B_I(im, 4), h = B_I(im, 8);
        Rect src = { 0, 0, w, h };
        float rot = B_F(ball, 0x10);
        if (t == PW_RAINBOW && g_bboard) rot += (float)(B_I(g_bboard, 0xc4) % 628) * 0.01f;
        G_DrawImageRotF(g, im, B_F(ball, 0x1c) - w / 2.0f, B_F(ball, 0x20) - h / 2.0f, (double)rot, &src);
    }
}

static void __thiscall H_BoardDraw(void *board, void *g)
{
    int i;
    G_BoardDraw(board, g);
    if (board != g_bboard) return;
    for (i = 0; i < NMINI; i++) {
        Mini *m = &g_mini[i];
        void *im = img(3);
        int a = 255, w, h;
        float hop;
        Rect src;
        if (!m->on || !im) continue;
        w = B_I(im, 4); h = B_I(im, 8);
        src.x = 0; src.y = 0; src.w = w; src.h = h;
        if (m->pri) a = 110;                                  /* under a tunnel */
        if (m->state == 1) a = a * (45 - m->t) / 45;
        if (a <= 0) continue;
        hop = m->state == 0 ? (float)((m->t / 3) % 6 < 3 ? (m->t / 3) % 3 : 3 - (m->t / 3) % 3) : -(float)m->t * 0.4f;
        if (a < 255) { G_SetColorize(g, 1); set_color(g, 255, 255, 255, a); }
        G_DrawImageRotF(g, im, m->x - w / 2.0f, m->y - h / 2.0f - hop, (double)m->ang, &src);
        G_SetColorize(g, 0);
    }
    for (i = 0; i < NPOP; i++) {
        Pop *p = &g_pop[i];
        MStr ms; int a;
        if (p->t <= 0 || !FONT_MAIN12) continue;
        a = p->t > 40 ? 255 : p->t * 255 / 40;
        mstr(&ms, p->s);
        G_SetFont(g, FONT_MAIN12);
        set_color(g, 0, 0, 0, a);
        G_DrawString(g, &ms, (int)p->x - text_w(FONT_MAIN12, p->s) / 2 + 1, (int)p->y + 1);
        set_color(g, 255, 230, 90, a);
        G_DrawString(g, &ms, (int)p->x - text_w(FONT_MAIN12, p->s) / 2, (int)p->y);
    }
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
    if (*(unsigned *)0x4fe880 == 0x421ce0 && *(unsigned *)0x4fe890 == 0x401b10) {   /* Board vtable: Update, KeyDown */
        void *u = (void *)H_BoardUpdate, *k = (void *)H_BoardKeyDown;
        patch((void *)0x4fe880, &u, 4);
        patch((void *)0x4fe890, &k, 4);
    } else zlog("frog: board vtable not as expected", 0, 0);
    /* special balls */
    if (*(unsigned *)0x4fe3c0 == 0x403f00 && *(unsigned *)0x4fe87c == 0x40d330 &&
        expect(0x405730, "\x55\x8b\xec\x8b\x45\x0c", 6) && expect(0x4175e8, "\xe8", 1) &&
        0x4175e8 + 5 + *(int *)0x4175e9 == 0x4172f0) {
        void *d = (void *)H_BallDraw, *bd = (void *)H_BoardDraw;
        patch((void *)0x4fe3c0, &d, 4);                         /* Ball vtable: Draw */
        patch((void *)0x4fe87c, &bd, 4);                        /* Board vtable: Draw */
        put_rel(0xE9, 0x405730, (void *)H_CountGroup, 0);       /* CountGroup -> rainbow aware */
        put_rel(0xE8, 0x4175e8, (void *)H_ActivatePower, 0);    /* power ball exploded */
        zlog("balls: hooks installed", 0, 0);
    } else zlog("balls: code not as expected, special balls off", 0, 0);
    /* Timeless mode */
    if (*(unsigned *)0x502b08 == 0x43bfd0 && *(unsigned *)0x502b0c == 0x435250 && *(unsigned *)0x502b38 == 0x435170 &&
        *(unsigned *)0x502af4 == 0x43bf10 && expect(0x4fde90, "\x8a\x87\x5b\x01\x00\x00", 6) &&
        expect(0x439393, "\xba\xb4\x23\x50\x00", 5) && expect(0x42138b, "\x68\x38\xef\x4f\x00", 5) &&
        expect(0x43b712, "\xb8\x38\xef\x4f\x00", 5) && expect(0x43b72a, "\x68\x38\xef\x4f\x00", 5) &&
        expect(0x43bee1, "\xe8", 1) && expect(0x43bf83, "\xe8", 1) && expect(0x421515, "\xe8", 1) && expect(0x43553c, "\xe8", 1)) {
        void *a = (void *)H_ScrAdded, *r = (void *)H_ScrRemoved, *z = (void *)H_ScrResize, *bt = (void *)H_ScrButton;
        void *lbl = g_lblStd, *pre = g_prefix;
        patch((void *)0x502b08, &a, 4);
        patch((void *)0x502b0c, &r, 4);
        patch((void *)0x502b38, &z, 4);
        patch((void *)0x502af4, &bt, 4);
        put_rel(0xE9, 0x4fde90, (void *)H_HudCave, 1);
        patch((void *)0x439394, &lbl, 4);                       /* "Standard" label on the screen */
        patch((void *)0x42138c, &pre, 4);                       /* high score table prefix */
        patch((void *)0x43b713, &pre, 4);
        patch((void *)0x43b72b, &pre, 4);
        put_rel(0xE8, 0x43bee1, (void *)H_ScrRefresh, 0);
        put_rel(0xE8, 0x43bf83, (void *)H_ScrRefresh, 0);
        put_rel(0xE8, 0x421515, (void *)H_SaveScores, 0);
        put_rel(0xE8, 0x43553c, (void *)H_SaveScores, 0);
        zlog("timeless: hooks installed", 0, 0);
    } else zlog("timeless: code not as expected, mode off", 0, 0);
    zlog("install: hooks installed", 0, 0);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) install();
    return TRUE;
}
