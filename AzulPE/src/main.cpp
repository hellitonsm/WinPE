#include "base.hpp"
#include <dbt.h>

HINSTANCE app{};
HWND desktopWindow{},taskbarWindow{},startWindow{},explorerWindow{},textWindow{};
int screenW=0,screenH=0,barH=0;
double uiScale=1;
bool winPE=false;
Text programFolder,systemFolder,windowsFolder,documentsFolder,desktopFolder;

namespace {
constexpr wchar_t DesktopClass[]=L"AzulPE.AreaDeTrabalho", BarClass[]=L"AzulPE.Barra", MenuClass[]=L"AzulPE.Iniciar";
constexpr UINT WM_RAISE_START=WM_APP+18;
constexpr UINT HotStart=11,HotExplorer=12;
HWND searchEdit{}, oldActive{};
HBRUSH searchBrush{};
std::vector<std::pair<RECT,StartAction>> startRects;
int desktopHot=-1, desktopSelected=-1, barHot=-1, barDown=-1, startScroll=0;
StartAction startHot=StartAction::none,startDown=StartAction::none,startKey=StartAction::none;
bool accessoriesOpen=true, toolsOpen=false, powerTracking=false, quitting=false, noNotepad=false, hasTaskmgr=false;

struct Shortcut {StartAction action;Icon pic;const wchar_t* title;};
constexpr std::array<Shortcut,4> desktopItems{{{StartAction::docs,DOCS,L"Documentos"},{StartAction::pc,PC,L"Este PC"},
    {StartAction::network,NET,L"Rede"},{StartAction::terminal,CONSOLE,L"Prompt de\nComando"}}};
struct StartRow { StartAction cmd; Icon pic; Text title,sub; int height=33,indent=0; };
struct StartLayout {
    RECT r,left,right,search,power,drop;
    int footerY, split;
    explicit StartLayout(HWND h) {
        r=client(h); split=u(274);
        footerY=r.bottom-u(55);
        left=area(u(3),u(3),split-u(3),r.bottom-u(6));
        right=area(split,u(3),r.right-split-u(3),r.bottom-u(6));
        search=area(u(14),footerY+u(9),split-u(26),u(33));
        power=area(split+u(13),footerY+u(9),width(right)-u(54),u(33));
        drop=area(power.right-1,power.top,u(27),height(power));
    }
};
bool startIsOpen(){return startWindow && IsWindowVisible(startWindow);}
void menuPosition(bool show) {
    const int h=(std::min)(u(476),screenH-barH-u(7)),w=(std::min)(u(463),screenW-u(12));
    SetWindowPos(startWindow,HWND_TOPMOST,u(7),screenH-barH-h,w,h,
        SWP_NOACTIVATE|(show?SWP_SHOWWINDOW:0));
}
void layoutShell() {
    screenW=GetSystemMetrics(SM_CXSCREEN);screenH=GetSystemMetrics(SM_CYSCREEN);barH=u(48);
    SetWindowPos(desktopWindow,nullptr,0,0,screenW,screenH,SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowPos(taskbarWindow,HWND_TOPMOST,0,screenH-barH,screenW,barH,SWP_NOACTIVATE);
    menuPosition(false);
    fitExplorerToDesktop();
    dropWallpaper();redraw(desktopWindow);redraw(taskbarWindow);redraw(startWindow);
}
RECT desktopRect(int i) {return at(14,13+i*83,99,78);}
int deskHit(POINT p) {
    for(int i=0;i<static_cast<int>(desktopItems.size());++i)if(in(desktopRect(i),p))return i;
    return -1;
}
void desktopPaint(HWND w) {
    Paint paint(w);auto d=paint.dc;auto r=paint.r;drawWallpaper(d,r);
    for(int i=0;i<static_cast<int>(desktopItems.size());++i) {
        const auto& item=desktopItems[i];auto a=desktopRect(i);
        if(i==desktopHot || i==desktopSelected) {
            roundBox(d,a,i==desktopSelected?0x183e69:0x15345b,i==desktopSelected?0x699bc5:0x41628b,8);
            rule(d,a.left+u(5),a.top+1,width(a)-u(10),0x507fa5);
        }
        icon(d,item.pic,a.left+(width(a)-u(43))/2,a.top+u(2),u(43));
        auto words=area(a.left+u(1),a.top+u(47),width(a)-u(2),u(33));
        auto shade=words;OffsetRect(&shade,1,1);
        label(d,item.title,shade,ft.strong,0x06162f,DT_CENTER|DT_WORDBREAK);
        label(d,item.title,words,ft.strong,0xeaf6ff,DT_CENTER|DT_WORDBREAK);
    }
    if(r.right>u(540)) {
        const int x=r.right-u(259),y=u(29);
        icon(d,BRAND,x,y,u(40));
        trackingLabel(d,L"AZUL PE",area(x+u(49),y+u(2),u(166),u(20)),ft.title,0xe0f1ff,2);
        trackingLabel(d,L"AMBIENTE DE RECUPERAÇÃO",area(x+u(50),y+u(23),u(197),u(15)),ft.tiny,0x8daed1,1);
    }
}
LRESULT CALLBACK desktopProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:desktopPaint(w);return 0;
        case WM_DISPLAYCHANGE:layoutShell();return 0;
        case WM_DEVICECHANGE:
            if(wp==DBT_DEVNODES_CHANGED||wp==DBT_DEVICEARRIVAL||wp==DBT_DEVICEREMOVECOMPLETE) SetTimer(w,4,450,nullptr);
            return TRUE;
        case WM_TIMER:
            if(wp==4){KillTimer(w,4);if(explorerWindow)PostMessageW(explorerWindow,WM_APP+17,0,0);}
            if(wp==5){KillTimer(w,5);showStart(true);}
            return 0;
        case WM_MOUSEMOVE: {auto n=deskHit(mousePoint(lp));if(n!=desktopHot){desktopHot=n;redraw(w);}trackMouse(w);return 0;}
        case WM_MOUSELEAVE:desktopHot=-1;redraw(w);return 0;
        case WM_LBUTTONDOWN:showStart(false);desktopSelected=deskHit(mousePoint(lp));SetFocus(w);redraw(w);return 0;
        case WM_LBUTTONDBLCLK: {auto i=deskHit(mousePoint(lp));if(i>=0)act(desktopItems[i].action,w);return 0;}
        case WM_KEYDOWN:
            if(wp==VK_DOWN||wp==VK_RIGHT||wp==VK_TAB){desktopSelected=(desktopSelected+1)%static_cast<int>(desktopItems.size());redraw(w);return 0;}
            if(wp==VK_UP||wp==VK_LEFT){desktopSelected=(desktopSelected+static_cast<int>(desktopItems.size())-1)%static_cast<int>(desktopItems.size());redraw(w);return 0;}
            if(wp==VK_RETURN&&desktopSelected>=0){act(desktopItems[desktopSelected].action,w);return 0;}
            if(wp==VK_ESCAPE){desktopSelected=-1;redraw(w);return 0;}
            break;
        case WM_CONTEXTMENU: {
            POINT p=mousePoint(lp);if(p.x==-1&&p.y==-1)p={u(24),u(24)};
            HMENU h=CreatePopupMenu();AppendMenuW(h,MF_STRING,1,L"Abrir Explorador Azul");
            AppendMenuW(h,MF_STRING,2,L"Abrir Prompt de Comando");AppendMenuW(h,MF_SEPARATOR,0,nullptr);
            AppendMenuW(h,MF_STRING,3,L"Sobre o Azul PE");AppendMenuW(h,MF_STRING,4,L"Sair do Azul PE");
            auto id=TrackPopupMenu(h,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,0,w,nullptr);DestroyMenu(h);
            if(id==1)act(StartAction::explorer,w);
            if(id==2)act(StartAction::terminal,w);
            if(id==3)act(StartAction::about,w);
            if(id==4)quitShell(w);
            return 0;
        }
        case WM_HOTKEY:
            if(wp==HotStart)showStart(!startIsOpen());
            if(wp==HotExplorer){showStart(false);showExplorer();}
            return 0;
        case WM_RAISE_START:showStart(true);return 0;
        case WM_CLOSE:quitShell(w);return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}

struct BarLayout {
    RECT r,begin,browse,term,viewer,net,clock,peek;
    BarLayout(HWND h) {
        r=client(h);
        begin=area(u(8),u(5),u(100),r.bottom-u(10));
        browse=area(begin.right+u(9),u(6),u(explorerWindow?162:45),r.bottom-u(12));
        term=area(browse.right+u(5),u(6),u(45),r.bottom-u(12));
        viewer=area(term.right+u(5),u(6),u(45),r.bottom-u(12));
        peek=area(r.right-u(19),1,u(19),r.bottom-1);
        clock=area(peek.left-u(91),1,u(90),r.bottom-1);
        net=area(clock.left-u(41),u(5),u(38),r.bottom-u(10));
    }
};
int hitBar(HWND w,POINT p) {
    BarLayout b(w);
    if(in(b.begin,p))return 0;
    if(in(b.browse,p))return 1;
    if(in(b.term,p))return 2;
    if(textWindow&&in(b.viewer,p))return 3;
    if(in(b.net,p))return 4;
    if(in(b.peek,p))return 5;
    return -1;
}
void barButton(HDC d,RECT r,int id,bool active,bool running=false) {
    const bool hover=id==barHot;
    if(active || hover || running || id==0) {
        const unsigned outline=active?0x459acc:(hover?0x547f9f:0x385772);
        roundBox(d,r,active?0x204462:(hover?0x253f5c:0x192e46),outline,8);
        tone(d,area(r.left+2,r.top+2,width(r)-4,height(r)/2-1),active?0x396e95:(hover?0x3e5b79:0x324b66),
            active?0x264d70:(hover?0x2e4a68:0x253951));
        if(active || running)rule(d,r.left+u(7),r.bottom-u(3),width(r)-u(14),active?0x69d6ff:0x5093c1);
        if(barDown==id)border(d,inset(r,2),0x3d6d8d);
    }
}
void paintBar(HWND w) {
    Paint p(w);BarLayout b(w);auto d=p.dc;auto r=b.r;
    tone(d,r,0x152b44,0x0e1829);tone(d,area(0,1,r.right,u(20)),0x29435e,0x192e47);
    rule(d,0,0,r.right,0x73aaca);rule(d,0,1,r.right,0x355472);
    barButton(d,b.begin,0,startIsOpen());
    icon(d,BRAND,b.begin.left+u(8),b.begin.top+(height(b.begin)-u(31))/2,u(31));
    label(d,L"Iniciar",area(b.begin.left+u(44),b.begin.top,u(54),height(b.begin)),ft.strong,0xeef8ff);
    barButton(d,b.browse,1,explorerWindow && !IsIconic(explorerWindow) && GetForegroundWindow()==explorerWindow,explorerWindow!=nullptr);
    icon(d,FOLDER,b.browse.left+u(9),b.browse.top+(height(b.browse)-u(26))/2,u(26));
    if(explorerWindow)label(d,L"Explorador Azul",area(b.browse.left+u(42),b.browse.top,width(b.browse)-u(47),height(b.browse)),ft.body,0xe1eefc);
    barButton(d,b.term,2,false);icon(d,CONSOLE,b.term.left+u(10),b.term.top+(height(b.term)-u(25))/2,u(25));
    if(textWindow) {barButton(d,b.viewer,3,GetForegroundWindow()==textWindow,true);icon(d,NOTE,b.viewer.left+u(10),b.viewer.top+(height(b.viewer)-u(25))/2,u(25));}
    if(b.net.left-u(100)>b.viewer.right) {
        auto x=b.net.left-u(102);
        roundBox(d,area(x,u(14),u(6),u(6)),0x62badc,0x89d0eb,6);
        trackingLabel(d,winPE?L"WINPE":L"SESSÃO LOCAL",area(x+u(12),u(9),u(94),u(17)),ft.tiny,0x98b4ce,0);
        label(d,L"Azul PE",area(x+u(12),u(24),u(83),u(16)),ft.tiny,0x728eab);
    }
    if(barHot==4)roundBox(d,b.net,0x284562,0x476882,7);
    glyph(d,Glyph::network,b.net.left+(width(b.net)-u(17))/2,b.net.top+(height(b.net)-u(17))/2,u(17),0xd0e5f7);
    solid(d,area(b.clock.left,b.clock.top+u(8),1,height(b.clock)-u(16)),0x36516d);
    SYSTEMTIME st{};GetLocalTime(&st);wchar_t timestr[18]{}, date[26]{};
    swprintf(timestr,18,L"%02u:%02u",st.wHour,st.wMinute);
    swprintf(date,26,L"%02u/%02u/%04u",st.wDay,st.wMonth,st.wYear);
    const UINT center=DT_CENTER|DT_VCENTER|DT_SINGLELINE;
    label(d,timestr,area(b.clock.left+1,b.clock.top+u(5),width(b.clock)-1,u(20)),ft.strong,0xe8f4ff,center);
    label(d,date,area(b.clock.left+1,b.clock.top+u(23),width(b.clock)-1,u(17)),ft.tiny,0xacbfd5,center);
    tone(d,b.peek,barHot==5?0x4680a6:0x2d4c66,barHot==5?0x234b70:0x17293d);
    solid(d,area(b.peek.left,b.peek.top,1,height(b.peek)),0x5b819d);
    glyph(d,Glyph::desktop,b.peek.left+u(4),u(18),u(11),0xb6d5ef);
}
LRESULT CALLBACK barProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:paintBar(w);return 0;
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_CREATE:SetTimer(w,1,1000,nullptr);return 0;
        case WM_TIMER:redraw(w);return 0;
        case WM_MOUSEMOVE:{auto k=hitBar(w,mousePoint(lp));if(k!=barHot){barHot=k;redraw(w);}trackMouse(w);return 0;}
        case WM_MOUSELEAVE:barHot=-1;redraw(w);return 0;
        case WM_LBUTTONDOWN:barDown=hitBar(w,mousePoint(lp));if(barDown!=-1)SetCapture(w);redraw(w);return 0;
        case WM_LBUTTONUP: {
            auto old=barDown;barDown=-1;if(GetCapture()==w)ReleaseCapture();
            if(old==hitBar(w,mousePoint(lp))) switch(old) {
                case 0:showStart(!startIsOpen());break;
                case 1:showStart(false);showExplorer({},true);break;
                case 2:showStart(false);act(StartAction::terminal,desktopWindow);break;
                case 3:if(textWindow){showStart(false);if(GetForegroundWindow()==textWindow&&!IsIconic(textWindow))ShowWindow(textWindow,SW_MINIMIZE);else{ShowWindow(textWindow,IsIconic(textWindow)?SW_RESTORE:SW_SHOW);SetForegroundWindow(textWindow);}}break;
                case 4:showStart(false);act(StartAction::network,desktopWindow);break;
                case 5:desktopOnly();break;
            }redraw(w);return 0;
        }
        case WM_CAPTURECHANGED:barDown=-1;redraw(w);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
std::vector<StartRow> allStartRows(bool expanded=false) {
    std::vector<StartRow> rows;
    rows.push_back({StartAction::explorer,FOLDER,L"Explorador Azul",L"Arquivos e unidades",48,0});
    rows.push_back({StartAction::terminal,CONSOLE,L"Prompt de Comando",L"",36,0});
    if(!noNotepad)rows.push_back({StartAction::notepad,NOTE,L"Bloco de Notas",L"",36,0});
    rows.push_back({StartAction::accessories,FOLDER,L"Acessórios",L"",33,0});
    if(accessoriesOpen||expanded) {
        rows.push_back({StartAction::scriptFolder,FOLDER,L"Pasta do shell",L"",31,1});
        rows.push_back({StartAction::network,NET,L"Informações de rede",L"",31,1});
    }
    rows.push_back({StartAction::tools,APPS,L"Ferramentas do sistema",L"",33,0});
    if(toolsOpen||expanded) {
        if(hasTaskmgr)rows.push_back({StartAction::taskmgr,APPS,L"Gerenciador de Tarefas",L"",31,1});
        rows.push_back({StartAction::about,INFO,L"Sobre o Azul PE",L"",31,1});
    }
    return rows;
}
std::vector<StartRow> startRows() {
    if(!searchEdit || GetWindowTextLengthW(searchEdit)==0)return allStartRows();
    Text q=folded(getText(searchEdit));
    auto rows=allStartRows(true);
    rows.push_back({StartAction::pc,PC,L"Este PC",L"",36,0});
    rows.push_back({StartAction::docs,DOCS,L"Documentos",L"",36,0});
    rows.push_back({StartAction::desktop,DESK,L"Área de Trabalho",L"",36,0});
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const StartRow& a){
        return a.cmd==StartAction::accessories||a.cmd==StartAction::tools||folded(a.title+L" "+a.sub).find(q)==Text::npos;
    }),rows.end());
    for(auto& a:rows)a.indent=0;
    return rows;
}
int startMaxScroll(const StartLayout& m,const std::vector<StartRow>& rows) {
    int h=0;for(auto& a:rows)h+=u(a.height)+u(3);
    return (std::max)(0,h-(m.footerY-u(58)));
}
void menuHit(RECT r,StartAction a) {startRects.emplace_back(r,a);}
StartAction startAt(POINT q) {for(auto s=startRects.rbegin();s!=startRects.rend();++s)if(in(s->first,q))return s->second;return StartAction::none;}
void drawStartRow(HDC d,RECT a,const StartRow& row,bool emphasis=false) {
    const auto id=row.cmd;
    const bool hover=startKey==id||startHot==id;
    if(hover||emphasis) {
        roundBox(d,a,hover?(startDown==id?0x234c70:0x2b4e6e):0x203650,hover?0x5685a8:0x38546f,6);
        rule(d,a.left+u(5),a.top+1,width(a)-u(10),hover?0x587b9d:0x3a5872);
    }
    const auto size=u(emphasis?34:22); const int x=a.left+u(row.indent?31:10), y=a.top+(height(a)-size)/2;
    icon(d,row.pic,x,y,size);
    const int xText=x+u(emphasis?45:31);
    label(d,row.title,area(xText,a.top+(row.sub.empty()?0:u(5)),a.right-xText-u(9),row.sub.empty()?height(a):u(21)),
        emphasis?ft.caption:ft.body,hover?0xffffff:0xe0ebf7);
    if(!row.sub.empty())label(d,row.sub,area(xText,a.top+u(24),a.right-xText-u(11),u(18)),ft.small,0xa5bfd6);
    if(id==StartAction::accessories || id==StartAction::tools) {
        bool open=(id==StartAction::accessories)?accessoriesOpen:toolsOpen;
        glyph(d,open?Glyph::down:Glyph::right,a.right-u(23),a.top+(height(a)-u(13))/2,u(13),0xa2bbd3);
    }
}
void startPaint(HWND w) {
    Paint p(w);StartLayout m(w);auto d=p.dc;auto r=m.r;
    startRects.clear();
    solid(d,r,0x081524);tone(d,m.left,0x172639,0x101d30);tone(d,m.right,0x2b4058,0x22415e);
    border(d,inset(r,1),0x4c6881);
    rule(d,3,2,r.right-6,0x1e354e);solid(d,area(m.split,3,1,r.bottom-6),0x0e1e31);
    solid(d,area(m.split+1,4,1,r.bottom-8),0x415a72);
    const bool filtering=searchEdit&&GetWindowTextLengthW(searchEdit)>0;
    trackingLabel(d,filtering?L"RESULTADOS DA PESQUISA":L"ATALHOS FIXADOS",at(19,16,227,22),ft.tiny,0x8ba9c7);
    rule(d,u(15),u(44),m.split-u(30),0x32475e);
    const auto rows=startRows();
    int max=startMaxScroll(m,rows);startScroll=std::clamp(startScroll,0,max);
    int y=u(51)-startScroll;
    const int bottom=m.footerY-u(7), top=u(49);
    auto nc=SaveDC(d);IntersectClipRect(d,m.left.left,top,m.left.right,bottom);
    for(const auto& a:rows) {
        RECT row=area(u(12),y,m.split-u(24),u(a.height));
        if(row.bottom>top&&row.top<bottom) {
            drawStartRow(d,row,a,a.cmd==StartAction::explorer);
            RECT hit=row;hit.top=std::max<LONG>(row.top,top);hit.bottom=std::min<LONG>(row.bottom,bottom);menuHit(hit,a.cmd);
        }
        y+=u(a.height)+u(3);
    }
    if(rows.empty()) {
        label(d,L"Nenhum programa encontrado",at(23,61,227,26),ft.strong,0xe0ebf7);
        label(d,L"Tente outro termo de pesquisa.",at(23,87,227,19),ft.small,0x92b0cf);
    }
    if(max>0) {
        const int vh=bottom-top, sh=(std::max)(u(18),vh*vh/(vh+max));
        if(sh<vh)solid(d,area(m.left.right-u(5),top+(vh-sh)*startScroll/max,u(2),sh),0x4e6884);
    }
    RestoreDC(d,nc);
    const int sideX=m.split+u(12), sideW=width(m.right)-u(23);
    icon(d,BRAND,sideX+u(3),u(17),u(46));
    label(d,winPE?L"Sessão WinPE":L"Sessão local",area(sideX+u(55),u(20),sideW-u(55),u(23)),ft.caption,0xf0f7ff);
    label(d,winPE?L"Recuperação local":L"Azul PE",area(sideX+u(55),u(43),sideW-u(55),u(17)),ft.small,0xb1c8de);
    rule(d,sideX+u(5),u(76),sideW-u(10),0x4b627b);
    const std::array<StartRow,5> side{{{StartAction::docs,DOCS,L"Documentos",L"",35,0},
        {StartAction::desktop,DESK,L"Área de Trabalho",L"",35,0},{StartAction::pc,PC,L"Este PC",L"",35,0},
        {StartAction::network,NET,L"Rede",L"",35,0},{StartAction::about,INFO,L"Sobre o Azul PE",L"",35,0}}};
    y=u(85);
    for(size_t i=0;i<side.size();++i) {
        if(i==3){rule(d,sideX+u(5),y+u(3),sideW-u(10),0x4b627b);y+=u(13);}
        auto a=area(sideX,y,sideW,u(side[i].height));
        if(a.bottom<bottom){drawStartRow(d,a,side[i]);menuHit(a,side[i].cmd);}y+=u(36);
    }
    if(m.footerY-y>u(66)) {
        const int ly=m.footerY-u(58);
        trackingLabel(d,L"UNIDADE DO SISTEMA",area(sideX+u(7),ly,sideW-u(14),u(15)),ft.tiny,0x91afcb,0);
        icon(d,DRIVE,sideX+u(7),ly+u(19),u(19));
        Text drive=windowsFolder.size()>2?windowsFolder.substr(0,2):L"—";
        label(d,drive,area(sideX+u(32),ly+u(17),sideW-u(37),u(22)),ft.body,0xcedff1);
    }
    rule(d,u(10),m.footerY,m.split-u(20),0x3a5068);
    rule(d,sideX,m.footerY,sideW,0x4e6781);
    roundBox(d,m.search,0x0d1b2b,GetFocus()==searchEdit?0x629fcb:0x425d78,5);
    glyph(d,Glyph::search,m.search.right-u(25),m.search.top+u(9),u(15),0x9ab6cf);
    for(auto region:{m.power,m.drop}) {
        const bool dropdown=region.left==m.drop.left;
        const auto id=dropdown?StartAction::powerMenu:(winPE?StartAction::shutdown:StartAction::exit);
        const bool over=startHot==id||startKey==id;
        tone(d,region,over?0x416988:0x34516e,over?0x294b6b:0x253e58);
        border(d,region,over?0x7aa4c6:0x68849f);rule(d,region.left+1,region.top+1,width(region)-2,over?0x668eae:0x4c6c8a);
        if(dropdown) glyph(d,Glyph::right,region.left+(width(region)-u(16))/2,region.top+(height(region)-u(16))/2,u(16),0xe0edf9);
        else {
            glyph(d,Glyph::power,region.left+u(8),region.top+u(8),u(16),0xe1edf7);
            label(d,winPE?L"Desligar":L"Encerrar",area(region.left+u(33),region.top,width(region)-u(36),height(region)),ft.strong,0xf2f7fd);
        }
        menuHit(region,id);
    }
}
void sizeStart(HWND w) {
    if(searchEdit) {
        StartLayout m(w);
        place(searchEdit,area(m.search.left+u(9),m.search.top+u(7),width(m.search)-u(38),u(20)));
    }
}
void fireStart(StartAction cmd) {
    if(cmd==StartAction::none)return;
    if(cmd==StartAction::accessories || cmd==StartAction::tools) {
        if(cmd==StartAction::accessories)accessoriesOpen=!accessoriesOpen;else toolsOpen=!toolsOpen;
        redraw(startWindow);return;
    }
    if(cmd==StartAction::powerMenu) {
        StartLayout m(startWindow);POINT pt{m.power.left,m.power.top-u(3)};ClientToScreen(startWindow,&pt);powerPopup(startWindow,pt);return;
    }
    showStart(false);act(cmd,explorerWindow&&!IsIconic(explorerWindow)?explorerWindow:desktopWindow);
}
void nextStartKey(int d) {
    // Compacta ações duplicadas (por exemplo, Rede) para a navegação por teclado.
    std::vector<StartAction> ids;
    for(auto a:startRows())if(std::find(ids.begin(),ids.end(),a.cmd)==ids.end())ids.push_back(a.cmd);
    for(auto k:{StartAction::docs,StartAction::desktop,StartAction::pc,StartAction::network,StartAction::about,winPE?StartAction::shutdown:StartAction::exit,StartAction::powerMenu})
        if(std::find(ids.begin(),ids.end(),k)==ids.end())ids.push_back(k);
    auto it=std::find(ids.begin(),ids.end(),startKey);
    int n=static_cast<int>(ids.size()),i=it==ids.end()?(d>0?0:n-1):(static_cast<int>(it-ids.begin())+d+n)%n;
    startKey=ids[i];startHot=StartAction::none;
    const auto rows=startRows();int top=0;StartLayout m(startWindow);
    for(auto& a:rows) {
        const int h=u(a.height)+u(3);
        if(a.cmd==startKey) {
            if(top<startScroll)startScroll=top;
            else if(top+h>startScroll+m.footerY-u(58))startScroll=top+h-(m.footerY-u(58));
            break;
        }
        top+=h;
    }
    redraw(startWindow);
}
LRESULT CALLBACK startProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_CREATE:
            searchEdit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,0,0,w,reinterpret_cast<HMENU>(1),app,nullptr);
            if(!searchEdit)return -1;
            searchBrush=CreateSolidBrush(col(0x0d1b2b));
            SendMessageW(searchEdit,WM_SETFONT,reinterpret_cast<WPARAM>(ft.small),TRUE);
            SendMessageW(searchEdit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
            SendMessageW(searchEdit,EM_SETLIMITTEXT,160,0);
            SendMessageW(searchEdit,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Pesquisar programas…"));
            return 0;
        case WM_SIZE:sizeStart(w);return 0;
        case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE && !powerTracking && IsWindowVisible(w))showStart(false);return 0;
        case WM_PAINT:startPaint(w);return 0;
        case WM_CTLCOLOREDIT: {
            HDC d=reinterpret_cast<HDC>(wp);SetTextColor(d,col(0xe1efff));SetBkColor(d,col(0x0d1b2b));return reinterpret_cast<LRESULT>(searchBrush);
        }
        case WM_COMMAND:if(HIWORD(wp)==EN_CHANGE){startKey=StartAction::none;startScroll=0;redraw(w);}return 0;
        case WM_MOUSEMOVE: {auto a=startAt(mousePoint(lp));if(a!=startHot||startKey!=StartAction::none){startHot=a;startKey=StartAction::none;redraw(w);}trackMouse(w);return 0;}
        case WM_MOUSELEAVE:startHot=StartAction::none;redraw(w);return 0;
        case WM_MOUSEWHEEL:startScroll-=GET_WHEEL_DELTA_WPARAM(wp)*u(35)/WHEEL_DELTA;redraw(w);return 0;
        case WM_LBUTTONDOWN:startDown=startAt(mousePoint(lp));if(startDown!=StartAction::none)SetCapture(w);redraw(w);return 0;
        case WM_CAPTURECHANGED:startDown=StartAction::none;redraw(w);return 0;
        case WM_LBUTTONUP: {
            const auto a=startAt(mousePoint(lp)),old=startDown;startDown=StartAction::none;
            if(GetCapture()==w)ReleaseCapture();
            if(a==old)fireStart(a);
            redraw(startWindow);return 0;
        }
        case WM_CLOSE:showStart(false);return 0;
        case WM_DESTROY:if(searchBrush)DeleteObject(searchBrush);searchBrush=nullptr;return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
bool routeStart(MSG& m) {
    const bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
    if(m.message==WM_KEYDOWN&&ctrl&&m.wParam==VK_ESCAPE){showStart(!startIsOpen());return true;}
    if(m.message==WM_KEYDOWN&&m.wParam==VK_APPS&&m.hwnd==desktopWindow) {SendMessageW(desktopWindow,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(desktopWindow),-1);return true;}
    if(m.hwnd!=startWindow&&m.hwnd!=searchEdit)return false;
    if(m.message==WM_KEYDOWN) {
        switch(m.wParam) {
            case VK_ESCAPE:showStart(false);if(oldActive&&IsWindow(oldActive)&&!IsIconic(oldActive))SetForegroundWindow(oldActive);return true;
            case VK_UP:nextStartKey(-1);return true;
            case VK_DOWN:nextStartKey(1);return true;
            case VK_TAB:nextStartKey((GetKeyState(VK_SHIFT)&0x8000)?-1:1);return true;
            case VK_RETURN: {
                const auto rows=startRows();
                if(startKey!=StartAction::none)fireStart(startKey);else if(!rows.empty())fireStart(rows.front().cmd);
                return true;
            }
        }
    }
    if(m.message==WM_CHAR&&(m.wParam==L'\r'||m.wParam==L'\t'||m.wParam==27))return true;
    return false;
}
Text known(int csidl) {
    wchar_t p[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,csidl|CSIDL_FLAG_DONT_VERIFY,nullptr,SHGFP_TYPE_CURRENT,p)) && exists(p) &&
        (GetFileAttributesW(p)&FILE_ATTRIBUTE_DIRECTORY))return rootedPath(p);
    return {};
}
bool detectPE() {
    HKEY key{}; if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;
    RegCloseKey(key);return true;
}
void showKnown(const Text& p,const wchar_t* title,HWND owner) {
    if(p.empty()) {alert(owner,Text(L"A pasta ")+title+L" não existe nesta sessão. Use Este PC para acessar as unidades.");showExplorer(L"::pc");}
    else showExplorer(p);
}
void cls(const wchar_t* name,WNDPROC proc,UINT style) {
    WNDCLASSW c{};c.style=style;c.hInstance=app;c.lpszClassName=name;c.lpfnWndProc=proc;
    c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=LoadIconW(app,MAKEINTRESOURCEW(1));RegisterClassW(&c);
}
}

