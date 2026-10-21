#include "paint.h"
#include <algorithm>
#include <cmath>

namespace side {
static D2D1_COLOR_F Color(unsigned rgb,float alpha=1){return D2D1::ColorF(rgb,alpha);}
Palette LightPalette(){return Palette{};}
Palette DarkPalette() {
    Palette p;
    p.canvas=0x171b1a;p.card=0x202624;p.soft=0x2a312e;p.ink=0xe7edea;p.muted=0xa0afa8;p.faint=0x74837c;p.line=0x333b38;
    p.accent=0x5cc79a;p.accentHover=0x74d3ab;p.tint=0x1d3a2d;p.on=0x0c2018;p.shell=0x39433f;p.edge=0x56625d;
    p.warn=0xf0b357;p.warnTint=0x3a2f1b;p.danger=0xf0897a;p.dangerTint=0x3e2420;p.shadow=0x000000;p.dark=true;
    return p;
}
Palette SystemPalette() {
    DWORD value=1,bytes=sizeof(value);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS&&!value)return DarkPalette();
    return LightPalette();
}
Painter::Painter(ID2D1RenderTarget* t,IDWriteFactory* w,Palette p):write(w),target(t),colors(p){target->CreateSolidColorBrush(Color(p.ink),&brush);}
Painter::~Painter(){for(auto& f:formats){Release(f.ellipsis);Release(f.format);}Release(brush);}
IDWriteTextFormat* Painter::GetFormat(float size,bool bold,IDWriteInlineObject** ellipsis) {
    for(auto& f:formats)if(f.size==size&&f.bold==bold){if(ellipsis)*ellipsis=f.ellipsis;return f.format;}
    Format entry{size,bold,nullptr,nullptr};
    if(FAILED(write->CreateTextFormat(L"Microsoft YaHei UI",nullptr,bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-cn",&entry.format)))return nullptr;
    entry.format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);entry.format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
    if(SUCCEEDED(write->CreateEllipsisTrimmingSign(entry.format,&entry.ellipsis)))entry.format->SetTrimming(&trim,entry.ellipsis);
    formats.push_back(entry);if(ellipsis)*ellipsis=entry.ellipsis;return entry.format;
}
void Painter::Fill(float x,float y,float w,float h,unsigned color,float radius,float alpha){if(w<=0||h<=0)return;brush->SetColor(Color(color,alpha));auto rect=D2D1::RectF(x,y,x+w,y+h);radius=std::min({radius,w/2,h/2});if(radius>0)target->FillRoundedRectangle(D2D1::RoundedRect(rect,radius,radius),brush);else target->FillRectangle(rect,brush);}
void Painter::Border(float x,float y,float w,float h,unsigned color,float radius,float thickness){if(w<=0||h<=0)return;brush->SetColor(Color(color));auto rect=D2D1::RectF(x,y,x+w,y+h);radius=std::min({radius,w/2,h/2});if(radius>0)target->DrawRoundedRectangle(D2D1::RoundedRect(rect,radius,radius),brush,thickness);else target->DrawRectangle(rect,brush,thickness);}
void Painter::Line(float x1,float y1,float x2,float y2,unsigned color,float thickness,float alpha){brush->SetColor(Color(color,alpha));target->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush,thickness);}
void Painter::Circle(float x,float y,float r,unsigned color,bool fill,float thickness,float alpha){brush->SetColor(Color(color,alpha));auto ellipse=D2D1::Ellipse(D2D1::Point2F(x,y),r,r);if(fill)target->FillEllipse(ellipse,brush);else target->DrawEllipse(ellipse,brush,thickness);}
void Painter::Text(const std::wstring& text,float x,float y,float w,float h,float size,unsigned color,bool bold,DWRITE_TEXT_ALIGNMENT alignment) {
    if(text.empty()||w<=0||h<=0)return;
    IDWriteTextFormat* format=GetFormat(size,bold);if(!format)return;
    format->SetTextAlignment(alignment);
    IDWriteTextLayout* layout=nullptr;
    if(SUCCEEDED(write->CreateTextLayout(text.c_str(),UINT32(text.size()),format,w,h,&layout))){brush->SetColor(Color(color));target->DrawTextLayout(D2D1::Point2F(x,y),layout,brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);}
    Release(layout);
}
float Painter::Measure(const std::wstring& text,float size,bool bold) {
    if(text.empty())return 0;
    IDWriteTextFormat* format=GetFormat(size,bold);if(!format)return 0;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextLayout* layout=nullptr;float width=0;
    if(SUCCEEDED(write->CreateTextLayout(text.c_str(),UINT32(text.size()),format,10000,100,&layout))){DWRITE_TEXT_METRICS metrics{};if(SUCCEEDED(layout->GetMetrics(&metrics)))width=metrics.widthIncludingTrailingWhitespace;}
    Release(layout);return width;
}
// Direct2D has no blur without effects; a few expanding low-alpha layers read as a soft drop shadow.
void Painter::Shadow(float x,float y,float w,float h,float radius,float strength){float alpha=(colors.dark?.10f:.035f)*strength;for(int i=1;i<=3;++i){float d=float(i);Fill(x-d+1,y-d+1+d*.9f,w+2*d-2,h+2*d-2,colors.shadow,radius+d,alpha/d);}}
void Painter::Card(float x,float y,float w,float h,float radius,unsigned fill,unsigned border,bool shadow){if(shadow)Shadow(x,y,w,h,radius);Fill(x,y,w,h,fill,radius);Border(x+.5f,y+.5f,w-1,h-1,border,radius);}
void Painter::Toggle(float x,float y,bool on){Fill(x,y,38,22,on?colors.accent:colors.soft,11);if(!on)Border(x+.5f,y+.5f,37,21,colors.edge,11);Circle(on?x+27:x+11,y+11,on?7.f:6.f,on?colors.on:colors.muted);}
void Painter::MouseIcon(float x,float y,float scale,unsigned color){Border(x,y,12*scale,19*scale,color,6*scale,1.3f*scale);Line(x+6*scale,y+3.5f*scale,x+6*scale,y+7*scale,color,1.4f*scale);}
void Painter::Check(float x,float y,unsigned color,float thickness){Line(x,y+4,x+3,y+7,color,thickness);Line(x+3,y+7,x+9,y,color,thickness);}
void Painter::Close(float x,float y,unsigned color){Line(x,y,x+9,y+9,color,1.4f);Line(x+9,y,x,y+9,color,1.4f);}
void Painter::Chevron(float x,float y,unsigned color){Line(x,y,x+4,y+4,color,1.4f);Line(x+4,y+4,x,y+8,color,1.4f);}
void Painter::ChevronDown(float x,float y,unsigned color){Line(x,y,x+4,y+4,color,1.4f);Line(x+4,y+4,x+8,y,color,1.4f);}
void Painter::Gear(float cx,float cy,unsigned color){for(int i=0;i<8;++i){float a=3.14159265f*float(i)/4,c=std::cos(a),s=std::sin(a);Line(cx+5.2f*c,cy+5.2f*s,cx+8.4f*c,cy+8.4f*s,color,2.6f);}Circle(cx,cy,5.6f,color,false,1.7f);Circle(cx,cy,2.2f,color,false,1.4f);}
void Painter::Pencil(float x,float y,unsigned color){Line(x+2,y+12,x+11,y+3,color,1.5f);Line(x+9.5f,y+1.5f,x+12.5f,y+4.5f,color,1.5f);Line(x+1.2f,y+13,x+3.4f,y+12.6f,color,1.4f);}
static std::vector<std::wstring> KeyLabels(const Binding& b){std::vector<std::wstring> keys;if(b.modifiers&ModCtrl)keys.push_back(L"Ctrl");if(b.modifiers&ModAlt)keys.push_back(L"Alt");if(b.modifiers&ModShift)keys.push_back(L"Shift");if(b.modifiers&ModWin)keys.push_back(L"Win");keys.push_back(KeyName(b.key));return keys;}
static float CapWidth(const std::wstring& key){return std::max(26.f,float(key.size())*6.8f+16);}
float Painter::KeycapsWidth(const Binding& b) const {if(!b.key)return 0;auto keys=KeyLabels(b);float total=0;for(const auto& k:keys)total+=CapWidth(k);return total+10*float(keys.size()-1);}
void Painter::Keycaps(const Binding& b,float x,float y,float maximumWidth,float height,bool raised) {
    if(!b.key){Text(L"保留鼠标原功能",x,y,maximumWidth,height,12,colors.faint);return;}
    if(KeycapsWidth(b)>maximumWidth){Text(ShortcutText(b),x,y,maximumWidth,height,12,colors.ink);return;}
    auto keys=KeyLabels(b);
    for(size_t i=0;i<keys.size();++i){
        float width=CapWidth(keys[i]);
        Fill(x,y,width,height,raised?colors.card:colors.soft,6);Border(x+.5f,y+.5f,width-1,height-1,colors.line,6);Line(x+5,y+height-.8f,x+width-5,y+height-.8f,colors.edge,1.2f);
        Text(keys[i],x+2,y,width-4,height-1,11,colors.ink,false,DWRITE_TEXT_ALIGNMENT_CENTER);x+=width;
        if(i+1<keys.size()){Text(L"+",x,y,10,height,10,colors.faint,false,DWRITE_TEXT_ALIGNMENT_CENTER);x+=10;}
    }
}
Graphics::~Graphics(){Release(mainTarget);Release(quickTarget);Release(write);Release(factory);}
bool Graphics::Init(){return SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&factory))&&SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&write)));}
bool Graphics::Ensure(HWND window,ID2D1HwndRenderTarget*& target) {
    if(target)return true;
    RECT rect{};GetClientRect(window,&rect);float dpi=float(GetDpiForWindow(window));
    auto properties=D2D1::RenderTargetProperties();properties.dpiX=dpi;properties.dpiY=dpi;
    return SUCCEEDED(factory->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(window,D2D1::SizeU(rect.right,rect.bottom)),&target));
}
void Graphics::Paint(HWND window,bool quick,const std::function<void(Painter&)>& draw) {
    auto*& target=quick?quickTarget:mainTarget;if(!Ensure(window,target))return;
    target->BeginDraw();target->Clear(Color(quick?palette.card:palette.canvas));
    {Painter painter(target,write,palette);draw(painter);}
    if(target->EndDraw()==D2DERR_RECREATE_TARGET)Release(target);
}
bool Graphics::SaveImage(const std::filesystem::path& path,UINT width,UINT height,const std::function<void(Painter&)>& draw) {
    IWICImagingFactory* wic=nullptr;IWICBitmap* bitmap=nullptr;ID2D1RenderTarget* target=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic));
    if(SUCCEEDED(hr))hr=wic->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap);
    if(SUCCEEDED(hr))hr=factory->CreateWicBitmapRenderTarget(bitmap,D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96),&target);
    if(SUCCEEDED(hr)){target->BeginDraw();target->Clear(Color(palette.canvas));{Painter painter(target,write,palette);draw(painter);}hr=target->EndDraw();}
    if(SUCCEEDED(hr))hr=wic->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);
    if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,nullptr);
    if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);
    if(SUCCEEDED(hr))hr=frame->SetSize(width,height);
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppPBGRA;
    if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
    if(SUCCEEDED(hr))hr=frame->WriteSource(bitmap,nullptr);
    if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();
    Release(frame);Release(encoder);Release(stream);Release(target);Release(bitmap);Release(wic);return SUCCEEDED(hr);
}
HICON MakeAppIcon(int size) {
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=size;info.bmiHeader.biHeight=-size;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP color=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);HGDIOBJ old=SelectObject(dc,color);
    memset(pixels,0,size*size*4);HBRUSH green=CreateSolidBrush(ToColorRef(LightPalette().accent));HGDIOBJ oldBrush=SelectObject(dc,green);HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,1,1,size-1,size-1,size/3,size/3);
    HPEN pen=CreatePen(PS_SOLID,std::max(1,size/16),RGB(255,255,255));SelectObject(dc,pen);SelectObject(dc,GetStockObject(HOLLOW_BRUSH));RoundRect(dc,size*10/32,size*7/32,size*22/32,size*26/32,size*10/32,size*10/32);MoveToEx(dc,size/2,size*10/32,nullptr);LineTo(dc,size/2,size*14/32);
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(green);
    auto* data=static_cast<DWORD*>(pixels);for(int i=0;i<size*size;++i)if(data[i]&0x00ffffff)data[i]|=0xff000000;
    SelectObject(dc,old);HBITMAP mask=CreateBitmap(size,size,1,1,nullptr);ICONINFO icon{};icon.fIcon=TRUE;icon.hbmColor=color;icon.hbmMask=mask;HICON result=CreateIconIndirect(&icon);DeleteObject(mask);DeleteObject(color);DeleteDC(dc);return result;
}
}
