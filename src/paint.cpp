#include "paint.h"
#include <algorithm>
#include <cmath>

namespace side {
static D2D1_COLOR_F Color(unsigned rgb,float alpha=1){return D2D1::ColorF(rgb,alpha);}
Palette SystemPalette() {
    Palette p;DWORD value=1,bytes=sizeof(value);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS&&!value)
        p={0x202725,0x272f2c,0xe6ede9,0xa5b5ad,0x3c4841,0x90d2b4,0x30473b,0x163326,0x47534c,0x64736b,true};
    return p;
}
Painter::Painter(ID2D1RenderTarget* t,IDWriteFactory* w,Palette p):write(w),target(t),colors(p){target->CreateSolidColorBrush(Color(p.ink),&brush);}
Painter::~Painter(){Release(brush);}
void Painter::Fill(float x,float y,float w,float h,unsigned color,float radius,float alpha){brush->SetColor(Color(color,alpha));auto rect=D2D1::RectF(x,y,x+w,y+h);if(radius)target->FillRoundedRectangle(D2D1::RoundedRect(rect,radius,radius),brush);else target->FillRectangle(rect,brush);}
void Painter::Border(float x,float y,float w,float h,unsigned color,float radius,float thickness){brush->SetColor(Color(color));auto rect=D2D1::RectF(x,y,x+w,y+h);if(radius)target->DrawRoundedRectangle(D2D1::RoundedRect(rect,radius,radius),brush,thickness);else target->DrawRectangle(rect,brush,thickness);}
void Painter::Line(float x1,float y1,float x2,float y2,unsigned color,float thickness,float alpha){brush->SetColor(Color(color,alpha));target->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush,thickness);}
void Painter::Circle(float x,float y,float r,unsigned color,bool fill,float thickness,float alpha){brush->SetColor(Color(color,alpha));auto ellipse=D2D1::Ellipse(D2D1::Point2F(x,y),r,r);if(fill)target->FillEllipse(ellipse,brush);else target->DrawEllipse(ellipse,brush,thickness);}
void Painter::Text(const std::wstring& text,float x,float y,float w,float h,float size,unsigned color,bool bold,DWRITE_TEXT_ALIGNMENT alignment) {
    if(text.empty()||w<=0||h<=0)return;
    IDWriteTextFormat* format=nullptr;IDWriteTextLayout* layout=nullptr;IDWriteInlineObject* ellipsis=nullptr;
    HRESULT hr=write->CreateTextFormat(L"Microsoft YaHei UI",nullptr,bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-cn",&format);
    if(FAILED(hr))return;
    format->SetTextAlignment(alignment);format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
    if(SUCCEEDED(write->CreateEllipsisTrimmingSign(format,&ellipsis)))format->SetTrimming(&trim,ellipsis);
    if(SUCCEEDED(write->CreateTextLayout(text.c_str(),UINT32(text.size()),format,w,h,&layout))){brush->SetColor(Color(color));target->DrawTextLayout(D2D1::Point2F(x,y),layout,brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);}
    Release(layout);Release(ellipsis);Release(format);
}
void Painter::MouseIcon(float x,float y,float scale,unsigned color){Border(x,y,12*scale,19*scale,color,6*scale,1.2f*scale);Line(x+6*scale,y+3*scale,x+6*scale,y+7*scale,color,1.3f*scale);}
void Painter::Check(float x,float y,unsigned color){Line(x,y+4,x+3,y+7,color,1.4f);Line(x+3,y+7,x+9,y,color,1.4f);}
void Painter::Close(float x,float y,unsigned color){Line(x,y,x+8,y+8,color,1.3f);Line(x+8,y,x,y+8,color,1.3f);}
void Painter::Chevron(float x,float y,unsigned color){Line(x,y,x+4,y+4,color,1.3f);Line(x+4,y+4,x,y+8,color,1.3f);}
void Painter::Keycaps(const Binding& b,float x,float y,float maximumWidth) {
    if(!b.key){Text(L"保留鼠标原功能",x,y,maximumWidth,25,12,colors.muted);return;}
    std::vector<std::wstring> keys;
    if(b.modifiers&ModCtrl)keys.push_back(L"Ctrl");if(b.modifiers&ModAlt)keys.push_back(L"Alt");if(b.modifiers&ModShift)keys.push_back(L"Shift");if(b.modifiers&ModWin)keys.push_back(L"Win");keys.push_back(KeyName(b.key));
    float total=0;for(const auto& k:keys)total+=std::max(25.f,float(k.size())*6.7f+15)+9;
    if(total>maximumWidth){Text(ShortcutText(b),x,y,maximumWidth,26,12,colors.ink);return;}
    for(size_t i=0;i<keys.size();++i){const auto& k=keys[i];float width=std::max(25.f,float(k.size())*6.7f+15);Fill(x,y,width,25,colors.soft,5);Border(x+.5f,y+.5f,width-1,24,colors.line,5);Line(x+4,y+24,x+width-4,y+24,colors.edge,.7f);Text(k,x+2,y,width-4,24,11,colors.ink,false,DWRITE_TEXT_ALIGNMENT_CENTER);x+=width;if(i+1<keys.size()){Text(L"+",x,y,9,25,10,colors.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);x+=9;}}
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
    target->BeginDraw();auto colors=SystemPalette();target->Clear(Color(colors.bg));
    {Painter painter(target,write,colors);draw(painter);}
    if(target->EndDraw()==D2DERR_RECREATE_TARGET)Release(target);
}
bool Graphics::SaveImage(const std::filesystem::path& path,UINT width,UINT height,const std::function<void(Painter&)>& draw) {
    IWICImagingFactory* wic=nullptr;IWICBitmap* bitmap=nullptr;ID2D1RenderTarget* target=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic));
    if(SUCCEEDED(hr))hr=wic->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap);
    if(SUCCEEDED(hr))hr=factory->CreateWicBitmapRenderTarget(bitmap,D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96),&target);
    if(SUCCEEDED(hr)){target->BeginDraw();target->Clear(Color(SystemPalette().bg));{Painter painter(target,write,SystemPalette());draw(painter);}hr=target->EndDraw();}
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
    memset(pixels,0,size*size*4);HBRUSH green=CreateSolidBrush(RGB(38,115,91));HGDIOBJ oldBrush=SelectObject(dc,green);HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,1,1,size-1,size-1,size/3,size/3);
    HPEN pen=CreatePen(PS_SOLID,std::max(1,size/16),RGB(255,255,255));SelectObject(dc,pen);SelectObject(dc,GetStockObject(HOLLOW_BRUSH));RoundRect(dc,size*10/32,size*7/32,size*22/32,size*26/32,size*10/32,size*10/32);MoveToEx(dc,size/2,size*10/32,nullptr);LineTo(dc,size/2,size*14/32);
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(green);
    auto* data=static_cast<DWORD*>(pixels);for(int i=0;i<size*size;++i)if(data[i]&0x00ffffff)data[i]|=0xff000000;
    SelectObject(dc,old);HBITMAP mask=CreateBitmap(size,size,1,1,nullptr);ICONINFO icon{};icon.fIcon=TRUE;icon.hbmColor=color;icon.hbmMask=mask;HICON result=CreateIconIndirect(&icon);DeleteObject(mask);DeleteObject(color);DeleteDC(dc);return result;
}
}
