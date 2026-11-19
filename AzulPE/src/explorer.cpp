#include "base.hpp"
#include <memory>

namespace {
constexpr int IdList=1010, IdAddress=1011, IdFilter=1012;
enum HitID { None, Back, Forward, Up, Address, Refresh, Filter, Organize, NewFolder, Copy, Paste, Rename, Delete, Minimize, Maximize, Close, CancelCopy, ClearFilter,
             Sort0=40, Sort1, Sort2, Sort3, SidePC=60, SideDocs, SideDesk, SideApp, SideDrive=100, MainDrive=200, Crumb=300 };
enum MenuID { OpenFile=1, CopyFileID, PasteID, NewID, RenameID, DeleteID, RefreshID, CopyPathID, CancelID };
struct Hit { RECT r; int id; };
struct CopyJob { Text from,to; DWORD error=0; std::atomic_bool cancel{false}; };
struct Explorer {
    HWND h{},list{},address{},filter{};
    HIMAGELIST icons{};
    Text path,query,notice;
    std::vector<Text> history{Text{}}, crumbs;
    size_t hi=0;
    std::vector<DriveInfo> drives;
    std::vector<Entry> all;
    std::vector<size_t> view;
    std::vector<Hit> hits;
    std::array<int,4> columns{};
    int sort=0, hot=0, down=0, driveSel=-1, driveScroll=0;
    bool descending=false, editing=false, muted=false, compact=false, maximized=false;
    RECT restore{};
    CopyJob* copy{};
} e;
Text copied;

struct Metrics {
    RECT r, addr, filter, side, main, hero, table, list, footer;
    explicit Metrics(HWND h) {
        r=client(h);
        const int sideW=u(width(r)<u(695)?146:172);
        side=area(1,u(123),sideW,r.bottom-u(123)-u(31));
        main=area(side.right,u(123),r.right-side.right-1,height(side));
        hero=area(main.left,main.top,width(main),u(75));
        table=area(main.left,hero.bottom,width(main),u(29));
        list=area(main.left,table.bottom,width(main),main.bottom-table.bottom);
        footer=area(1,main.bottom,r.right-2,r.bottom-main.bottom-1);
        const int filterW=u(width(r)<u(735)?158:209);
        filter=area(r.right-filterW-u(11),u(50),filterW,u(28));
        addr=area(u(110),u(50),filter.left-u(119),u(28));
    }
    int gridW() const {return width(main)-u(44);}
    int gridCols() const {return gridW()<u(406)?1:2;}
    int cardW() const {return (gridW()-(gridCols()-1)*u(11))/gridCols();}
    int gridTop() const {return main.top+u(106);}
    int maxScroll(size_t n) const {int rows=(static_cast<int>(n)+gridCols()-1)/gridCols();return std::max<int>(0,rows*u(91)-(main.bottom-gridTop()-u(4)));}
};

const Entry* picked() {
    const int n=ListView_GetNextItem(e.list,-1,LVNI_SELECTED);
    return n>=0 && static_cast<size_t>(n)<e.view.size()?&e.all[e.view[n]]:nullptr;
}
Text pickedName() {const auto* p=picked();return p?p->name:Text{};}
Text heading() {
    if(e.path.empty()) return L"Este PC";
    for(const auto& d:e.drives) if(CompareStringOrdinal(e.path.c_str(),-1,d.path.c_str(),-1,TRUE)==CSTR_EQUAL) return d.name;
    return leafName(e.path);
}
bool equalPath(const Text& a,const Text& b) {return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;}
bool inList(int i) {return i>=0 && static_cast<size_t>(i)<e.view.size();}
bool inDisks(int i) {return i>=0 && static_cast<size_t>(i)<e.drives.size();}
void updateColumns(const Metrics& m) {
    const int x=(std::max)(u(100),width(m.list)-GetSystemMetrics(SM_CXVSCROLL));
    e.compact=x<u(460);
    e.columns={x*(e.compact?51:43)/100,x*(e.compact?29:26)/100,e.compact?0:(x*19/100),0};
    e.columns[3]=x-e.columns[0]-e.columns[1]-e.columns[2];
    for(int i=0;i<4;++i) ListView_SetColumnWidth(e.list,i,e.columns[i]);
}
void sizeContent() {
    if(!e.address) return;
    Metrics m(e.h);
    place(e.address,area(m.addr.left+u(7),m.addr.top+u(5),width(m.addr)-u(36),u(20)),e.editing);
    place(e.filter,area(m.filter.left+u(27),m.filter.top+u(5),width(m.filter)-u(50),u(20)));
    place(e.list,m.list,!e.path.empty()&&!e.view.empty());
    updateColumns(m);
    e.driveScroll=std::clamp(e.driveScroll,0,m.maxScroll(e.drives.size()));
    redraw(e.h);
}
int cmpText(const Text& a,const Text& b) {return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)-CSTR_EQUAL;}
void fillRows(const Text& keep={},bool resetTop=false) {
    e.view.clear();
    const Text q=folded(e.query);
    for(size_t i=0;i<e.all.size();++i) if(q.empty() || folded(e.all[i].name).find(q)!=Text::npos) e.view.push_back(i);
    auto number=[](uint64_t a,uint64_t b){return a<b?-1:(a>b?1:0);};
    std::sort(e.view.begin(),e.view.end(),[&](size_t i,size_t j) {
        const auto& a=e.all[i];const auto& b=e.all[j];
        if(a.folder()!=b.folder()) return a.folder();
        int cmp=0;
        switch(e.sort) {
            case 1:cmp=static_cast<int>(CompareFileTime(&a.date,&b.date));break;
            case 2:cmp=cmpText(a.kind,b.kind);break;
            case 3:cmp=number(a.size,b.size);break;
        }
        if(!cmp)cmp=cmpText(a.name,b.name);
        if(!cmp)cmp=a.name==b.name?0:(a.name<b.name?-1:1);
        return e.descending?cmp>0:cmp<0;
    });
    e.muted=true;
    ListView_SetItemState(e.list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_SetItemCountEx(e.list,static_cast<int>(e.view.size()),LVSICF_NOSCROLL);
    if(resetTop && !e.view.empty())ListView_EnsureVisible(e.list,0,FALSE);
    if(!keep.empty()) for(size_t i=0;i<e.view.size();++i) if(e.all[e.view[i]].name==keep) {
        ListView_SetItemState(e.list,static_cast<int>(i),LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        ListView_EnsureVisible(e.list,static_cast<int>(i),FALSE); break;
    }
    e.muted=false;
    sizeContent();redraw(e.list);
}
void cue() {
    SendMessageW(e.filter,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(e.path.empty()?L"Filtrar unidades":L"Filtrar nesta pasta"));
    InvalidateRect(e.filter,nullptr,TRUE);
}
void finishAddress() {
    if(e.editing) {e.editing=false;ShowWindow(e.address,SW_HIDE);redraw(e.h);}
}
void focusContent() {
    if(!e.view.empty()&&IsWindowVisible(e.list)) SetFocus(e.list);
    else SetFocus(e.h);
}
void beginAddress() {
    e.editing=true;
    SetWindowTextW(e.address,e.path.c_str()); sizeContent();SetFocus(e.address); SendMessageW(e.address,EM_SETSEL,0,-1);
}
bool navigate(const Text& p,bool saveHistory=true) {
    DWORD code=0; std::vector<Entry> next;
    if(!p.empty()) {
        DWORD prev=0; BOOL changed=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&prev);
        DWORD a=GetFileAttributesW(apiPath(p).c_str()); if(a==INVALID_FILE_ATTRIBUTES)code=GetLastError();
        if(changed) SetThreadErrorMode(prev,nullptr);
        if(code || a==INVALID_FILE_ATTRIBUTES || !(a&FILE_ATTRIBUTE_DIRECTORY)) {
            errorAlert(e.h,L"Não foi possível abrir a pasta.",p,code?code:ERROR_DIRECTORY); return false;
        }
        code=readFolder(p,next);
        if(code) {errorAlert(e.h,L"Não foi possível listar esta pasta.",p,code);return false;}
    }
    if(saveHistory&&!equalPath(e.path,p)) {
        e.history.resize(e.hi+1);e.history.push_back(p); ++e.hi;
        if(e.history.size()>128) {e.history.erase(e.history.begin()); --e.hi;}
    }
    e.path=p;e.all=std::move(next);e.notice.clear();e.query.clear();e.driveSel=-1;e.driveScroll=0;
    if(p.empty())e.drives=readDrives();
    e.muted=true;SetWindowTextW(e.filter,L"");cue();e.muted=false;
    finishAddress();fillRows({},true);focusContent();
    SetWindowTextW(e.h,(heading()+L" — Explorador Azul").c_str()); refreshTaskbar();return true;
}
void reload(const Text& select={},bool drivesToo=false) {
    const Text keep=select.empty()?pickedName():select;
    if(e.path.empty() || drivesToo) e.drives=readDrives();
    if(!e.path.empty()) {
        std::vector<Entry> next;
        if(auto err=readFolder(e.path,next)) {errorAlert(e.h,L"Não foi possível atualizar a pasta.",e.path,err);return;}
        e.all=std::move(next);
    }
    fillRows(keep);
}
void acceptAddress() {
    Text typed=getText(e.address);
    if(typed.empty() || folded(typed)==L"este pc") {navigate({});return;}
    Text p=rootedPath(typed,e.path);
    if(p.empty()) {alert(e.h,L"Digite um caminho válido, por exemplo C:\\ ou \\\\servidor\\pasta.");return;}
    DWORD prev=0; const BOOL changed=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&prev);
    auto a=GetFileAttributesW(apiPath(p).c_str()); const auto code=GetLastError();
    if(changed) SetThreadErrorMode(prev,nullptr);
    if(a==INVALID_FILE_ATTRIBUTES) {errorAlert(e.h,L"Caminho não encontrado.",p,code);return;}
    if(a&FILE_ATTRIBUTE_DIRECTORY) navigate(p); else {finishAddress();focusContent();openItem(e.h,p,a);}
}
std::vector<int> diskView() {
    std::vector<int> v; const Text q=folded(e.query);
    for(size_t i=0;i<e.drives.size();++i) if(q.empty() || folded(e.drives[i].name+L" "+e.drives[i].type).find(q)!=Text::npos) v.push_back(static_cast<int>(i));
    return v;
}
void openEntry() {
    if(e.path.empty()) {if(inDisks(e.driveSel))navigate(e.drives[e.driveSel].path);return;}
    const auto* p=picked();if(!p) return;
    Text full=join(e.path,p->name); DWORD attr=p->attr;
    if(attr&FILE_ATTRIBUTE_DIRECTORY) navigate(full);
    else openItem(e.h,full,attr);
}
bool validName(const Text& s) {
    if(s.empty() || s.size()>255 || s.back()==L'.' || s.back()==L' ' || s.find_first_of(L"\\/:*?\"<>|")!=Text::npos) return false;
    for(auto c:s) if(c<32) return false;
    auto base=folded(s.substr(0,s.find(L'.')));
    while(!base.empty()&&base.back()==L' ')base.pop_back();
    if(base==L"con" || base==L"prn" || base==L"aux" || base==L"nul" || base==L"conin$" || base==L"conout$")return false;
    if(base.size()==4 && (base.rfind(L"com",0)==0||base.rfind(L"lpt",0)==0) && Text(L"123456789¹²³").find(base[3])!=Text::npos)return false;
    return true;
}
void createFolder() {
    if(e.path.empty() || e.copy) return;
    Text name;
    if(!requestName(e.h,L"Nova pasta",L"Nome para a nova pasta:",L"Nova pasta",name))return;
    if(!validName(name)) {alert(e.h,L"Esse nome de pasta não é válido.",L"Nova pasta",MB_OK|MB_ICONWARNING);return;}
    Text p=join(e.path,name);
    if(CreateDirectoryW(apiPath(p).c_str(),nullptr)){e.notice=L"Pasta criada: "+name;reload(name);focusContent();}
    else errorAlert(e.h,L"Não foi possível criar a pasta.",p,GetLastError());
}
void renameEntry() {
    if(e.copy) return;
    const auto* p=picked();if(!p)return;
    Text original=p->name, name;
    if(!requestName(e.h,L"Renomear",L"Novo nome do item:",original,name) || name==original) return;
    if(!validName(name)) {alert(e.h,L"Esse nome não é válido.",L"Renomear",MB_OK|MB_ICONWARNING);return;}
    Text src=apiPath(join(e.path,original)), dst=join(e.path,name);
    if(MoveFileExW(src.c_str(),apiPath(dst).c_str(),0)) {e.notice=L"Item renomeado: "+name;reload(name);focusContent();}
    else errorAlert(e.h,L"Não foi possível renomear o item.",dst,GetLastError());
}
void deleteEntry() {
    if(e.copy) return;
    const auto* s=picked();if(!s)return;
    const bool dir=s->folder(); const bool link=(s->attr&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
    const Text p=join(e.path,s->name), name=s->name;
    if(link) {errorAlert(e.h,L"O Explorador Azul não remove links nem junções.",p,ERROR_NOT_SUPPORTED);return;}
    if(MessageBoxW(e.h,(Text(L"Excluir permanentemente ")+ (dir?L"esta pasta vazia?":L"este arquivo?")+L"\n\n"+p+L"\n\nNão há Lixeira no Azul PE.").c_str(),
        L"Confirmar exclusão",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING|MB_SETFOREGROUND)!=IDYES)return;
    const Text native=apiPath(p);
    if(dir?RemoveDirectoryW(native.c_str()):DeleteFileW(native.c_str())) {e.notice=L"Item excluído: "+name;reload();}
    else errorAlert(e.h,L"Não foi possível excluir o item.",p,GetLastError());
}
void copyEntry() {
    if(e.copy) return;
    const auto* p=picked();if(!p)return;
    if(p->attr&FILE_ATTRIBUTE_REPARSE_POINT) {errorAlert(e.h,L"Este item não pode ser copiado.",join(e.path,p->name),ERROR_NOT_SUPPORTED);return;}
    copied=join(e.path,p->name); e.notice=L"Pronto para colar: "+p->name;redraw(e.h);
}
void copyPath() {
    const auto* p=picked();Text s=p?join(e.path,p->name):e.path;
    if(s.empty() || !OpenClipboard(e.h))return;
    auto n=(s.size()+1)*sizeof(wchar_t);HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,n);
    if(h) {
        auto* b=GlobalLock(h);
        if(b) {std::copy_n(s.c_str(),s.size()+1,static_cast<wchar_t*>(b));GlobalUnlock(h);}
        if(b&&EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,h)) {e.notice=L"Caminho copiado para a área de transferência.";redraw(e.h);}
        else GlobalFree(h);
    }
    CloseClipboard();
}
void pasteEntry() {
    if(e.path.empty() || e.copy || copied.empty())return;
    Text to=join(e.path,leafName(copied));
    if(equalPath(copied,to)) {alert(e.h,L"Escolha outra pasta para colar o item. O destino atual é igual à origem.");return;}
    auto j=std::make_unique<CopyJob>();j->from=copied;j->to=to;
    auto* job=j.get(); const HWND w=e.h;
    try {
        std::thread([w,job](){
            try {job->error=copyItem(job->from,job->to,job->cancel);}
            catch(...) {job->error=ERROR_NOT_ENOUGH_MEMORY;}
            PostMessageW(w,WM_COPY_READY,0,reinterpret_cast<LPARAM>(job));
        }).detach();
        e.copy=j.release(); e.notice=L"Copiando: "+leafName(to);redraw(e.h);
    } catch(...) {alert(e.h,L"Não foi possível iniciar a cópia.");}
}
void cancelCopy() {if(e.copy) {e.copy->cancel=true;e.notice=L"Cancelando a cópia…";redraw(e.h);}}
void copyDone(CopyJob* job) {
    std::unique_ptr<CopyJob> clean(job);e.copy=nullptr;
    const bool here=equalPath(e.path,parentPath(job->to));
    if(!job->error) {
        e.notice=L"Cópia concluída: "+leafName(job->to);
        if(here)reload(leafName(job->to));
    } else if(job->error==ERROR_REQUEST_ABORTED || job->error==ERROR_CANCELLED) {
        e.notice=L"Cópia cancelada.";if(here)reload();
    } else {
        e.notice=L"Não foi possível concluir a cópia.";
        if(job->error==ERROR_DIRECTORY) alert(e.h,L"Uma pasta não pode ser copiada para dentro dela mesma.");
        else errorAlert(e.h,L"Não foi possível concluir a cópia.",job->to,job->error);
        if(here)reload();
    }
    redraw(e.h);
}
void organize(POINT pt) {
    bool sel=picked()!=nullptr, dir=!e.path.empty();
    auto menu=CreatePopupMenu();
    const auto add=[&](UINT id,const wchar_t* text,bool enabled){AppendMenuW(menu,MF_STRING|(enabled?0:MF_GRAYED),id,text);};
    add(OpenFile,L"Abrir\tEnter",sel);AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    add(CopyFileID,L"Copiar item\tCtrl+C",sel&&!e.copy);add(PasteID,L"Colar item\tCtrl+V",dir&&!copied.empty()&&!e.copy);
    add(CopyPathID,L"Copiar caminho",dir);AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    add(NewID,L"Nova pasta\tCtrl+Shift+N",dir&&!e.copy);add(RenameID,L"Renomear\tF2",sel&&!e.copy);add(DeleteID,L"Excluir\tDel",sel&&!e.copy);
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);add(RefreshID,L"Atualizar\tF5",true);
    if(e.copy)add(CancelID,L"Cancelar cópia",true);
    UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON|TPM_NONOTIFY,pt.x,pt.y,0,e.h,nullptr);DestroyMenu(menu);
    switch(cmd) {
        case OpenFile:openEntry();break; case CopyFileID:copyEntry();break;case PasteID:pasteEntry();break;
        case NewID:createFolder();break;case RenameID:renameEntry();break;case DeleteID:deleteEntry();break;
        case RefreshID:e.notice.clear();reload({},true);break;case CopyPathID:copyPath();break;case CancelID:cancelCopy();break;
    }
}
void registerHit(RECT r,int id) {e.hits.push_back({r,id});}
int hitAt(POINT p) {
    for(auto i=e.hits.rbegin(); i!=e.hits.rend();++i) if(in(i->r,p))return i->id;
    return None;
}
void chromeButton(HDC d,RECT r,int id,Glyph g) {
    const bool hover=e.hot==id;
    if(id==Close)tone(d,r,hover?0xffc2b3:0xf5b1a7,hover?0xd94433:0xc45d56);
    else tone(d,r,hover?0xdff7ff:0xc2ddf5,hover?0x72bbea:0x78a6d0);
    border(d,r,id==Close?0xa84a45:0x6290b6);
    rule(d,r.left+1,r.top+1,width(r)-2,id==Close?0xffd2c6:0xe0f0fd);
    glyph(d,g,r.left+(width(r)-u(16))/2,r.top+(height(r)-u(16))/2,u(16),id==Close?0xffffff:0x244969);
    registerHit(r,id);
}
void navButton(HDC d,RECT r,int id,Glyph g,bool enabled,bool main=false) {
    const bool hover=e.hot==id;
    if(enabled && main) {
        roundBox(d,r,hover?0x60aadf:0x4c94ca,0x336c99,31);
        roundBox(d,inset(r,2),hover?0x77c6f4:0x619fd4,hover?0xa1e2fd:0x8abfe8,28);
    } else if(hover&&enabled) roundBox(d,r,0xd9edff,0x9fbfdc,7);
    glyph(d,g,r.left+(width(r)-u(19))/2,r.top+(height(r)-u(19))/2,u(19),main&&enabled?0xffffff:(enabled?0x315e85:0x9bb2c7));
    if(enabled)registerHit(r,id);
}
void addressPaint(HDC d,const Metrics& m) {
    tone(d,m.addr,0xfafcff,0xffffff);border(d,m.addr,e.editing?0x579bd0:0x9cb8d3);
    rule(d,m.addr.left+1,m.addr.top+1,width(m.addr)-2,0xf0f7ff);
    registerHit(m.addr,Address);
    auto refresh=area(m.addr.right-u(28),m.addr.top+1,u(27),height(m.addr)-2);
    if(e.hot==Refresh)solid(d,refresh,0xddefff);
    solid(d,area(refresh.left,refresh.top+u(3),1,height(refresh)-u(6)),0xd2e3f0);
    glyph(d,Glyph::refresh,refresh.left+u(7),refresh.top+u(5),u(14),0x477399);registerHit(refresh,Refresh);
    if(e.editing) return;
    icon(d,e.path.empty()?PC:FOLDER,m.addr.left+u(6),m.addr.top+u(5),u(17));
    e.crumbs.clear(); e.crumbs.push_back({});
    std::vector<Text> names{L"Este PC"};
    if(!e.path.empty()) {
        std::vector<Text> reverse;
        for(Text p=e.path;!p.empty();p=parentPath(p)) reverse.push_back(p);
        for(auto i=reverse.rbegin();i!=reverse.rend();++i) {
            auto name=leafName(*i);if(name.size()==3&&name[1]==L':')name.resize(2);
            e.crumbs.push_back(*i); names.push_back(name);
        }
    }
    int x=m.addr.left+u(27), limit=refresh.left-u(4);
    int sum=0;std::vector<int> cell;
    for(const auto& n:names){ cell.push_back(textWidth(d,n,ft.small)+u(18));sum+=cell.back(); }
    int first=0;
    while(first+1<static_cast<int>(names.size()) && sum+(first?u(21):0)>limit-x) sum-=cell[first++];
    if(first) {label(d,L"…",area(x,m.addr.top,u(19),height(m.addr)),ft.strong,0x6d87a1);x+=u(18);}
    for(int i=first;i<static_cast<int>(names.size()) && x<limit;++i) {
        int cw=(std::min)(cell[i],limit-x);auto a=area(x,m.addr.top+u(2),cw,height(m.addr)-u(4));
        if(e.hot==Crumb+i)roundBox(d,a,0xe3f2ff,0xb7d4ee,4);
        label(d,names[i],area(x+u(3),m.addr.top,cw-u(14),height(m.addr)),ft.small,0x243e59);
        if(cw>=u(22)) glyph(d,Glyph::right,x+cw-u(12),m.addr.top+u(9),u(10),0x7292ad);
        if(i+1<static_cast<int>(names.size()))registerHit(a,Crumb+i);
        x+=cw;
    }
}
void toolbarButton(HDC d,RECT r,int id,const Text& text,Glyph g,bool enabled,bool withIcon=true) {
    bool hover=enabled&&e.hot==id;
    if(hover) {roundBox(d,r,e.down==id?0xc3e1f9:0xe9f6ff,0xa0bfdc,5);}
    if(withIcon)glyph(d,g,r.left+u(7),r.top+(height(r)-u(15))/2,u(15),enabled?0x3d78a3:0x90abc1);
    label(d,text,area(r.left+u(withIcon?27:9),r.top,width(r)-u(withIcon?31:12),height(r)),ft.small,enabled?0x294967:0x7891a8);
    if(id==Organize)glyph(d,Glyph::down,r.right-u(19),r.top+(height(r)-u(12))/2,u(12),0x4e728f);
    if(enabled)registerHit(r,id);
}
void sideItem(HDC d,RECT r,int id,const Text& name,Icon art,bool current) {
    if(current || e.hot==id) {
        roundBox(d,r,current?0xd8eaf9:0xe5f1fb,current?0xa7c7e3:0xc0dbef,5);
        if(current)solid(d,area(r.left+1,r.top+u(4),u(2),height(r)-u(8)),0x4c96c8);
    }
    icon(d,art,r.left+u(8),r.top+(height(r)-u(19))/2,u(19));
    label(d,name,area(r.left+u(34),r.top,width(r)-u(37),height(r)),current?ft.strong:ft.small,0x274662);
    registerHit(r,id);
}
void drawSide(HDC d,const Metrics& m) {
    tone(d,m.side,0xf0f6fc,0xeaf2fa);
    solid(d,area(m.side.right-1,m.side.top,1,height(m.side)),0xc9d9e9);
    int x=m.side.left+u(9), w=width(m.side)-u(17), y=m.side.top+u(15);
    trackingLabel(d,L"LOCAIS",area(x+u(9),y,w-u(13),u(17)),ft.tiny,0x597c9a);y+=u(23);
    sideItem(d,area(x,y,w,u(30)),SidePC,L"Este PC",PC,e.path.empty());y+=u(33);
    sideItem(d,area(x,y,w,u(30)),SideDocs,L"Documentos",DOCS,!documentsFolder.empty()&&equalPath(e.path,documentsFolder));y+=u(33);
    sideItem(d,area(x,y,w,u(30)),SideDesk,L"Área de Trabalho",DESK,!desktopFolder.empty()&&equalPath(e.path,desktopFolder));y+=u(33);
    sideItem(d,area(x,y,w,u(30)),SideApp,L"Pasta do shell",FOLDER,equalPath(e.path,programFolder));y+=u(41);
    if(y+u(35)>=m.side.bottom)return;
    rule(d,x+u(9),y,w-u(19),0xd0deec);y+=u(12);
    trackingLabel(d,L"UNIDADES",area(x+u(9),y,w-u(13),u(17)),ft.tiny,0x597c9a);y+=u(23);
    for(size_t i=0;i<e.drives.size();++i) {
        if(y+u(29)>m.side.bottom)break;
        const auto& a=e.drives[i];
        sideItem(d,area(x,y,w,u(28)),SideDrive+static_cast<int>(i),a.name,a.icon,equalPath(e.path,a.path));y+=u(29);
    }
}
void emptyView(HDC d,const Metrics& m,bool disks) {
    RECT r=disks?area(m.main.left,m.gridTop(),width(m.main),m.main.bottom-m.gridTop()):m.list;
    if(height(r)<u(82))return;
    const int cx=(r.left+r.right)/2, y=r.top+(std::min)(u(49),(height(r)-u(77))/2);
    icon(d,e.query.empty()?(disks?DRIVE:FOLDER):INFO,cx-u(20),y,u(40));
    const auto align=DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS;
    label(d,!e.query.empty()?L"Nenhum resultado":(disks?L"Nenhuma unidade disponível":L"Esta pasta está vazia"),area(r.left+u(12),y+u(44),width(r)-u(24),u(21)),ft.caption,0x2d526f,align);
    label(d,!e.query.empty()?L"Tente outro termo ou limpe o filtro.":(disks?L"Conecte uma unidade e pressione F5.":L"Crie uma pasta ou cole um item aqui."),area(r.left+u(12),y+u(68),width(r)-u(24),u(19)),ft.small,0x607e98,align);
}
void drawDriveCard(HDC d,RECT a,int i) {
    const auto& v=e.drives[i];const bool selected=e.driveSel==i,hover=e.hot==MainDrive+i;
    roundBox(d,a,selected?0xe4f3ff:(hover?0xf1f8ff:0xf8fbfe),selected?0x80b6e1:(hover?0xa9cdea:0xd3e1ef),9);
    icon(d,v.icon,a.left+u(11),a.top+u(15),u(41));
    const int x=a.left+u(60), w=a.right-x-u(14);
    label(d,v.name,area(x,a.top+u(10),w,u(22)),ft.strong,0x183957);
    if(v.total) {
        const auto bar=area(x,a.top+u(38),w,u(8));
        roundBox(d,bar,0xe5ecf3,0xc5d7e8,4);
        const auto used=static_cast<double>(v.total>(v.free)?v.total-v.free:0)/static_cast<double>(v.total);
        int bw=static_cast<int>((width(bar)-2)*std::clamp(used,0.0,1.0));
        if(bw>0) tone(d,area(bar.left+1,bar.top+1,bw,height(bar)-2),used>.92?0xf2a473:0x78c9ef,used>.92?0xd85c4f:0x318bd0);
    } else label(d,v.type,area(x,a.top+u(32),w,u(17)),ft.small,0x497593);
    label(d,v.total?v.detail:L"Clique duas vezes para abrir",area(x,a.top+u(51),w,u(17)),ft.small,0x597693);
}
void drivesPaint(HDC d,const Metrics& m) {
    auto ds=diskView();
    trackingLabel(d,L"UNIDADES E DISPOSITIVOS",area(m.main.left+u(22),m.main.top+u(78),width(m.main)-u(50),u(17)),ft.tiny,0x537596);
    if(ds.empty()) {emptyView(d,m,true);return;}
    int left=m.main.left+u(22), top=m.gridTop(), cols=m.gridCols(), w=m.cardW();
    const int max=m.maxScroll(ds.size());e.driveScroll=std::clamp(e.driveScroll,0,max);
    auto clip=SaveDC(d); IntersectClipRect(d,m.main.left,top,m.main.right,m.main.bottom);
    for(size_t j=0;j<ds.size();++j) {
        RECT a=area(left+(static_cast<int>(j)%cols)*(w+u(11)),top+(static_cast<int>(j)/cols)*u(91)-e.driveScroll,w,u(79));
        if(a.bottom<=top || a.top>=m.main.bottom)continue;
        drawDriveCard(d,a,ds[j]); a.top=std::max<LONG>(top,a.top);a.bottom=(std::min)(m.main.bottom,a.bottom);
        registerHit(a,MainDrive+ds[j]);
    }
    if(max>0) {
        int viewport=m.main.bottom-top-u(5); int h=(std::max)(u(18),viewport*viewport/(viewport+max));
        if(viewport>h)roundBox(d,area(m.main.right-u(10),top+e.driveScroll*(viewport-h)/max,u(4),h),0xa0bcd4,0xa0bcd4,4);
    }
    RestoreDC(d,clip);
}
void paintExplorer() {
    Paint p(e.h);Metrics m(e.h);const auto r=m.r;auto d=p.dc;
    e.hits.clear();
    tone(d,r,0xb0d6f4,0x76a4ce);border(d,r,0x3e6c98);
    tone(d,area(1,1,r.right-2,u(38)),0xcfe5f9,0x8ab9e2);
    tone(d,area(2,2,r.right-4,u(17)),0xdff0ff,0xb7d7f3);rule(d,3,1,r.right-6,0xf3fbff);
    icon(d,FOLDER,u(11),u(9),u(21));
    label(d,L"Explorador Azul",area(u(40),u(2),r.right-u(177),u(35)),ft.caption,0x193d62);
    int buttonX=r.right-u(119);chromeButton(d,area(buttonX,1,u(34),u(25)),Minimize,Glyph::minus);
    chromeButton(d,area(buttonX+u(33),1,u(34),u(25)),Maximize,e.maximized?Glyph::restore:Glyph::maximize);
    chromeButton(d,area(buttonX+u(66),1,u(44),u(25)),Close,Glyph::close);
    tone(d,area(1,u(39),r.right-2,u(48)),0xe6f1fc,0xd4e5f5);
    rule(d,1,u(39),r.right-2,0xeaf7ff);
    navButton(d,at(12,48,31,31),Back,Glyph::left,e.hi>0,true);
    navButton(d,at(48,49,26,29),Forward,Glyph::right,e.hi+1<e.history.size());
    navButton(d,at(77,49,26,29),Up,Glyph::up,!e.path.empty());
    addressPaint(d,m);
    solid(d,m.filter,0xffffff);border(d,m.filter,GetFocus()==e.filter?0x579bd0:0x9cb8d3);
    glyph(d,Glyph::search,m.filter.left+u(8),m.filter.top+u(7),u(14),0x6089ab);
    auto clear=area(m.filter.right-u(22),m.filter.top+1,u(21),height(m.filter)-2);
    if(!e.query.empty()) {if(e.hot==ClearFilter)solid(d,clear,0xe1f1fd); glyph(d,Glyph::close,clear.left+u(4),clear.top+u(6),u(13),0x547f9f);registerHit(clear,ClearFilter);}
    registerHit(area(m.filter.left,m.filter.top,u(25),height(m.filter)),Filter);
    tone(d,area(1,u(87),r.right-2,u(36)),0xf7fbff,0xdcebf8);
    rule(d,1,u(86),r.right-2,0xb9cfe4);rule(d,1,u(122),r.right-2,0xb8cfe4);
    bool isPath=!e.path.empty()&&!e.copy, has=picked()!=nullptr&&!e.copy;
    toolbarButton(d,at(8,91,91,27),Organize,L"Organizar",Glyph::down,true,false);
    solid(d,at(106,95,1,20),0xc3d8eb);
    toolbarButton(d,at(115,91,105,27),NewFolder,L"Nova pasta",Glyph::plus,isPath);
    toolbarButton(d,at(223,91,77,27),Copy,L"Copiar",Glyph::copy,has);
    toolbarButton(d,at(303,91,74,27),Paste,L"Colar",Glyph::paste,isPath&&!copied.empty()&&!e.copy);
    if(r.right>u(697)) {
        toolbarButton(d,at(381,91,97,27),Rename,L"Renomear",Glyph::pen,has);
        toolbarButton(d,at(480,91,80,27),Delete,L"Excluir",Glyph::trash,has);
    }
    if(r.right>u(557)) {
        glyph(d,Glyph::details,r.right-u(92),u(97),u(15),0x5e84a2);
        label(d,L"Detalhes",area(r.right-u(70),u(90),u(61),u(29)),ft.small,0x5a7a98);
    }
    drawSide(d,m);solid(d,m.main,0xffffff);
    label(d,heading(),area(m.main.left+u(22),m.main.top+u(15),width(m.main)-u(42),u(35)),ft.heading,0x173954);
    Text sub=e.path.empty()?L"Unidades disponíveis nesta sessão":e.path;
    label(d,sub,area(m.main.left+u(23),m.main.top+u(52),width(m.main)-u(44),u(17)),ft.small,0x60809e,DT_SINGLELINE|DT_VCENTER|DT_PATH_ELLIPSIS);
    if(e.path.empty())drivesPaint(d,m);
    else {
        tone(d,m.table,0xf6faff,0xecf4fb);rule(d,m.table.left,m.table.top,width(m.table),0xd3e2ee);
        rule(d,m.table.left,m.table.bottom-1,width(m.table),0xcbdce9);
        const std::array<Text,4> titles={L"Nome",L"Data de modificação",L"Tipo",L"Tamanho"};
        int x=m.table.left;
        for(int i=0;i<4;++i) {
            const int w=e.columns[i];if(w==0)continue;
            RECT a=area(x,m.table.top+1,w,height(m.table)-2);
            if(e.hot==Sort0+i)solid(d,a,0xe3f0fa);
            label(d,titles[i],area(x+u(i==0?9:7),a.top,w-u(i==e.sort?24:12),height(a)),ft.small,0x395c79);
            if(e.sort==i)glyph(d,e.descending?Glyph::down:Glyph::up,x+w-u(17),a.top+u(7),u(13),0x5983a3);
            solid(d,area(a.right-1,a.top,1,height(a)),0xd9e6f1);registerHit(a,Sort0+i);x+=w;
        }
        if(e.view.empty()) emptyView(d,m,false);
    }
    tone(d,m.footer,0xf0f7ff,0xe0edf9);rule(d,m.footer.left,m.footer.top,width(m.footer),0xc2d7eb);
    Text footer;
    if(e.path.empty()) {const auto n=diskView().size();footer=std::to_wstring(n)+(n==1?L" unidade":L" unidades"); if(inDisks(e.driveSel))footer+=L"    ·    "+e.drives[e.driveSel].name;}
    else {
        size_t n=e.view.size();footer=std::to_wstring(n)+(n==1?L" item":L" itens");
        if(!e.query.empty())footer+=L" de "+std::to_wstring(e.all.size());
        if(auto p2=picked())footer+=L"    ·    1 selecionado"+(p2->folder()?Text{}:L"    ·    "+p2->sizeLabel);
    }
    const int fx=m.footer.left+u(12), fy=m.footer.top, fw=width(m.footer)-u(24)-(e.copy?u(92):(r.right>u(665)?u(110):0));
    if(!e.notice.empty()) {
        label(d,e.notice,area(fx,fy,fw,u(30)),ft.small,0x365c7b);
    } else label(d,footer,area(fx,fy,fw,u(30)),ft.small,0x466b8a);
    if(e.copy) {
        auto a=area(m.footer.right-u(96),fy+u(4),u(88),u(23));
        roundBox(d,a,e.hot==CancelCopy?0xd6ebfb:0xecf6ff,0xb3cce2,5);
        label(d,L"Cancelar",a,ft.small,0x3c6383,DT_CENTER|DT_SINGLELINE|DT_VCENTER);registerHit(a,CancelCopy);
    } else if(r.right>u(665)) label(d,L"Explorador Azul",area(r.right-u(121),fy,u(106),u(30)),ft.small,0x6d89a4,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
    border(d,r,0x446f94);
}
void changeHistory(int delta) {
    auto i=static_cast<int>(e.hi)+delta;
    if(i>=0 && static_cast<size_t>(i)<e.history.size()) {
        Text p=e.history[i];
        if(navigate(p,false)) {e.hi=static_cast<size_t>(i);redraw(e.h);}
    }
}
void selectDriveByKey(int key) {
    auto ds=diskView();if(ds.empty())return;
    Metrics m(e.h);const int cols=m.gridCols();
    auto it=std::find(ds.begin(),ds.end(),e.driveSel);
    int pos=it==ds.end()?0:static_cast<int>(it-ds.begin());
    if(it!=ds.end()) switch(key) {
        case VK_LEFT: --pos;break;case VK_RIGHT:++pos;break;case VK_UP:pos-=cols;break;case VK_DOWN:pos+=cols;break;
        case VK_HOME:pos=0;break;case VK_END:pos=static_cast<int>(ds.size())-1;break;
    }
    pos=std::clamp(pos,0,static_cast<int>(ds.size())-1);e.driveSel=ds[pos];
    int y=(pos/cols)*u(91), visible=m.main.bottom-m.gridTop();
    if(y<e.driveScroll)e.driveScroll=y;
    else if(y+u(79)>e.driveScroll+visible)e.driveScroll=y+u(79)-visible;
    redraw(e.h);
}
void maximize(bool on) {
    if(IsIconic(e.h)) ShowWindow(e.h,SW_RESTORE);
    if(e.maximized==on) return;
    if(on) GetWindowRect(e.h,&e.restore);
    e.maximized=on;
    if(on) fitExplorerToDesktop();
    else {
        const int w=(std::min)(width(e.restore),screenW), h=(std::min)(height(e.restore),screenH-barH);
        const int x=std::clamp<int>(e.restore.left,0,screenW-w), y=std::clamp<int>(e.restore.top,0,screenH-barH-h);
        SetWindowPos(e.h,nullptr,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    redraw(e.h);
}
void perform(int id) {
    if(id>=Crumb) {
        int k=id-Crumb;if(static_cast<size_t>(k)<e.crumbs.size()) {Text p=e.crumbs[k];navigate(p);} return;
    }
    if(id>=MainDrive) {if(inDisks(id-MainDrive)){e.driveSel=id-MainDrive;e.notice.clear();SetFocus(e.h);redraw(e.h);}return;}
    if(id>=SideDrive) {if(inDisks(id-SideDrive)){Text p=e.drives[id-SideDrive].path;navigate(p);}return;}
    if(id>=Sort0&&id<=Sort3) {
        int next=id-Sort0;if(next==e.sort)e.descending=!e.descending;else {e.sort=next;e.descending=false;}
        fillRows(pickedName());return;
    }
    switch(id) {
        case Back:changeHistory(-1);break;case Forward:changeHistory(1);break;
        case Up:if(!e.path.empty())navigate(parentPath(e.path));break;
        case Address:beginAddress();break;
        case Filter:SetFocus(e.filter);break;
        case Refresh:e.notice.clear();reload({},true);break;
        case ClearFilter:SetWindowTextW(e.filter,L"");SetFocus(e.filter);break;
        case Organize:{POINT q{u(8),u(119)};ClientToScreen(e.h,&q);organize(q);break;}
        case NewFolder:createFolder();break;case Copy:copyEntry();break;case Paste:pasteEntry();break;
        case Rename:renameEntry();break;case Delete:deleteEntry();break;
        case Close:SendMessageW(e.h,WM_CLOSE,0,0);break;
        case Minimize:ShowWindow(e.h,SW_MINIMIZE);refreshTaskbar();break;
        case Maximize:maximize(!e.maximized);break;
        case SidePC:navigate({});break;
        case SideDocs:act(StartAction::docs,e.h);break;case SideDesk:act(StartAction::desktop,e.h);break;
        case SideApp:navigate(programFolder);break;
        case CancelCopy:cancelCopy();break;
    }
}
LRESULT CALLBACK explorerProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
        case WM_NCCALCSIZE:case WM_NCPAINT:return 0;
        case WM_ERASEBKGND:return 1;
        case WM_CREATE: {
            e.h=w;
            e.address=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL,0,0,0,0,w,reinterpret_cast<HMENU>(IdAddress),app,nullptr);
            e.filter=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,0,0,w,reinterpret_cast<HMENU>(IdFilter),app,nullptr);
            e.list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|LVS_SINGLESEL|LVS_NOCOLUMNHEADER,
                0,0,0,0,w,reinterpret_cast<HMENU>(IdList),app,nullptr);
            if(!e.address||!e.filter||!e.list) return -1;
            for(HWND a:{e.address,e.filter,e.list}) SendMessageW(a,WM_SETFONT,reinterpret_cast<WPARAM>(ft.body),FALSE);
            for(HWND a:{e.address,e.filter}) SendMessageW(a,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
            SendMessageW(e.address,EM_SETLIMITTEXT,32760,0);SendMessageW(e.filter,EM_SETLIMITTEXT,240,0);
            ListView_SetExtendedListViewStyle(e.list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
            ListView_SetBkColor(e.list,col(0xffffff));ListView_SetTextColor(e.list,col(0x233e58));
            ListView_SetTextBkColor(e.list,CLR_NONE);
            e.icons=ImageList_Create(u(23),u(30),ILC_COLOR32|ILC_MASK,ICON_COUNT,0);
            if(e.icons) {
                for(int j=0;j<ICON_COUNT;++j) {auto jicon=nativeIcon(static_cast<Icon>(j),u(19),u(23),u(30));ImageList_AddIcon(e.icons,jicon);if(jicon)DestroyIcon(jicon);}
                ListView_SetImageList(e.list,e.icons,LVSIL_SMALL);
            }
            for(int i=0;i<4;++i){LVCOLUMNW c{};c.mask=LVCF_WIDTH|LVCF_FMT;c.cx=u(100);c.fmt=i==3?LVCFMT_RIGHT:LVCFMT_LEFT;ListView_InsertColumn(e.list,i,&c);}
            cue();e.drives=readDrives();sizeContent();return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mm=reinterpret_cast<MINMAXINFO*>(lp);
            if(screenW&&screenH) {
                // O Explorer fica na tela primária e não cobre a barra de tarefas ao maximizar.
                mm->ptMaxPosition={0,0}; mm->ptMaxSize={screenW,screenH-barH};
                mm->ptMinTrackSize={(std::min)(screenW,u(571)),(std::min)(screenH-barH,u(375))};
            }
            return 0;
        }
        case WM_NCHITTEST: {
            POINT p=mousePoint(lp);ScreenToClient(w,&p);const RECT r=client(w);
            auto id=hitAt(p);
            if(id==Minimize||id==Maximize||id==Close)return HTCLIENT;
            if(!e.maximized) {
                const int t=u(5);const bool l=p.x<t,rr=p.x>=r.right-t,tt=p.y<t,b=p.y>=r.bottom-t;
                if(l&&tt)return HTTOPLEFT;
                if(rr&&tt)return HTTOPRIGHT;
                if(l&&b)return HTBOTTOMLEFT;
                if(rr&&b)return HTBOTTOMRIGHT;
                if(l)return HTLEFT;
                if(rr)return HTRIGHT;
                if(tt)return HTTOP;
                if(b)return HTBOTTOM;
            }
            return p.y<u(39)?HTCAPTION:HTCLIENT;
        }
        case WM_NCLBUTTONDBLCLK:
            if(wp==HTCAPTION) {maximize(!e.maximized);return 0;}
            break;
        case WM_SYSCOMMAND:
            switch(wp&0xfff0) {
                case SC_MAXIMIZE: maximize(true);return 0;
                case SC_RESTORE: if(e.maximized&&!IsIconic(w)){maximize(false);return 0;}break;
                case SC_MOVE:case SC_SIZE: if(e.maximized) return 0;break;
            }
            break;
        case WM_NCACTIVATE:return TRUE;
        case WM_ACTIVATE: refreshTaskbar();redraw(w);return 0;
        case WM_SIZE: if(wp!=SIZE_MINIMIZED)sizeContent();refreshTaskbar();return 0;
        case WM_PAINT:paintExplorer();return 0;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: {
            auto d=reinterpret_cast<HDC>(wp);SetTextColor(d,col(0x264764));SetBkColor(d,col(0xffffff));
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        }
        case WM_MOUSEMOVE: {
            int x=hitAt(mousePoint(lp));if(x!=e.hot){e.hot=x;redraw(w);}trackMouse(w);return 0;
        }
        case WM_MOUSELEAVE: e.hot=0;redraw(w);return 0;
        case WM_LBUTTONDOWN: e.down=hitAt(mousePoint(lp));if(e.down)SetCapture(w);redraw(w);return 0;
        case WM_CAPTURECHANGED: if(e.down){e.down=0;redraw(w);}return 0;
        case WM_LBUTTONUP: {
            int k=hitAt(mousePoint(lp)),old=e.down;e.down=0;
            if(GetCapture()==w)ReleaseCapture();
            if(k&&k==old)perform(k);
            redraw(explorerWindow);return 0;
        }
        case WM_LBUTTONDBLCLK: {
            int id=hitAt(mousePoint(lp));if(id>=MainDrive && id<Crumb && inDisks(id-MainDrive)){Text p=e.drives[id-MainDrive].path;navigate(p);}return 0;
        }
        case WM_MOUSEWHEEL: if(e.path.empty()){e.driveScroll-=GET_WHEEL_DELTA_WPARAM(wp)*u(92)/WHEEL_DELTA;redraw(w);return 0;}break;
        case WM_COMMAND:
            if(LOWORD(wp)==IdFilter) {
                if(HIWORD(wp)==EN_CHANGE&&!e.muted) {
                    e.query=getText(e.filter);e.notice.clear();e.driveSel=-1;e.driveScroll=0;fillRows({},true);
                }
                if(HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==EN_KILLFOCUS) redraw(w);
            }
            if(LOWORD(wp)==IdAddress&&HIWORD(wp)==EN_KILLFOCUS&&e.editing)finishAddress();
            return 0;
        case WM_CONTEXTMENU: {
            if(!e.path.empty()) {
                POINT pt=mousePoint(lp);
                if(pt.x==-1 && pt.y==-1){Metrics m(w);pt={m.list.left+u(12),m.list.top+u(20)};ClientToScreen(w,&pt);}
                organize(pt);
            }return 0;
        }
        case WM_NOTIFY: {
            auto* nm=reinterpret_cast<NMHDR*>(lp);if(nm->hwndFrom!=e.list)break;
            switch(nm->code) {
                case LVN_GETDISPINFOW: {
                    auto& item=reinterpret_cast<NMLVDISPINFOW*>(lp)->item;
                    if(!inList(item.iItem))return 0;
                    const auto& x=e.all[e.view[item.iItem]];
                    if(item.mask&LVIF_TEXT) {
                        const Text* s=&x.name;
                        if(item.iSubItem==1)s=&x.dateLabel;
                        if(item.iSubItem==2)s=&x.kind;
                        if(item.iSubItem==3)s=&x.sizeLabel;
                        if(item.pszText && item.cchTextMax) lstrcpynW(item.pszText,s->c_str(),item.cchTextMax);
                    }
                    if(item.mask&LVIF_IMAGE)item.iImage=x.folder()?FOLDER:((folded(x.kind)==L"txt"||folded(x.kind)==L"log")?NOTE:FILEICON);
                    return 0;
                }
                case LVN_ITEMCHANGED: case LVN_ODSTATECHANGED:if(!e.muted){if(!e.copy)e.notice.clear();redraw(w);}return 0;
                case NM_DBLCLK: if(reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem>=0)openEntry();return 0;
                case NM_CUSTOMDRAW: {
                    auto* cd=reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
                    if(cd->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
                    if(cd->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                        cd->clrText=col(0x28455f);cd->clrTextBk=col((cd->nmcd.dwItemSpec&1)?0xf8fbfe:0xffffff);
                    }
                    return CDRF_DODEFAULT;
                }
            }
            break;
        }
        case WM_APP+17: e.drives=readDrives(); e.driveSel=-1; redraw(w);return 0;
        case WM_COPY_READY:copyDone(reinterpret_cast<CopyJob*>(lp));return 0;
        case WM_CLOSE:
            if(e.copy){e.notice=L"Aguarde a cópia ou use Cancelar antes de fechar.";redraw(w);return 0;}
            DestroyWindow(w);return 0;
        case WM_DESTROY:
            explorerWindow=nullptr;
            if(e.icons){ListView_SetImageList(e.list,nullptr,LVSIL_SMALL);ImageList_Destroy(e.icons);}
            e=Explorer{};refreshTaskbar();return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
}
void registerExplorer() {
    WNDCLASSW c{};c.style=CS_DBLCLKS;c.hInstance=app;c.lpszClassName=EXPLORER_CLASS;c.lpfnWndProc=explorerProc;
    c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=LoadIconW(app,MAKEINTRESOURCEW(1));RegisterClassW(&c);
}
bool isCopyActive() {return e.copy!=nullptr;}
void fitExplorerToDesktop() {
    if(e.h && e.maximized && !IsIconic(e.h))
        SetWindowPos(e.h,nullptr,0,0,screenW,screenH-barH,SWP_NOZORDER|SWP_NOACTIVATE);
}
void showExplorer(const Text& path,bool toggle) {
    bool created=false;
    if(!explorerWindow) {
        const int w=(std::min)(u(858),screenW-u(22)),h=(std::min)(u(557),screenH-barH-u(22));
        const int x=screenW>=u(1310)?screenW-w-u(81):(screenW-w)/2;
        const int y=(std::max)(u(10),(screenH-barH-h)*43/100);
        explorerWindow=CreateWindowExW(WS_EX_APPWINDOW,EXPLORER_CLASS,L"Este PC — Explorador Azul",
            WS_POPUP|WS_THICKFRAME|WS_SYSMENU|WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_CLIPCHILDREN,x,y,w,h,nullptr,nullptr,app,nullptr);
        if(!explorerWindow) {alert(desktopWindow,L"Não foi possível iniciar o Explorador Azul.");return;}
        created=true;
    }
    if(toggle && !created && GetForegroundWindow()==explorerWindow && !IsIconic(explorerWindow)) {
        ShowWindow(explorerWindow,SW_MINIMIZE);refreshTaskbar();return;
    }
    ShowWindow(explorerWindow,IsIconic(explorerWindow)?SW_RESTORE:SW_SHOW);
    fitExplorerToDesktop();SetForegroundWindow(explorerWindow);
    if(path==L"::pc") navigate({});
    else if(!path.empty()) {
        Text p=rootedPath(path,e.path);
        if(!p.empty())navigate(p);
        else alert(explorerWindow,L"Não foi possível interpretar o caminho informado.");
    } else if(created) {focusContent();}
    refreshTaskbar();
}
bool routeExplorer(MSG& m) {
    if(!explorerWindow || (m.hwnd!=explorerWindow&&!IsChild(explorerWindow,m.hwnd)))return false;
    if(m.message==WM_SYSKEYDOWN) {
        if(m.wParam==VK_LEFT){perform(Back);return true;}
        if(m.wParam==VK_RIGHT){perform(Forward);return true;}
        if(m.wParam==VK_UP){perform(Up);return true;}
        return false;
    }
    if(m.message==WM_CHAR && (m.wParam==L'\r'||m.wParam==L'\t'||m.wParam==27))return true;
    if(m.message!=WM_KEYDOWN)return false;
    const auto key=m.wParam;const bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0,shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
    if(key==VK_F5){perform(Refresh);return true;}
    if(key==VK_F4 || (ctrl&&key=='L')){beginAddress();return true;}
    if(ctrl&&key=='F'){SetFocus(e.filter);SendMessageW(e.filter,EM_SETSEL,0,-1);return true;}
    if(key==VK_TAB||key==VK_F6) {
        if(m.hwnd==e.address) {if(shift)focusContent();else SetFocus(e.filter);}
        else if(m.hwnd==e.filter) {if(shift)beginAddress();else focusContent();}
        else {if(shift)SetFocus(e.filter);else beginAddress();}
        return true;
    }
    if(m.hwnd==e.address) {
        if(key==VK_RETURN){acceptAddress();return true;}
        if(key==VK_ESCAPE){finishAddress();focusContent();return true;}
        return false;
    }
    if(m.hwnd==e.filter) {
        if(key==VK_RETURN||key==VK_DOWN){focusContent();if(e.path.empty())selectDriveByKey(VK_HOME);else if(!picked()&&!e.view.empty())ListView_SetItemState(e.list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);return true;}
        if(key==VK_ESCAPE){SetWindowTextW(e.filter,L"");focusContent();return true;}
        return false;
    }
    if(key==VK_RETURN){openEntry();return true;}
    if(key==VK_F2){renameEntry();return true;}
    if(key==VK_DELETE){deleteEntry();return true;}
    if(key==VK_BACK){perform(Up);return true;}
    if(ctrl&&shift&&key=='N'){createFolder();return true;}
    if(ctrl&&key=='C'){copyEntry();return true;}
    if(ctrl&&key=='V'){pasteEntry();return true;}
    if(key==VK_F10&&shift) {SendMessageW(explorerWindow,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(e.list),-1);return true;}
    if(e.path.empty()&&(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_HOME||key==VK_END)) {selectDriveByKey(static_cast<int>(key));return true;}
    return false;
}
