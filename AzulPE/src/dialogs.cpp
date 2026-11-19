#include "base.hpp"

namespace {
struct Input { const Text *title,*description,*initial; Text* output; HWND edit{}; };
HWND control(HWND parent,const wchar_t* type,const wchar_t* text,DWORD style,DWORD extended,RECT r,int id,HFONT font) {
    HWND h=CreateWindowExW(extended,type,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,width(r),height(r),parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app,nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return h;
}
void center(HWND w,HWND parent,int ww,int hh) {
    RECT p{0,0,screenW,screenH-barH};
    if(parent && !IsIconic(parent)) GetWindowRect(parent,&p);
    SetWindowPos(w,nullptr,std::clamp<int>((p.left+p.right-ww)/2,0,(std::max)(0,screenW-ww)),
        std::clamp<int>((p.top+p.bottom-hh)/2,0,(std::max)(0,screenH-barH-hh)),ww,hh,SWP_NOZORDER|SWP_NOACTIVATE);
}
INT_PTR CALLBACK nameDlg(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    auto* i=reinterpret_cast<Input*>(GetWindowLongPtrW(w,DWLP_USER));
    if(m==WM_INITDIALOG) {
        i=reinterpret_cast<Input*>(lp); SetWindowLongPtrW(w,DWLP_USER,lp);
        SetWindowTextW(w,i->title->c_str()); center(w,GetParent(w),u(425),u(159));
        auto r=client(w);
        control(w,L"STATIC",i->description->c_str(),SS_LEFT,0,area(u(15),u(13),r.right-u(30),u(21)),0,ft.body);
        i->edit=control(w,L"EDIT",i->initial->c_str(),WS_TABSTOP|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,area(u(15),u(38),r.right-u(30),u(26)),100,ft.body);
        SendMessageW(i->edit,EM_SETLIMITTEXT,255,0);
        control(w,L"BUTTON",L"OK",BS_DEFPUSHBUTTON|WS_TABSTOP,0,area(r.right-u(190),r.bottom-u(44),u(82),u(28)),IDOK,ft.strong);
        control(w,L"BUTTON",L"Cancelar",BS_PUSHBUTTON|WS_TABSTOP,0,area(r.right-u(100),r.bottom-u(44),u(85),u(28)),IDCANCEL,ft.body);
        SetFocus(i->edit); SendMessageW(i->edit,EM_SETSEL,0,-1); return FALSE;
    }
    if(m==WM_COMMAND && i) {
        if(LOWORD(wp)==IDOK) {*i->output=getText(i->edit); EndDialog(w,IDOK);return TRUE;}
        if(LOWORD(wp)==IDCANCEL) {EndDialog(w,IDCANCEL);return TRUE;}
    }
    if(m==WM_CLOSE) {EndDialog(w,IDCANCEL);return TRUE;}
    return FALSE;
}
constexpr wchar_t VIEW_CLASS[]=L"AzulPE.Leitura";
HWND textEdit{};
Text viewerFooter;
LRESULT CALLBACK textProc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
        case WM_CREATE: textEdit=control(w,L"EDIT",L"",WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOHSCROLL|ES_AUTOVSCROLL,0,at(0,0,20,20),1,ft.body); return 0;
        case WM_SIZE: {auto r=client(w);place(textEdit,area(0,0,r.right,std::max<int>(1,r.bottom-u(29))));redraw(w);return 0;}
        case WM_SETFOCUS: SetFocus(textEdit);return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {Paint p(w);solid(p.dc,p.r,0xf4f8fd);rule(p.dc,0,p.r.bottom-u(29),p.r.right,0xc3d5e8);label(p.dc,viewerFooter,area(u(12),p.r.bottom-u(28),p.r.right-u(24),u(28)),ft.small,0x446382);return 0;}
        case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: SetTextColor(reinterpret_cast<HDC>(wp),col(0x192f4b)); SetBkColor(reinterpret_cast<HDC>(wp),col(0xffffff));return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        case WM_DESTROY: textWindow=nullptr; textEdit=nullptr; return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
bool requestName(HWND owner,const Text& caption,const Text& body,const Text& current,Text& answer) {
    struct alignas(4) Dialog { DLGTEMPLATE win; WORD menu, type, title; } x{};
    x.win.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME; x.win.cx=235; x.win.cy=86;
    Input i{&caption,&body,&current,&answer};
    return DialogBoxIndirectParamW(app,&x.win,owner,nameDlg,reinterpret_cast<LPARAM>(&i))==IDOK;
}
void showText(const Text& p,HWND owner) {
    HANDLE h=CreateFileW(apiPath(p).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE) {errorAlert(owner,L"Não foi possível ler o arquivo.",p,GetLastError());return;}
    constexpr DWORD cap=512*1024;
    std::vector<char> data(cap+1); DWORD got=0;
    BOOL ok=ReadFile(h,data.data(),cap+1,&got,nullptr); auto err=GetLastError(); CloseHandle(h);
    if(!ok) {errorAlert(owner,L"Não foi possível ler o arquivo.",p,err);return;}
    const bool partial=got>cap; got=(std::min)(got,cap);
    Text text; Text encoding=L"UTF-8";
    const auto* b=reinterpret_cast<const unsigned char*>(data.data());
    if(got>=2 && ((b[0]==0xff && b[1]==0xfe)||(b[0]==0xfe && b[1]==0xff))) {
        const bool little=b[0]==0xff; encoding=little?L"UTF-16 LE":L"UTF-16 BE";
        text.reserve(got/2);
        for(size_t i=2;i+1<got;i+=2) text.push_back(static_cast<wchar_t>(little?b[i]|(b[i+1]<<8):(b[i]<<8)|b[i+1]));
    } else {
        const int off=(got>=3 && b[0]==0xef && b[1]==0xbb && b[2]==0xbf)?3:0;
        const int size=static_cast<int>(got)-off;
        UINT cp=CP_UTF8; DWORD flags=MB_ERR_INVALID_CHARS;
        if(size>0) {
            int n=MultiByteToWideChar(cp,flags,data.data()+off,size,nullptr,0);
            if(!n) {cp=1252; flags=0;encoding=L"Windows-1252";n=MultiByteToWideChar(cp,flags,data.data()+off,size,nullptr,0);}
            if(n>0) {text.resize(n); MultiByteToWideChar(cp,flags,data.data()+off,size,text.data(),n);}
        }
    }
    Text lines; lines.reserve(text.size());
    wchar_t prev=0;
    for(auto c:text) {
        if(c==L'\n' && prev!=L'\r') lines+=L'\r';
        if(c!=L'\n' && prev==L'\r') lines+=L'\n';
        lines+=c?c:L'·'; prev=c;
    }
    if(prev==L'\r')lines+=L'\n';
    if(!textWindow) {
        WNDCLASSW cls{}; cls.hInstance=app;cls.lpszClassName=VIEW_CLASS;cls.lpfnWndProc=textProc;
        cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(app,MAKEINTRESOURCEW(1)); RegisterClassW(&cls);
        textWindow=CreateWindowExW(WS_EX_APPWINDOW,VIEW_CLASS,L"Leitor de texto",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            30,30,u(650),u(450),nullptr,nullptr,app,nullptr);
        if(!textWindow) return;
        center(textWindow,owner,(std::min)(screenW-u(20),u(660)),(std::min)(screenH-barH-u(12),u(450)));
    }
    SetWindowTextW(textWindow,(leafName(p)+L" — Leitor de texto").c_str());
    viewerFooter=L"Somente leitura    ·    "+encoding+(partial?L"    ·    Exibindo os primeiros 512 KB":L"");
    SendMessageW(textEdit,EM_SETLIMITTEXT,static_cast<WPARAM>(lines.size()+1024),0);
    SetWindowTextW(textEdit,lines.c_str()); SendMessageW(textEdit,EM_SETSEL,0,0);
    if(IsIconic(textWindow)) ShowWindow(textWindow,SW_RESTORE); else ShowWindow(textWindow,SW_SHOW);
    SetForegroundWindow(textWindow); SetFocus(textEdit); redraw(textWindow);
}
void openItem(HWND owner,const Text& path,DWORD attributes) {
    if(attributes&FILE_ATTRIBUTE_DIRECTORY) {showExplorer(path);return;}
    const Text name=leafName(path); auto dot=name.rfind(L'.'); Text ext=dot==Text::npos?L"":folded(name.substr(dot));
    const std::array<Text,11> textual{L".txt",L".log",L".ini",L".md",L".csv",L".json",L".xml",L".reg",L".config",L".inf",L".nfo"};
    if(std::find(textual.begin(),textual.end(),ext)!=textual.end()) {showText(path,owner);return;}
    const std::array<Text,14> runnable{L".exe",L".com",L".bat",L".cmd",L".msi",L".lnk",L".url",L".ps1",L".vbs",L".js",L".scr",L".wsf",L".hta",L".cpl"};
    if(std::find(runnable.begin(),runnable.end(),ext)!=runnable.end() &&
        MessageBoxW(owner,(L"Este arquivo pode executar código. Abra somente se confiar na sua origem.\n\n"+path+L"\n\nDeseja continuar?").c_str(),
        L"Abrir arquivo",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING|MB_SETFOREGROUND)!=IDYES) return;
    if(ext==L".exe" || ext==L".com") {launch(path,{},parentPath(path),owner);return;}
    Text dir=parentPath(path);
    SHELLEXECUTEINFOW s{}; s.cbSize=sizeof(s); s.hwnd=owner; s.fMask=SEE_MASK_FLAG_NO_UI|SEE_MASK_NOASYNC;
    s.lpVerb=L"open";s.lpFile=path.c_str();s.lpDirectory=dir.c_str();s.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&s)) alert(owner,L"Não há um aplicativo disponível para abrir este arquivo, ou a associação falhou.\n\n"+path);
}
