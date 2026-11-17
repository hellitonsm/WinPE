/* menu_iniciar.c - Shell moderno e Área de Trabalho para Windows PE (WinPE).
   Contém:
   1. Barra de Tarefas fixa na base estilo Windows 10/11 com botão Iniciar,
      atalhos rápidos com ícones vetoriais modernos e relógio com data.
   2. Menu Iniciar com design escuro Fluent: perfil do usuário, categorias
      laterais, botões de energia estilizados (Desligar / Reiniciar),
      caixa de busca dinâmica com ícone de lupa e grade de aplicativos fixados
      com ícones vetoriais de alta fidelidade desenhados em GDI.
   3. Explorador de Arquivos integrado completamente modernizado no tema escuro:
      barra de navegação completa (Voltar, Avançar, Subir, Atualizar, Início),
      barra de endereços moderna com redimensionamento fluido, filtro dinâmico
      em tempo real, botões de ação rápida (+ Pasta e CMD Aqui), barra lateral
      com unidades e espaço livre, lista com ícones por extensão e detalhes,
      menu de contexto completo com botão direito e barra de status.
*/

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================================================================
   Identificadores de Controlo
   ========================================================================= */

/* Menu Iniciar */
#define ID_SEARCH           100
#define ID_SEARCH_CLEAR     101
#define ID_POWEROFF         200
#define ID_REBOOT           201
#define ID_CLOSE            204
#define TILE_BASE           300

/* Timers */
#define IDT_CLOCK           1
#define IDT_HOVER           2

/* Explorador de Arquivos */
#define IDX_PATH            600
#define IDX_GO              601
#define IDX_UP              602
#define IDX_BACK            603
#define IDX_FWD             604
#define IDX_HOME            605
#define IDX_REFRESH         606
#define IDX_NEWFOLDER       607
#define IDX_CMDHERE         608
#define IDX_FILTER          609
#define IDX_LIST            610
#define IDX_SIDE            611
#define IDX_STATUS          612

/* Menu de Contexto do Explorador */
#define IDM_OPEN            701
#define IDM_OPEN_NOTEPAD    702
#define IDM_CMD_HERE        703
#define IDM_COPY_PATH       704
#define IDM_NEW_FOLDER      705
#define IDM_REFRESH         706

/* Dimensões do Explorador */
#define EXP_TOOLBAR_H       48
#define EXP_HEADER_H        28
#define EXP_SIDE_W          210
#define EXP_STATUS_H        26
#define EXP_ITEM_H          26
#define EXP_ICON_SZ         20

/* Dimensões da Barra de Tarefas e Menu Iniciar */
#define TASKBAR_H           42
#define START_BTN_W         50
#define QUICK_W             42

#define MENU_W              780
#define MENU_H              560
#define MENU_RAIL_W         210
#define MENU_HEADER_H       58
#define MENU_RAIL_ITEM_H    32
#define MENU_POWER_H        64
#define MENU_TILE_W         122
#define MENU_TILE_H         92
#define MENU_TILE_GAP       12
#define MENU_TILES_ROW      4

/* =========================================================================
   Paleta de Cores Moderna (Fluent Dark Theme)
   ========================================================================= */

/* Barra de tarefas */
#define C_TB_BG             RGB(18, 18, 20)
#define C_TB_LINE           RGB(40, 40, 44)
#define C_TB_HOVER          RGB(38, 38, 44)
#define C_TB_PRESS          RGB(48, 48, 56)

/* Menu Iniciar */
#define C_MENU_BG           RGB(22, 24, 30)
#define C_MENU_RAIL         RGB(16, 17, 22)
#define C_MENU_RAIL_HV      RGB(36, 42, 54)
#define C_MENU_BORDER       RGB(42, 46, 58)
#define C_MENU_CARD         RGB(32, 35, 44)
#define C_MENU_CARD_HV      RGB(40, 50, 68)
#define C_MENU_CARD_PRESS   RGB(26, 28, 36)
#define C_MENU_CARD_BORDER  RGB(48, 54, 68)
#define C_MENU_CARD_BORDER_HV RGB(0, 120, 215)
#define C_MENU_SEARCH       RGB(28, 31, 40)
#define C_MENU_SEARCH_BD    RGB(58, 66, 84)
#define C_MENU_PLATE        RGB(26, 29, 38)

/* Explorador */
#define C_EXP_BG            RGB(24, 24, 28)
#define C_EXP_TOOLBAR       RGB(32, 32, 38)
#define C_EXP_SIDEBAR       RGB(20, 20, 24)
#define C_EXP_HEADER        RGB(30, 30, 36)
#define C_EXP_LIST          RGB(24, 24, 28)
#define C_EXP_ITEM_HV       RGB(40, 40, 48)
#define C_EXP_ITEM_SEL      RGB(0, 102, 204)
#define C_EXP_LINE          RGB(48, 48, 56)
#define C_EXP_STATUS        RGB(22, 22, 26)

/* Cores Comuns / Acentos */
#define C_ACCENT            RGB(0, 120, 215)
#define C_ACCENT_HOVER      RGB(30, 144, 255)
#define C_ACCENT_LIGHT      RGB(96, 180, 255)
#define C_TEXT_WHITE        RGB(245, 245, 248)
#define C_TEXT_MUTED        RGB(160, 160, 172)
#define C_TEXT_SUBTLE       RGB(110, 110, 122)
#define C_DANGER            RGB(215, 45, 45)
#define C_DANGER_HOVER      RGB(240, 65, 65)
#define C_WARNING           RGB(220, 150, 0)
#define C_SUCCESS           RGB(16, 148, 64)

/* =========================================================================
   Sistema de Tipografia (Fontes Segoe UI com ClearType)
   ========================================================================= */

static HFONT g_fTitle   = NULL;   /* 16px Semibold */
static HFONT g_fRegular = NULL;   /* 13px Normal */
static HFONT g_fBold    = NULL;   /* 13px Bold */
static HFONT g_fSmall   = NULL;   /* 11px Normal */
static HFONT g_fTiny    = NULL;   /* 10px Normal */
static HFONT g_fClock   = NULL;   /* 16px Bold */
static HFONT g_fMono    = NULL;   /* 12px Consolas */

static HFONT CreateUIFont(const WCHAR *facename, int size, int weight) {
    return CreateFontW(
        size, 0, 0, 0, weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        facename);
}

static void InitFonts(void) {
    g_fTitle   = CreateUIFont(L"Segoe UI", -16, FW_SEMIBOLD);
    g_fRegular = CreateUIFont(L"Segoe UI", -13, FW_NORMAL);
    g_fBold    = CreateUIFont(L"Segoe UI", -13, FW_BOLD);
    g_fSmall   = CreateUIFont(L"Segoe UI", -11, FW_NORMAL);
    g_fTiny    = CreateUIFont(L"Segoe UI", -10, FW_NORMAL);
    g_fClock   = CreateUIFont(L"Segoe UI", -16, FW_BOLD);
    g_fMono    = CreateUIFont(L"Consolas",  -12, FW_NORMAL);

    if (!g_fRegular) g_fRegular = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_fBold)    g_fBold    = g_fRegular;
    if (!g_fTitle)   g_fTitle   = g_fRegular;
    if (!g_fSmall)   g_fSmall   = g_fRegular;
    if (!g_fTiny)    g_fTiny    = g_fRegular;
    if (!g_fClock)   g_fClock   = g_fRegular;
    if (!g_fMono)    g_fMono    = (HFONT)GetStockObject(ANSI_FIXED_FONT);
}

static void CleanupFonts(void) {
    if (g_fTitle && g_fTitle != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fTitle);
    if (g_fRegular && g_fRegular != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fRegular);
    if (g_fBold && g_fBold != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fBold);
    if (g_fSmall && g_fSmall != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fSmall);
    if (g_fTiny && g_fTiny != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fTiny);
    if (g_fClock && g_fClock != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_fClock);
    if (g_fMono && g_fMono != (HFONT)GetStockObject(ANSI_FIXED_FONT)) DeleteObject(g_fMono);
}

/* =========================================================================
   Estruturas de Dados
   ========================================================================= */

typedef struct {
    WCHAR name[MAX_PATH];
    WCHAR path[MAX_PATH];
    WCHAR ext[16];
    DWORD attrs;
    ULARGE_INTEGER size;
    FILETIME ft;
    BOOL isDir;
    BOOL isDrive;
} EXPITEM;

typedef struct {
    WCHAR path[MAX_PATH];
    WCHAR filter[64];
    WCHAR hist[64][MAX_PATH];
    int histCount;
    int histPos;
    EXPITEM *items;
    int nItems, capItems;
    int *filtered;
    int nFiltered;
    BOOL drivesMode;
    EXPITEM *places;
    int nPlaces, capPlaces;
} EXPLORER;

typedef struct {
    const wchar_t *name;
    const wchar_t *desc;
    const wchar_t *cmd;
    const wchar_t *args;
    int iconType;
} APP;

enum {
    ICON_CMD = 1,
    ICON_NOTEPAD,
    ICON_REGEDIT,
    ICON_EXPLORER,
    ICON_TASKMGR,
    ICON_NETWORK,
    ICON_CALC,
    ICON_PAINT,
    ICON_WORDPAD,
    ICON_SETTINGS,
    ICON_CONTROL,
    ICON_PC,
    ICON_KEYBOARD,
    ICON_DISK,
    ICON_DRIVERS,
    ICON_SYSINFO,
    ICON_DISPLAY,
    ICON_PROPERTIES
};

#define CMD_EXPLORER  L"#explorer"
#define FS_ESTEPC     L"shell:EstePC"
#define FS_WINDOWS    L"shell:Windows"
#define FS_SYSTEM32   L"shell:System32"
#define FS_DRIVERS    L"shell:Drivers"
#define FS_DESKTOP    L"shell:Desktop"
#define FS_DOCS       L"shell:Documentos"
#define FS_IMGENS     L"shell:Imagens"

static const APP APPS[] = {
    { L"Prompt de Comando",   L"Terminal WinPE",          L"cmd.exe",      L"",                                             ICON_CMD },
    { L"Bloco de Notas",      L"Editor de texto rápido",   L"notepad.exe",  L"",                                             ICON_NOTEPAD },
    { L"Editor do Registro",  L"Regedit x64",             L"regedit.exe",  L"",                                             ICON_REGEDIT },
    { L"Explorador",          L"Gerenciador de arquivos",  CMD_EXPLORER,    FS_ESTEPC,                                       ICON_EXPLORER },
    { L"Ger. de Tarefas",     L"Monitor de processos",     L"taskmgr.exe",  L"",                                             ICON_TASKMGR },
    { L"Este Computador",     L"Unidades de disco",       CMD_EXPLORER,    FS_ESTEPC,                                       ICON_PC },
    { L"Rede e Conexões",     L"Painel de rede",          L"control.exe",  L"ncpa.cpl",                                     ICON_NETWORK },
    { L"Partições (DiskPart)",L"Gerenciar discos",        L"cmd.exe",      L"/k diskpart",                                  ICON_DISK },
    { L"Calculadora",         L"Utilitário de cálculo",    L"calc.exe",     L"",                                             ICON_CALC },
    { L"Paint",               L"Editor de imagens",        L"mspaint.exe",  L"",                                             ICON_PAINT },
    { L"Wordpad",             L"Editor avançado de texto", L"write.exe",    L"",                                             ICON_WORDPAD },
    { L"Configurações",       L"Configurações do sistema", L"ms-settings:", L"",                                             ICON_SETTINGS },
    { L"Painel de Controle",  L"Ferramentas e opções",    L"control.exe",  L"",                                             ICON_CONTROL },
    { L"Propriedades",        L"Propriedades do sistema",  L"control.exe",  L"sysdm.cpl",                                    ICON_PROPERTIES },
    { L"Resolução e Vídeo",   L"Ajustes de exibição",     L"control.exe",  L"desk.cpl",                                     ICON_DISPLAY },
    { L"Teclado Virtual",     L"Acessibilidade",          L"osk.exe",      L"",                                             ICON_KEYBOARD }
};
#define N_APPS (sizeof(APPS) / sizeof(APPS[0]))

typedef struct {
    const wchar_t *label;
    const wchar_t *cmd;
    const wchar_t *args;
    int iconType;
} FOLDER;

static const FOLDER FOLDERS[] = {
    { L"Este Computador",    CMD_EXPLORER, FS_ESTEPC,    ICON_PC },
    { L"Área de Trabalho",   CMD_EXPLORER, FS_DESKTOP,   ICON_DISPLAY },
    { L"Documentos",         CMD_EXPLORER, FS_DOCS,      ICON_NOTEPAD },
    { L"Downloads",          CMD_EXPLORER, L"shell:Downloads", ICON_EXPLORER },
    { L"Imagens",            CMD_EXPLORER, FS_IMGENS,    ICON_PAINT },
    { L"Windows",            CMD_EXPLORER, FS_WINDOWS,   ICON_SETTINGS },
    { L"System32",           CMD_EXPLORER, FS_SYSTEM32,  ICON_DRIVERS },
    { L"Drivers",            CMD_EXPLORER, FS_DRIVERS,   ICON_DRIVERS },
    { L"Painel de Controle", L"control.exe", L"",        ICON_CONTROL }
};
#define N_FOLDERS (sizeof(FOLDERS) / sizeof(FOLDERS[0]))

typedef struct {
    const wchar_t *name;
    const wchar_t *cmd;
    const wchar_t *args;
    int iconType;
} QUICKAPP;

static const QUICKAPP QUICKAPPS[] = {
    { L"CMD",        L"cmd.exe",     L"",       ICON_CMD },
    { L"Arquivos",   CMD_EXPLORER,   FS_ESTEPC, ICON_EXPLORER },
    { L"Tarefas",    L"taskmgr.exe", L"",       ICON_TASKMGR },
    { L"Bloco Notas",L"notepad.exe", L"",       ICON_NOTEPAD },
    { L"Registro",   L"regedit.exe", L"",       ICON_REGEDIT }
};
#define N_QUICKAPPS (sizeof(QUICKAPPS) / sizeof(QUICKAPPS[0]))

