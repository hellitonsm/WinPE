/*
 * ReactOS Explorer
 *
 * Copyright 2006 - 2007 Thomas Weidenmueller <w3seek@reactos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "precomp.h"
#include <browseui_undoc.h>
#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
   WinPE debug overlay: mostra o andamento da inicializacao e excecoes
   numa janela na tela, para poder diagnosticar falhas antes do reboot.
   ------------------------------------------------------------------------- */
#define LNT_DEBUG_WINPE 1

#if LNT_DEBUG_WINPE
static HWND g_dbgEdit = NULL;

static void DbgLog(const WCHAR *fmt, ...)
{
    WCHAR buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 1023, fmt, ap);
    buf[1023] = 0;
    va_end(ap);

    FILE *f = _wfopen(L"X:\\libernt_explorer_debug.log", L"a");
    if (f)
    {
        fwprintf(f, L"%s\n", buf);
        fclose(f);
    }

    if (g_dbgEdit)
    {
        int n = (int)SendMessageW(g_dbgEdit, EM_GETLINECOUNT, 0, 0);
        SendMessageW(g_dbgEdit, EM_SETSEL, n, n);
        SendMessageW(g_dbgEdit, EM_REPLACESEL, FALSE, (LPARAM)buf);
        SendMessageW(g_dbgEdit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

static LRESULT CALLBACK DbgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_dbgEdit = CreateWindowExW(0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            4, 4, 764, 500, hwnd, NULL,
            ((CREATESTRUCTW *)lParam)->hInstance, NULL);
        if (g_dbgEdit)
        {
            HFONT f = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Consolas");
            SendMessageW(g_dbgEdit, WM_SETFONT, (WPARAM)f, TRUE);
        }
        break;
    case WM_SIZE:
        if (g_dbgEdit)
            MoveWindow(g_dbgEdit, 4, 4, LOWORD(lParam) - 8, HIWORD(lParam) - 8, TRUE);
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void DbgInitOverlay(HINSTANCE hInstance)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DbgWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LNTDebugClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc))
        return;

    HWND w = CreateWindowExW(WS_EX_TOPMOST, L"LNTDebugClass",
        L"LiberNT Explorer - Debug (WinPE)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 20, 20, 780, 540,
        NULL, NULL, hInstance, NULL);
    if (w)
    {
        ShowWindow(w, SW_SHOW);
        UpdateWindow(w);
    }
}

static LONG WINAPI DbgExceptionFilter(PEXCEPTION_POINTERS ep)
{
    DbgLog(L"EXCEPTION code=0x%08X address=0x%p",
           ep->ExceptionRecord->ExceptionCode,
           ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif /* LNT_DEBUG_WINPE */

HINSTANCE hExplorerInstance;
HANDLE hProcessHeap;
HKEY hkExplorer = NULL;
BOOL bExplorerIsShell = FALSE;

class CExplorerModule : public CComModule
{
public:
};

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

CExplorerModule gModule;
CAtlWinModule   gWinModule;

static VOID InitializeAtlModule(HINSTANCE hInstance, BOOL bInitialize)
{
    if (bInitialize)
    {
        gModule.Init(ObjectMap, hInstance, NULL);
    }
    else
    {
        gModule.Term();
    }
}

static void
InitializeServerAdminUI()
{
    HKEY hKey = SHGetShellKey(SHKEY_Root_HKCU | SHKEY_Key_Explorer, L"Advanced", TRUE);
    if (!hKey)
        return;

    DWORD value, size = sizeof(value), type;
    DWORD error = SHGetValueW(hKey, NULL, L"ServerAdminUI", &type, &value, &size);
    if (error || type != REG_DWORD || size != sizeof(value))
    {
        // The value doesn't exist or is invalid, calculate and apply a default value
        value = IsOS(OS_ANYSERVER) && IsUserAnAdmin();
        SHSetValueW(hKey, NULL, L"ServerAdminUI", REG_DWORD, &value, sizeof(value));
        if (value)
        {
            // TODO: Apply registry tweaks with RegInstallW; RegServerAdmin in the REGINST resource in shell32.
            #if !ROSPOLICY_SHELL_NODEFKEYBOARDCUES
            SystemParametersInfo(SPI_SETKEYBOARDCUES, 0, IntToPtr(TRUE), SPIF_SENDCHANGE | SPIF_UPDATEINIFILE);
            #endif
        }
    }
    RegCloseKey(hKey);
}

#if !WIN7_DEBUG_MODE
static BOOL
SetShellReadyEvent(IN LPCWSTR lpEventName)
{
    HANDLE hEvent;

    hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, lpEventName);
    if (hEvent != NULL)
    {
        SetEvent(hEvent);

        CloseHandle(hEvent);
        return TRUE;
    }

    return FALSE;
}

static VOID
HideMinimizedWindows(IN BOOL bHide)
{
    MINIMIZEDMETRICS mm;

    mm.cbSize = sizeof(mm);
    if (!SystemParametersInfoW(SPI_GETMINIMIZEDMETRICS, sizeof(mm), &mm, 0))
    {
        ERR("SystemParametersInfoW failed with %lu\n", GetLastError());
        return;
    }
    if (bHide)
        mm.iArrange |= ARW_HIDE;
    else
        mm.iArrange &= ~ARW_HIDE;
    if (!SystemParametersInfoW(SPI_SETMINIMIZEDMETRICS, sizeof(mm), &mm, 0))
        ERR("SystemParametersInfoW failed with %lu\n", GetLastError());
}
#endif

static BOOL
IsExplorerSystemShell()
{
    BOOL bIsSystemShell = TRUE; // Assume we are the system shell by default.
    WCHAR szPath[MAX_PATH];

    if (!GetModuleFileNameW(NULL, szPath, _countof(szPath)))
        return FALSE;

    LPWSTR szExplorer = PathFindFileNameW(szPath);

    HKEY hKeyWinlogon;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                      0, KEY_READ, &hKeyWinlogon) == ERROR_SUCCESS)
    {
        LSTATUS Status;
        DWORD dwType;
        WCHAR szShell[MAX_PATH];
        DWORD cbShell = sizeof(szShell);

        // TODO: Add support for paths longer than MAX_PATH
        Status = RegQueryValueExW(hKeyWinlogon, L"Shell", 0, &dwType, (LPBYTE)szShell, &cbShell);
        if (Status == ERROR_SUCCESS)
        {
            if ((dwType != REG_SZ && dwType != REG_EXPAND_SZ) || !StrStrIW(szShell, szExplorer))
                bIsSystemShell = FALSE;
        }

        RegCloseKey(hKeyWinlogon);
    }

    return bIsSystemShell;
}

