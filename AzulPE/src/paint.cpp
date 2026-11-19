#include "base.hpp"
#include <cstring>

Fonts ft;
static std::array<HBITMAP, ICON_COUNT> art{};
static const BYTE* iconBytes{};
static const BITMAPINFO* wall{};
static HBITMAP cache{};
static HANDLE interRegular{}, interSemibold{};
static int cacheW{}, cacheH{};

Paint::Paint(HWND w) : hwnd(w), r(client(w)) {
    target=BeginPaint(hwnd,&ps);
    dc=CreateCompatibleDC(target);
    if (dc && width(r)>0 && height(r)>0) canvas=CreateCompatibleBitmap(target,width(r),height(r));
    if (canvas) old=static_cast<HBITMAP>(SelectObject(dc,canvas));
    else { if(dc) DeleteDC(dc); dc=target; }
}
Paint::~Paint() {
    if(canvas) {
        const auto a=ps.rcPaint;
        BitBlt(target,a.left,a.top,width(a),height(a),dc,a.left,a.top,SRCCOPY);
        SelectObject(dc,old); DeleteObject(canvas); DeleteDC(dc);
    }
    EndPaint(hwnd,&ps);
}
void solid(HDC d, RECT r, unsigned hex) {
    HBRUSH b=CreateSolidBrush(col(hex)); FillRect(d,&r,b); DeleteObject(b);
}
void border(HDC d, RECT r, unsigned hex) {
    HBRUSH b=CreateSolidBrush(col(hex)); FrameRect(d,&r,b); DeleteObject(b);
}
static TRIVERTEX vertex(int x,int y,unsigned hex) {
    return {x,y,static_cast<COLOR16>(((hex>>16)&255)<<8),static_cast<COLOR16>(((hex>>8)&255)<<8),static_cast<COLOR16>((hex&255)<<8),0};
}
void tone(HDC d, RECT r, unsigned top, unsigned bottom, bool horizontal) {
    if(width(r)<=0 || height(r)<=0) return;
    TRIVERTEX v[2]={vertex(r.left,r.top,top),vertex(r.right,r.bottom,bottom)};
    GRADIENT_RECT seg{0,1}; GradientFill(d,v,2,&seg,1,horizontal?GRADIENT_FILL_RECT_H:GRADIENT_FILL_RECT_V);
}
void roundBox(HDC d, RECT r, unsigned fill, unsigned line, int radius) {
    HPEN p=CreatePen(PS_SOLID,1,col(line)); HBRUSH b=CreateSolidBrush(col(fill));
    auto pp=SelectObject(d,p), bb=SelectObject(d,b);
    RoundRect(d,r.left,r.top,r.right,r.bottom,u(radius),u(radius));
    SelectObject(d,pp); SelectObject(d,bb); DeleteObject(p); DeleteObject(b);
}
void rule(HDC d,int x,int y,int w,unsigned hex) { solid(d,area(x,y,w,1),hex); }
void label(HDC d,const Text& text,RECT r,HFONT font,unsigned color,UINT flags) {
    if(text.empty() || width(r)<=0 || height(r)<=0) return;
    SetBkMode(d,TRANSPARENT); SetTextColor(d,col(color));
    auto prev=SelectObject(d,font);
    DrawTextW(d,text.c_str(),static_cast<int>(text.size()),&r,flags|DT_NOPREFIX);
    SelectObject(d,prev);
}
void trackingLabel(HDC d,const Text& text,RECT r,HFONT font,unsigned hex,int pixels) {
    int prev=SetTextCharacterExtra(d,u(pixels));
    label(d,text,r,font,hex);
    SetTextCharacterExtra(d,prev);
}
int textWidth(HDC d,const Text& text,HFONT font) {
    auto prev=SelectObject(d,font); SIZE s{};
    GetTextExtentPoint32W(d,text.c_str(),static_cast<int>(text.size()),&s);
    SelectObject(d,prev); return s.cx;
}
static const void* findData(int id,DWORD* length=nullptr) {
    auto h=FindResourceW(app,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!h) return nullptr;
    if(length) *length=SizeofResource(app,h);
    return LockResource(LoadResource(app,h));
}
static HBITMAP bitmap32(int w,int h,void** pix) {
    BITMAPINFO data{};
    auto& b=data.bmiHeader; b.biSize=sizeof(BITMAPINFOHEADER); b.biWidth=w; b.biHeight=-h;
    b.biPlanes=1; b.biBitCount=32; b.biCompression=BI_RGB;
    return CreateDIBSection(nullptr,&data,DIB_RGB_COLORS,pix,nullptr,0);
}
static HFONT mkFont(int s,int weight=FW_NORMAL,const wchar_t* face=L"Inter") {
    return CreateFontW(-u(s),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
}
void setupPaint() {
    if(ft.body) return;
    DWORD fontSize=0, added=0;
    if(const void* bytes=findData(103,&fontSize)) interRegular=AddFontMemResourceEx(const_cast<void*>(bytes),fontSize,nullptr,&added);
    if(const void* bytes=findData(104,&fontSize)) interSemibold=AddFontMemResourceEx(const_cast<void*>(bytes),fontSize,nullptr,&added);
    auto face=interRegular?L"Inter":L"Segoe UI";
    ft={mkFont(10,FW_SEMIBOLD,face),mkFont(12,FW_NORMAL,face),mkFont(13,FW_NORMAL,face),
        mkFont(13,FW_SEMIBOLD,face),mkFont(14,FW_SEMIBOLD,face),mkFont(17,FW_SEMIBOLD,face),mkFont(26,FW_SEMIBOLD,face)};
    wall=static_cast<const BITMAPINFO*>(findData(101));
    DWORD length{}; iconBytes=static_cast<const BYTE*>(findData(102,&length));
    if(!iconBytes || length<ICON_COUNT*64*64*4) { iconBytes=nullptr; return; }
    for(int i=0;i<ICON_COUNT;++i) {
        void* dest{}; art[i]=bitmap32(64,64,&dest);
        if(dest) memcpy(dest,iconBytes+i*64*64*4,64*64*4);
    }
}
void dropWallpaper() { if(cache) DeleteObject(cache); cache=nullptr; cacheW=cacheH=0; }
void freePaint() {
    dropWallpaper();
    for(auto& a:art) { if(a) DeleteObject(a); a=nullptr; }
    for(auto f:{ft.tiny,ft.small,ft.body,ft.strong,ft.caption,ft.title,ft.heading}) if(f) DeleteObject(f);
    ft={};
    if(interRegular) RemoveFontMemResourceEx(interRegular);
    if(interSemibold) RemoveFontMemResourceEx(interSemibold);
    interRegular=interSemibold=nullptr;
}
void drawWallpaper(HDC dc,RECT r) {
    int w=width(r), h=height(r);
    if(!wall || w<=0 || h<=0) {tone(dc,r,0x051332,0x166194);return;}
    if(!cache || cacheW!=w || cacheH!=h) {
        dropWallpaper();
        cache=CreateCompatibleBitmap(dc,w,h);
        if(cache) {
            HDC mem=CreateCompatibleDC(dc); auto prev=SelectObject(mem,cache);
            SetStretchBltMode(mem,HALFTONE); SetBrushOrgEx(mem,0,0,nullptr);
            auto& hdr=wall->bmiHeader;
            // Conserva toda a curva de luz nas resoluções 4:3 comuns em WinPE.
            StretchDIBits(mem,0,0,w,h,0,0,hdr.biWidth,hdr.biHeight,
                reinterpret_cast<const BYTE*>(wall)+hdr.biSize,wall,DIB_RGB_COLORS,SRCCOPY);
            SelectObject(mem,prev); DeleteDC(mem); cacheW=w; cacheH=h;
        }
    }
    if(!cache) {tone(dc,r,0x051332,0x166194);return;}
    HDC mem=CreateCompatibleDC(dc); auto prev=SelectObject(mem,cache);
    BitBlt(dc,r.left,r.top,w,h,mem,0,0,SRCCOPY);
    SelectObject(mem,prev); DeleteDC(mem);
}
void icon(HDC d,Icon v,int x,int y,int size) {
    if(!art[v] || size<=0) return;
    HDC mem=CreateCompatibleDC(d); auto prev=SelectObject(mem,art[v]);
    AlphaBlend(d,x,y,size,size,mem,0,0,64,64,{AC_SRC_OVER,0,255,AC_SRC_ALPHA});
    SelectObject(mem,prev); DeleteDC(mem);
}
HICON nativeIcon(Icon v,int size,int canvasW,int canvasH) {
    if(!iconBytes) return nullptr;
    void* pix{}; HBITMAP color=bitmap32(canvasW,canvasH,&pix);
    if(!color || !pix) return nullptr;
    memset(pix,0,static_cast<size_t>(canvasW)*canvasH*4);
    auto* dst=static_cast<BYTE*>(pix); const BYTE* src=iconBytes+static_cast<int>(v)*64*64*4;
    const int ox=(canvasW-size)/2, oy=(canvasH-size)/2;
    for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
        const int y1=y*64/size,y2=(y+1)*64/size,x1=x*64/size,x2=(x+1)*64/size;
        unsigned mean[4]{}; unsigned count=0;
        for(int sy=y1;sy<(std::max)(y2,y1+1);++sy) for(int sx=x1;sx<(std::max)(x2,x1+1);++sx) {
            auto p=src+(sy*64+sx)*4; for(int k=0;k<4;++k) mean[k]+=p[k]; ++count;
        }
        auto p=dst+((y+oy)*canvasW+x+ox)*4;
        for(int k=0;k<4;++k) p[k]=static_cast<BYTE>(mean[k]/count);
    }
    std::vector<BYTE> maskData(static_cast<size_t>(((canvasW+15)/16)*2*canvasH),0);
    HBITMAP mask=CreateBitmap(canvasW,canvasH,1,1,maskData.data());
    ICONINFO ii{TRUE,0,0,mask,color}; HICON result=CreateIconIndirect(&ii);
    if(mask) DeleteObject(mask);
    DeleteObject(color); return result;
}
void glyph(HDC d,Glyph v,int x,int y,int size,unsigned hex) {
    auto px=[&](int n){return x+(n*size+8)/16;}; auto py=[&](int n){return y+(n*size+8)/16;};
    HPEN p=CreatePen(PS_SOLID,(std::max)(1,size/10),col(hex));
    auto op=SelectObject(d,p), ob=SelectObject(d,GetStockObject(NULL_BRUSH));
    auto lines=[&](std::initializer_list<POINT> pts) {
        bool first=true; for(auto q:pts) {if(first) MoveToEx(d,px(q.x),py(q.y),nullptr); else LineTo(d,px(q.x),py(q.y)); first=false;}
    };
    auto box=[&](int l,int t,int r,int b){ Rectangle(d,px(l),py(t),px(r),py(b)); };
    switch(v) {
        case Glyph::left: lines({{10,4},{5,8},{10,12}}); break;
        case Glyph::right: lines({{6,4},{11,8},{6,12}}); break;
        case Glyph::up: lines({{4,8},{8,4},{12,8}}); lines({{8,4},{8,13}}); break;
        case Glyph::down: lines({{4,6},{8,10},{12,6}}); break;
        case Glyph::search: Ellipse(d,px(2),py(2),px(11),py(11)); lines({{10,10},{14,14}}); break;
        case Glyph::refresh: lines({{13,9},{12,12},{9,14},{6,14},{3,12},{2,9},{3,6},{5,4},{8,3},{11,4},{13,6}}); lines({{13,2},{13,6},{9,6}}); break;
        case Glyph::plus: lines({{3,8},{13,8}}); lines({{8,3},{8,13}}); break;
        case Glyph::copy: box(2,6,10,14); lines({{5,4},{5,2},{13,2},{13,11},{11,11}}); break;
        case Glyph::paste: box(3,4,13,14); box(6,2,10,6); break;
        case Glyph::pen: lines({{3,14},{4,10},{11,3},{14,6},{7,13},{3,14}}); break;
        case Glyph::trash: lines({{4,6},{5,14},{11,14},{12,6}}); lines({{2,4},{14,4}}); lines({{6,4},{6,2},{10,2},{10,4}}); break;
        case Glyph::details: for(int i=4;i<=12;i+=4) {lines({{2,i},{4,i}}); lines({{6,i},{14,i}});} break;
        case Glyph::minus: lines({{4,11},{12,11}}); break;
        case Glyph::maximize: box(4,4,12,12); break;
        case Glyph::restore: box(3,6,11,14); lines({{5,5},{5,3},{13,3},{13,11},{11,11}}); break;
        case Glyph::close: lines({{4,4},{12,12}}); lines({{12,4},{4,12}}); break;
        case Glyph::network: box(2,3,14,11); lines({{8,10},{8,14}}); lines({{4,14},{12,14}}); break;
        case Glyph::desktop: box(2,3,14,13); lines({{2,10},{14,10}}); break;
        case Glyph::power: lines({{5,4},{3,6},{2,9},{3,12},{5,14},{8,15},{11,14},{13,12},{14,9},{13,6},{11,4}}); lines({{8,1},{8,8}}); break;
        case Glyph::check: lines({{3,8},{6,11},{13,4}}); break;
    }
    SelectObject(d,op); SelectObject(d,ob); DeleteObject(p);
}
void trackMouse(HWND w) { TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,w,0}; TrackMouseEvent(&t); }
void place(HWND h,RECT r,bool show) {
    SetWindowPos(h,nullptr,r.left,r.top,(std::max)(1,width(r)),(std::max)(1,height(r)),SWP_NOZORDER|SWP_NOACTIVATE|(show?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
}
