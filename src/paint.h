#pragma once
#include "model.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <functional>

namespace side {
template<class T> inline void Release(T*& value){if(value){value->Release();value=nullptr;}}
// Layered surfaces: canvas (window) < card < soft (hover / secondary fill). Colours are 0xRRGGBB.
struct Palette {
    unsigned canvas=0xf4f6f5,card=0xffffff,soft=0xedf1ef,ink=0x1c2622,muted=0x63726b,faint=0x8e9c95,line=0xe0e6e3;
    unsigned accent=0x1f7a5a,accentHover=0x186a4d,tint=0xe3f2ea,on=0xffffff,shell=0xe8edea,edge=0xc3cec8;
    unsigned warn=0x9a5b00,warnTint=0xfdf1dc,danger=0xb93a27,dangerTint=0xfce9e5,shadow=0x0c1a14;
    bool dark=false;
};
Palette LightPalette();
Palette DarkPalette();
Palette SystemPalette();
inline COLORREF ToColorRef(unsigned rgb){return RGB((rgb>>16)&0xff,(rgb>>8)&0xff,rgb&0xff);}
class Painter {
    struct Format{float size;bool bold;IDWriteTextFormat* format;IDWriteInlineObject* ellipsis;};
    ID2D1SolidColorBrush* brush=nullptr;
    IDWriteFactory* write=nullptr;
    std::vector<Format> formats;
    IDWriteTextFormat* GetFormat(float size,bool bold,IDWriteInlineObject** ellipsis=nullptr);
public:
    ID2D1RenderTarget* target=nullptr;
    Palette colors;
    Painter(ID2D1RenderTarget*,IDWriteFactory*,Palette);
    ~Painter();
    Painter(const Painter&)=delete;
    Painter& operator=(const Painter&)=delete;
    void Fill(float x,float y,float w,float h,unsigned color,float radius=0,float alpha=1);
    void Border(float x,float y,float w,float h,unsigned color,float radius=0,float thickness=1);
    void Line(float x1,float y1,float x2,float y2,unsigned color,float thickness=1,float alpha=1);
    void Circle(float x,float y,float r,unsigned color,bool fill=true,float thickness=1,float alpha=1);
    void Text(const std::wstring&,float x,float y,float w,float h,float size,unsigned color,bool bold=false,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING);
    float Measure(const std::wstring&,float size,bool bold=false);
    void Shadow(float x,float y,float w,float h,float radius,float strength=1);
    void Card(float x,float y,float w,float h,float radius,unsigned fill,unsigned border,bool shadow=true);
    void Toggle(float x,float y,bool on);
    void MouseIcon(float x,float y,float scale,unsigned color);
    void Check(float x,float y,unsigned color,float thickness=1.5f);
    void Close(float x,float y,unsigned color);
    void Chevron(float x,float y,unsigned color);
    void ChevronDown(float x,float y,unsigned color);
    void Gear(float cx,float cy,unsigned color);
    void Pencil(float x,float y,unsigned color);
    float KeycapsWidth(const Binding&) const;
    void Keycaps(const Binding&,float x,float y,float maximumWidth=240,float height=26,bool raised=false);
};
class Graphics {
public:
    ID2D1Factory* factory=nullptr;
    IDWriteFactory* write=nullptr;
    ID2D1HwndRenderTarget* mainTarget=nullptr;
    ID2D1HwndRenderTarget* quickTarget=nullptr;
    Palette palette=SystemPalette();
    ~Graphics();
    bool Init();
    void RefreshPalette(){palette=SystemPalette();}
    bool Ensure(HWND,ID2D1HwndRenderTarget*&);
    void Paint(HWND,bool quick,const std::function<void(Painter&)>&);
    bool SaveImage(const std::filesystem::path&,UINT width,UINT height,const std::function<void(Painter&)>&);
};
HICON MakeAppIcon(int size);
}