/* =========================================================================
   Variáveis Globais do Shell
   ========================================================================= */

static HWND g_hwndTaskbar = NULL;
static HWND g_hwndMenu    = NULL;
static HHOOK g_hMouseHook = NULL;
static HHOOK g_hKbdHook   = NULL;

static BOOL s_menuVisible           = FALSE;
static BOOL s_startBtnHover         = FALSE;
static BOOL s_startBtnPressed       = FALSE;
static BOOL s_ignoreNextStartClick  = FALSE;
static int  s_quickHover            = -1;

static int  s_tileHover             = -1;
static int  s_tileScroll            = 0;
static int  s_railHover             = -1;
static int  s_powerHover            = -1; /* 1 = poweroff, 2 = reboot */
static WCHAR s_user[128]            = {0};
static WCHAR s_query[128]           = {0};
static WNDPROC s_oldEditProc        = NULL;

/* Protótipos */
static void ToggleStartMenu(BOOL show);
static void RebuildTiles(HWND hwnd);
static void LayoutTiles(HWND hwnd);
static void OpenFileExplorer(const wchar_t *startPath);
static LRESULT CALLBACK ExpWndProc(HWND, UINT, WPARAM, LPARAM);
static void ExpNavigate(HWND hwnd, const WCHAR *path);
static void ExpEnumerate(HWND hwnd);

/* =========================================================================
   Rotinas de Renderização Vetorial GDI (Alta Fidelidade)
   ========================================================================= */

static void FillRoundRect(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, br);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, radius, radius);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
}

static void DrawIconCMD(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.78);
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 6, RGB(12, 12, 14), RGB(60, 60, 68));

    RECT rTitle = { x + 1, y + 1, x + w - 1, y + 6 };
    HBRUSH brT = CreateSolidBrush(RGB(35, 35, 42));
    FillRect(dc, &rTitle, brT);
    DeleteObject(brT);

    HBRUSH brD = CreateSolidBrush(RGB(200, 70, 70));
    RECT rDot = { x + 3, y + 2, x + 5, y + 4 };
    FillRect(dc, &rDot, brD);
    DeleteObject(brD);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 255, 130));
    HFONT oldF = (HFONT)SelectObject(dc, g_fMono);
    RECT rTxt = { x + 4, y + 6, x + w - 2, y + h - 1 };
    DrawTextW(dc, L">_", 2, &rTxt, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SelectObject(dc, oldF);
}

static void DrawIconNotepad(HDC dc, int cx, int cy, int sz) {
    int w = (int)(sz * 0.75), h = sz;
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(0, 120, 215), RGB(0, 90, 170));
    FillRoundRect(dc, x + 3, y + 4, w - 6, h - 8, 3, RGB(255, 255, 255), RGB(220, 220, 220));

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 200, 225));
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    for (int ly = y + 8; ly < y + h - 8; ly += 4) {
        MoveToEx(dc, x + 6, ly, NULL);
        LineTo(dc, x + w - 6, ly);
    }
    SelectObject(dc, oldP);
    DeleteObject(pen);

    HPEN pPen = CreatePen(PS_SOLID, 2, RGB(255, 185, 0));
    oldP = (HPEN)SelectObject(dc, pPen);
    MoveToEx(dc, x + w - 4, y + h - 12, NULL);
    LineTo(dc, x + w - 10, y + h - 4);
    SelectObject(dc, oldP);
    DeleteObject(pPen);
}

static void DrawIconRegedit(HDC dc, int cx, int cy, int sz) {
    int r = sz / 2 - 2;
    FillRoundRect(dc, cx - r, cy - r, r * 2, r * 2, 4, RGB(20, 140, 230), RGB(10, 100, 180));

    HPEN p = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN oldP = (HPEN)SelectObject(dc, p);
    MoveToEx(dc, cx, cy - r, NULL); LineTo(dc, cx, cy + r);
    MoveToEx(dc, cx - r, cy, NULL); LineTo(dc, cx + r, cy);

    FillRoundRect(dc, cx + 2, cy - r - 4, r - 1, r - 1, 2, RGB(0, 210, 255), RGB(255, 255, 255));
    SelectObject(dc, oldP);
    DeleteObject(p);
}

static void DrawIconExplorer(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.82);
    int x = cx - w / 2, y = cy - h / 2;

    HBRUSH brTab = CreateSolidBrush(RGB(200, 140, 0));
    RECT rTab = { x, y + 2, x + (int)(w * 0.45), y + 9 };
    FillRect(dc, &rTab, brTab);
    DeleteObject(brTab);

    HBRUSH brSheet = CreateSolidBrush(RGB(230, 245, 255));
    RECT rSheet = { x + 3, y + 4, x + w - 4, y + 10 };
    FillRect(dc, &rSheet, brSheet);
    DeleteObject(brSheet);

    FillRoundRect(dc, x, y + 7, w, h - 7, 4, RGB(255, 195, 30), RGB(215, 150, 0));

    RECT rGleam = { x + 2, y + 9, x + w - 2, y + 11 };
    HBRUSH brGleam = CreateSolidBrush(RGB(255, 225, 120));
    FillRect(dc, &rGleam, brGleam);
    DeleteObject(brGleam);
}

static void DrawIconTaskmgr(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.85);
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(16, 24, 20), RGB(40, 70, 50));

    HPEN pGrid = CreatePen(PS_DOT, 1, RGB(25, 55, 35));
    HPEN oldP = (HPEN)SelectObject(dc, pGrid);
    MoveToEx(dc, x + 2, cy, NULL); LineTo(dc, x + w - 2, cy);
    MoveToEx(dc, cx, y + 2, NULL); LineTo(dc, cx, y + h - 2);

    HPEN pGraph = CreatePen(PS_SOLID, 2, RGB(0, 255, 128));
    SelectObject(dc, pGraph);
    int my = cy;
    MoveToEx(dc, x + 3, my, NULL);
    LineTo(dc, x + 7, my);
    LineTo(dc, x + 10, y + 4);
    LineTo(dc, x + 15, y + h - 5);
    LineTo(dc, x + 19, my - 3);
    LineTo(dc, x + 22, my);
    LineTo(dc, x + w - 3, my);

    SelectObject(dc, oldP);
    DeleteObject(pGrid);
    DeleteObject(pGraph);
}

static void DrawIconNetwork(HDC dc, int cx, int cy, int sz) {
    int r = sz / 2 - 2;
    HBRUSH br = CreateSolidBrush(RGB(0, 130, 220));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 90, 170));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, br);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);

    HPEN pLines = CreatePen(PS_SOLID, 1, RGB(180, 230, 255));
    SelectObject(dc, pLines);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - r / 2, cy - r, cx + r / 2, cy + r);
    MoveToEx(dc, cx - r, cy, NULL); LineTo(dc, cx + r, cy);

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
    DeleteObject(pLines);
}

static void DrawIconCalc(HDC dc, int cx, int cy, int sz) {
    int w = (int)(sz * 0.75), h = sz;
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(45, 50, 60), RGB(70, 80, 95));
    FillRoundRect(dc, x + 3, y + 4, w - 6, 7, 2, RGB(180, 220, 200), RGB(130, 160, 140));

    HBRUSH brBtn = CreateSolidBrush(RGB(80, 90, 105));
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 2; col++) {
            RECT r = { x + 4 + col * ((w - 10) / 2 + 2),
                       y + 14 + row * ((h - 18) / 3),
                       x + 4 + col * ((w - 10) / 2 + 2) + (w - 12) / 2,
                       y + 14 + row * ((h - 18) / 3) + (h - 22) / 3 };
            FillRect(dc, &r, brBtn);
        }
    }
    DeleteObject(brBtn);
}

static void DrawIconPaint(HDC dc, int cx, int cy, int sz) {
    int r = sz / 2 - 2;
    HBRUSH brPal = CreateSolidBrush(RGB(235, 215, 180));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(190, 160, 120));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, brPal);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, cx - r, cy - r, cx + r, cy + r, r, r);

    HBRUSH bRed = CreateSolidBrush(RGB(230, 40, 40));
    HBRUSH bBlue = CreateSolidBrush(RGB(0, 120, 230));
    HBRUSH bYel = CreateSolidBrush(RGB(255, 200, 0));
    HBRUSH bGrn = CreateSolidBrush(RGB(40, 180, 60));

    RECT r1 = { cx - 6, cy - 8, cx - 2, cy - 4 }; FillRect(dc, &r1, bRed);
    RECT r2 = { cx + 2, cy - 6, cx + 6, cy - 2 }; FillRect(dc, &r2, bBlue);
    RECT r3 = { cx - 7, cy + 1, cx - 3, cy + 5 }; FillRect(dc, &r3, bYel);
    RECT r4 = { cx,     cy + 3, cx + 4, cy + 7 }; FillRect(dc, &r4, bGrn);

    DeleteObject(bRed); DeleteObject(bBlue); DeleteObject(bYel); DeleteObject(bGrn);
    SelectObject(dc, oldB); SelectObject(dc, oldP);
    DeleteObject(brPal); DeleteObject(pen);
}

static void DrawIconWordpad(HDC dc, int cx, int cy, int sz) {
    int w = (int)(sz * 0.75), h = sz;
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(255, 255, 255), RGB(180, 190, 205));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 102, 204));
    HFONT oldF = (HFONT)SelectObject(dc, g_fBold);
    RECT rA = { x + 3, y + 2, x + w - 3, y + 14 };
    DrawTextW(dc, L"A", 1, &rA, DT_CENTER | DT_SINGLELINE);
    SelectObject(dc, oldF);

    HPEN p = CreatePen(PS_SOLID, 1, RGB(160, 175, 195));
    HPEN oldP = (HPEN)SelectObject(dc, p);
    for (int ly = y + 16; ly < y + h - 5; ly += 4) {
        MoveToEx(dc, x + 4, ly, NULL);
        LineTo(dc, x + w - 4, ly);
    }
    SelectObject(dc, oldP);
    DeleteObject(p);
}

static void DrawIconSettings(HDC dc, int cx, int cy, int sz) {
    int r = sz / 2 - 2;
    HBRUSH br = CreateSolidBrush(RGB(140, 155, 175));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 115, 135));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, br);
    HPEN oldP = (HPEN)SelectObject(dc, pen);

    RoundRect(dc, cx - 4, cy - r, cx + 4, cy + r, 2, 2);
    RoundRect(dc, cx - r, cy - 4, cx + r, cy + 4, 2, 2);

    int rBody = (int)(r * 0.75);
    Ellipse(dc, cx - rBody, cy - rBody, cx + rBody, cy + rBody);

    HBRUSH brHole = CreateSolidBrush(C_MENU_CARD);
    SelectObject(dc, brHole);
    int rHole = (int)(r * 0.35);
    Ellipse(dc, cx - rHole, cy - rHole, cx + rHole, cy + rHole);

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
    DeleteObject(brHole);
}

static void DrawIconControl(HDC dc, int cx, int cy, int sz) {
    int r = sz / 2 - 2;
    FillRoundRect(dc, cx - r, cy - r, r * 2, r * 2, 6, RGB(0, 120, 215), RGB(0, 90, 170));

    HPEN p = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HPEN oldP = (HPEN)SelectObject(dc, p);
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, br);

    for (int i = 0; i < 3; i++) {
        int ly = cy - 5 + i * 5;
        MoveToEx(dc, cx - 8, ly, NULL);
        LineTo(dc, cx + 8, ly);
        int tx = cx - 4 + ((i * 5) % 9);
        Ellipse(dc, tx - 2, ly - 2, tx + 2, ly + 2);
    }

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(p);
    DeleteObject(br);
}

static void DrawIconDisplay(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.62);
    int x = cx - w / 2, y = cy - h / 2 - 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(28, 36, 48), RGB(80, 110, 150));

    HBRUSH brSky = CreateSolidBrush(RGB(64, 156, 255));
    RECT rScr = { x + 3, y + 3, x + w - 3, y + h - 3 };
    FillRect(dc, &rScr, brSky);
    DeleteObject(brSky);

    HBRUSH brHill = CreateSolidBrush(RGB(36, 170, 110));
    RECT rHill = { x + 3, y + h - 8, x + w - 3, y + h - 3 };
    FillRect(dc, &rHill, brHill);
    DeleteObject(brHill);

    HBRUSH brBase = CreateSolidBrush(RGB(168, 178, 194));
    RECT rStand = { cx - 2, y + h, cx + 2, y + h + 3 };
    RECT rFoot  = { cx - 8, y + h + 3, cx + 8, y + h + 5 };
    FillRect(dc, &rStand, brBase);
    FillRect(dc, &rFoot,  brBase);
    DeleteObject(brBase);
}

static void DrawIconProperties(HDC dc, int cx, int cy, int sz) {
    int w = (int)(sz * 0.72), h = sz;
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(246, 247, 250), RGB(168, 178, 194));

    HPEN pLine = CreatePen(PS_SOLID, 1, RGB(190, 200, 214));
    HPEN oldP = (HPEN)SelectObject(dc, pLine);
    for (int ly = y + 6; ly < y + 14; ly += 3) {
        MoveToEx(dc, x + 4, ly, NULL);
        LineTo(dc, x + w - 4, ly);
    }
    SelectObject(dc, oldP);
    DeleteObject(pLine);

    int r = sz / 5;
    if (r < 4) r = 4;
    int bx = x + w / 2, by = y + (h * 2) / 3;
    HBRUSH br = CreateSolidBrush(RGB(0, 120, 215));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 90, 170));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, br);
    oldP = (HPEN)SelectObject(dc, pen);
    Ellipse(dc, bx - r, by - r, bx + r, by + r);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(br);
    DeleteObject(pen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT oldF = (HFONT)SelectObject(dc, g_fBold);
    RECT ri = { bx - r, by - r - 1, bx + r, by + r };
    DrawTextW(dc, L"i", 1, &ri, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldF);
}

