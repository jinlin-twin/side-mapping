#include "app.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <sstream>
#include <fstream>

namespace side {
constexpr int Enable=1,Settings=2,Quick=3,EditFirst=4,EditSecond=5,Count=6,Rename=7,ChooseDevice=8;
constexpr int CloseModalAction=20,Record=21,SaveBinding=22,Restore=23,Startup=24,ConnectNotice=25,OpenFolder=26,SaveName=27;
constexpr int ProfileBase=100,PresetBase=1000;
constexpr UINT RefreshTimer=1,HighlightTimer=2,NoticeTimer=3,QuickTimer=4;
static constexpr wchar_t MainClass[]=L"SideMapping.Native.Main";
static constexpr wchar_t QuickClass[]=L"SideMapping.Native.Quick";
static constexpr float W=700,H=582;
void App::AddHit(float x,float y,float w,float h,int action){hits.push_back({D2D1::RectF(x,y,x+w,y+h),action});}
int App::HitTest(float x,float y) const {for(auto it=hits.rbegin();it!=hits.rend();++it)if(x>=it->rect.left&&x<=it->rect.right&&y>=it->rect.top&&y<=it->rect.bottom)return it->action;return 0;}
void App::Button(Painter& p,const std::wstring& text,float x,float y,float w,float h,int action,bool primary) {
    auto& c=p.colors;bool hovered=hoverAction==action;
    p.Fill(x,y,w,h,primary?c.green:(hovered?c.soft:c.bg),8);p.Border(x+.5f,y+.5f,w-1,h-1,primary?c.green:c.line,8);
    p.Text(text,x+8,y,w-16,h,12,primary?c.on:c.ink,false,DWRITE_TEXT_ALIGNMENT_CENTER);AddHit(x,y,w,h,action);
}
bool App::SelectedConnected() const {
    if(selected<0||selected>=int(config.profiles.size()))return false;
    return std::any_of(devices.begin(),devices.end(),[&](const Device& device){return FindProfile(config,device)==selected;});
}
std::wstring App::RoutingStatus() const {
    if(!canWrite)return L"配置读取失败，映射已暂停；原文件未被覆盖";
    if(!engineReady&&!previewMode)return L"输入监听不可用，映射已暂停；请重新启动";
    if(!rawReady&&!previewMode)return L"设备监听不可用，映射已暂停；请重新启动";
    if(!config.enabled)return L"映射已暂停，鼠标保持原有功能";
    if(devices.empty()&&!previewMode)return L"连接鼠标后，自动载入它的绑定";
    if(devices.size()>1&&!selectionConfirmed)return L"按一下鼠标侧键，确认当前使用的鼠标";
    if(active<0)return L"尚未识别当前鼠标，保留鼠标原功能";
    if(selected!=active)return L"正在编辑未使用的鼠标；当前生效："+config.profiles[active].name;
    return L"换一只鼠标，自动切换它的绑定";
}
void App::DrawMain(Painter& p) {
    hits.clear();auto& c=p.colors;
    p.Fill(0,0,W,H,c.bg);
    p.Fill(24,20,32,32,c.green,10);p.MouseIcon(34,27,.85f,c.on);
    p.Text(L"侧映",67,17,130,26,19,c.ink,true);p.Text(L"鼠标快捷键",68,43,150,16,10,c.muted);
    p.Text(config.enabled?L"映射已开启":L"映射已暂停",472,22,90,30,12,c.ink);
    p.Fill(567,29,33,19,config.enabled?c.green:c.edge,10);p.Circle(config.enabled?590:577,38.5f,6.4f,c.on);AddHit(465,20,141,34,Enable);
    p.Line(637,29,650,29,c.muted,1.3f);p.Line(637,37,650,37,c.muted,1.3f);p.Line(637,45,650,45,c.muted,1.3f);p.Circle(641,29,2.5f,c.bg);p.Circle(646,37,2.5f,c.bg);p.Circle(641,45,2.5f,c.bg);AddHit(620,18,45,40,Settings);
    p.Line(0,72,W,72,c.line);
    if(config.profiles.empty()) {
        p.Text(L"连接一只鼠标，开始设置",36,110,628,38,21,c.ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        p.Circle(350,276,93,c.soft);p.MouseIcon(327,233,4,c.edge);
        p.Text(L"设备识别后会自动出现在这里",70,397,560,26,14,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        p.Text(L"首次接入保留原功能，不会自动改键",70,429,560,22,12,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    } else {
        if(selected<0||selected>=int(config.profiles.size()))selected=0;
        if(config.profiles.size()<=3) {
            float width=644.f/float(config.profiles.size());p.Fill(28,88,644,48,c.soft,10);
            for(size_t i=0;i<config.profiles.size();++i){float x=32+float(i)*width;bool chosen=selected==int(i);if(chosen){p.Fill(x,92,width-8,40,c.bg,8);p.Border(x+.5f,92.5f,width-9,39,c.line,8);}p.Text(config.profiles[i].name,x+12,92,width-32,40,13,chosen?c.ink:c.muted,chosen);AddHit(x,92,width-8,40,ProfileBase+int(i));}
        } else Button(p,config.profiles[selected].name+L"  ▾",28,88,644,48,ChooseDevice);
        const auto& profile=config.profiles[selected];
        p.Text(profile.name,28,153,235,35,20,c.ink,true);Button(p,L"改名",267,157,46,27,Rename);
        Button(p,std::to_wstring(profile.sideCount)+L" 个侧键 ▾",395,157,105,28,Count);
        std::wstring status=selected==active&&config.enabled&&canWrite&&selectionConfirmed&&engineReady&&rawReady?L"当前使用":SelectedConnected()?L"已连接":L"未连接";
        if(!config.enabled&&selected==active)status=L"映射暂停";
        p.Fill(562,159,110,25,status==L"当前使用"?c.tint:c.soft,13);p.Circle(575,171.5f,2.4f,status==L"当前使用"?c.green:c.muted);p.Text(status,584,159,79,25,11,status==L"当前使用"?c.green:c.muted);
        p.Circle(189,320,116,c.line,false,.7f);p.Circle(189,320,102,c.soft,false,1);
        auto transform=D2D1::Matrix3x2F::Rotation(-13,D2D1::Point2F(189,320));
        p.target->SetTransform(transform);
        p.Fill(138,222,127,211,c.ink,58,.04f);p.Fill(125,211,128,211,c.shell,58);p.Border(125.5f,211.5f,127,210,c.edge,58);
        p.Line(189,212,189,290,c.edge,.9f);p.Line(135,292,242,292,c.edge,.8f);
        p.Fill(183,237,12,31,c.muted,6);for(int i=0;i<6;++i)p.Line(186,242+3.5f*i,192,242+3.5f*i,c.shell,.8f);
        p.Border(184,355,10,14,c.edge,5,.8f);
        for(int i=0;i<profile.sideCount;++i){int button=PhysicalButton(profile,i);float y=291+44.f*i;bool pulse=highlight==button;unsigned color=config.enabled?c.green:c.muted;p.Fill(116,y,20,37,color,6);if(pulse)p.Border(113,y-3,26,43,color,8,2);p.Text(std::to_wstring(i+1),116,y,20,37,12,c.on,true,DWRITE_TEXT_ALIGNMENT_CENTER);auto point=transform.TransformPoint(D2D1::Point2F(126,y+18.5f));AddHit(point.x-17,point.y-23,34,46,button==0?EditFirst:EditSecond);}
        p.target->SetTransform(D2D1::Matrix3x2F::Identity());
        p.Text(L"点一下侧键，设置快捷键",57,454,265,24,11,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        for(int i=0;i<profile.sideCount;++i) {
            int button=PhysicalButton(profile,i);
            float x=348,y=profile.sideCount==1?281:238+100.f*i;bool pulse=highlight==button;
            p.Fill(x,y,324,89,hoverAction==(button==0?EditFirst:EditSecond)?c.soft:c.bg,12);p.Border(x+.5f,y+.5f,323,88,pulse?c.green:c.line,12,pulse?1.5f:1);
            p.Fill(x+16,y+29,28,28,c.tint,8);p.Text(std::to_wstring(i+1),x+16,y+29,28,28,12,c.green,false,DWRITE_TEXT_ALIGNMENT_CENTER);
            p.Text(L"侧键 "+std::to_wstring(i+1),x+58,y+13,115,23,11,c.muted);
            p.Text(ActionName(profile.bindings[button]),x+159,y+13,131,23,12,c.ink,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
            p.Keycaps(profile.bindings[button],x+58,y+44,228);p.Chevron(x+302,y+40,c.muted);AddHit(x,y,324,89,button==0?EditFirst:EditSecond);
        }
        p.Text(L"在本窗口按侧键，识别设备与按键",348,454,324,24,11,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    p.Line(29,501,34,496,c.green,1.2f);p.Line(34,496,34,501,c.green,1.2f);p.Line(34,496,39,496,c.green,1.2f);
    p.Line(40,502,35,507,c.green,1.2f);p.Line(35,507,35,502,c.green,1.2f);p.Line(35,507,30,507,c.green,1.2f);
    p.Text(RoutingStatus(),48,487,624,29,11,c.muted);
    p.Fill(0,528,W,54,c.soft);p.Line(0,528,W,528,c.line);
    p.Check(27,551,c.muted);p.Text(notice.empty()?L"绑定保存在程序旁的 config.json":notice,45,538,407,34,11,c.muted);
    Button(p,L"快捷键速览    Ctrl + Alt + M",462,540,210,31,Quick);
    if(keyboardFocus>=0&&keyboardFocus<int(hits.size())&&modal==Modal::None){auto rect=hits[keyboardFocus].rect;p.Border(rect.left-2,rect.top-2,rect.right-rect.left+4,rect.bottom-rect.top+4,c.green,8,1.5f);}
    if(modal!=Modal::None)DrawModal(p);
}
void App::DrawModal(Painter& p) {
    auto& c=p.colors;p.Fill(0,0,W,H,c.ink,0,.15f);hits.clear();
    const float x=170,y=modal==Modal::Edit?103:135,width=360,height=modal==Modal::Edit?386:modal==Modal::Settings?300:260;
    p.Fill(x+3,y+9,width,height,c.ink,15,.08f);p.Fill(x,y,width,height,c.bg,15);p.Border(x+.5f,y+.5f,width-1,height-1,c.line,15);
    std::wstring title=modal==Modal::Edit?L"设置侧键 "+std::to_wstring(selected>=0&&config.profiles[selected].sideCount==1?1:editingButton+1):modal==Modal::Settings?L"设置":L"给鼠标起个名字";
    p.Text(title,x+22,y+17,260,31,16,c.ink,true);p.Close(x+326,y+28,c.muted);AddHit(x+311,y+13,36,36,CloseModalAction);
    if(modal==Modal::Edit) {
        p.Text(L"绑定到快捷键",x+22,y+66,300,24,11,c.muted);
        p.Fill(x+22,y+96,316,64,recording?c.tint:c.soft,10);p.Border(x+22.5f,y+96.5f,315,63,recording?c.green:c.edge,10);
        if(recording)p.Text(L"请按下快捷键…",x+32,y+98,296,60,16,c.green,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        else if(!draft.key)p.Text(L"点击这里，按下快捷键",x+32,y+98,296,60,14,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        else p.Text(ShortcutText(draft),x+32,y+98,296,60,17,c.ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit(x+22,y+96,316,64,Record);
        p.Text(recording?L"按 Esc 取消录制；系统保留组合无法录入":L"点击上方录制，也可以选一个常用操作",x+22,y+166,316,25,10,c.muted);
        for(int i=0;i<6;++i){float px=x+22+(i%2)*162.f,py=y+204+(i/2)*39.f;bool chosen=draft==presets[i];p.Fill(px,py,154,32,chosen?c.tint:c.bg,7);p.Border(px+.5f,py+.5f,153,31,chosen?c.green:c.line,7);p.Text(ActionName(presets[i]),px+10,py,66,32,11,c.ink);p.Text(ShortcutText(presets[i]),px+73,py,72,32,10,c.muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);AddHit(px,py,154,32,PresetBase+i);}
        p.Text(L"恢复原功能",x+22,y+337,130,30,11,c.muted);AddHit(x+22,y+333,130,36,Restore);
        Button(p,L"保存绑定",x+230,y+334,108,34,SaveBinding,true);
    }else if(modal==Modal::Settings) {
        auto checkbox=[&](const std::wstring& text,float top,bool checked,int action){p.Text(text,x+22,top,270,43,13,c.ink);p.Fill(x+312,top+13,17,17,checked?c.green:c.bg,4);if(checked)p.Check(x+316,top+17,c.on);else p.Border(x+312.5f,top+13.5f,16,16,c.edge,4);AddHit(x+22,top,316,43,action);};
        p.Line(x+22,y+64,x+338,y+64,c.line);checkbox(L"开机时启动",y+70,previewMode?false:IsStartupEnabled(),Startup);
        p.Line(x+22,y+117,x+338,y+117,c.line);checkbox(L"接入鼠标后显示绑定提示",y+123,config.showOnConnect,ConnectNotice);
        p.Line(x+22,y+170,x+338,y+170,c.line);p.Text(L"快捷键速览",x+22,y+184,127,30,12,c.ink);p.Text(hotkeyReady||previewMode?L"Ctrl + Alt + M":L"快捷键被占用，可从托盘打开",x+143,y+184,195,30,11,c.muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
        Button(p,L"打开配置文件夹",x+22,y+235,151,34,OpenFolder);p.Text(L"侧映 0.1.0",x+225,y+235,113,34,11,c.muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }else {
        p.Text(L"设备名称",x+22,y+66,316,24,11,c.muted);
        p.Text(L"名称只用于方便区分，不改变设备身份",x+22,y+140,316,24,11,c.muted);
        Button(p,L"保存名称",x+230,y+202,108,34,SaveName,true);
    }
}
void App::DrawQuick(Painter& p) {
    auto& c=p.colors;p.Fill(0,0,328,232,c.bg);
    p.MouseIcon(21,22,.95f,c.green);p.Text(L"快捷键速览",46,16,220,27,15,c.ink,true);
    std::wstring name=active>=0?config.profiles[active].name:L"未连接鼠标";p.Text(name,46,43,240,20,11,c.muted);
    p.Close(301,28,c.muted);
    if(active>=0) {
        const auto& profile=config.profiles[active];
        bool effective=config.enabled&&selectionConfirmed&&canWrite&&engineReady&&rawReady;
        for(int i=0;i<profile.sideCount;++i){int button=PhysicalButton(profile,i);float y=80+i*62.f;p.Line(21,y,307,y,c.line);p.Text(L"侧键 "+std::to_wstring(i+1),21,y+7,100,19,10,c.muted);p.Text(effective?ActionName(profile.bindings[button]):L"原始功能",21,y+27,100,25,12,c.ink,true);if(effective)p.Keycaps(profile.bindings[button],133,y+21,174);else p.Text(L"映射已暂停",133,y+20,174,28,11,c.muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);}
    } else p.Text(L"连接鼠标后显示它的绑定",21,87,286,73,13,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    p.Text(escapeHotkey||previewMode?L"Esc 收起 · 也可再次按 Ctrl + Alt + M":L"再次按 Ctrl + Alt + M 收起",21,207,286,20,10,c.muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
}
bool App::Save() {
    if(!canWrite){Notify(L"原配置无法读取，本次更改未保存");return false;}
    std::wstring error;if(!SaveConfig(configPath,config,error)){config.enabled=false;UpdateMapping();Notify(error,true);return false;}return true;
}
void App::Notify(const std::wstring& message,bool balloon) {
    notice=message;InvalidateRect(window,nullptr,FALSE);SetTimer(window,NoticeTimer,6000,nullptr);
    if(balloon&&!previewMode){NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;data.uFlags=NIF_INFO;wcsncpy_s(data.szInfo,message.c_str(),_TRUNCATE);wcscpy_s(data.szInfoTitle,L"侧映");data.dwInfoFlags=NIIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&data);}
}
void App::UpdateMapping() {
    MappingState next;next.enabled=engineReady&&rawReady&&canWrite&&config.enabled&&selectionConfirmed&&active>=0;next.generation=++generation;
    if(active>=0){next.bindings=EffectiveBindings(config.profiles[active]);next.sideCount=2;}
    input.Update(next);InvalidateRect(window,nullptr,FALSE);if(quickVisible)InvalidateRect(quickWindow,nullptr,FALSE);
}
void App::RefreshDevices(bool initial) {
    const int oldActive=active;const auto oldDevices=devices;devices=EnumerateMice();
    devices.erase(std::remove_if(devices.begin(),devices.end(),[](const Device& d){return !IsMouseCandidate(d);}),devices.end());
    bool changed=false;int newlyConnected=-1;int newCount=0;
    for(const auto& device:devices) {
        int existing=FindProfile(config,device);
        if(existing<0&&config.profiles.size()<100) {
            Profile p;p.id=device.id;p.hardwareId=device.hardwareId;p.name=device.name==L"鼠标"?L"鼠标 "+std::to_wstring(config.profiles.size()+1):device.name;
            if(p.name.size()>60)p.name.resize(60);
            config.profiles.push_back(std::move(p));changed=true;
        }else if(existing>=0&&config.profiles[existing].id!=device.id) {
            // A unique model fallback keeps the same profile when the USB port changes.
            if(config.preferredProfile==config.profiles[existing].id)config.preferredProfile=device.id;
            config.profiles[existing].id=device.id;changed=true;
        }
        if(!initial&&std::none_of(oldDevices.begin(),oldDevices.end(),[&](const Device& old){return old.id==device.id;})){newlyConnected=FindProfile(config,device);++newCount;}
    }
    active=devices.size()==1?FindProfile(config,devices.front()):-1;
    if(devices.size()>1&&newCount==1)active=newlyConnected;
    else if(devices.size()>1)for(const auto& device:devices)if(device.id==config.preferredProfile)active=FindProfile(config,device);
    selectionConfirmed=active>=0;
    if(active>=0&&config.preferredProfile!=config.profiles[active].id){config.preferredProfile=config.profiles[active].id;changed=true;}
    if(active>=0&&(initial||oldActive!=active))selected=active;
    if(selected<0&&!config.profiles.empty())selected=0;
    if(changed&&canWrite)Save();UpdateMapping();
    if(!initial&&oldActive!=active&&active>=0&&config.showOnConnect)ShowQuick(true);
}
void App::AddTray() {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=WM_TRAY;data.hIcon=icon;wcscpy_s(data.szTip,L"侧映 · 鼠标快捷键");
    trayReady=Shell_NotifyIconW(NIM_ADD,&data)!=FALSE;data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);
}
void App::ShowMain(){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);SetFocus(window);InvalidateRect(window,nullptr,FALSE);}
void App::TrayMenu() {
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"打开侧映");AppendMenuW(menu,MF_STRING,2,L"快捷键速览\tCtrl+Alt+M");AppendMenuW(menu,MF_STRING,3,config.enabled?L"暂停映射":L"恢复映射");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,4,L"打开配置文件夹");AppendMenuW(menu,MF_STRING,5,L"退出侧映");
    POINT point{};GetCursorPos(&point);SetForegroundWindow(window);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);DestroyMenu(menu);PostMessageW(window,WM_NULL,0,0);
    if(result==1)ShowMain();else if(result==2)ShowQuick();else if(result==3)Activate(Enable);else if(result==4)Activate(OpenFolder);else if(result==5){exiting=true;DestroyWindow(window);}
}
void App::ShowQuick(bool automatic) {
    if(!quickWindow)return;
    if(quickVisible&&!automatic){HideQuick();return;}
    if(!quickVisible){escapeHotkey=RegisterHotKey(window,2,MOD_NOREPEAT,VK_ESCAPE)!=FALSE;quickVisible=true;}
    POINT cursor{};GetCursorPos(&cursor);HMONITOR monitor=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);
    UINT dpi=GetDpiForWindow(window);int width=MulDiv(328,dpi,96),height=MulDiv(232,dpi,96),margin=MulDiv(22,dpi,96);
    SetWindowPos(quickWindow,HWND_TOPMOST,info.rcWork.right-width-margin,info.rcWork.bottom-height-margin,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(quickWindow,nullptr,FALSE);if(automatic)SetTimer(window,QuickTimer,4500,nullptr);else KillTimer(window,QuickTimer);
}
void App::HideQuick(){if(!quickVisible)return;ShowWindow(quickWindow,SW_HIDE);quickVisible=false;if(escapeHotkey)UnregisterHotKey(window,2);escapeHotkey=false;KillTimer(window,QuickTimer);}
void App::OpenEdit(int index) {
    if(selected<0||index<0||index>1||(config.profiles[selected].sideCount==1&&index!=config.profiles[selected].singleSideIndex))return;
    HideQuick();editingButton=index;draft=config.profiles[selected].bindings[index];modal=Modal::Edit;recording=false;keyboardFocus=-1;InvalidateRect(window,nullptr,FALSE);
}
void App::CloseModal(){input.CancelRecording();recording=false;modal=Modal::None;if(nameEdit){DestroyWindow(nameEdit);nameEdit=nullptr;}keyboardFocus=-1;InvalidateRect(window,nullptr,FALSE);SetFocus(window);}
void App::Activate(int action) {
    if(action==Enable){config.enabled=!config.enabled;Save();UpdateMapping();Notify(config.enabled?L"映射已恢复":L"已暂停，原始功能生效");}
    else if(action==Settings){HideQuick();modal=Modal::Settings;InvalidateRect(window,nullptr,FALSE);}
    else if(action==Quick)ShowQuick();
    else if(action==EditFirst||action==EditSecond)OpenEdit(action==EditFirst?0:1);
    else if(action>=ProfileBase&&action<ProfileBase+100){selected=action-ProfileBase;highlight=-1;InvalidateRect(window,nullptr,FALSE);}
    else if(action==ChooseDevice){HMENU menu=CreatePopupMenu();for(size_t i=0;i<config.profiles.size();++i)AppendMenuW(menu,MF_STRING|(int(i)==selected?MF_CHECKED:0),UINT_PTR(ProfileBase+i),config.profiles[i].name.c_str());POINT pt{28,136};float scale=GetDpiForWindow(window)/96.f;pt.x=int(pt.x*scale);pt.y=int(pt.y*scale);ClientToScreen(window,&pt);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(result)Activate(result);}
    else if(action==Count&&selected>=0){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|(config.profiles[selected].sideCount==1?MF_CHECKED:0),1,L"1 个侧键");AppendMenuW(menu,MF_STRING|(config.profiles[selected].sideCount==2?MF_CHECKED:0),2,L"2 个侧键");POINT pt{};GetCursorPos(&pt);int count=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(count){config.profiles[selected].sideCount=count;Save();UpdateMapping();}}
    else if(action==CloseModalAction)CloseModal();
    else if(action==Record){recording=true;input.BeginRecording();InvalidateRect(window,nullptr,FALSE);}
    else if(action==Restore){input.CancelRecording();recording=false;draft={};InvalidateRect(window,nullptr,FALSE);}
    else if(action>=PresetBase&&action<PresetBase+6){input.CancelRecording();recording=false;draft=presets[action-PresetBase];InvalidateRect(window,nullptr,FALSE);}
    else if(action==SaveBinding&&modal==Modal::Edit&&!recording){if(!ValidBinding(draft))return;config.profiles[selected].bindings[editingButton]=draft;if(Save()){CloseModal();UpdateMapping();Notify(L"已保存 · "+config.profiles[selected].name+L" / 侧键 "+std::to_wstring(config.profiles[selected].sideCount==1?1:editingButton+1));}}
    else if(action==Startup){std::wstring error;if(!SetStartupEnabled(!IsStartupEnabled(),executable.wstring(),error))Notify(error,true);InvalidateRect(window,nullptr,FALSE);}
    else if(action==ConnectNotice){config.showOnConnect=!config.showOnConnect;Save();InvalidateRect(window,nullptr,FALSE);}
    else if(action==OpenFolder)ShellExecuteW(window,L"open",executable.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    else if(action==Rename&&selected>=0){modal=Modal::Rename;float scale=GetDpiForWindow(window)/96.f;nameEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",config.profiles[selected].name.c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,int(192*scale),int(233*scale),int(316*scale),int(35*scale),window,nullptr,GetModuleHandleW(nullptr),nullptr);SendMessageW(nameEdit,EM_SETLIMITTEXT,40,0);SendMessageW(nameEdit,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);SendMessageW(nameEdit,EM_SETSEL,0,-1);SetFocus(nameEdit);InvalidateRect(window,nullptr,FALSE);}
    else if(action==SaveName&&nameEdit){wchar_t name[81]{};GetWindowTextW(nameEdit,name,81);std::wstring value=name;auto first=value.find_first_not_of(L" \t\r\n"),last=value.find_last_not_of(L" \t\r\n");if(first==std::wstring::npos){Notify(L"设备名称不能为空");return;}config.profiles[selected].name=value.substr(first,last-first+1);if(Save()){CloseModal();Notify(L"设备名称已保存");}}
}
void App::ResizeForDpi(UINT dpi,const RECT* suggested) {
    RECT rect{0,0,MulDiv(int(W),dpi,96),MulDiv(int(H),dpi,96)};
    AdjustWindowRectExForDpi(&rect,DWORD(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE)),dpi);
    SetWindowPos(window,nullptr,suggested?suggested->left:0,suggested?suggested->top:0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOZORDER|(suggested?0:SWP_NOMOVE));
    if(graphics.mainTarget)graphics.mainTarget->SetDpi(float(dpi),float(dpi));
    if(graphics.quickTarget)graphics.quickTarget->SetDpi(float(dpi),float(dpi));
    if(nameEdit){float scale=dpi/96.f;SetWindowPos(nameEdit,nullptr,int(192*scale),int(233*scale),int(316*scale),int(35*scale),SWP_NOZORDER);}
}
LRESULT CALLBACK App::MainProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);app->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    return app?app->Handle(message,wp,lp):DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK App::QuickProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message){
    case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(hwnd,&ps);app->graphics.Paint(hwnd,true,[&](Painter& p){app->DrawQuick(p);});EndPaint(hwnd,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_LBUTTONUP:{float scale=GetDpiForWindow(hwnd)/96.f;if(GET_X_LPARAM(lp)/scale>280&&GET_Y_LPARAM(lp)/scale<60)app->HideQuick();return 0;}
    case WM_SIZE:if(app->graphics.quickTarget)app->graphics.quickTarget->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));return 0;
    case WM_DPICHANGED:{UINT dpi=HIWORD(wp);if(app->graphics.quickTarget)app->graphics.quickTarget->SetDpi(float(dpi),float(dpi));auto* rect=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,HWND_TOPMOST,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOACTIVATE);return 0;}
    case WM_CLOSE:app->HideQuick();return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT App::Handle(UINT message,WPARAM wp,LPARAM lp) {
    if(message==taskbarCreated&&taskbarCreated){AddTray();return 0;}
    switch(message) {
    case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(window,&ps);graphics.Paint(window,false,[&](Painter& p){DrawMain(p);});EndPaint(window,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:
        if(wp==SIZE_MINIMIZED){if(trayReady)ShowWindow(window,SW_HIDE);return 0;}
        if(graphics.mainTarget)graphics.mainTarget->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));return 0;
    case WM_DPICHANGED:ResizeForDpi(HIWORD(wp),reinterpret_cast<RECT*>(lp));return 0;
    case WM_SETTINGCHANGE:Release(graphics.mainTarget);Release(graphics.quickTarget);InvalidateRect(window,nullptr,FALSE);InvalidateRect(quickWindow,nullptr,FALSE);return 0;
    case WM_DISPLAYCHANGE:HideQuick();return 0;
    case WM_SETCURSOR:if(LOWORD(lp)==HTCLIENT&&hoverAction){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_MOUSEMOVE:{float scale=GetDpiForWindow(window)/96.f;int next=HitTest(GET_X_LPARAM(lp)/scale,GET_Y_LPARAM(lp)/scale);if(next!=hoverAction){hoverAction=next;InvalidateRect(window,nullptr,FALSE);}TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);return 0;}
    case WM_MOUSELEAVE:hoverAction=0;InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_LBUTTONUP:{float scale=GetDpiForWindow(window)/96.f;int action=HitTest(GET_X_LPARAM(lp)/scale,GET_Y_LPARAM(lp)/scale);if(action)Activate(action);return 0;}
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){if(modal!=Modal::None){if(recording){recording=false;input.CancelRecording();InvalidateRect(window,nullptr,FALSE);}else CloseModal();}else if(quickVisible)HideQuick();return 0;}
        if(wp==VK_TAB&&!recording&&!hits.empty()){int step=(GetKeyState(VK_SHIFT)&0x8000)?-1:1;keyboardFocus=(keyboardFocus+step+int(hits.size()))%int(hits.size());InvalidateRect(window,nullptr,FALSE);return 0;}
        if((wp==VK_RETURN||wp==VK_SPACE)&&!recording&&keyboardFocus>=0&&keyboardFocus<int(hits.size())){Activate(hits[keyboardFocus].action);return 0;}
        break;
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE&&recording){input.CancelRecording();recording=false;InvalidateRect(window,nullptr,FALSE);}return 0;
    case WM_CAPTURED:if(modal==Modal::Edit){Binding candidate{unsigned(lp),unsigned(wp)};if(ValidBinding(candidate)){draft=candidate;recording=false;InvalidateRect(window,nullptr,FALSE);}else{recording=false;Notify(L"这个按键不能作为快捷键，请重新录制");}}return 0;
    case WM_CAPTURE_CANCEL:recording=false;InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_INPUT_ERROR:Notify(L"快捷键发送失败：若目标程序以管理员运行，请以相同权限启动侧映",true);return 0;
    case WM_INPUT_DEVICE_CHANGE:{MappingState stopped;stopped.generation=++generation;input.Update(stopped);SetTimer(window,RefreshTimer,180,nullptr);return 0;}
    case WM_INPUT:{
        UINT bytes=0;GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,nullptr,&bytes,sizeof(RAWINPUTHEADER));
        if(bytes&&bytes<65536){std::vector<BYTE> buffer(bytes);if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,buffer.data(),&bytes,sizeof(RAWINPUTHEADER))==bytes){auto* raw=reinterpret_cast<RAWINPUT*>(buffer.data());if(raw->header.dwType==RIM_TYPEMOUSE){const auto& mouse=raw->data.mouse;for(const auto& device:devices)if(device.handle==raw->header.hDevice){
            int index=FindProfile(config,device);bool sideDown=(mouse.usButtonFlags&(RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN))!=0;
            if(index>=0&&sideDown&&GetForegroundWindow()==window&&modal==Modal::None){bool changed=active!=index||!selectionConfirmed;int button=(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)?0:1;auto& profile=config.profiles[index];if(profile.singleSideIndex!=button){if(profile.sideCount==1){profile.bindings[button]=profile.bindings[profile.singleSideIndex];profile.bindings[profile.singleSideIndex]={};}profile.singleSideIndex=button;changed=true;}active=selected=index;selectionConfirmed=true;config.preferredProfile=profile.id;if(changed){Save();UpdateMapping();Notify(L"已识别当前鼠标："+profile.name);}}
            else if(index>=0&&active>=0&&index!=active&&(mouse.lLastX||mouse.lLastY||sideDown)&&GetForegroundWindow()!=window){active=-1;selectionConfirmed=false;UpdateMapping();Notify(L"检测到另一鼠标输入，映射暂停；打开侧映并按侧键确认",true);}
            if(index==selected&&sideDown){highlight=(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)?0:1;SetTimer(window,HighlightTimer,650,nullptr);InvalidateRect(window,nullptr,FALSE);}break;
        }}}}
        return DefWindowProcW(window,message,wp,lp);
    }
    case WM_POWERBROADCAST:if(wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND)SetTimer(window,RefreshTimer,900,nullptr);return TRUE;
    case WM_TIMER:
        if(wp==500&&smokeMode){
            KillTimer(window,500);bool ok=engineReady&&rawReady&&IsWindow(window)&&IsWindow(quickWindow);
            ok=ok&&((GetWindowLongPtrW(quickWindow,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0);
            config.enabled=false;
            if(!config.profiles.empty()){
                selected=0;OpenEdit(0);Activate(PresetBase);Activate(SaveBinding);
                Config verification;std::wstring error;ok=ok&&LoadConfig(configPath,verification,error)&&!verification.enabled&&!verification.profiles.empty()&&verification.profiles[0].bindings[0]==presets[0];
                modal=Modal::Settings;CloseModal();
            }
            smokeResult=ok?0:3;
            std::ofstream report(configPath.parent_path()/L"native-smoke-result.txt");
            report<<(ok?"PASS":"FAIL")<<": native window, input hooks, raw input registration, no-activate quick window, isolated config save, dialog actions, and clean shutdown.\n";
            report<<"hook_ready="<<engineReady<<" raw_ready="<<rawReady<<" tray_ready="<<trayReady<<" candidates="<<devices.size()<<"\n";
            report.close();exiting=true;DestroyWindow(window);return 0;
        }
        if(wp==RefreshTimer){KillTimer(window,RefreshTimer);RefreshDevices();}else if(wp==HighlightTimer){KillTimer(window,HighlightTimer);highlight=-1;InvalidateRect(window,nullptr,FALSE);}else if(wp==NoticeTimer){KillTimer(window,NoticeTimer);notice.clear();InvalidateRect(window,nullptr,FALSE);}else if(wp==QuickTimer)HideQuick();return 0;
    case WM_HOTKEY:if(wp==1)ShowQuick();else if(wp==2)HideQuick();return 0;
    case WM_TRAY:if(LOWORD(lp)==WM_CONTEXTMENU||LOWORD(lp)==WM_RBUTTONUP)TrayMenu();else if(LOWORD(lp)==NIN_SELECT||LOWORD(lp)==NIN_KEYSELECT||LOWORD(lp)==WM_LBUTTONDBLCLK)ShowMain();return 0;
    case WM_SHOW_MAIN:ShowMain();return 0;
    case WM_CLOSE:if(trayReady&&!exiting){CloseModal();ShowWindow(window,SW_HIDE);}else DestroyWindow(window);return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_DESTROY:{HideQuick();UnregisterHotKey(window,1);input.Stop();NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;Shell_NotifyIconW(NIM_DELETE,&data);if(quickWindow){DestroyWindow(quickWindow);quickWindow=nullptr;}PostQuitMessage(0);return 0;}
    }
    return DefWindowProcW(window,message,wp,lp);
}
int App::Run(HINSTANCE instance,const std::filesystem::path& smokeFolder) {
    smokeMode=!smokeFolder.empty();
    if(smokeMode){std::filesystem::create_directories(smokeFolder);configPath=smokeFolder/L"config.json";background=true;}else configPath=executable.parent_path()/L"config.json";
    std::wstring configError;canWrite=LoadConfig(configPath,config,configError);if(!canWrite)config.enabled=false;
    if(!graphics.Init()){MessageBoxW(nullptr,L"无法初始化系统绘图组件。",L"侧映",MB_ICONERROR);return 1;}
    icon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,32,32,LR_DEFAULTCOLOR));if(!icon)icon=MakeAppIcon(32);WNDCLASSEXW klass{};klass.cbSize=sizeof(klass);klass.hInstance=instance;klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);klass.hIcon=icon;klass.hIconSm=icon;klass.lpfnWndProc=MainProc;klass.lpszClassName=MainClass;klass.style=CS_HREDRAW|CS_VREDRAW;RegisterClassExW(&klass);
    klass.lpfnWndProc=QuickProc;klass.lpszClassName=QuickClass;RegisterClassExW(&klass);
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    UINT dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(int(W),dpi,96),MulDiv(int(H),dpi,96)};AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);
    window=CreateWindowExW(0,MainClass,L"侧映 · 鼠标快捷键",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,this);
    if(!window){DestroyIcon(icon);return 1;}
    quickWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,QuickClass,L"侧映 · 快捷键速览",WS_POPUP|WS_BORDER,0,0,328,232,window,nullptr,instance,this);
    DWORD corner=2;DwmSetWindowAttribute(window,33,&corner,sizeof(corner));if(quickWindow)DwmSetWindowAttribute(quickWindow,33,&corner,sizeof(corner));
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");AddTray();
    hotkeyReady=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'M')!=FALSE;
    RAWINPUTDEVICE mouse{0x01,0x02,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window};rawReady=RegisterRawInputDevices(&mouse,1,sizeof(mouse))!=FALSE;
    engineReady=input.Start(window);if(smokeMode)config.enabled=false;RefreshDevices(true);
    if(!canWrite)Notify(configError,true);else if(!engineReady||!rawReady)Notify(RoutingStatus(),true);else if(!hotkeyReady)Notify(L"速览快捷键被占用，可从托盘菜单打开");
    if((!background||!trayReady)&&!smokeMode)ShowMain();
    if(smokeMode)SetTimer(window,500,500,nullptr);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0){
        if(nameEdit&&message.hwnd==nameEdit&&message.message==WM_KEYDOWN){if(message.wParam==VK_RETURN){Activate(SaveName);continue;}if(message.wParam==VK_ESCAPE){CloseModal();continue;}if(message.wParam=='A'&&(GetKeyState(VK_CONTROL)&0x8000)){SendMessageW(nameEdit,EM_SETSEL,0,-1);continue;}}
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    if(icon)DestroyIcon(icon);return smokeMode?smokeResult:int(message.wParam);
}
bool App::RenderPreviews(const std::filesystem::path& folder) {
    previewMode=true;engineReady=true;rawReady=true;hotkeyReady=true;selectionConfirmed=true;
    if(!graphics.Init())return false;
    Profile a;a.id=L"demo-a";a.hardwareId=L"demo-a";a.name=L"鼠标 A";a.sideCount=1;a.bindings[0]={ModAlt,VK_TAB};
    Profile b;b.id=L"demo-b";b.hardwareId=L"demo-b";b.name=L"鼠标 B";b.sideCount=2;b.bindings={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'}};
    config.profiles={a,b};selected=active=1;Device d;d.id=b.id;d.hardwareId=b.hardwareId;devices={d};
    std::filesystem::create_directories(folder);
    bool ok=graphics.SaveImage(folder/L"native-main.png",UINT(W),UINT(H),[&](Painter& p){DrawMain(p);});
    modal=Modal::Edit;draft=b.bindings[0];ok=graphics.SaveImage(folder/L"native-editor.png",UINT(W),UINT(H),[&](Painter& p){DrawMain(p);})&&ok;
    modal=Modal::Settings;ok=graphics.SaveImage(folder/L"native-settings.png",UINT(W),UINT(H),[&](Painter& p){DrawMain(p);})&&ok;
    modal=Modal::None;ok=graphics.SaveImage(folder/L"native-quick.png",328,232,[&](Painter& p){DrawQuick(p);})&&ok;
    selected=0;ok=graphics.SaveImage(folder/L"native-one-button.png",UINT(W),UINT(H),[&](Painter& p){DrawMain(p);})&&ok;
    return ok;
}
}
