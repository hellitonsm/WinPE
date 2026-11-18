/* lnt_probe.c - testa carregar cada DLL do bundle e mostra o resultado na tela */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>

static HWND g_edit = NULL;

static void Log(const WCHAR *s) {
    FILE *f = _wfopen(L"X:\\libernt_probe_debug.log", L"a");
    if (f) { fwprintf(f, L"%s\n", s); fclose(f); }
    if (g_edit) {
        int n = (int)SendMessageW(g_edit, EM_GETLINECOUNT, 0, 0);
        SendMessageW(g_edit, EM_SETSEL, n, n);
        SendMessageW(g_edit, EM_REPLACESEL, FALSE, (LPARAM)s);
        SendMessageW(g_edit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_CREATE) {
        g_edit = CreateWindowExW(0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            8, 8, 760, 500, hwnd, NULL, ((CREATESTRUCTW *)l)->hInstance, NULL);
        HFONT f = CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"Consolas");
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)f, TRUE);
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int s) {
    static const WCHAR *names[] = {
        L"shell32.dll", L"shlwapi.dll", L"comctl32.dll",
        L"explorerframe.dll", L"gdiplus.dll", L"uxtheme.dll",
        L"shdocvw.dll", L"browseui.dll", L"windows.storage.dll", NULL
    };
    WCHAR line[512];
    (void)p; (void)c; (void)s;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = h; wc.lpszClassName = L"LNTProbeClass";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(WS_EX_TOPMOST, L"LNTProbeClass", L"LNT Probe",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 80, 780, 540, NULL, NULL, h, NULL);
    ShowWindow(w, SW_SHOW); UpdateWindow(w);

    for (int i = 0; names[i]; i++) {
        WCHAR path[MAX_PATH];
        wsprintfW(path, L"X:\\LiberNT\\%s", names[i]);
        HMODULE m = LoadLibraryW(path);
        if (m) { wsprintfW(line, L"OK   %s", names[i]); FreeLibrary(m); }
        else   { wsprintfW(line, L"FAIL %s  err=0x%08X", names[i], GetLastError()); }
        Log(line);
    }
    Log(L"probe concluido");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