static void DrawIconPC(HDC dc, int cx, int cy, int sz) {
    int w = (int)(sz * 0.85), h = (int)(sz * 0.65);
    int x = cx - w / 2, y = cy - h / 2 - 3;

    FillRoundRect(dc, x, y, w, h, 4, RGB(45, 50, 60), RGB(80, 90, 105));

    RECT rScr = { x + 2, y + 2, x + w - 2, y + h - 2 };
    HBRUSH brScr = CreateSolidBrush(RGB(0, 120, 215));
    FillRect(dc, &rScr, brScr);
    DeleteObject(brScr);

    HBRUSH brBase = CreateSolidBrush(RGB(140, 150, 165));
    RECT rStand = { cx - 2, y + h, cx + 2, y + h + 3 };
    RECT rFoot  = { cx - 7, y + h + 3, cx + 7, y + h + 5 };
    FillRect(dc, &rStand, brBase);
    FillRect(dc, &rFoot,  brBase);
    DeleteObject(brBase);
}

static void DrawIconKeyboard(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.6);
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(45, 50, 60), RGB(80, 90, 105));

    HBRUSH brKey = CreateSolidBrush(RGB(200, 210, 225));
    for (int kx = x + 3; kx < x + w - 4; kx += 4) {
        RECT r = { kx, y + 3, kx + 3, y + 5 };
        FillRect(dc, &r, brKey);
    }
    for (int kx = x + 4; kx < x + w - 4; kx += 4) {
        RECT r = { kx, y + 7, kx + 3, y + 9 };
        FillRect(dc, &r, brKey);
    }
    RECT rSpace = { cx - 6, y + 10, cx + 6, y + 12 };
    FillRect(dc, &rSpace, brKey);
    DeleteObject(brKey);
}

static void DrawIconDisk(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.7);
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 4, RGB(70, 75, 85), RGB(120, 130, 145));

    HBRUSH brP = CreateSolidBrush(RGB(180, 190, 205));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, brP);
    Ellipse(dc, cx - 6, cy - 6, cx + 6, cy + 6);
    SelectObject(dc, oldB);
    DeleteObject(brP);

    HBRUSH brLed = CreateSolidBrush(RGB(0, 255, 128));
    RECT rLed = { x + w - 6, y + h - 5, x + w - 3, y + h - 2 };
    FillRect(dc, &rLed, brLed);
    DeleteObject(brLed);
}

static void DrawIconDrivers(HDC dc, int cx, int cy, int sz) {
    int w = sz, h = (int)(sz * 0.75);
    int x = cx - w / 2, y = cy - h / 2;

    FillRoundRect(dc, x, y, w, h, 3, RGB(20, 110, 50), RGB(10, 70, 30));
    FillRoundRect(dc, cx - 5, cy - 5, 10, 10, 2, RGB(20, 20, 24), RGB(50, 50, 60));

    HBRUSH brGold = CreateSolidBrush(RGB(255, 200, 0));
    for (int px = x + 3; px < x + w - 3; px += 4) {
        RECT rPin = { px, y + h - 3, px + 2, y + h };
        FillRect(dc, &rPin, brGold);
    }
    DeleteObject(brGold);
}

static void DrawAppIcon(HDC dc, int type, int cx, int cy, int sz) {
    switch (type) {
    case ICON_CMD:        DrawIconCMD(dc, cx, cy, sz); break;
    case ICON_NOTEPAD:    DrawIconNotepad(dc, cx, cy, sz); break;
    case ICON_REGEDIT:    DrawIconRegedit(dc, cx, cy, sz); break;
    case ICON_EXPLORER:   DrawIconExplorer(dc, cx, cy, sz); break;
    case ICON_TASKMGR:    DrawIconTaskmgr(dc, cx, cy, sz); break;
    case ICON_NETWORK:    DrawIconNetwork(dc, cx, cy, sz); break;
    case ICON_CALC:       DrawIconCalc(dc, cx, cy, sz); break;
    case ICON_PAINT:      DrawIconPaint(dc, cx, cy, sz); break;
    case ICON_WORDPAD:    DrawIconWordpad(dc, cx, cy, sz); break;
    case ICON_SETTINGS:   DrawIconSettings(dc, cx, cy, sz); break;
    case ICON_CONTROL:    DrawIconControl(dc, cx, cy, sz); break;
    case ICON_PC:         DrawIconPC(dc, cx, cy, sz); break;
    case ICON_KEYBOARD:   DrawIconKeyboard(dc, cx, cy, sz); break;
    case ICON_DISK:       DrawIconDisk(dc, cx, cy, sz); break;
    case ICON_DRIVERS:    DrawIconDrivers(dc, cx, cy, sz); break;
    case ICON_DISPLAY:    DrawIconDisplay(dc, cx, cy, sz); break;
    case ICON_PROPERTIES: DrawIconProperties(dc, cx, cy, sz); break;
    case ICON_SYSINFO:    DrawIconProperties(dc, cx, cy, sz); break;
    default:
        DrawIconExplorer(dc, cx, cy, sz);
        break;
    }
}

static void DrawIconPower(HDC dc, int cx, int cy, int r, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    Arc(dc, cx - r, cy - r, cx + r, cy + r, cx + (int)(r * 0.7), cy - (int)(r * 0.7), cx - (int)(r * 0.7), cy - (int)(r * 0.7));
    MoveToEx(dc, cx, cy - r - 2, NULL);
    LineTo(dc, cx, cy);

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(pen);
}

static void DrawIconRestart(HDC dc, int cx, int cy, int r, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    Arc(dc, cx - r, cy - r, cx + r, cy + r, cx + r, cy, cx, cy - r);

    POINT tri[3];
    tri[0].x = cx - 3; tri[0].y = cy - r - 4;
    tri[1].x = cx + 5; tri[1].y = cy - r;
    tri[2].x = cx - 2; tri[2].y = cy - r + 4;

    HBRUSH br = CreateSolidBrush(color);
    HBRUSH prev = (HBRUSH)SelectObject(dc, br);
    Polygon(dc, tri, 3);

    SelectObject(dc, prev);
    DeleteObject(br);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(pen);
}

static void DrawNavArrow(HDC dc, int cx, int cy, int dir, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldP = (HPEN)SelectObject(dc, pen);

    if (dir == 0) {
        MoveToEx(dc, cx + 4, cy - 5, NULL);
        LineTo(dc, cx - 2, cy);
        LineTo(dc, cx + 4, cy + 5);
    } else if (dir == 1) {
        MoveToEx(dc, cx - 4, cy - 5, NULL);
        LineTo(dc, cx + 2, cy);
        LineTo(dc, cx - 4, cy + 5);
    } else if (dir == 2) {
        MoveToEx(dc, cx - 5, cy + 3, NULL);
        LineTo(dc, cx, cy - 3);
        LineTo(dc, cx + 5, cy + 3);
    } else {
        DrawIconRestart(dc, cx, cy, 6, color);
    }

    SelectObject(dc, oldP);
    DeleteObject(pen);
}

static void DrawIconSearch(HDC dc, int cx, int cy, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldP = (HPEN)SelectObject(dc, pen);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    Ellipse(dc, cx - 5, cy - 5, cx + 3, cy + 3);
    MoveToEx(dc, cx + 2, cy + 2, NULL);
    LineTo(dc, cx + 7, cy + 7);

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(pen);
}

static void DrawFileIconByExt(HDC dc, int x, int y, int sz, BOOL isDir, BOOL isDrive, const WCHAR *ext) {
    int cx = x + sz / 2, cy = y + sz / 2;
    if (isDir) {
        DrawIconExplorer(dc, cx, cy, sz);
        return;
    }
    if (isDrive) {
        DrawIconDisk(dc, cx, cy, sz);
        return;
    }

    int w = sz - 4, h = sz - 1;
    int fx = x + 2, fy = y;
    FillRoundRect(dc, fx, fy, w, h, 3, RGB(250, 250, 252), RGB(180, 190, 200));

    POINT tri[3];
    tri[0].x = fx + w - 5; tri[0].y = fy;
    tri[1].x = fx + w;     tri[1].y = fy + 5;
    tri[2].x = fx + w - 5; tri[2].y = fy + 5;
    HBRUSH brF = CreateSolidBrush(RGB(200, 210, 220));
    HBRUSH oldB = (HBRUSH)SelectObject(dc, brF);
    Polygon(dc, tri, 3);
    SelectObject(dc, oldB);
    DeleteObject(brF);

    if (ext && ext[0]) {
        COLORREF bgC = RGB(100, 115, 135);
        if (!_wcsicmp(ext, L"exe") || !_wcsicmp(ext, L"com")) bgC = RGB(0, 120, 215);
        else if (!_wcsicmp(ext, L"bat") || !_wcsicmp(ext, L"cmd") || !_wcsicmp(ext, L"ps1")) bgC = RGB(20, 20, 20);
        else if (!_wcsicmp(ext, L"dll") || !_wcsicmp(ext, L"sys")) bgC = RGB(140, 50, 50);
        else if (!_wcsicmp(ext, L"wim") || !_wcsicmp(ext, L"iso") || !_wcsicmp(ext, L"zip")) bgC = RGB(200, 130, 0);
        else if (!_wcsicmp(ext, L"txt") || !_wcsicmp(ext, L"log") || !_wcsicmp(ext, L"ini")) bgC = RGB(70, 70, 80);

        RECT rB = { fx + 1, fy + h - 8, fx + w - 1, fy + h };
        HBRUSH brB = CreateSolidBrush(bgC);
        FillRect(dc, &rB, brB);
        DeleteObject(brB);
    }
}

/* =========================================================================
   Controlo de Execução e Auxiliares
   ========================================================================= */

static void GetUserNameStr(WCHAR *buf, int cch) {
    DWORD n = (DWORD)cch;
    if (!GetUserNameW(buf, &n)) {
        lstrcpyW(buf, L"Administrador");
    }
}

static BOOL ExpPathExists(const WCHAR *p) {
    return p && p[0] && GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static BOOL ExpPathIsDir(const WCHAR *p) {
    DWORD a = p ? GetFileAttributesW(p) : INVALID_FILE_ATTRIBUTES;
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static BOOL ExpResolveStart(const wchar_t *arg, WCHAR *out, int cch) {
    out[0] = 0;
    if (!arg || !arg[0]) return FALSE;

    if (!_wcsnicmp(arg, L"shell:", 6)) {
        const WCHAR *k = arg + 6;
        WCHAR winDir[MAX_PATH];
        GetWindowsDirectoryW(winDir, MAX_PATH);

        if (!lstrcmpiW(k, L"EstePC")) return FALSE;
        if (!lstrcmpiW(k, L"Windows")) { lstrcpynW(out, winDir, cch); return TRUE; }
        if (!lstrcmpiW(k, L"System32")) { wsprintfW(out, L"%s\\System32", winDir); return TRUE; }
        if (!lstrcmpiW(k, L"Drivers")) { wsprintfW(out, L"%s\\System32\\Drivers", winDir); return TRUE; }

        if (!lstrcmpiW(k, L"Desktop")) {
            return SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, out) == S_OK && out[0];
        }
        if (!lstrcmpiW(k, L"Documentos")) {
            return SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, out) == S_OK && out[0];
        }
        if (!lstrcmpiW(k, L"Imagens")) {
            return SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, 0, out) == S_OK && out[0];
        }
        if (!lstrcmpiW(k, L"Downloads")) {
            return SHGetFolderPathW(NULL, 0x000c /* CSIDL_DOWNLOADS */, NULL, 0, out) == S_OK && out[0];
        }
        return FALSE;
    }

    if (ExpPathExists(arg)) {
        lstrcpynW(out, arg, cch);
        return TRUE;
    }
    return FALSE;
}

static void Launch(const wchar_t *cmd, const wchar_t *args) {
    if (cmd && wcscmp(cmd, CMD_EXPLORER) == 0) {
        WCHAR start[MAX_PATH] = {0};
        if (ExpResolveStart(args, start, MAX_PATH)) OpenFileExplorer(start);
        else OpenFileExplorer(NULL);
        ToggleStartMenu(FALSE);
        return;
    }
    ShellExecuteW(NULL, L"open", cmd, (args && args[0]) ? args : NULL, NULL, SW_SHOWNORMAL);
    ToggleStartMenu(FALSE);
}

static void DoPower(int id) {
    if (id == ID_POWEROFF) {
        ShellExecuteW(NULL, L"open", L"wpeutil.exe", L"shutdown", NULL, SW_HIDE);
        ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0);
    } else if (id == ID_REBOOT) {
        ShellExecuteW(NULL, L"open", L"wpeutil.exe", L"reboot", NULL, SW_HIDE);
        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
    }
}

/* =========================================================================
   Hooks de Mouse e Teclado
   ========================================================================= */

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_menuVisible) {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_NCLBUTTONDOWN) {
            MSLLHOOKSTRUCT *ms = (MSLLHOOKSTRUCT *)lParam;
            POINT pt = ms->pt;
            RECT rcMenu, rcTb, rcBtn;
            GetWindowRect(g_hwndMenu, &rcMenu);
            GetWindowRect(g_hwndTaskbar, &rcTb);
            rcBtn.left = rcTb.left;
            rcBtn.top = rcTb.top;
            rcBtn.right = rcTb.left + START_BTN_W;
            rcBtn.bottom = rcTb.bottom;

            if (!PtInRect(&rcMenu, pt)) {
                if (PtInRect(&rcBtn, pt)) {
                    s_ignoreNextStartClick = TRUE;
                }
                ToggleStartMenu(FALSE);
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

static BOOL g_kCtrlDown = FALSE;
static BOOL g_kWinDown  = FALSE;
static BOOL g_kWinChord = FALSE;

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
        DWORD vk = kb->vkCode;
        BOOL down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
            g_kCtrlDown = down;
        } else if (vk == VK_LWIN || vk == VK_RWIN) {
            if (down) {
                if (!g_kWinDown) { g_kWinDown = TRUE; g_kWinChord = FALSE; }
            } else if (g_kWinDown) {
                g_kWinDown = FALSE;
                if (!g_kWinChord) {
                    ToggleStartMenu(!s_menuVisible);
                }
            }
        } else if (down && g_kWinDown) {
            g_kWinChord = TRUE;
        }

        if (down && vk == VK_ESCAPE && g_kCtrlDown) {
            ToggleStartMenu(!s_menuVisible);
            return 1;
        }
    }
    return CallNextHookEx(g_hKbdHook, nCode, wParam, lParam);
}