void refreshTaskbar(){redraw(taskbarWindow);}
void desktopOnly() {
    showStart(false);
    if(explorerWindow&&!IsIconic(explorerWindow))ShowWindow(explorerWindow,SW_MINIMIZE);
    if(textWindow&&!IsIconic(textWindow))ShowWindow(textWindow,SW_MINIMIZE);
    SetForegroundWindow(desktopWindow);SetFocus(desktopWindow);refreshTaskbar();
}
void showStart(bool show) {
    if(!startWindow)return;
    if(show) {
        if(startIsOpen())return;
        oldActive=GetForegroundWindow();startHot=startDown=startKey=StartAction::none;startScroll=0;
        SetWindowTextW(searchEdit,L"");menuPosition(true);SetForegroundWindow(startWindow);SetFocus(searchEdit);
        redraw(startWindow);
    } else if(startIsOpen()) {
        SetWindowPos(startWindow,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
        redraw(desktopWindow);
        if(explorerWindow)RedrawWindow(explorerWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    refreshTaskbar();
}
void quitShell(HWND owner) {
    if(quitting)return;
    if(isCopyActive()){showExplorer();alert(explorerWindow,L"Aguarde a cópia ou cancele antes de encerrar.");return;}
    if(MessageBoxW(owner,L"Fechar o Azul PE? Isso não desliga nem reinicia o computador.",L"Encerrar Azul PE",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION|MB_SETFOREGROUND)!=IDYES)return;
    quitting=true;
    if(explorerWindow)DestroyWindow(explorerWindow);
    if(textWindow)DestroyWindow(textWindow);
    if(startWindow){DestroyWindow(startWindow); startWindow=nullptr;}
    if(taskbarWindow){DestroyWindow(taskbarWindow);taskbarWindow=nullptr;}
    if(desktopWindow){DestroyWindow(desktopWindow);desktopWindow=nullptr;}
}
void powerPopup(HWND owner,POINT anchor) {
    HMENU h=CreatePopupMenu();const bool support=winPE&&!sysTool(L"wpeutil.exe").empty();
    AppendMenuW(h,MF_STRING|(support?0:MF_GRAYED),1,L"Desligar o computador");
    AppendMenuW(h,MF_STRING|(support?0:MF_GRAYED),2,L"Reiniciar o computador");
    AppendMenuW(h,MF_SEPARATOR,0,nullptr);AppendMenuW(h,MF_STRING,3,L"Sair do Azul PE");
    powerTracking=true;auto k=TrackPopupMenu(h,TPM_RETURNCMD|TPM_NONOTIFY|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON,anchor.x,anchor.y,0,owner,nullptr);powerTracking=false;
    DestroyMenu(h);
    if(k) {showStart(false);act(k==1?StartAction::shutdown:(k==2?StartAction::restart:StartAction::exit),desktopWindow);}
}
void act(StartAction a,HWND owner) {
    switch(a) {
        case StartAction::explorer:showExplorer();break;
        case StartAction::pc:showExplorer(L"::pc");break;
        case StartAction::docs:showKnown(documentsFolder,L"Documentos",owner);break;
        case StartAction::desktop:showKnown(desktopFolder,L"Área de Trabalho",owner);break;
        case StartAction::scriptFolder:showExplorer(programFolder);break;
        case StartAction::terminal:launch(sysTool(L"cmd.exe"),L"/d /k",systemFolder,owner);break;
        case StartAction::notepad:launch(sysTool(L"notepad.exe"),{},systemFolder,owner);break;
        case StartAction::taskmgr:launch(sysTool(L"taskmgr.exe"),{},systemFolder,owner);break;
        case StartAction::network: {
            Text p=sysTool(L"ipconfig.exe");
            if(p.empty())alert(owner,L"O ipconfig.exe não está presente nesta imagem. Para habilitar a rede em WinPE, inicialize a sessão com wpeinit e inclua o driver apropriado.",L"Informações de rede");
            else launch(sysTool(L"cmd.exe"),L"/d /k \"\""+p+L"\" /all\"",systemFolder,owner);
            break;
        }
        case StartAction::about: {
            Text s=L"Azul PE · versão 1.0\n\nShell Win32 para recuperação: área de trabalho, menu Iniciar e Explorador Azul.\n\n";
            s+=winPE?L"Ambiente: Windows PE\n":L"Ambiente: sessão local (fora do WinPE)\n";
            s+=L"Pasta do programa: "+programFolder+L"\n\nCtrl+Esc: Iniciar   ·   Ctrl+L: endereço   ·   F5: atualizar";
            alert(owner,s,L"Sobre o Azul PE");break;
        }
        case StartAction::restart:case StartAction::shutdown: {
            const Text util=winPE?sysTool(L"wpeutil.exe"):Text{};
            if(util.empty()){alert(owner,L"Esta ação só está disponível em uma sessão WinPE com wpeutil.exe.");break;}
            if(isCopyActive()){alert(owner,L"Aguarde ou cancele a cópia antes de prosseguir.");break;}
            const bool restart=a==StartAction::restart;
            if(MessageBoxW(owner,restart?L"Reiniciar o computador agora?\n\nAlterações mantidas apenas no disco RAM serão perdidas.":L"Desligar o computador agora?\n\nAlterações mantidas apenas no disco RAM serão perdidas.",L"Confirmar ação do WinPE",
                MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING|MB_SETFOREGROUND)==IDYES)launch(util,restart?L"Reboot":L"Shutdown",systemFolder,owner);
            break;
        }
        case StartAction::exit:quitShell(owner);break;
        default:break;
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    app=instance;
    HeapSetInformation(nullptr,HeapEnableTerminationOnCorruption,nullptr,0);
    HANDLE one=CreateMutexW(nullptr,FALSE,L"Local\\AzulPE.Shell.1");
    if(one&&GetLastError()==ERROR_ALREADY_EXISTS) {
        if(auto d=FindWindowW(DesktopClass,nullptr)) {
            DWORD pid=0;GetWindowThreadProcessId(d,&pid);
            if(pid)AllowSetForegroundWindow(pid);
            PostMessageW(d,WM_RAISE_START,0,0);
        }
        CloseHandle(one);return 0;
    }
    // Os controles se ajustam ao DPI inicial e também cabem em 800 × 600.
    SetProcessDPIAware();
    screenW=GetSystemMetrics(SM_CXSCREEN);screenH=GetSystemMetrics(SM_CYSCREEN);
    HDC dc=GetDC(nullptr);int dpi=dc?GetDeviceCaps(dc,LOGPIXELSX):96;if(dc)ReleaseDC(nullptr,dc);
    uiScale=std::clamp(dpi/96.0,0.69,(std::max)(0.69,(std::min)({1.75,screenW/780.0,screenH/560.0})));
    barH=u(48);
    wchar_t sys[MAX_PATH]{};GetSystemDirectoryW(sys,MAX_PATH);systemFolder=sys;
    GetWindowsDirectoryW(sys,MAX_PATH);windowsFolder=sys;
    Text p(32768,L'\0');DWORD n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));p.resize(n);programFolder=parentPath(rootedPath(p));
    winPE=detectPE(); noNotepad=sysTool(L"notepad.exe").empty();hasTaskmgr=!sysTool(L"taskmgr.exe").empty();
    desktopFolder=known(CSIDL_DESKTOPDIRECTORY);documentsFolder=known(CSIDL_PERSONAL);
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&cc);
    setupPaint();
    cls(DesktopClass,desktopProc,CS_DBLCLKS);cls(BarClass,barProc,0);cls(MenuClass,startProc,0);registerExplorer();
    desktopWindow=CreateWindowExW(WS_EX_TOOLWINDOW,DesktopClass,L"Área de Trabalho — Azul PE",WS_POPUP,0,0,screenW,screenH,nullptr,nullptr,app,nullptr);
    if(desktopWindow)taskbarWindow=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,BarClass,L"Barra de tarefas — Azul PE",WS_POPUP,0,screenH-barH,screenW,barH,nullptr,nullptr,app,nullptr);
    if(taskbarWindow)startWindow=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,MenuClass,L"Iniciar — Azul PE",WS_POPUP|WS_CLIPCHILDREN,0,0,u(463),u(476),nullptr,nullptr,app,nullptr);
    if(!desktopWindow||!taskbarWindow||!startWindow) {alert(nullptr,L"Não foi possível criar a interface do Azul PE.");return 1;}
    layoutShell();ShowWindow(desktopWindow,SW_SHOW);ShowWindow(taskbarWindow,SW_SHOWNOACTIVATE);
    SetForegroundWindow(desktopWindow);SetFocus(desktopWindow);
    if(winPE){RegisterHotKey(desktopWindow,HotStart,MOD_CONTROL|MOD_NOREPEAT,VK_ESCAPE);RegisterHotKey(desktopWindow,HotExplorer,MOD_WIN|MOD_NOREPEAT,'E');}
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool explorer=false, menu=false; Text firstPath;
    for(int i=1;argv&&i<argc;++i) {
        const Text arg=argv[i];
        if(arg==L"--explorer")explorer=true;
        if(arg==L"--inicio")menu=true;
        if(arg==L"--caminho"&&i+1<argc){explorer=true;firstPath=argv[++i];}
    }
    if(argv)LocalFree(argv);
    if(explorer)showExplorer(firstPath);
    if(menu)SetTimer(desktopWindow,5,230,nullptr);
    MSG m{};
    while(GetMessageW(&m,nullptr,0,0)>0) {
        if(routeStart(m)||routeExplorer(m))continue;
        TranslateMessage(&m);DispatchMessageW(&m);
    }
    freePaint();if(one)CloseHandle(one);return static_cast<int>(m.wParam);
}
