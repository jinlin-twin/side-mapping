#pragma once
#include "model.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <functional>

namespace side {
template<class T> inline void Release(T*& value){if(value){value->Release();value=nullptr;}}
struct Palette {
    unsigned bg=0xffffff,soft=0xf4f7f6,ink=0x23312c,muted=0x697970,line=0xe3eae6,green=0x26735b,tint=0xeaf4ee,on=0xffffff,shell=0xe9efec,edge=0xc8d4cd;
    bool dark=false;
};
Palette SystemPalette();
class Painter {
    ID2D1SolidColorBrush* brush=nullptr;
    IDWriteFactory* write=nullptr;
public:
    ID2D1RenderTarget* target=nullptr;
    Palette colors;
    Painter(ID2D1RenderTarget*,IDWriteFactory*,Palette);
    ~Painter();
    void Fill(float x,float y,float w,float h,unsigned color,float radius=0,float alpha=1);
    void Border(float x,float y,float w,float h,unsigned color,float radius=0,float thickness=1);
    void Line(float x1,float y1,float x2,float y2,unsigned color,float thickness=1,float alpha=1);
    void Circle(float x,float y,float r,unsigned color,bool fill=true,float thickness=1,float alpha=1);
    void Text(const std::wstring&,float x,float y,float w,float h,float size,unsigned color,bool bold=false,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING);
    void MouseIcon(float x,float y,float scale,unsigned color);
    void Check(float x,float y,unsigned color);
    void Close(float x,float y,unsigned color);
    void Chevron(float x,float y,unsigned color);
    void Keycaps(const Binding&,float x,float y,float maximumWidth=240);
};
class Graphics {
public:
    ID2D1Factory* factory=nullptr;
    IDWriteFactory* write=nullptr;
    ID2D1HwndRenderTarget* mainTarget=nullptr;
    ID2D1HwndRenderTarget* quickTarget=nullptr;
    ~Graphics();
    bool Init();
    bool Ensure(HWND,ID2D1HwndRenderTarget*&);
    void Paint(HWND,bool quick,const std::function<void(Painter&)>&);
    bool SaveImage(const std::filesystem::path&,UINT width,UINT height,const std::function<void(Painter&)>&);
};
HICON MakeAppIcon(int size);
}