/* =========================================================================
   Controlo do Menu Iniciar
   ========================================================================= */

static void ToggleStartMenu(BOOL show) {
    if (show == s_menuVisible) return;
    s_menuVisible = show;

    if (s_menuVisible) {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int menuW = (MENU_W > screenW - 20) ? (screenW - 20) : MENU_W;
        int menuH = (MENU_H > screenH - TASKBAR_H - 10) ? (screenH - TASKBAR_H - 10) : MENU_H;
        int menuX = 0;
        int menuY = screenH - TASKBAR_H - menuH;

        SetWindowPos(g_hwndMenu, HWND_TOPMOST, menuX, menuY, menuW, menuH, SWP_SHOWWINDOW);
        ShowWindow(g_hwndMenu, SW_SHOW);
        SetForegroundWindow(g_hwndMenu);
        SetFocus(GetDlgItem(g_hwndMenu, ID_SEARCH));
        InvalidateRect(g_hwndMenu, NULL, TRUE);
        UpdateWindow(g_hwndMenu);

        if (!g_hMouseHook) {
            g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(NULL), 0);
        }
    } else {
        if (g_hMouseHook) {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }
        ShowWindow(g_hwndMenu, SW_HIDE);
        s_query[0] = 0;
        SetDlgItemTextW(g_hwndMenu, ID_SEARCH, L"");
        RebuildTiles(g_hwndMenu);
    }

    if (g_hwndTaskbar) {
        InvalidateRect(g_hwndTaskbar, NULL, FALSE);
        UpdateWindow(g_hwndTaskbar);
    }
}

static BOOL AppMatches(const APP *a) {
    if (!s_query[0]) return TRUE;
    WCHAR tmp[128], q[128];
    lstrcpynW(tmp, a->name, 128);
    CharLowerW(tmp);
    lstrcpynW(q, s_query, 128);
    CharLowerW(q);
    return wcsstr(tmp, q) != NULL;
}

static void RebuildTiles(HWND hwnd) {
    for (int i = 0; i < (int)N_APPS; i++) {
        HWND h = GetDlgItem(hwnd, TILE_BASE + i);
        if (h) DestroyWindow(h);
    }

    int tx = MENU_RAIL_W + 24;
    int ty = MENU_HEADER_H + 46;
    int visible = 0;
    int viewBottom = MENU_H - 16;

    for (int i = 0; i < (int)N_APPS; i++) {
        if (!AppMatches(&APPS[i])) continue;
        int vx = tx + (visible % MENU_TILES_ROW) * (MENU_TILE_W + MENU_TILE_GAP);
        int vy = ty + (visible / MENU_TILES_ROW) * (MENU_TILE_H + MENU_TILE_GAP);
        BOOL show = (vy + MENU_TILE_H >= ty && vy <= viewBottom);

        HWND ht = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | (show ? WS_VISIBLE : 0) | BS_OWNERDRAW,
            vx, vy, MENU_TILE_W, MENU_TILE_H,
            hwnd, (HMENU)(UINT_PTR)(TILE_BASE + visible),
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

        SetWindowLongPtrW(ht, GWLP_USERDATA, (LONG_PTR)i);
        visible++;
    }
    s_tileHover = -1;
    s_tileScroll = 0;
}

static void LayoutTiles(HWND hwnd) {
    int tx = MENU_RAIL_W + 24;
    int ty = MENU_HEADER_H + 46;
    int visible = 0;
    int viewBottom = MENU_H - 16;

    for (int i = 0; i < (int)N_APPS; i++) {
        if (!AppMatches(&APPS[i])) continue;
        HWND ht = GetDlgItem(hwnd, TILE_BASE + visible);
        if (ht) {
            int col = visible % MENU_TILES_ROW;
            int row = visible / MENU_TILES_ROW;
            int vx = tx + col * (MENU_TILE_W + MENU_TILE_GAP);
            int vy = ty + row * (MENU_TILE_H + MENU_TILE_GAP) - s_tileScroll;
            if (vy + MENU_TILE_H < ty || vy > viewBottom) {
                ShowWindow(ht, SW_HIDE);
            } else {
                MoveWindow(ht, vx, vy, MENU_TILE_W, MENU_TILE_H, TRUE);
                ShowWindow(ht, SW_SHOW);
            }
        }
        visible++;
    }
}

static int MaxTileScroll(void) {
    int n = 0;
    for (int i = 0; i < (int)N_APPS; i++) if (AppMatches(&APPS[i])) n++;
    int rows = (n + MENU_TILES_ROW - 1) / MENU_TILES_ROW;
    int content = rows * (MENU_TILE_H + MENU_TILE_GAP);
    int view = MENU_H - (MENU_HEADER_H + 46) - 16;
    int max = content - view;
    return max > 0 ? max : 0;
}

static LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            for (int i = 0; i < (int)N_APPS; i++) {
                if (AppMatches(&APPS[i])) {
                    Launch(APPS[i].cmd, APPS[i].args);
                    break;
                }
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            ToggleStartMenu(FALSE);
            return 0;
        }
    }
    return CallWindowProcW(s_oldEditProc, hwnd, msg, wParam, lParam);
}

static void DrawTile(HDC dc, const APP *a, int x, int y, int w, int h, BOOL hot) {
    COLORREF bgCol = hot ? C_MENU_CARD_HV : C_MENU_CARD;
    COLORREF borderCol = hot ? C_MENU_CARD_BORDER_HV : C_MENU_CARD_BORDER;

    FillRoundRect(dc, x, y, w, h, 10, bgCol, borderCol);

    if (hot) {
        RECT accent = { x + 14, y + 1, x + w - 14, y + 3 };
        HBRUSH brA = CreateSolidBrush(C_ACCENT);
        FillRect(dc, &accent, brA);
        DeleteObject(brA);
    }

    int plate = 40;
    int px = x + (w - plate) / 2;
    int py = y + 8;
    FillRoundRect(dc, px, py, plate, plate, 12,
                  hot ? RGB(30, 40, 58) : C_MENU_PLATE,
                  hot ? RGB(30, 40, 58) : C_MENU_PLATE);
    DrawAppIcon(dc, a->iconType, x + w / 2, py + plate / 2, 26);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, hot ? C_TEXT_WHITE : RGB(230, 232, 238));
    HFONT oldF = (HFONT)SelectObject(dc, g_fSmall);
    RECT rcText = { x + 6, y + 50, x + w - 6, y + h - 4 };
    DrawTextW(dc, a->name, -1, &rcText, DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    SelectObject(dc, oldF);
}