static IShellWindows *g_pShellWindows = NULL;
static DWORD g_dwShellWindowsCookie = 0;

class CShellWindowsClassFactory : public IClassFactory
{
public:
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppv = static_cast<IClassFactory *>(this);
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override
    {
        return 2;
    }

    STDMETHOD_(ULONG, Release)() override
    {
        return 1;
    }

    STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;
        *ppv = NULL;
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;
        if (!g_pShellWindows)
            return E_UNEXPECTED;
        return g_pShellWindows->QueryInterface(riid, ppv);
    }

    STDMETHOD(LockServer)(BOOL fLock) override
    {
        return S_OK;
    }
};

static CShellWindowsClassFactory g_ShellWindowsClassFactory;

static VOID
RegisterShellWindows(VOID)
{
    typedef HRESULT (WINAPI *PFN_DLLGETCLASSOBJECT)(REFCLSID, REFIID, LPVOID *);
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    PFN_DLLGETCLASSOBJECT pfnGetClassObject =
        hShell32 ? (PFN_DLLGETCLASSOBJECT)GetProcAddress(hShell32, "DllGetClassObject") : NULL;
    CComPtr<IClassFactory> pFactory;

    if (!pfnGetClassObject ||
        FAILED(pfnGetClassObject(CLSID_ShellWindows, IID_PPV_ARG(IClassFactory, &pFactory))) ||
        FAILED(pFactory->CreateInstance(NULL, IID_PPV_ARG(IShellWindows, &g_pShellWindows))))
    {
        return;
    }

    if (FAILED(CoRegisterClassObject(CLSID_ShellWindows, &g_ShellWindowsClassFactory, CLSCTX_LOCAL_SERVER,
                                     REGCLS_MULTIPLEUSE, &g_dwShellWindowsCookie)))
    {
        g_pShellWindows->Release();
        g_pShellWindows = NULL;
        g_dwShellWindowsCookie = 0;
    }
}

static VOID
RevokeShellWindows(VOID)
{
    if (g_dwShellWindowsCookie)
    {
        CoRevokeClassObject(g_dwShellWindowsCookie);
        g_dwShellWindowsCookie = 0;
    }
    if (g_pShellWindows)
    {
        g_pShellWindows->Release();
        g_pShellWindows = NULL;
    }
}

#if !WIN7_COMPAT_MODE
static INT
StartWithCommandLine(IN HINSTANCE hInstance)
{
    BOOL b = FALSE;
    EXPLORER_CMDLINE_PARSE_RESULTS parseResults = { 0 };

    if (SHExplorerParseCmdLine(&parseResults))
        b = SHCreateFromDesktop(&parseResults);

    if (parseResults.strPath)
        SHFree(parseResults.strPath);

    if (parseResults.pidlPath)
        ILFree(parseResults.pidlPath);

    if (parseResults.pidlRoot)
        ILFree(parseResults.pidlRoot);

    return b;
}
#endif

static INT
StartWithDesktop(IN HINSTANCE hInstance)
{
#if LNT_DEBUG_WINPE
    DbgLog(L"StartWithDesktop: iniciando");
#endif
    InitializeAtlModule(hInstance, TRUE);

    if (RegOpenKeyW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
        &hkExplorer) != ERROR_SUCCESS)
    {
        WCHAR Message[256];
        LoadStringW(hInstance, IDS_STARTUP_ERROR, Message, _countof(Message));
#if LNT_DEBUG_WINPE
        DbgLog(L"ERRO: HKCU\\...\\Explorer nao abriu (GetLastError=%lu)", GetLastError());
#endif
        MessageBox(NULL, Message, NULL, MB_ICONERROR);
        return 1;
    }

