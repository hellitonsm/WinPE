#include "base.hpp"
#include <cstring>

Text folded(Text s) {
    if(!s.empty()) CharLowerBuffW(s.data(),static_cast<DWORD>(s.size()));
    for(auto& c:s) switch(c) {
        case L'á': case L'à': case L'â': case L'ã': case L'ä': c=L'a'; break;
        case L'é': case L'è': case L'ê': case L'ë': c=L'e'; break;
        case L'í': case L'ì': case L'î': case L'ï': c=L'i'; break;
        case L'ó': case L'ò': case L'ô': case L'õ': case L'ö': c=L'o'; break;
        case L'ú': case L'ù': case L'û': case L'ü': c=L'u'; break;
        case L'ç': c=L'c'; break;
    }
    return s;
}
Text join(const Text& p,const Text& n) { return p.empty()?n:p+(p.back()==L'\\'?L"":L"\\")+n; }
static size_t rootSize(const Text& p) {
    if(p.size()>=3 && p[1]==L':' && p[2]==L'\\') return 3;
    if(p.rfind(L"\\\\",0)==0) {
        auto server=p.find(L'\\',2); if(server==Text::npos) return 0;
        auto share=p.find(L'\\',server+1);
        if(server+1>=p.size()) return 0;
        return share==Text::npos?p.size():share+1;
    }
    return 0;
}
static Text unprefix(Text x) {
    if(x.rfind(L"\\\\?\\UNC\\",0)==0) return L"\\\\"+x.substr(8);
    if(x.rfind(L"\\\\?\\",0)==0 && x.size()>=7 && x[5]==L':') return x.substr(4);
    return x;
}
Text apiPath(const Text& p) {
    if(p.rfind(L"\\\\?\\",0)==0) return p;
    return p.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+p.substr(2):L"\\\\?\\"+p;
}
Text rootedPath(const Text& typed,const Text& base) {
    auto a=typed.find_first_not_of(L" \t\r\n"), b=typed.find_last_not_of(L" \t\r\n");
    if(a==Text::npos) return {};
    Text x=typed.substr(a,b-a+1);
    if(x.size()>1 && x.front()==L'\"' && x.back()==L'\"') x=x.substr(1,x.size()-2);
    DWORD chars=ExpandEnvironmentStringsW(x.c_str(),nullptr,0);
    if(chars && chars<32768) {Text env(chars,L'\0'); if(ExpandEnvironmentStringsW(x.c_str(),env.data(),chars)==chars) {env.pop_back(); x=std::move(env);}}
    for(auto& c:x) if(c==L'/') c=L'\\';
    x=unprefix(x);
    if(x.rfind(L"\\\\.\\",0)==0 || x.rfind(L"\\\\?\\",0)==0 || x.find_first_of(L"\"*?<>|")!=Text::npos) return {};
    if(x.size()==2 && x[1]==L':') x+=L'\\';
    if(!rootSize(x)) {
        if(base.empty() || x.empty() || x.find(L':')!=Text::npos || x.rfind(L"\\\\",0)==0) return {};
        if(x.front()==L'\\') x=join(base.substr(0,rootSize(base)),x.substr(1));
        else x=join(base,x);
    }
    DWORD n=GetFullPathNameW(x.c_str(),0,nullptr,nullptr);
    if(!n || n>32765) return {};
    Text r(n,L'\0'); DWORD m=GetFullPathNameW(x.c_str(),n,r.data(),nullptr);
    if(!m || m>=n) return {};
    r.resize(m); const auto root=rootSize(r);
    if(!root || r.find(L':', r[1]==L':'?2:0)!=Text::npos) return {};
    while(r.size()>root && r.back()==L'\\') r.pop_back();
    if(r.size()>1 && r[1]==L':') r[0]=towupper(r[0]);
    if(r.size()==root && r.back()!=L'\\') r+=L'\\';
    return r;
}
Text parentPath(const Text& p) {
    auto root=rootSize(p); if(!root || p.size()<=root) return {};
    size_t n=p.rfind(L'\\'); if(n==Text::npos) return {};
    return p.substr(0,(std::max)(n,root));
}
Text leafName(const Text& p) {
    if(p.empty() || p.size()<=rootSize(p)) return p;
    auto i=p.rfind(L'\\'); return i==Text::npos?p:p.substr(i+1);
}
Text prettyBytes(uint64_t n) {
    if(n<1024) return std::to_wstring(n)+(n==1?L" byte":L" bytes");
    static const wchar_t* suffix[]={L"KB",L"MB",L"GB",L"TB",L"PB",L"EB"};
    double d=static_cast<double>(n)/1024; int i=0;
    while(d>=1024 && i<5) {d/=1024; ++i;}
    wchar_t b[64]; swprintf(b,64,d<100?L"%.1f %ls":L"%.0f %ls",d,suffix[i]);
    Text s=b; std::replace(s.begin(),s.end(),L'.',L','); return s;
}
Text prettyDate(FILETIME date) {
    FILETIME local{}; SYSTEMTIME x{}; wchar_t b[48]{};
    if(!FileTimeToLocalFileTime(&date,&local) || !FileTimeToSystemTime(&local,&x) || x.wYear<1980) return L"—";
    swprintf(b,48,L"%02u/%02u/%04u  %02u:%02u",x.wDay,x.wMonth,x.wYear,x.wHour,x.wMinute); return b;
}
void alert(HWND owner,const Text& body,const Text& title,UINT flags) {
    MessageBoxW(owner,body.c_str(),title.c_str(),flags|MB_SETFOREGROUND);
}
void errorAlert(HWND owner,const Text& why,const Text& path,DWORD code) {
    wchar_t* msg=nullptr;
    DWORD k=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,code,0,reinterpret_cast<LPWSTR>(&msg),0,nullptr);
    Text reason;
    switch(code) {
        case ERROR_ACCESS_DENIED: reason=L"Acesso negado."; break;
        case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: reason=L"O arquivo ou caminho não foi encontrado."; break;
        case ERROR_ALREADY_EXISTS: case ERROR_FILE_EXISTS: reason=L"Já existe um item com esse nome no destino. Nada foi sobrescrito."; break;
        case ERROR_DIR_NOT_EMPTY: reason=L"A pasta não está vazia. O Azul PE não exclui pastas recursivamente."; break;
        case ERROR_NOT_SUPPORTED: reason=L"Esta operação não segue links, junções ou pontos de montagem."; break;
        default: reason=k?Text(msg,k):L"Erro do Windows: "+std::to_wstring(code); break;
    }
    if(msg) LocalFree(msg);
    alert(owner,why+(path.empty()?L"":L"\n\n"+path)+L"\n\n"+reason,L"Azul PE",MB_OK|MB_ICONWARNING);
}
Text sysTool(const Text& name) { Text p=join(systemFolder,name); return exists(p)?p:Text{}; }
bool launch(const Text& exe,const Text& args,const Text& dir,HWND owner) {
    if(exe.empty()) {alert(owner,L"Esta ferramenta não está disponível nesta imagem do Windows.");return false;}
    Text cmd=L"\""+exe+L"\""+(args.empty()?L"":L" "+args);
    STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
    const DWORD flags=winPE ? 0 : (folded(leafName(exe))==L"cmd.exe"?CREATE_NEW_CONSOLE:0);
    if(CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,flags,nullptr,dir.empty()?nullptr:dir.c_str(),&si,&pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
    }
    const DWORD code=GetLastError();
    // Executáveis podem vir de caminhos que ultrapassam MAX_PATH.
    if(exe.size()>=MAX_PATH && exe.find(L'"')==Text::npos) {
        Text extended=apiPath(exe), extendedDir=dir.empty()?dir:apiPath(dir);
        if(CreateProcessW(extended.c_str(),cmd.data(),nullptr,nullptr,FALSE,flags,nullptr,extendedDir.empty()?nullptr:extendedDir.c_str(),&si,&pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
        }
    }
    errorAlert(owner,L"Não foi possível abrir a ferramenta.",exe,code);return false;
}
std::vector<DriveInfo> readDrives() {
    std::vector<DriveInfo> v;
    const DWORD bits=GetLogicalDrives();
    for(unsigned i=0;i<26;++i) if(bits&(1u<<i)) {
        DriveInfo d;
        d.path=Text(1,static_cast<wchar_t>(L'A'+i))+L":\\";
        auto type=GetDriveTypeW(d.path.c_str());
        Text labelText; wchar_t volume[128]{};
        if(type==DRIVE_FIXED || type==DRIVE_RAMDISK || type==DRIVE_REMOVABLE) {
            DWORD previous=0; BOOL changed=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&previous);
            if(GetVolumeInformationW(d.path.c_str(),volume,128,nullptr,nullptr,nullptr,nullptr,0)) labelText=volume;
            ULARGE_INTEGER free{}, total{}, caller{};
            if(GetDiskFreeSpaceExW(d.path.c_str(),&caller,&total,&free)) { d.free=free.QuadPart; d.total=total.QuadPart; }
            if(changed) SetThreadErrorMode(previous,nullptr);
        }
        switch(type) {
            case DRIVE_REMOVABLE: d.type=L"Unidade removível"; d.icon=USB; break;
            case DRIVE_REMOTE: d.type=L"Unidade de rede"; d.icon=NET; break;
            case DRIVE_CDROM: d.type=L"Unidade óptica"; break;
            case DRIVE_RAMDISK: d.type=L"Disco RAM"; break;
            case DRIVE_FIXED: d.type=L"Disco local"; break;
            default: d.type=L"Unidade"; break;
        }
        if(winPE && d.path.substr(0,2)==windowsFolder.substr(0,2) && labelText.empty()) labelText=L"Ambiente WinPE";
        d.name=(labelText.empty()?d.type:labelText)+L" ("+d.path.substr(0,2)+L")";
        d.detail=d.total?prettyBytes(d.free)+L" livres de "+prettyBytes(d.total):d.type;
        v.push_back(std::move(d));
    }
    return v;
}
DWORD readFolder(const Text& p,std::vector<Entry>& entries) {
    entries.clear();
    WIN32_FIND_DATAW f{}; const Text mask=apiPath(join(p,L"*"));
    DWORD previous=0; BOOL changed=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&previous);
    HANDLE h=FindFirstFileW(mask.c_str(),&f);
    if(h==INVALID_HANDLE_VALUE) {
        const DWORD code=GetLastError();
        if(changed) SetThreadErrorMode(previous,nullptr);
        return code==ERROR_FILE_NOT_FOUND?ERROR_SUCCESS:code;
    }
    do {
        if(wcscmp(f.cFileName,L".")==0 || wcscmp(f.cFileName,L"..")==0) continue;
        Entry e; e.name=f.cFileName; e.attr=f.dwFileAttributes; e.date=f.ftLastWriteTime;
        e.size=(static_cast<uint64_t>(f.nFileSizeHigh)<<32)|f.nFileSizeLow;
        if(e.folder()) e.kind=(e.attr&FILE_ATTRIBUTE_REPARSE_POINT)?L"Junção / link":L"Pasta de arquivos";
        else {
            auto dot=e.name.rfind(L'.'); Text ext=dot!=Text::npos?e.name.substr(dot+1):L"";
            if(!ext.empty()) CharUpperBuffW(ext.data(),static_cast<DWORD>(ext.size()));
            e.kind=ext.empty()?L"Arquivo":ext;
            e.sizeLabel=prettyBytes(e.size);
        }
        e.dateLabel=prettyDate(e.date); entries.push_back(std::move(e));
    } while(FindNextFileW(h,&f));
    auto code=GetLastError(); FindClose(h);
    if(changed) SetThreadErrorMode(previous,nullptr);
    return code==ERROR_NO_MORE_FILES?ERROR_SUCCESS:code;
}
static Text physical(const Text& p) {
    HANDLE h=CreateFileW(apiPath(p).c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(h==INVALID_HANDLE_VALUE) return {};
    const DWORD mode=FILE_NAME_NORMALIZED|VOLUME_NAME_NT;
    DWORD n=GetFinalPathNameByHandleW(h,nullptr,0,mode);
    Text s(n,L'\0'); DWORD m=n?GetFinalPathNameByHandleW(h,s.data(),n,mode):0; CloseHandle(h);
    if(!m || m>=n) return {};
    s.resize(m); while(!s.empty() && s.back()==L'\\') s.pop_back();
    return s;
}
static bool within(const Text& path,const Text& root) {
    if(path.size()<root.size()) return false;
    return CompareStringOrdinal(path.c_str(),static_cast<int>(root.size()),root.c_str(),static_cast<int>(root.size()),TRUE)==CSTR_EQUAL
        && (path.size()==root.size() || path[root.size()]==L'\\');
}
struct Made { Text path; bool dir; };
static DWORD CALLBACK fileProgress(LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,LARGE_INTEGER,DWORD,DWORD,HANDLE,HANDLE,LPVOID data) {
    return static_cast<std::atomic_bool*>(data)->load()?PROGRESS_CANCEL:PROGRESS_CONTINUE;
}
static DWORD copyPart(const Text& from,const Text& to,std::atomic_bool& cancel,std::vector<Made>& made,unsigned depth) {
    if(cancel.load()) return ERROR_CANCELLED;
    if(depth>96) return ERROR_FILENAME_EXCED_RANGE;
    const Text a=apiPath(from), b=apiPath(to);
    DWORD attr=GetFileAttributesW(a.c_str());
    if(attr==INVALID_FILE_ATTRIBUTES) return GetLastError();
    if(attr&FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_NOT_SUPPORTED;
    if(!(attr&FILE_ATTRIBUTE_DIRECTORY)) {
        if(!CopyFileExW(a.c_str(),b.c_str(),fileProgress,&cancel,nullptr,COPY_FILE_FAIL_IF_EXISTS)) return GetLastError();
        made.push_back({b,false}); return ERROR_SUCCESS;
    }
    if(!CreateDirectoryW(b.c_str(),nullptr)) return GetLastError();
    made.push_back({b,true});
    WIN32_FIND_DATAW fd{}; HANDLE handle=FindFirstFileW(apiPath(join(from,L"*")).c_str(),&fd);
    if(handle==INVALID_HANDLE_VALUE) {DWORD e=GetLastError();return e==ERROR_FILE_NOT_FOUND?ERROR_SUCCESS:e;}
    DWORD code=ERROR_SUCCESS;
    for(;;) {
        Text n=fd.cFileName;
        if(n!=L"." && n!=L"..") code=copyPart(join(from,n),join(to,n),cancel,made,depth+1);
        if(code) break;
        if(!FindNextFileW(handle,&fd)) {DWORD e=GetLastError(); if(e!=ERROR_NO_MORE_FILES)code=e;break;}
    }
    FindClose(handle);return code;
}
DWORD copyItem(const Text& from,const Text& to,std::atomic_bool& cancel) {
    DWORD attr=GetFileAttributesW(apiPath(from).c_str());
    if(attr==INVALID_FILE_ATTRIBUTES) return GetLastError();
    if(attr&FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_NOT_SUPPORTED;
    if(attr&FILE_ATTRIBUTE_DIRECTORY) {
        Text a=physical(from), b=physical(parentPath(to));
        if(a.empty() || b.empty()) return ERROR_ACCESS_DENIED;
        if(within(b,a)) return ERROR_DIRECTORY;
    }
    std::vector<Made> made;
    DWORD code=copyPart(from,to,cancel,made,0);
    if(code) for(auto i=made.rbegin();i!=made.rend();++i) {
        DWORD now=GetFileAttributesW(i->path.c_str());
        if(now==INVALID_FILE_ATTRIBUTES || (now&FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if(i->dir) RemoveDirectoryW(i->path.c_str());
        else { if(now&FILE_ATTRIBUTE_READONLY) SetFileAttributesW(i->path.c_str(),now&~FILE_ATTRIBUTE_READONLY); DeleteFileW(i->path.c_str()); }
    }
    return code;
}