static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        int sw = MENU_W - MENU_RAIL_W - 80;
        HWND hSearch = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            MENU_RAIL_W + 56, 18, sw - 40, 24,
            hwnd, (HMENU)(UINT_PTR)ID_SEARCH,
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

        if (hSearch) {
            SendMessageW(hSearch, WM_SETFONT, (WPARAM)g_fRegular, TRUE);
            SendMessageW(hSearch, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Pesquisar aplicativos");
            s_oldEditProc = (WNDPROC)SetWindowLongPtrW(hSearch, GWLP_WNDPROC, (LONG_PTR)SearchEditProc);
        }

        CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            MENU_W - 40, 16, 26, 26,
            hwnd, (HMENU)(UINT_PTR)ID_CLOSE,
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

        RebuildTiles(hwnd);

        int pw = (MENU_RAIL_W - 28) / 2;
        CreateWindowW(L"BUTTON", L"Desligar",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            12, MENU_H - 48, pw, 34,
            hwnd, (HMENU)(UINT_PTR)ID_POWEROFF,
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

        CreateWindowW(L"BUTTON", L"Reiniciar",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            12 + pw + 8, MENU_H - 48, pw, 34,
            hwnd, (HMENU)(UINT_PTR)ID_REBOOT,
            (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

        GetUserNameStr(s_user, 128);
        SetTimer(hwnd, IDT_HOVER, 40, NULL);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC winDC = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC hdc = CreateCompatibleDC(winDC);
        HBITMAP memBM = CreateCompatibleBitmap(winDC, rc.right > 0 ? rc.right : 1, rc.bottom > 0 ? rc.bottom : 1);
        HBITMAP oldBM = (HBITMAP)SelectObject(hdc, memBM);

        /* Fundo do Menu Iniciar */
        HBRUSH bg = CreateSolidBrush(C_MENU_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        /* Barra Lateral Esquerda (Fluent Sidebar) */
        RECT rRail = { 0, 0, MENU_RAIL_W, rc.bottom };
        HBRUSH brRail = CreateSolidBrush(C_MENU_RAIL);
        FillRect(hdc, &rRail, brRail);
        DeleteObject(brRail);

        /* Borda externa e faixa de acento no topo */
        HPEN borderPen = CreatePen(PS_SOLID, 1, C_MENU_BORDER);
        HPEN oldP = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);

        MoveToEx(hdc, MENU_RAIL_W, 0, NULL);
        LineTo(hdc, MENU_RAIL_W, rc.bottom);
        SelectObject(hdc, oldP);
        SelectObject(hdc, oldB);
        DeleteObject(borderPen);

        RECT rAccent = { 1, 1, rc.right - 1, 3 };
        HBRUSH brAccent = CreateSolidBrush(C_ACCENT);
        FillRect(hdc, &rAccent, brAccent);
        DeleteObject(brAccent);

        /* Cartão do perfil */
        FillRoundRect(hdc, 10, 12, MENU_RAIL_W - 20, 44, 10, RGB(28, 32, 42), RGB(46, 52, 66));

        int avR = 14, avX = 30, avY = 34;
        HBRUSH brRing = CreateSolidBrush(RGB(120, 176, 255));
        HBRUSH oldAv = (HBRUSH)SelectObject(hdc, brRing);
        Ellipse(hdc, avX - avR - 2, avY - avR - 2, avX + avR + 2, avY + avR + 2);
        SelectObject(hdc, oldAv);
        DeleteObject(brRing);

        HBRUSH brAv = CreateSolidBrush(C_ACCENT);
        oldAv = (HBRUSH)SelectObject(hdc, brAv);
        Ellipse(hdc, avX - avR, avY - avR, avX + avR, avY + avR);
        SelectObject(hdc, oldAv);
        DeleteObject(brAv);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT oldF = (HFONT)SelectObject(hdc, g_fBold);
        WCHAR initial[2] = { s_user[0] ? s_user[0] : L'A', 0 };
        RECT rInit = { avX - avR, avY - avR, avX + avR, avY + avR };
        DrawTextW(hdc, initial, 1, &rInit, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        /* Nome de Usuário e Subtítulo */
        SelectObject(hdc, g_fRegular);
        SetTextColor(hdc, C_TEXT_WHITE);
        RECT rUser = { avX + avR + 10, 18, MENU_RAIL_W - 16, 34 };
        DrawTextW(hdc, s_user, -1, &rUser, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        SelectObject(hdc, g_fSmall);
        SetTextColor(hdc, C_TEXT_MUTED);
        RECT rPE = { avX + avR + 10, 34, MENU_RAIL_W - 16, 50 };
        DrawTextW(hdc, L"Windows PE x64", -1, &rPE, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        HPEN sep = CreatePen(PS_SOLID, 1, C_MENU_BORDER);
        oldP = (HPEN)SelectObject(hdc, sep);

        SetTextColor(hdc, C_TEXT_MUTED);
        SelectObject(hdc, g_fTiny);
        RECT rSec1 = { 16, 62, MENU_RAIL_W - 16, 76 };
        DrawTextW(hdc, L"LOCAIS E SISTEMA", -1, &rSec1, DT_LEFT | DT_SINGLELINE);

        /* Itens da barra lateral */
        int fy = 80;
        for (int i = 0; i < (int)N_FOLDERS; i++) {
            if (fy + MENU_RAIL_ITEM_H > MENU_H - MENU_POWER_H) break;

            BOOL hot = (s_railHover == i);
            if (hot) {
                FillRoundRect(hdc, 8, fy, MENU_RAIL_W - 16, MENU_RAIL_ITEM_H, 6,
                              C_MENU_RAIL_HV, C_MENU_RAIL_HV);
                RECT rBar = { 10, fy + 7, 13, fy + MENU_RAIL_ITEM_H - 7 };
                HBRUSH brBar = CreateSolidBrush(C_ACCENT);
                FillRect(hdc, &rBar, brBar);
                DeleteObject(brBar);
            }

            DrawAppIcon(hdc, FOLDERS[i].iconType, 28, fy + MENU_RAIL_ITEM_H / 2, 16);

            SelectObject(hdc, g_fRegular);
            SetTextColor(hdc, hot ? C_TEXT_WHITE : RGB(220, 220, 225));
            RECT rItem = { 46, fy, MENU_RAIL_W - 12, fy + MENU_RAIL_ITEM_H };
            DrawTextW(hdc, FOLDERS[i].label, -1, &rItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            fy += MENU_RAIL_ITEM_H + 2;
        }

        /* Divisor acima dos botões de energia */
        MoveToEx(hdc, 12, MENU_H - 58, NULL); LineTo(hdc, MENU_RAIL_W - 12, MENU_H - 58);
        SelectObject(hdc, oldP);
        DeleteObject(sep);

        /* Caixa de busca */
        int sx = MENU_RAIL_W + 24;
        int sy = 12;
        int sw = MENU_W - sx - 52;
        FillRoundRect(hdc, sx, sy, sw, 34, 16, C_MENU_SEARCH, C_MENU_SEARCH_BD);
        DrawIconSearch(hdc, sx + 16, sy + 17, C_ACCENT_LIGHT);

        /* Cabeçalho da grade */
        RECT rMark = { sx, 64, sx + 3, 76 };
        HBRUSH brMark = CreateSolidBrush(C_ACCENT);
        FillRect(hdc, &rMark, brMark);
        DeleteObject(brMark);

        SelectObject(hdc, g_fTitle);
        SetTextColor(hdc, C_TEXT_WHITE);
        int nmatch = 0;
        for (int i = 0; i < (int)N_APPS; i++) if (AppMatches(&APPS[i])) nmatch++;

        WCHAR groupTitle[96];
        if (s_query[0])
            wsprintfW(groupTitle, L"Resultados da pesquisa (%d)", nmatch);
        else
            lstrcpyW(groupTitle, L"Ferramentas fixadas");

        RECT rGT = { sx + 12, 56, MENU_W - 20, 80 };
        DrawTextW(hdc, groupTitle, -1, &rGT, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        SelectObject(hdc, oldF);

        BitBlt(winDC, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, oldBM);
        DeleteObject(memBM);
        DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT_WHITE);
        SetBkColor(dc, C_MENU_SEARCH);
        static HBRUSH s_editBrush = NULL;
        if (!s_editBrush) s_editBrush = CreateSolidBrush(C_MENU_SEARCH);
        return (LRESULT)s_editBrush;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lParam;
        if (di->CtlType != ODT_BUTTON) break;
        int id = di->CtlID;
        HDC dc = di->hDC;
        BOOL hot = (di->itemState & ODS_SELECTED) != 0;

        if (id >= TILE_BASE && id < TILE_BASE + (int)N_APPS) {
            int idx = (int)GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA);
            if (idx >= 0 && idx < (int)N_APPS) {
                DrawTile(dc, &APPS[idx], di->rcItem.left, di->rcItem.top,
                         di->rcItem.right - di->rcItem.left,
                         di->rcItem.bottom - di->rcItem.top,
                         hot || (s_tileHover == id));
                return TRUE;
            }
        }

        if (id == ID_POWEROFF || id == ID_REBOOT) {
            BOOL isPower = (id == ID_POWEROFF);
            BOOL isHover = isPower ? (s_powerHover == 1) : (s_powerHover == 2);
            COLORREF bgC = isHover ? (isPower ? RGB(64, 28, 32) : RGB(58, 46, 18)) : RGB(28, 30, 38);
            COLORREF borderC = isHover ? (isPower ? RGB(214, 72, 72) : RGB(214, 164, 42)) : RGB(48, 52, 64);

            FillRoundRect(dc, di->rcItem.left, di->rcItem.top,
                          di->rcItem.right - di->rcItem.left,
                          di->rcItem.bottom - di->rcItem.top, 8, bgC, borderC);

            int cy = di->rcItem.top + (di->rcItem.bottom - di->rcItem.top) / 2;
            int icX = di->rcItem.left + 16;
            COLORREF iconCol = isHover ? RGB(255, 255, 255) : (isPower ? RGB(240, 90, 90) : RGB(255, 190, 40));

            if (isPower) DrawIconPower(dc, icX, cy, 6, iconCol);
            else        DrawIconRestart(dc, icX, cy, 6, iconCol);

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            HFONT oldF = (HFONT)SelectObject(dc, g_fSmall);
            RECT rT = { icX + 12, di->rcItem.top, di->rcItem.right - 4, di->rcItem.bottom };
            DrawTextW(dc, isPower ? L"Desligar" : L"Reiniciar", -1, &rT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, oldF);
            return TRUE;
        }

        if (id == ID_CLOSE) {
            RECT r = di->rcItem;
            int cx = (r.left + r.right) / 2;
            int cy = (r.top + r.bottom) / 2;
            int rad = 11;
            HBRUSH br = CreateSolidBrush(hot ? C_DANGER : C_MENU_SEARCH);
            HPEN penBg = CreatePen(PS_SOLID, 1, hot ? C_DANGER : C_MENU_SEARCH_BD);
            HBRUSH oldB = (HBRUSH)SelectObject(dc, br);
            HPEN oldPn = (HPEN)SelectObject(dc, penBg);
            Ellipse(dc, cx - rad, cy - rad, cx + rad, cy + rad);
            SelectObject(dc, oldB);
            SelectObject(dc, oldPn);
            DeleteObject(br);
            DeleteObject(penBg);

            HPEN p = CreatePen(PS_SOLID, 2, hot ? RGB(255, 255, 255) : C_TEXT_MUTED);
            HPEN oldP = (HPEN)SelectObject(dc, p);
            MoveToEx(dc, r.left + 7, r.top + 7, NULL);
            LineTo(dc, r.right - 7, r.bottom - 7);
            MoveToEx(dc, r.right - 7, r.top + 7, NULL);
            LineTo(dc, r.left + 7, r.bottom - 7);
            SelectObject(dc, oldP);
            DeleteObject(p);
            return TRUE;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        int hover = -1;
        if (pt.x >= 0 && pt.x < MENU_RAIL_W && pt.y >= 80 && pt.y < MENU_H - MENU_POWER_H) {
            hover = (pt.y - 80) / (MENU_RAIL_ITEM_H + 2);
            if (hover >= (int)N_FOLDERS) hover = -1;
        }
        if (hover != s_railHover) {
            s_railHover = hover;
            RECT rRail = { 0, 80, MENU_RAIL_W, MENU_H - MENU_POWER_H };
            InvalidateRect(hwnd, &rRail, FALSE);
        }

        int pHover = -1;
        if (pt.y >= MENU_H - 48 && pt.y <= MENU_H - 14) {
            int pw = (MENU_RAIL_W - 28) / 2;
            if (pt.x >= 12 && pt.x <= 12 + pw) pHover = 1;
            else if (pt.x >= 12 + pw + 8 && pt.x <= 12 + pw * 2 + 8) pHover = 2;
        }
        if (pHover != s_powerHover) {
            s_powerHover = pHover;
            HWND hP = GetDlgItem(hwnd, ID_POWEROFF);
            HWND hR = GetDlgItem(hwnd, ID_REBOOT);
            if (hP) InvalidateRect(hP, NULL, FALSE);
            if (hR) InvalidateRect(hR, NULL, FALSE);
        }
        break;
    }

    case WM_TIMER: {
        if (wParam == IDT_HOVER) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            int tx = MENU_RAIL_W + 24;
            int ty = MENU_HEADER_H + 46;
            int visible = 0, hover = -1;

            for (int i = 0; i < (int)N_APPS; i++) {
                if (!AppMatches(&APPS[i])) continue;
                int vx = tx + (visible % MENU_TILES_ROW) * (MENU_TILE_W + MENU_TILE_GAP);
                int vy = ty + (visible / MENU_TILES_ROW) * (MENU_TILE_H + MENU_TILE_GAP) - s_tileScroll;
                if (pt.x >= vx && pt.x < vx + MENU_TILE_W && pt.y >= vy && pt.y < vy + MENU_TILE_H) {
                    hover = TILE_BASE + visible;
                    break;
                }
                visible++;
            }
            if (hover != s_tileHover) {
                int old = s_tileHover;
                s_tileHover = hover;
                if (old >= TILE_BASE) {
                    HWND hOld = GetDlgItem(hwnd, old);
                    if (hOld) InvalidateRect(hOld, NULL, FALSE);
                }
                if (s_tileHover >= TILE_BASE) {
                    HWND hNew = GetDlgItem(hwnd, s_tileHover);
                    if (hNew) InvalidateRect(hNew, NULL, FALSE);
                }
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_CLOSE) {
            ToggleStartMenu(FALSE);
            return 0;
        }
        if (id == ID_POWEROFF || id == ID_REBOOT) {
            DoPower(id);
            return 0;
        }
        if (id == ID_SEARCH && code == EN_CHANGE) {
            GetDlgItemTextW(hwnd, ID_SEARCH, s_query, (int)(sizeof(s_query) / sizeof(s_query[0])));
            RebuildTiles(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (id >= TILE_BASE && id - TILE_BASE < N_APPS) {
            HWND ht = GetDlgItem(hwnd, id);
            int idx = (int)GetWindowLongPtrW(ht, GWLP_USERDATA);
            if (idx >= 0 && idx < N_APPS) {
                Launch(APPS[idx].cmd, APPS[idx].args);
            }
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        int fy = 80;
        for (int j = 0; j < (int)N_FOLDERS; j++) {
            if (fy + MENU_RAIL_ITEM_H > MENU_H - MENU_POWER_H) break;
            if (pt.x >= 8 && pt.x < MENU_RAIL_W - 8 && pt.y >= fy && pt.y < fy + MENU_RAIL_ITEM_H) {
                Launch(FOLDERS[j].cmd, FOLDERS[j].args);
                return 0;
            }
            fy += MENU_RAIL_ITEM_H + 2;
        }
        break;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int max = MaxTileScroll();
        int old = s_tileScroll;
        if (delta > 0)      s_tileScroll -= MENU_TILE_H + MENU_TILE_GAP;
        else if (delta < 0) s_tileScroll += MENU_TILE_H + MENU_TILE_GAP;
        if (s_tileScroll < 0) s_tileScroll = 0;
        if (s_tileScroll > max) s_tileScroll = max;
        if (s_tileScroll != old) {
            LayoutTiles(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE && s_menuVisible) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rcTb, rcBtn;
            GetWindowRect(g_hwndTaskbar, &rcTb);
            rcBtn.left = rcTb.left;
            rcBtn.top = rcTb.top;
            rcBtn.right = rcTb.left + START_BTN_W;
            rcBtn.bottom = rcTb.bottom;

            if (PtInRect(&rcBtn, pt)) {
                s_ignoreNextStartClick = TRUE;
            }
            ToggleStartMenu(FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            ToggleStartMenu(FALSE);
            return 0;
        }
        break;

    case WM_CLOSE:
        ToggleStartMenu(FALSE);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

/* =========================================================================
   Barra de Tarefas Estilo Windows 10/11
   ========================================================================= */

static LRESULT CALLBACK TaskbarWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, IDT_CLOCK, 1000, NULL);
        break;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_CLOCK);
        PostQuitMessage(0);
        break;

    case WM_DISPLAYCHANGE: {
        int sw = LOWORD(lParam);
        int sh = HIWORD(lParam);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, sh - TASKBAR_H, sw, TASKBAR_H, SWP_NOACTIVATE);
        if (g_hwndMenu) {
            int mw = (MENU_W > sw - 20) ? (sw - 20) : MENU_W;
            int mh = (MENU_H > sh - TASKBAR_H - 10) ? (sh - TASKBAR_H - 10) : MENU_H;
            SetWindowPos(g_hwndMenu, HWND_TOPMOST, 0, sh - TASKBAR_H - mh, mw, mh, SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }

    case WM_HOTKEY:
        if (wParam == 101) {
            OpenFileExplorer(NULL);
            return 0;
        }
        if (wParam == 100 || wParam == 102) {
            ToggleStartMenu(!s_menuVisible);
            return 0;
        }
        break;

    case WM_MOUSEMOVE: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);

        BOOL oldBtnHover = s_startBtnHover;
        int oldQuickHover = s_quickHover;

        s_startBtnHover = (pt.x >= 0 && pt.x < START_BTN_W && pt.y >= 0 && pt.y < TASKBAR_H);

        s_quickHover = -1;
        for (int i = 0; i < (int)N_QUICKAPPS; i++) {
            int qx = START_BTN_W + 8 + i * (QUICK_W + 4);
            if (pt.x >= qx && pt.x < qx + QUICK_W && pt.y >= 4 && pt.y < TASKBAR_H - 4) {
                s_quickHover = i;
                break;
            }
        }

        if (oldBtnHover != s_startBtnHover || oldQuickHover != s_quickHover) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }

    case WM_MOUSELEAVE:
        if (s_startBtnHover || s_quickHover != -1) {
            s_startBtnHover = FALSE;
            s_quickHover = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONDOWN: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        if (pt.x >= 0 && pt.x < START_BTN_W) {
            SetCapture(hwnd);
            s_startBtnPressed = TRUE;
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            if (s_menuVisible) ToggleStartMenu(FALSE);
            for (int i = 0; i < (int)N_QUICKAPPS; i++) {
                int qx = START_BTN_W + 8 + i * (QUICK_W + 4);
                if (pt.x >= qx && pt.x < qx + QUICK_W && pt.y >= 4 && pt.y < TASKBAR_H - 4) {
                    Launch(QUICKAPPS[i].cmd, QUICKAPPS[i].args);
                    break;
                }
            }
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        if (s_startBtnPressed) {
            ReleaseCapture();
            s_startBtnPressed = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);

            if (pt.x >= 0 && pt.x < START_BTN_W && pt.y >= 0 && pt.y < TASKBAR_H) {
                if (s_ignoreNextStartClick) {
                    s_ignoreNextStartClick = FALSE;
                } else {
                    ToggleStartMenu(!s_menuVisible);
                }
            }
        }
        break;
    }

    case WM_TIMER:
        if (wParam == IDT_CLOCK) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            RECT rcClock = { rc.right - 100, 0, rc.right, rc.bottom };
            InvalidateRect(hwnd, &rcClock, FALSE);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

        HBRUSH bg = CreateSolidBrush(C_TB_BG);
        FillRect(memDC, &rc, bg);
        DeleteObject(bg);

        HPEN linePen = CreatePen(PS_SOLID, 1, C_TB_LINE);
        HPEN oldPen = (HPEN)SelectObject(memDC, linePen);
        MoveToEx(memDC, 0, 0, NULL); LineTo(memDC, rc.right, 0);
        SelectObject(memDC, oldPen);
        DeleteObject(linePen);

        RECT rcBtn = { 0, 0, START_BTN_W, rc.bottom };
        if (s_menuVisible || s_startBtnPressed) {
            HBRUSH btnBg = CreateSolidBrush(C_TB_PRESS);
            FillRect(memDC, &rcBtn, btnBg);
            DeleteObject(btnBg);
        } else if (s_startBtnHover) {
            HBRUSH btnBg = CreateSolidBrush(C_TB_HOVER);
            FillRect(memDC, &rcBtn, btnBg);
            DeleteObject(btnBg);
        }

        int cx = START_BTN_W / 2, cy = rc.bottom / 2;
        COLORREF logoColor = (s_menuVisible || s_startBtnPressed || s_startBtnHover)
                             ? RGB(255, 255, 255) : C_ACCENT;
        HBRUSH logoBr = CreateSolidBrush(logoColor);

        RECT r1 = { cx - 8, cy - 8, cx - 1, cy - 1 };
        RECT r2 = { cx + 1, cy - 8, cx + 8, cy - 1 };
        RECT r3 = { cx - 8, cy + 1, cx - 1, cy + 8 };
        RECT r4 = { cx + 1, cy + 1, cx + 8, cy + 8 };
        FillRect(memDC, &r1, logoBr);
        FillRect(memDC, &r2, logoBr);
        FillRect(memDC, &r3, logoBr);
        FillRect(memDC, &r4, logoBr);
        DeleteObject(logoBr);

        if (s_menuVisible) {
            RECT rcAcc = { 0, rc.bottom - 2, START_BTN_W, rc.bottom };
            HBRUSH accBr = CreateSolidBrush(C_ACCENT);
            FillRect(memDC, &rcAcc, accBr);
            DeleteObject(accBr);
        }

        for (int i = 0; i < (int)N_QUICKAPPS; i++) {
            int qx = START_BTN_W + 8 + i * (QUICK_W + 4);
            int qy = 4;
            RECT rq = { qx, qy, qx + QUICK_W, rc.bottom - 4 };

            if (s_quickHover == i) {
                FillRoundRect(memDC, qx, qy, QUICK_W, rc.bottom - 8, 4, C_TB_HOVER, C_TB_LINE);
            }

            DrawAppIcon(memDC, QUICKAPPS[i].iconType, qx + QUICK_W / 2, rc.bottom / 2, 22);
        }

        SYSTEMTIME st;
        GetLocalTime(&st);
        WCHAR szTime[16], szDate[16];
        wsprintfW(szTime, L"%02d:%02d", st.wHour, st.wMinute);
        wsprintfW(szDate, L"%02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);

        SetBkMode(memDC, TRANSPARENT);
        HFONT oldF = (HFONT)SelectObject(memDC, g_fClock);
        RECT rcTime = { rc.right - 95, 3, rc.right - 10, 22 };
        RECT rcDate = { rc.right - 95, 22, rc.right - 10, 38 };

        SetTextColor(memDC, C_TEXT_WHITE);
        DrawTextW(memDC, szTime, -1, &rcTime, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        SelectObject(memDC, g_fSmall);
        SetTextColor(memDC, C_TEXT_MUTED);
        DrawTextW(memDC, szDate, -1, &rcDate, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(memDC, oldF);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        break;
    }

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

/* =========================================================================
   EXPLORADOR DE ARQUIVOS INTEGRADO (Modern Dark Explorer)
   ========================================================================= */

static int g_expCascade = 0;

static const WCHAR *ExpExtFor(const WCHAR *name) {
    const WCHAR *dot = wcsrchr(name, L'.');
    if (dot && dot != name) return dot + 1;
    return L"";
}

static void ExpFormatSize(ULARGE_INTEGER sz, WCHAR *out, int cch) {
    const ULONGLONG kb = 1024u, mb = kb * 1024u, gb = mb * 1024u;
    if (sz.QuadPart >= gb) {
        double v = (double)sz.QuadPart / gb;
        wsprintfW(out, L"%.2f GB", v);
    } else if (sz.QuadPart >= mb) {
        double v = (double)sz.QuadPart / mb;
        wsprintfW(out, L"%.2f MB", v);
    } else if (sz.QuadPart >= kb) {
        double v = (double)sz.QuadPart / kb;
        wsprintfW(out, L"%.0f KB", v);
    } else {
        wsprintfW(out, L"%u B", sz.LowPart);
    }
}

static void ExpFormatDate(FILETIME ft, WCHAR *out, int cch) {
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st) || st.wYear < 1980) {
        lstrcpynW(out, L"", cch);
        return;
    }
    wsprintfW(out, L"%02d/%02d/%04d %02d:%02d",
              st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
}

static EXPLORER *ExpGet(HWND hwnd) {
    return (EXPLORER *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

static void ExpFreeList(EXPITEM **pp, int *n, int *cap) {
    if (*pp) free(*pp);
    *pp = NULL;
    *n = 0;
    *cap = 0;
}

static void ExpClearContent(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    ExpFreeList(&e->items, &e->nItems, &e->capItems);
    if (e->filtered) { free(e->filtered); e->filtered = NULL; }
    e->nFiltered = 0;
    SendDlgItemMessageW(hwnd, IDX_LIST, LB_RESETCONTENT, 0, 0);
}

static void ExpClearPlaces(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    ExpFreeList(&e->places, &e->nPlaces, &e->capPlaces);
    SendDlgItemMessageW(hwnd, IDX_SIDE, LB_RESETCONTENT, 0, 0);
}

static int ExpPush(EXPITEM **pp, int *n, int *cap, const EXPITEM *it) {
    if (*n >= *cap) {
        int nc = *cap ? *cap * 2 : 64;
        EXPITEM *np = (EXPITEM *)realloc(*pp, nc * sizeof(EXPITEM));
        if (!np) return -1;
        *pp = np;
        *cap = nc;
    }
    (*pp)[*n] = *it;
    return (*n)++;
}

static EXPITEM *g_sortRef = NULL;
static int ExpCmp(const void *pa, const void *pb) {
    int a = *(const int *)pa, b = *(const int *)pb;
    const EXPITEM *ia = &g_sortRef[a], *ib = &g_sortRef[b];
    if (ia->isDir != ib->isDir) return ia->isDir ? -1 : 1;
    int r = lstrcmpiW(ia->name, ib->name);
    if (r) return r;
    return (a > b) - (a < b);
}

static void ExpApplyFilter(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;

    if (e->filtered) { free(e->filtered); e->filtered = NULL; }
    e->nFiltered = 0;

    SendDlgItemMessageW(hwnd, IDX_LIST, LB_RESETCONTENT, 0, 0);

    if (e->nItems <= 0) return;

    e->filtered = (int *)calloc(e->nItems, sizeof(int));
    if (!e->filtered) return;

    WCHAR q[64] = {0};
    lstrcpynW(q, e->filter, 64);
    CharLowerW(q);

    for (int i = 0; i < e->nItems; i++) {
        if (q[0]) {
            WCHAR tmp[MAX_PATH];
            lstrcpynW(tmp, e->items[i].name, MAX_PATH);
            CharLowerW(tmp);
            if (!wcsstr(tmp, q)) continue;
        }
        e->filtered[e->nFiltered++] = i;
        SendDlgItemMessageW(hwnd, IDX_LIST, LB_ADDSTRING, 0, (LPARAM)L"");
    }

    WCHAR st[256];
    if (e->drivesMode) {
        wsprintfW(st, L"%d unidade(s)", e->nFiltered);
    } else {
        int d = 0, f = 0;
        for (int i = 0; i < e->nFiltered; i++) {
            if (e->items[e->filtered[i]].isDir) d++; else f++;
        }
        wsprintfW(st, L"%d pasta(s), %d arquivo(s)", d, f);
    }
    SetDlgItemTextW(hwnd, IDX_STATUS, st);
}

static void ExpAddPlace(HWND hwnd, const WCHAR *label, const WCHAR *path, BOOL isDrives) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    EXPITEM p;
    ZeroMemory(&p, sizeof(p));
    lstrcpynW(p.name, label, MAX_PATH);
    lstrcpynW(p.path, path ? path : L"", MAX_PATH);
    p.isDir = TRUE;
    p.isDrive = isDrives;
    int idx = ExpPush(&e->places, &e->nPlaces, &e->capPlaces, &p);
    if (idx >= 0) {
        int pos = (int)SendDlgItemMessageW(hwnd, IDX_SIDE, LB_ADDSTRING, 0, 0);
        SendDlgItemMessageW(hwnd, IDX_SIDE, LB_SETITEMDATA, pos, (LPARAM)idx);
    }
}

static void ExpBuildPlaces(HWND hwnd) {
    ExpClearPlaces(hwnd);

    ExpAddPlace(hwnd, L"Este Computador", L"", TRUE);

    WCHAR buf[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, buf) == S_OK && buf[0])
        ExpAddPlace(hwnd, L"Área de Trabalho", buf, FALSE);
    if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, buf) == S_OK && buf[0])
        ExpAddPlace(hwnd, L"Documentos", buf, FALSE);
    if (SHGetFolderPathW(NULL, 0x000c /* CSIDL_DOWNLOADS */, NULL, 0, buf) == S_OK && buf[0])
        ExpAddPlace(hwnd, L"Downloads", buf, FALSE);

    WCHAR winDir[MAX_PATH];
    GetWindowsDirectoryW(winDir, MAX_PATH);
    ExpAddPlace(hwnd, L"Windows", winDir, FALSE);
    wsprintfW(buf, L"%s\\System32", winDir);
    if (ExpPathExists(buf)) ExpAddPlace(hwnd, L"System32", buf, FALSE);
    wsprintfW(buf, L"%s\\System32\\Drivers", winDir);
    if (ExpPathExists(buf)) ExpAddPlace(hwnd, L"Drivers", buf, FALSE);

    WCHAR drives[512] = {0};
    DWORD dl = GetLogicalDriveStringsW(511, drives);
    for (WCHAR *d = drives; dl && *d; d += lstrlenW(d) + 1) {
        UINT type = GetDriveTypeW(d);
        if (type < 2) continue;
        WCHAR vol[128] = {0};
        GetVolumeInformationW(d, vol, 127, NULL, NULL, NULL, NULL, 0);
        if (!vol[0]) {
            if (type == DRIVE_CDROM) lstrcpynW(vol, L"Unidade de CD/DVD", 127);
            else if (type == DRIVE_RAMDISK) lstrcpynW(vol, L"Disco RAM WinPE", 127);
            else lstrcpynW(vol, L"Disco Local", 127);
        }
        wsprintfW(buf, L"%s (%s)", d, vol);
        ExpAddPlace(hwnd, buf, d, TRUE);
    }
}

static void ExpPushHistory(HWND hwnd, const WCHAR *path) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    if (e->histPos + 1 < 64) {
        e->histPos++;
    } else {
        memmove(e->hist[0], e->hist[1], 63 * sizeof(e->hist[0]));
        e->histPos = 63;
    }
    lstrcpynW(e->hist[e->histPos], path, MAX_PATH);
    e->histCount = e->histPos + 1;
}

static void ExpEnumerate(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    ExpClearContent(hwnd);

    if (e->drivesMode) {
        WCHAR drives[512] = {0};
        DWORD dl = GetLogicalDriveStringsW(511, drives);
        int count = 0;
        for (WCHAR *d = drives; dl && *d; d += lstrlenW(d) + 1) count++;
        EXPITEM *tmp = (EXPITEM *)calloc(count ? count : 1, sizeof(EXPITEM));
        if (!tmp) return;

        int n = 0;
        for (WCHAR *d = drives; dl && *d; d += lstrlenW(d) + 1) {
            UINT type = GetDriveTypeW(d);
            if (type < 2) continue;
            WCHAR vol[128] = {0};
            GetVolumeInformationW(d, vol, 127, NULL, NULL, NULL, NULL, 0);
            if (!vol[0]) {
                if (type == DRIVE_CDROM) lstrcpynW(vol, L"Unidade Óptica", 127);
                else if (type == DRIVE_RAMDISK) lstrcpynW(vol, L"RAMDisk WinPE", 127);
                else lstrcpynW(vol, L"Disco Local", 127);
            }
            wsprintfW(tmp[n].name, L"%s (%s)", d, vol);
            lstrcpynW(tmp[n].path, d, MAX_PATH);
            tmp[n].isDrive = TRUE;
            tmp[n].isDir = FALSE;
            ULARGE_INTEGER freeAvail, total, totalFree;
            if (GetDiskFreeSpaceExW(d, &freeAvail, &total, &totalFree)) {
                tmp[n].size = totalFree;
            }
            n++;
        }

        for (int i = 0; i < n; i++) {
            ExpPush(&e->items, &e->nItems, &e->capItems, &tmp[i]);
        }
        free(tmp);
        ExpApplyFilter(hwnd);
        return;
    }

    EXPITEM *dirtmp = NULL, *filetmp = NULL;
    int nd = 0, nf = 0, capd = 0, capf = 0;
    WCHAR spec[MAX_PATH];
    WIN32_FIND_DATAW fd;

    for (int pass = 0; pass < 2; pass++) {
        wsprintfW(spec, L"%s\\*", e->path);
        HANDLE h = FindFirstFileW(spec, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            if (pass == 0) {
                e->drivesMode = TRUE;
                lstrcpynW(e->path, L"", MAX_PATH);
                SetDlgItemTextW(hwnd, IDX_PATH, L"Este Computador");
                ExpEnumerate(hwnd);
            }
            return;
        }
        do {
            if (!lstrcmpW(fd.cFileName, L".") || !lstrcmpW(fd.cFileName, L"..")) continue;
            BOOL isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if ((pass == 0) != isDir) continue;

            EXPITEM cur;
            ZeroMemory(&cur, sizeof(cur));
            lstrcpynW(cur.name, fd.cFileName, MAX_PATH);
            wsprintfW(cur.path, L"%s\\%s", e->path, fd.cFileName);
            lstrcpynW(cur.ext, ExpExtFor(fd.cFileName), 16);
            cur.attrs = fd.dwFileAttributes;
            cur.isDir = isDir;
            cur.size.LowPart = fd.nFileSizeLow;
            cur.size.HighPart = fd.nFileSizeHigh;
            cur.ft = fd.ftLastWriteTime;

            EXPITEM **arr = isDir ? &dirtmp : &filetmp;
            int *cnt = isDir ? &nd : &nf;
            int *cp = isDir ? &capd : &capf;
            ExpPush(arr, cnt, cp, &cur);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    for (int g = 0; g < 2; g++) {
        EXPITEM *arr = (g == 0) ? dirtmp : filetmp;
        int cnt = (g == 0) ? nd : nf;
        if (cnt > 1) {
            int *ord = (int *)calloc(cnt, sizeof(int));
            if (ord) {
                for (int i = 0; i < cnt; i++) ord[i] = i;
                g_sortRef = arr;
                qsort(ord, cnt, sizeof(int), ExpCmp);
                for (int i = 0; i < cnt; i++) {
                    ExpPush(&e->items, &e->nItems, &e->capItems, &arr[ord[i]]);
                }
                free(ord);
            }
        } else if (cnt == 1) {
            ExpPush(&e->items, &e->nItems, &e->capItems, &arr[0]);
        }
    }

    if (dirtmp) free(dirtmp);
    if (filetmp) free(filetmp);

    ExpApplyFilter(hwnd);
}

static void ExpNavigate(HWND hwnd, const WCHAR *path) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e || !path) return;

    if (!path[0]) {
        e->drivesMode = TRUE;
        lstrcpynW(e->path, L"", MAX_PATH);
        SetDlgItemTextW(hwnd, IDX_PATH, L"Este Computador");
        ExpEnumerate(hwnd);
        SetWindowTextW(hwnd, L"Este Computador - Explorador de Arquivos");
        return;
    }

    WCHAR clean[MAX_PATH];
    lstrcpynW(clean, path, MAX_PATH);
    if (!ExpPathIsDir(clean)) {
        SetDlgItemTextW(hwnd, IDX_STATUS, L"Caminho não encontrado ou inacessível.");
        return;
    }

    e->drivesMode = FALSE;
    lstrcpynW(e->path, clean, MAX_PATH);
    SetDlgItemTextW(hwnd, IDX_PATH, clean);
    ExpPushHistory(hwnd, clean);
    ExpEnumerate(hwnd);

    WCHAR title[MAX_PATH + 48];
    const WCHAR *base = wcsrchr(clean, L'\\');
    base = (base && base[1]) ? base + 1 : clean;
    if (!base[0]) base = clean;
    wsprintfW(title, L"%s - Explorador de Arquivos", base);
    SetWindowTextW(hwnd, title);
}

static void ExpGoBack(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e || e->histPos <= 0) return;
    e->histPos--;
    ExpNavigate(hwnd, e->hist[e->histPos]);
}

static void ExpGoFwd(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e || e->histPos + 1 >= e->histCount) return;
    e->histPos++;
    ExpNavigate(hwnd, e->hist[e->histPos]);
}

static void ExpGoUp(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e || e->drivesMode) return;
    WCHAR parent[MAX_PATH];
    lstrcpyW(parent, e->path);
    WCHAR *cut = wcsrchr(parent, L'\\');
    if (!cut) {
        ExpNavigate(hwnd, L"");
        return;
    }
    if (cut == parent) { cut[1] = 0; }
    else { *cut = 0; }
    ExpNavigate(hwnd, parent);
}

static void ExpOpenSelected(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    HWND hList = GetDlgItem(hwnd, IDX_LIST);
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= e->nFiltered) return;

    int realIdx = e->filtered[sel];
    const EXPITEM *it = &e->items[realIdx];

    if (it->isDrive || it->isDir) {
        if (it->path[0]) ExpNavigate(hwnd, it->path);
        else ExpNavigate(hwnd, L"");
        return;
    }

    ShellExecuteW(hwnd, L"open", it->path, NULL, NULL, SW_SHOWNORMAL);
}

static void ExpCreateNewFolder(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e || e->drivesMode || !e->path[0]) return;

    WCHAR newPath[MAX_PATH];
    for (int i = 1; i < 100; i++) {
        if (i == 1) wsprintfW(newPath, L"%s\\Nova Pasta", e->path);
        else        wsprintfW(newPath, L"%s\\Nova Pasta (%d)", e->path, i);
        if (!ExpPathExists(newPath)) {
            CreateDirectoryW(newPath, NULL);
            ExpEnumerate(hwnd);
            break;
        }
    }
}

static void ExpOpenCmdHere(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    WCHAR dir[MAX_PATH];
    if (e && !e->drivesMode && e->path[0]) lstrcpynW(dir, e->path, MAX_PATH);
    else lstrcpynW(dir, L"X:\\", MAX_PATH);

    ShellExecuteW(hwnd, L"open", L"cmd.exe", NULL, dir, SW_SHOWNORMAL);
}

static void ExpCopySelectedPath(HWND hwnd) {
    EXPLORER *e = ExpGet(hwnd);
    if (!e) return;
    HWND hList = GetDlgItem(hwnd, IDX_LIST);
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    const WCHAR *p = (sel >= 0 && sel < e->nFiltered) ? e->items[e->filtered[sel]].path : e->path;

    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        size_t bytes = (lstrlenW(p) + 1) * sizeof(WCHAR);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            memcpy(GlobalLock(hMem), p, bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

/* =========================================================================
   Layout e Desenho do Explorador
   ========================================================================= */

static void ExpLayout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    /* Grupo de Navegação à esquerda */
    MoveWindow(GetDlgItem(hwnd, IDX_BACK),      8, 8, 30, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_FWD),      40, 8, 30, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_UP),       72, 8, 30, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_REFRESH), 104, 8, 30, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_HOME),    136, 8, 30, 32, TRUE);

    int filterW = 150;
    int cmdW = 76;
    int newFolderW = 86;
    int gap = 6;

    int rightClusterW = filterW + gap + cmdW + gap + newFolderW + 12;
    int navClusterW = 172;

    int pathX = navClusterW + 6;
    int pathW = w - pathX - rightClusterW - 36;
    if (pathW < 120) pathW = 120;

    MoveWindow(GetDlgItem(hwnd, IDX_PATH),      pathX, 8, pathW, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_GO),        pathX + pathW + 4, 8, 30, 32, TRUE);

    int actX = pathX + pathW + 38;
    MoveWindow(GetDlgItem(hwnd, IDX_NEWFOLDER), actX, 8, newFolderW, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_CMDHERE),   actX + newFolderW + gap, 8, cmdW, 32, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_FILTER),    w - filterW - 8, 8, filterW, 32, TRUE);

    int top = EXP_TOOLBAR_H + EXP_HEADER_H;
    int bot = h - EXP_STATUS_H;

    MoveWindow(GetDlgItem(hwnd, IDX_SIDE),   0, top, EXP_SIDE_W, bot - top, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_LIST),   EXP_SIDE_W, top, w - EXP_SIDE_W, bot - top, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDX_STATUS), 0, bot, w, EXP_STATUS_H, TRUE);
}

