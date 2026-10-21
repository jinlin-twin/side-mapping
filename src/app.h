#pragma once
#include "input.h"
#include "paint.h"
#include <shellapi.h>

namespace side {
constexpr UINT WM_TRAY=WM_APP+40,WM_SHOW_MAIN=WM_APP+41;
class App {
    struct Hit{D2D1_RECT_F rect;int action;};
    enum class Modal{None,Edit,Settings,Rename};
    enum class Severity{Ok,Info,Warning,Error};
    enum class NamePurpose{Device,NewScheme,Scheme};
    NamePurpose namePurpose=NamePurpose::Device;
    HWND window=nullptr,quickWindow=nullptr,nameEdit=nullptr;
    HICON icon=nullptr;
    HFONT editFont=nullptr;
    HBRUSH editBrush=nullptr;
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
    bool smokeMode=false,quickAutomatic=false;
    int smokeResult=0;
    Modal modal=Modal::None;
    Binding draft{};
    std::wstring notice;
    UINT taskbarCreated=0;
    std::array<Binding,8> presets={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'},Binding{ModCtrl,'Z'},Binding{ModCtrl,'S'},Binding{ModAlt,VK_TAB},Binding{ModWin,'D'},Binding{ModCtrl,'W'},Binding{ModWin|ModShift,'S'}};
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
    void PlaceQuick(HMONITOR monitor,bool show);
    void OpenEdit(int index);
    void OpenNameDialog(NamePurpose);
    void SchemeMenu();
    void SwitchScheme(int profileIndex,size_t schemeIndex);
    void CloseModal();
    void Activate(int action);
    int HitTest(float x,float y) const;
    void AddHit(float x,float y,float w,float h,int action);
    void DrawMain(Painter&);
    void DrawQuick(Painter&);
    void DrawModal(Painter&);
    void DrawEmpty(Painter&);
    void DrawDeviceCard(Painter&);
    void DrawBindings(Painter&);
    void DrawFocus(Painter&);
    void ModalFrame(Painter&,float y,float height,const std::wstring& title,const std::wstring& subtitle);
    void Button(Painter&,const std::wstring&,float x,float y,float w,float h,int action,bool primary=false);
    bool ProfileConnected(int index) const;
    std::wstring RoutingStatus() const;
    Severity RoutingSeverity() const;
    POINT MenuAnchor(int action) const;
    RECT NameEditRect(UINT dpi) const;
    void UpdateEditFont(UINT dpi);
    void ApplyWindowTheme();
    void ResizeForDpi(UINT dpi,const RECT* suggested=nullptr);
public:
    App(std::filesystem::path exe,bool bg=false):executable(std::move(exe)),background(bg){}
    int Run(HINSTANCE,const std::filesystem::path& smokeFolder={});
    bool RenderPreviews(const std::filesystem::path& folder);
};
}
