#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <string>
#include <thread>
#include <vector>

using Text = std::wstring;
constexpr UINT WM_COPY_READY = WM_APP + 14;
constexpr wchar_t EXPLORER_CLASS[] = L"AzulPE.Explorador";

enum Icon { BRAND, FOLDER, DOCS, PC, NET, CONSOLE, NOTE, DRIVE, USB, FILEICON, INFO, DESK, APPS, POWER, ICON_COUNT };
enum class Glyph { left, right, up, down, search, refresh, plus, copy, paste, pen, trash, details, minus, maximize, restore, close, network, desktop, power, check };
enum class StartAction { none, explorer, terminal, notepad, accessories, scriptFolder, desktop, docs, pc, network, about, tools, taskmgr, powerMenu, shutdown, restart, exit };

struct Fonts { HFONT tiny{}, small{}, body{}, strong{}, caption{}, title{}, heading{}; };
extern Fonts ft;
extern HINSTANCE app;
extern HWND desktopWindow, taskbarWindow, startWindow, explorerWindow, textWindow;
extern int screenW, screenH, barH;
extern double uiScale;
extern bool winPE;
extern Text programFolder, systemFolder, windowsFolder, documentsFolder, desktopFolder;

constexpr COLORREF col(unsigned hex) { return RGB(hex >> 16, (hex >> 8) & 255, hex & 255); }
inline int u(int logical) { return static_cast<int>(std::lround(logical * uiScale)); }
inline RECT area(int x, int y, int w, int h) { return RECT{x, y, x+w, y+h}; }
inline int width(RECT r) { return r.right-r.left; }
inline int height(RECT r) { return r.bottom-r.top; }
inline RECT inset(RECT r, int n) { InflateRect(&r, -n, -n); return r; }
inline RECT at(int x, int y, int w, int h) { return area(u(x), u(y), u(w), u(h)); }
inline bool in(RECT r, POINT p) { return PtInRect(&r, p) != 0; }
inline POINT mousePoint(LPARAM p) { return POINT{GET_X_LPARAM(p), GET_Y_LPARAM(p)}; }
inline RECT client(HWND h) { RECT r{}; GetClientRect(h,&r); return r; }
inline Text getText(HWND h) { int n = GetWindowTextLengthW(h); Text s(static_cast<size_t>(n)+1, L'\0'); int k = GetWindowTextW(h,s.data(),n+1); s.resize(k); return s; }
inline bool exists(const Text& p) { return !p.empty() && GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
inline void redraw(HWND w) { if (w) InvalidateRect(w, nullptr, FALSE); }

struct Paint {
    HWND hwnd;
    PAINTSTRUCT ps{};
    HDC target{}, dc{};
    HBITMAP canvas{}, old{};
    RECT r{};
    explicit Paint(HWND w);
    ~Paint();
    Paint(const Paint&) = delete;
    Paint& operator=(const Paint&) = delete;
};
void solid(HDC d, RECT r, unsigned hex);
void border(HDC d, RECT r, unsigned hex);
void tone(HDC d, RECT r, unsigned top, unsigned bottom, bool horizontal = false);
void roundBox(HDC d, RECT r, unsigned fill, unsigned line, int radius = 4);
void rule(HDC d, int x, int y, int w, unsigned hex);
void label(HDC d, const Text& text, RECT r, HFONT font, unsigned color, UINT flags = DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
void trackingLabel(HDC d, const Text& text, RECT r, HFONT font, unsigned hex, int pixels = 1);
int textWidth(HDC d, const Text& text, HFONT font);
void icon(HDC d, Icon v, int x, int y, int size);
HICON nativeIcon(Icon v, int size, int canvasW, int canvasH);
void glyph(HDC d, Glyph v, int x, int y, int size, unsigned hex);
void setupPaint();
void freePaint();
void drawWallpaper(HDC dc, RECT r);
void dropWallpaper();
void trackMouse(HWND w);
void place(HWND h, RECT r, bool show = true);
Text folded(Text s);
Text apiPath(const Text& path);
Text rootedPath(const Text& typed, const Text& base = {});
Text parentPath(const Text& path);
Text leafName(const Text& path);
Text join(const Text& p, const Text& n);
Text prettyBytes(uint64_t size);
Text prettyDate(FILETIME ft);
void alert(HWND owner, const Text& body, const Text& title = L"Azul PE", UINT flags = MB_OK|MB_ICONINFORMATION);
void errorAlert(HWND owner, const Text& why, const Text& path, DWORD error);
bool launch(const Text& exe, const Text& arguments, const Text& workDir, HWND owner);
Text sysTool(const Text& name);
bool requestName(HWND owner, const Text& caption, const Text& body, const Text& current, Text& answer);
void openItem(HWND owner, const Text& p, DWORD attributes);
void showText(const Text& p, HWND owner);

struct DriveInfo {
    Text path, name, type, detail;
    Icon icon = DRIVE;
    uint64_t free = 0, total = 0;
};
std::vector<DriveInfo> readDrives();
struct Entry {
    Text name, kind, dateLabel, sizeLabel;
    DWORD attr;
    FILETIME date;
    uint64_t size;
    bool folder() const { return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};
DWORD readFolder(const Text& p, std::vector<Entry>& entries);
DWORD copyItem(const Text& from, const Text& to, std::atomic_bool& cancel);

void registerExplorer();
void fitExplorerToDesktop();
void showExplorer(const Text& path = {}, bool toggle = false);
void showStart(bool show);
void refreshTaskbar();
void desktopOnly();
void act(StartAction a, HWND owner);
void quitShell(HWND owner);
void powerPopup(HWND owner, POINT anchor);
bool isCopyActive();
bool routeExplorer(MSG& m);