static LRESULT CALLBACK ExpSearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        SetWindowTextW(hwnd, L"");
        return 0;
    }
    WNDPROC old = (WNDPROC)GetPropW(hwnd, L"OldProc");
    if (old) return CallWindowProcW(old, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ExpPathEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        WCHAR buf[MAX_PATH];
        GetWindowTextW(hwnd, buf, MAX_PATH);
        if (buf[0]) ExpNavigate(GetParent(hwnd), buf);
        return 0;
    }
    WNDPROC old = (WNDPROC)GetPropW(hwnd, L"OldProc");
    if (old) return CallWindowProcW(old, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ExpWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        EXPLORER *e = (EXPLORER *)calloc(1, sizeof(EXPLORER));
        if (!e) return -1;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)e);

        HINSTANCE inst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_BACK, inst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_FWD, inst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_UP, inst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_REFRESH, inst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_HOME, inst, NULL);

        HWND hPath = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_PATH, inst, NULL);
        if (hPath) {
            SendMessageW(hPath, WM_SETFONT, (WPARAM)g_fRegular, TRUE);
            WNDPROC old = (WNDPROC)SetWindowLongPtrW(hPath, GWLP_WNDPROC, (LONG_PTR)ExpPathEditProc);
            SetPropW(hPath, L"OldProc", (HANDLE)old);
        }

        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_GO, inst, NULL);

        CreateWindowW(L"BUTTON", L"+ Pasta", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_NEWFOLDER, inst, NULL);
        CreateWindowW(L"BUTTON", L">_ CMD", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_CMDHERE, inst, NULL);

        HWND hFilter = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_FILTER, inst, NULL);
        if (hFilter) {
            SendMessageW(hFilter, WM_SETFONT, (WPARAM)g_fRegular, TRUE);
            WNDPROC old = (WNDPROC)SetWindowLongPtrW(hFilter, GWLP_WNDPROC, (LONG_PTR)ExpSearchEditProc);
            SetPropW(hFilter, L"OldProc", (HANDLE)old);
        }

        CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_SIDE, inst, NULL);

        CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_LIST, inst, NULL);

        CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, hwnd, (HMENU)(UINT_PTR)IDX_STATUS, inst, NULL);

        ExpBuildPlaces(hwnd);
        ExpLayout(hwnd);

        if (cs && cs->lpCreateParams) {
            ExpNavigate(hwnd, (const WCHAR *)cs->lpCreateParams);
        } else {
            WCHAR winDir[MAX_PATH];
            GetWindowsDirectoryW(winDir, MAX_PATH);
            if (winDir[0]) ExpNavigate(hwnd, winDir);
            else ExpNavigate(hwnd, L"");
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        RECT rTb = { 0, 0, rc.right, EXP_TOOLBAR_H };
        HBRUSH brTb = CreateSolidBrush(C_EXP_TOOLBAR);
        FillRect(hdc, &rTb, brTb);
        DeleteObject(brTb);

        RECT rHdr = { EXP_SIDE_W, EXP_TOOLBAR_H, rc.right, EXP_TOOLBAR_H + EXP_HEADER_H };
        HBRUSH brHdr = CreateSolidBrush(C_EXP_HEADER);
        FillRect(hdc, &rHdr, brHdr);
        DeleteObject(brHdr);

        RECT rSideHdr = { 0, EXP_TOOLBAR_H, EXP_SIDE_W, EXP_TOOLBAR_H + EXP_HEADER_H };
        HBRUSH brSH = CreateSolidBrush(C_EXP_SIDEBAR);
        FillRect(hdc, &rSideHdr, brSH);
        DeleteObject(brSH);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, C_TEXT_MUTED);
        HFONT oldF = (HFONT)SelectObject(hdc, g_fSmall);

        RECT rSideTitle = { 12, EXP_TOOLBAR_H, EXP_SIDE_W, EXP_TOOLBAR_H + EXP_HEADER_H };
        DrawTextW(hdc, L"LOCAIS E UNIDADES", -1, &rSideTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rColNome = { EXP_SIDE_W + 36, EXP_TOOLBAR_H, rc.right - 340, EXP_TOOLBAR_H + EXP_HEADER_H };
        DrawTextW(hdc, L"Nome", -1, &rColNome, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rColData = { rc.right - 330, EXP_TOOLBAR_H, rc.right - 180, EXP_TOOLBAR_H + EXP_HEADER_H };
        DrawTextW(hdc, L"Data de Modificação", -1, &rColData, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rColTipo = { rc.right - 170, EXP_TOOLBAR_H, rc.right - 90, EXP_TOOLBAR_H + EXP_HEADER_H };
        DrawTextW(hdc, L"Tipo", -1, &rColTipo, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rColTam = { rc.right - 85, EXP_TOOLBAR_H, rc.right - 12, EXP_TOOLBAR_H + EXP_HEADER_H };
        DrawTextW(hdc, L"Tamanho", -1, &rColTam, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        HPEN pen = CreatePen(PS_SOLID, 1, C_EXP_LINE);
        HPEN oldP = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, EXP_TOOLBAR_H, NULL); LineTo(hdc, rc.right, EXP_TOOLBAR_H);
        MoveToEx(hdc, 0, EXP_TOOLBAR_H + EXP_HEADER_H, NULL); LineTo(hdc, rc.right, EXP_TOOLBAR_H + EXP_HEADER_H);
        MoveToEx(hdc, EXP_SIDE_W, EXP_TOOLBAR_H, NULL); LineTo(hdc, EXP_SIDE_W, rc.bottom - EXP_STATUS_H);
        SelectObject(hdc, oldP);
        DeleteObject(pen);

        SelectObject(hdc, oldF);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        mis->itemHeight = EXP_ITEM_H;
        return TRUE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lParam;
        EXPLORER *e = ExpGet(hwnd);

        if (di->CtlType == ODT_BUTTON) {
            int id = di->CtlID;
            BOOL sel = (di->itemState & ODS_SELECTED) != 0;
            RECT r = di->rcItem;

            COLORREF bgC = sel ? RGB(50, 50, 60) : RGB(36, 36, 44);
            FillRoundRect(di->hDC, r.left, r.top, r.right - r.left, r.bottom - r.top, 4, bgC, C_EXP_LINE);

            int cx = (r.left + r.right) / 2;
            int cy = (r.top + r.bottom) / 2;

            if (id == IDX_BACK)    DrawNavArrow(di->hDC, cx, cy, 0, C_TEXT_WHITE);
            else if (id == IDX_FWD)DrawNavArrow(di->hDC, cx, cy, 1, C_TEXT_WHITE);
            else if (id == IDX_UP) DrawNavArrow(di->hDC, cx, cy, 2, C_TEXT_WHITE);
            else if (id == IDX_REFRESH) DrawNavArrow(di->hDC, cx, cy, 3, C_TEXT_WHITE);
            else if (id == IDX_HOME) DrawIconPC(di->hDC, cx, cy, 18);
            else if (id == IDX_GO)   DrawNavArrow(di->hDC, cx, cy, 1, C_ACCENT_LIGHT);
            else if (id == IDX_NEWFOLDER || id == IDX_CMDHERE) {
                SetBkMode(di->hDC, TRANSPARENT);
                SetTextColor(di->hDC, C_TEXT_WHITE);
                HFONT oldF = (HFONT)SelectObject(di->hDC, g_fSmall);
                DrawTextW(di->hDC, (id == IDX_NEWFOLDER) ? L"+ Pasta" : L">_ CMD",
                          -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(di->hDC, oldF);
            }
            return TRUE;
        }

        if (di->CtlID == IDX_LIST) {
            if (!e || di->itemID < 0 || di->itemID >= e->nFiltered) break;
            int realIdx = e->filtered[di->itemID];
            const EXPITEM *it = &e->items[realIdx];
            BOOL sel = (di->itemState & ODS_SELECTED) != 0;
            RECT rc = di->rcItem;

            HBRUSH bg = CreateSolidBrush(sel ? C_EXP_ITEM_SEL : C_EXP_LIST);
            FillRect(di->hDC, &rc, bg);
            DeleteObject(bg);

            int iy = rc.top + (rc.bottom - rc.top - EXP_ICON_SZ) / 2;
            DrawFileIconByExt(di->hDC, rc.left + 6, iy, EXP_ICON_SZ, it->isDir, it->isDrive, it->ext);

            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, sel ? RGB(255, 255, 255) : C_TEXT_WHITE);
            HFONT oldF = (HFONT)SelectObject(di->hDC, g_fRegular);

            RECT rcName = { rc.left + 36, rc.top, rc.right - 340, rc.bottom };
            DrawTextW(di->hDC, it->name, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(di->hDC, g_fSmall);
            SetTextColor(di->hDC, sel ? RGB(230, 240, 255) : C_TEXT_MUTED);

            WCHAR sDate[32] = {0};
            ExpFormatDate(it->ft, sDate, 32);
            RECT rcDate = { rc.right - 330, rc.top, rc.right - 180, rc.bottom };
            DrawTextW(di->hDC, sDate, -1, &rcDate, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            WCHAR sType[32];
            if (it->isDrive) lstrcpynW(sType, L"Unidade de Disco", 32);
            else if (it->isDir) lstrcpynW(sType, L"Pasta de arquivos", 32);
            else if (!_wcsicmp(it->ext, L"exe") || !_wcsicmp(it->ext, L"com")) lstrcpynW(sType, L"Aplicativo", 32);
            else if (!_wcsicmp(it->ext, L"cmd") || !_wcsicmp(it->ext, L"bat")) lstrcpynW(sType, L"Script", 32);
            else if (!_wcsicmp(it->ext, L"wim")) lstrcpynW(sType, L"Imagem WIM", 32);
            else if (!_wcsicmp(it->ext, L"sys") || !_wcsicmp(it->ext, L"dll")) lstrcpynW(sType, L"Extensão do Sistema", 32);
            else lstrcpynW(sType, L"Arquivo", 32);

            RECT rcTipo = { rc.right - 170, rc.top, rc.right - 90, rc.bottom };
            DrawTextW(di->hDC, sType, -1, &rcTipo, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            WCHAR sSize[32] = {0};
            if (it->isDrive && it->size.QuadPart) {
                ExpFormatSize(it->size, sSize, 32);
            } else if (!it->isDir && !it->isDrive) {
                ExpFormatSize(it->size, sSize, 32);
            }
            RECT rcTam = { rc.right - 85, rc.top, rc.right - 12, rc.bottom };
            DrawTextW(di->hDC, sSize, -1, &rcTam, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(di->hDC, oldF);
            return TRUE;
        }

        if (di->CtlID == IDX_SIDE) {
            if (!e || di->itemID < 0 || di->itemID >= e->nPlaces) break;
            const EXPITEM *pl = &e->places[di->itemID];
            BOOL sel = (di->itemState & ODS_SELECTED) != 0;
            RECT rc = di->rcItem;

            HBRUSH bg = CreateSolidBrush(sel ? C_EXP_ITEM_SEL : C_EXP_SIDEBAR);
            FillRect(di->hDC, &rc, bg);
            DeleteObject(bg);

            int iy = rc.top + (rc.bottom - rc.top - 18) / 2;
            if (pl->isDrive) DrawIconDisk(di->hDC, rc.left + 14, iy + 9, 16);
            else             DrawIconExplorer(di->hDC, rc.left + 14, iy + 9, 16);

            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, sel ? RGB(255, 255, 255) : C_TEXT_WHITE);
            HFONT oldF = (HFONT)SelectObject(di->hDC, g_fRegular);

            RECT rcTxt = { rc.left + 28, rc.top, rc.right - 6, rc.bottom };
            DrawTextW(di->hDC, pl->name, -1, &rcTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(di->hDC, oldF);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == GetDlgItem(hwnd, IDX_SIDE)) {
            SetTextColor(dc, C_TEXT_WHITE);
            SetBkColor(dc, C_EXP_SIDEBAR);
            static HBRUSH s_sideBr = NULL;
            if (!s_sideBr) s_sideBr = CreateSolidBrush(C_EXP_SIDEBAR);
            return (LRESULT)s_sideBr;
        }
        SetTextColor(dc, C_TEXT_WHITE);
        SetBkColor(dc, C_EXP_LIST);
        static HBRUSH s_listBr = NULL;
        if (!s_listBr) s_listBr = CreateSolidBrush(C_EXP_LIST);
        return (LRESULT)s_listBr;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT_WHITE);
        SetBkColor(dc, RGB(32, 32, 38));
        static HBRUSH s_expEditBr = NULL;
        if (!s_expEditBr) s_expEditBr = CreateSolidBrush(RGB(32, 32, 38));
        return (LRESULT)s_expEditBr;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT_MUTED);
        SetBkColor(dc, C_EXP_STATUS);
        static HBRUSH s_statBr = NULL;
        if (!s_statBr) s_statBr = CreateSolidBrush(C_EXP_STATUS);
        return (LRESULT)s_statBr;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == IDX_BACK)    { ExpGoBack(hwnd); return 0; }
        if (id == IDX_FWD)     { ExpGoFwd(hwnd); return 0; }
        if (id == IDX_UP)      { ExpGoUp(hwnd); return 0; }
        if (id == IDX_REFRESH) { ExpEnumerate(hwnd); return 0; }
        if (id == IDX_HOME)    { ExpNavigate(hwnd, L""); return 0; }
        if (id == IDX_NEWFOLDER) { ExpCreateNewFolder(hwnd); return 0; }
        if (id == IDX_CMDHERE)   { ExpOpenCmdHere(hwnd); return 0; }

        if (id == IDX_GO) {
            WCHAR buf[MAX_PATH];
            GetDlgItemTextW(hwnd, IDX_PATH, buf, MAX_PATH);
            if (buf[0]) ExpNavigate(hwnd, buf);
            return 0;
        }

        if (id == IDX_FILTER && code == EN_CHANGE) {
            EXPLORER *e = ExpGet(hwnd);
            if (e) {
                GetDlgItemTextW(hwnd, IDX_FILTER, e->filter, 64);
                ExpApplyFilter(hwnd);
            }
            return 0;
        }

        if (id == IDX_LIST && code == LBN_DBLCLK) {
            ExpOpenSelected(hwnd);
            return 0;
        }

        if (id == IDX_SIDE && (code == LBN_SELCHANGE || code == LBN_DBLCLK)) {
            EXPLORER *e = ExpGet(hwnd);
            int sel = (int)SendDlgItemMessageW(hwnd, IDX_SIDE, LB_GETCURSEL, 0, 0);
            if (e && sel >= 0 && sel < e->nPlaces) {
                const EXPITEM *pl = &e->places[sel];
                if (pl->path[0]) ExpNavigate(hwnd, pl->path);
                else ExpNavigate(hwnd, L"");
            }
            return 0;
        }
        break;
    }

    case WM_CONTEXTMENU: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_OPEN, L"Abrir");
        AppendMenuW(hMenu, MF_STRING, IDM_OPEN_NOTEPAD, L"Abrir no Bloco de Notas");
        AppendMenuW(hMenu, MF_STRING, IDM_CMD_HERE, L"Abrir Prompt de Comando Aqui");
        AppendMenuW(hMenu, MF_STRING, IDM_COPY_PATH, L"Copiar Caminho");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, IDM_NEW_FOLDER, L"Nova Pasta");
        AppendMenuW(hMenu, MF_STRING, IDM_REFRESH, L"Atualizar (F5)");

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == IDM_OPEN) ExpOpenSelected(hwnd);
        else if (cmd == IDM_OPEN_NOTEPAD) {
            EXPLORER *e = ExpGet(hwnd);
            if (e) {
                int sel = (int)SendDlgItemMessageW(hwnd, IDX_LIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < e->nFiltered) {
                    ShellExecuteW(hwnd, L"open", L"notepad.exe", e->items[e->filtered[sel]].path, NULL, SW_SHOWNORMAL);
                }
            }
        }
        else if (cmd == IDM_CMD_HERE) ExpOpenCmdHere(hwnd);
        else if (cmd == IDM_COPY_PATH) ExpCopySelectedPath(hwnd);
        else if (cmd == IDM_NEW_FOLDER) ExpCreateNewFolder(hwnd);
        else if (cmd == IDM_REFRESH) ExpEnumerate(hwnd);
        return 0;
    }

    case WM_SIZE:
        ExpLayout(hwnd);
        break;

    case WM_KEYDOWN:
        if (wParam == VK_F5) { ExpEnumerate(hwnd); return 0; }
        if (wParam == VK_BACK) { ExpGoUp(hwnd); return 0; }
        if (wParam == VK_RETURN) { ExpOpenSelected(hwnd); return 0; }
        if (wParam == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY: {
        EXPLORER *e = ExpGet(hwnd);
        if (e) {
            ExpFreeList(&e->items, &e->nItems, &e->capItems);
            ExpFreeList(&e->places, &e->nPlaces, &e->capPlaces);
            if (e->filtered) free(e->filtered);
            RemovePropW(GetDlgItem(hwnd, IDX_PATH), L"OldProc");
            RemovePropW(GetDlgItem(hwnd, IDX_FILTER), L"OldProc");
            free(e);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

static void OpenFileExplorer(const wchar_t *startPath) {
    WCHAR title[MAX_PATH + 48] = L"Explorador de Arquivos";
    if (startPath && startPath[0]) {
        const WCHAR *base = wcsrchr(startPath, L'\\');
        base = (base && base[1]) ? base + 1 : startPath;
        wsprintfW(title, L"%s - Explorador de Arquivos", base);
    }

    WCHAR *param = NULL;
    if (startPath && startPath[0]) {
        size_t n = lstrlenW(startPath) + 1;
        param = (WCHAR *)malloc(n * sizeof(WCHAR));
        if (param) lstrcpynW(param, startPath, (int)n);
    }

    int cw = 920, ch = 580;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (cw > sw - 20) cw = sw - 20;
    if (ch > sh - TASKBAR_H - 20) ch = sh - TASKBAR_H - 20;
    int cx = (sw - cw) / 2 + ((g_expCascade++ % 6) - 3) * 24;
    int cy = (sh - TASKBAR_H - ch) / 2 + ((g_expCascade % 6) - 3) * 24;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    HWND h = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"WinPEExplorerClass", title,
        WS_OVERLAPPEDWINDOW,
        cx, cy, cw, ch,
        NULL, NULL, GetModuleHandleW(NULL), param);

    if (param) free(param);

    if (h) {
        ShowWindow(h, SW_SHOW);
        UpdateWindow(h);
        SetForegroundWindow(h);
    }
}

/* =========================================================================
   Ponto de Entrada (WinMain)
   ========================================================================= */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    InitFonts();

    WNDCLASSW wcTb = {0};
    wcTb.lpfnWndProc = TaskbarWndProc;
    wcTb.hInstance = hInstance;
    wcTb.lpszClassName = L"WinPETaskbarClass";
    wcTb.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcTb.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!RegisterClassW(&wcTb)) return 1;

    WNDCLASSW wcExp = {0};
    wcExp.lpfnWndProc = ExpWndProc;
    wcExp.hInstance = hInstance;
    wcExp.lpszClassName = L"WinPEExplorerClass";
    wcExp.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcExp.hbrBackground = CreateSolidBrush(C_EXP_BG);
    wcExp.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!RegisterClassW(&wcExp)) return 1;

    WNDCLASSW wcMenu = {0};
    wcMenu.lpfnWndProc = MenuWndProc;
    wcMenu.hInstance = hInstance;
    wcMenu.lpszClassName = L"MenuIniciarClass";
    wcMenu.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcMenu.hbrBackground = CreateSolidBrush(C_MENU_BG);
    wcMenu.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!RegisterClassW(&wcMenu)) return 1;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_hwndTaskbar = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"WinPETaskbarClass", L"Taskbar",
        WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, screenH - TASKBAR_H, screenW, TASKBAR_H,
        NULL, NULL, hInstance, NULL);
    if (!g_hwndTaskbar) return 1;

    int menuW = (MENU_W > screenW - 20) ? (screenW - 20) : MENU_W;
    int menuH = (MENU_H > screenH - TASKBAR_H - 10) ? (screenH - TASKBAR_H - 10) : MENU_H;
    int menuX = 0;
    int menuY = screenH - TASKBAR_H - menuH;

    g_hwndMenu = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"MenuIniciarClass", L"Menu Iniciar",
        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        menuX, menuY, menuW, menuH,
        g_hwndTaskbar, NULL, hInstance, NULL);
    if (!g_hwndMenu) return 1;

    ShowWindow(g_hwndMenu, SW_HIDE);
    ShowWindow(g_hwndTaskbar, SW_SHOW);
    UpdateWindow(g_hwndTaskbar);

    RegisterHotKey(g_hwndTaskbar, 101, MOD_WIN, 'E');
    RegisterHotKey(g_hwndTaskbar, 100, MOD_WIN, 'S');
    RegisterHotKey(g_hwndTaskbar, 102, 0, VK_F1);

    g_hKbdHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                   GetModuleHandleW(NULL), 0);

    if (lpCmdLine && strstr(lpCmdLine, "exp")) {
        OpenFileExplorer(NULL);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);
    if (g_hKbdHook) UnhookWindowsHookEx(g_hKbdHook);
    UnregisterHotKey(g_hwndTaskbar, 101);
    UnregisterHotKey(g_hwndTaskbar, 100);
    UnregisterHotKey(g_hwndTaskbar, 102);

    CleanupFonts();
    return (int)msg.wParam;
}
