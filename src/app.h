#pragma once
#include "input.h"
#include "paint.h"
#include <shellapi.h>

namespace side {
constexpr UINT WM_TRAY=WM_APP+40,WM_SHOW_MAIN=WM_APP+41;
class App {
    struct Hit{D2D1_RECT_F rect;int action;};
    enum class Modal{None,Edit,Settings,Rename};
    HWND window=nullptr,quickWindow=nullptr,nameEdit=nullptr;
    HICON icon=nullptr;
    Graphics graphics;
    InputEngine input;
    Config config;
    std::vector<Device> devices;
    std::vector<Hit> hits;
    std::filesystem::path executable,configPath;
    int selected=-1,active=-1,editingButton=0,highlight=-1,keyboardFocus=-1,hoverAction=0;
    unsigned generation=0;
    bool canWrite=true,engineReady=false,quickVisible=false,escapeHotkey=false,previewMode=false,recording=false,hotkeyReady=false,rawReady=false;
    bool background=false,exiting=false,trayReady=false,selectionConfirmed=false;
    bool smokeMode=false;
    int smokeResult=0;
    Modal modal=Modal::None;
    Binding draft{};
    std::wstring notice;
    UINT taskbarCreated=0;
    std::array<Binding,6> presets={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'},Binding{ModCtrl,'Z'},Binding{ModAlt,VK_TAB},Binding{ModCtrl,'S'},Binding{ModWin,'D'}};
    static LRESULT CALLBACK MainProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK QuickProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT Handle(UINT,WPARAM,LPARAM);
    void RefreshDevices(bool initial=false);
    void UpdateMapping();
    bool Save();
    void Notify(const std::wstring&,bool balloon=false);
    void AddTray();
    void TrayMenu();
    void ShowMain();
    void ShowQuick(bool automatic=false);
    void HideQuick();
    void OpenEdit(int index);
    void CloseModal();
    void Activate(int action);
    int HitTest(float x,float y) const;
    void AddHit(float x,float y,float w,float h,int action);
    void DrawMain(Painter&);
    void DrawQuick(Painter&);
    void DrawModal(Painter&);
    void Button(Painter&,const std::wstring&,float x,float y,float w,float h,int action,bool primary=false);
    bool SelectedConnected() const;
    std::wstring RoutingStatus() const;
    void ResizeForDpi(UINT dpi,const RECT* suggested=nullptr);
public:
    App(std::filesystem::path exe,bool bg=false):executable(std::move(exe)),background(bg){}
    int Run(HINSTANCE,const std::filesystem::path& smokeFolder={});
    bool RenderPreviews(const std::filesystem::path& folder);
};
}