#if LNT_DEBUG_WINPE
    DbgLog(L"HKCU\\...\\Explorer OK");
#endif
    hExplorerInstance = hInstance;
    hProcessHeap = GetProcessHeap();

    g_TaskbarSettings.Load();

    InitCommonControls();
    OleInitialize(NULL);
#if LNT_DEBUG_WINPE
    DbgLog(L"OleInitialize OK");
#endif

#if !WIN7_COMPAT_MODE
    /* Initialize shell dde support */
    _ShellDDEInit(TRUE);
#endif

    /* Initialize shell icons */
    FileIconInit(TRUE);

    /* Initialize CLSID_ShellWindows class */
    RegisterShellWindows();

    InitializeServerAdminUI();

    CComPtr<ITrayWindow> Tray;
    CreateTrayWindow(&Tray);
#if LNT_DEBUG_WINPE
    DbgLog(L"CreateTrayWindow: Tray=0x%p", (void *)Tray.p);
#endif

#if !WIN7_DEBUG_MODE
    /* This not only hides the minimized window captions in the bottom
    left screen corner, but is also needed in order to receive
    HSHELL_* notification messages (which are required for taskbar
    buttons to work right) */
    HideMinimizedWindows(TRUE);

    ProcessRunOnceItems(); // Must be executed before the desktop is created

    HANDLE hShellDesktop = NULL;
    if (Tray != NULL)
        hShellDesktop = DesktopCreateWindow(Tray);
#if LNT_DEBUG_WINPE
    DbgLog(L"DesktopCreateWindow: hShellDesktop=0x%p", hShellDesktop);
#endif

    /* WinXP: Notify msgina to hide the welcome screen */
    if (!SetShellReadyEvent(L"msgina: ShellReadyEvent"))
        SetShellReadyEvent(L"Global\\msgina: ShellReadyEvent");

    if (DoStartStartupItems(Tray))
    {
        ProcessStartupItems();
        DoFinishStartupItems();
    }
    ReleaseStartupMutex(); // For ProcessRunOnceItems
#endif

    if (Tray != NULL)
    {
#if LNT_DEBUG_WINPE
        DbgLog(L"Entrando em TrayMessageLoop");
#endif
        TrayMessageLoop(Tray);
#if LNT_DEBUG_WINPE
        DbgLog(L"TrayMessageLoop terminou");
#endif
#if !WIN7_DEBUG_MODE
        HideMinimizedWindows(FALSE);
#endif
    }

#if !WIN7_DEBUG_MODE
    if (hShellDesktop != NULL)
        DesktopDestroyShellWindow(hShellDesktop);
#endif

    RevokeShellWindows();
    OleUninitialize();

    RegCloseKey(hkExplorer);
    hkExplorer = NULL;

    InitializeAtlModule(hInstance, FALSE);

    return 0;
}

EXTERN_C INT WINAPI
Explorer_Main(IN HINSTANCE hInstance,
              IN LPTSTR lpCmdLine)
{
    /*
     * The shell consumes physical system metrics and must opt out of
     * DPI-unaware metric virtualization before creating any windows.
     */
    if (!IsProcessDPIAware())
        SetProcessDPIAware();

    /*
    * Set our shutdown parameters: we want to shutdown the very last,
    * but before any TaskMgr instance (which has a shutdown level of 1).
    */
    SetProcessShutdownParameters(2, 0);

    InitRSHELL();

    TRACE("Explorer starting... Command line: %S\n", lpCmdLine);
#if LNT_DEBUG_WINPE
    DbgLog(L"Explorer_Main: InitRSHELL ok, cmdline=%s", lpCmdLine);
#endif

#if !WIN7_COMPAT_MODE
    bExplorerIsShell = (GetShellWindow() == NULL) && IsExplorerSystemShell();
#if LNT_DEBUG_WINPE
    DbgLog(L"bExplorerIsShell=%d (GetShellWindow=0x%p)", bExplorerIsShell, GetShellWindow());
#endif

    if (!bExplorerIsShell)
    {
        return StartWithCommandLine(hInstance);
    }
#else
    bExplorerIsShell = IsExplorerSystemShell();
#endif

    return StartWithDesktop(hInstance);
}

INT WINAPI
_tWinMain(IN HINSTANCE hInstance,
          IN HINSTANCE hPrevInstance,
          IN LPTSTR lpCmdLine,
          IN INT nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

#if LNT_DEBUG_WINPE
    DbgInitOverlay(hInstance);
    SetUnhandledExceptionFilter(DbgExceptionFilter);
    DbgLog(L"WinMain: iniciado (PID=%lu)", GetCurrentProcessId());
#endif

    __try
    {
        INT ret = Explorer_Main(hInstance, lpCmdLine);
#if LNT_DEBUG_WINPE
        DbgLog(L"Explorer_Main retornou %d", ret);
#endif
        return ret;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
#if LNT_DEBUG_WINPE
        DbgLog(L"EXCEPTION no WinMain: code=0x%08X", GetExceptionCode());
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#endif
        return 1;
    }
}
