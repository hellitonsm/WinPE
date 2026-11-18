/* lnt_launch.c - Launcher de debug para o explorer.exe do LiberNT no WinPE.
   Evita o reboot do WinPE quando o explorer falha: se o processo terminar,
   mostra o codigo de saida na tela e abre cmd.exe para manter o sistema vivo.
   Compile: x86_64-w64-mingw32-gcc wpetool/source/lnt_launch.c -o lnt_launch.exe -mwindows
*/
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>

static void Log(const WCHAR *msg) {
    FILE *f = _wfopen(L"X:\\libernt_launch_debug.log", L"a");
    if (f) { fwprintf(f, L"%s\n", msg); fclose(f); }
}

static void Show(const WCHAR *msg) {
    Log(msg);
    MessageBoxW(NULL, msg, L"LNT Launcher (WinPE)", MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmdline, int show) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR msg[512];

    (void)h; (void)p; (void)cmdline; (void)show;

    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    Log(L"Launcher iniciado");

    WCHAR exe[MAX_PATH] = L"X:\\LiberNT\\explorer.exe";
    if (!CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        wsprintfW(msg, L"CreateProcess falhou: err=%lu", GetLastError());
        Show(msg);
    } else {
        Log(L"explorer.exe iniciado, aguardando...");
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        wsprintfW(msg, L"explorer.exe terminou (exit=0x%08X). Abrindo cmd.exe.", code);
        Show(msg);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessW(L"cmd.exe", NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                       NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        Show(L"Nao foi possivel abrir cmd.exe");
        Sleep(INFINITE);
    }
    return 0;
}
