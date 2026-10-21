#include "app.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <algorithm>
#include <sstream>
#include <fstream>

namespace side {
constexpr int Enable=1,Settings=2,Quick=3,EditFirst=4,EditSecond=5,Count=6,Rename=7,ChooseDevice=8;
constexpr int ChooseScheme=9,NewScheme=10,RenameScheme=11,DeleteScheme=12,SchemeBase=2000;
constexpr int CloseModalAction=20,Record=21,SaveBinding=22,Restore=23,Startup=24,ConnectNotice=25,OpenFolder=26,SaveName=27,CancelModal=28;
constexpr int ProfileBase=100,PresetBase=1000;
constexpr UINT RefreshTimer=1,HighlightTimer=2,NoticeTimer=3,QuickTimer=4;
static constexpr wchar_t MainClass[]=L"SideMapping.Native.Main";
static constexpr wchar_t QuickClass[]=L"SideMapping.Native.Quick";
static constexpr float W=700,H=582,QW=328,QH=232;
// Dialog geometry is shared by drawing, hit testing and the native name edit box.
static constexpr float ModalX=160,ModalW=380,EditY=70,EditH=440,SettingsY=104,SettingsH=346,RenameY=160,RenameH=232;
static constexpr auto Center=DWRITE_TEXT_ALIGNMENT_CENTER,Trailing=DWRITE_TEXT_ALIGNMENT_TRAILING;
static const Binding QuickHotkey{ModCtrl|ModAlt,'M'};
static std::wstring MenuLabel(const std::wstring& text){std::wstring label;for(wchar_t ch:text){label+=ch;if(ch==L'&')label+=ch;}return label;}
static std::wstring PhysicalName(int button){return button==0?L"后退键":L"前进键";}
void App::AddHit(float x,float y,float w,float h,int action){hits.push_back({D2D1::RectF(x,y,x+w,y+h),action});}
int App::HitTest(float x,float y) const {for(auto it=hits.rbegin();it!=hits.rend();++it)if(x>=it->rect.left&&x<=it->rect.right&&y>=it->rect.top&&y<=it->rect.bottom)return it->action;return 0;}
void App::Button(Painter& p,const std::wstring& text,float x,float y,float w,float h,int action,bool primary) {
    auto& c=p.colors;bool hovered=hoverAction==action;
    if(primary)p.Fill(x,y,w,h,hovered?c.accentHover:c.accent,9);
    else{p.Fill(x,y,w,h,hovered?c.soft:c.card,9);p.Border(x+.5f,y+.5f,w-1,h-1,hovered?c.edge:c.line,9);}
    p.Text(text,x+8,y,w-16,h,12,primary?c.on:c.ink,primary,Center);AddHit(x,y,w,h,action);
}
bool App::ProfileConnected(int index) const {
    if(index<0||index>=int(config.profiles.size()))return false;
    if(previewMode&&index==active)return true;
    return std::any_of(devices.begin(),devices.end(),[&](const Device& device){return FindProfile(config,device)==index;});
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
    return L"映射生效中 · 换一只鼠标时会自动切换到它的绑定";
}
// Must follow the same order as RoutingStatus so colour and message always agree.
App::Severity App::RoutingSeverity() const {
    if(!canWrite||((!engineReady||!rawReady)&&!previewMode))return Severity::Error;
    if(!config.enabled)return Severity::Warning;
    if(devices.empty()&&!previewMode)return Severity::Info;
    if((devices.size()>1&&!selectionConfirmed)||active<0)return Severity::Warning;
    if(selected!=active)return Severity::Info;
    return Severity::Ok;
}
void App::DrawMain(Painter& p) {
    hits.clear();auto& c=p.colors;
    p.Fill(0,0,W,H,c.canvas);
    // Header: brand, mapping switch, settings.
    p.Fill(24,18,34,34,c.accent,10);p.MouseIcon(35.6f,26.6f,.9f,c.on);
    p.Text(L"侧映",68,15,140,22,17,c.ink,true);p.Text(L"鼠标侧键快捷键",68,37,200,16,11,c.muted);
    bool live=config.enabled&&canWrite;
    if(hoverAction==Enable)p.Fill(456,18,180,34,c.soft,17);
    p.Text(live?L"映射已开启":L"映射已暂停",466,18,112,34,12,live?c.ink:c.muted,false,Trailing);
    p.Toggle(588,24,live);AddHit(456,18,180,34,Enable);
    if(hoverAction==Settings)p.Fill(642,18,34,34,c.soft,10);
    p.Gear(659,35,c.muted);AddHit(642,18,34,34,Settings);
    if(config.profiles.empty())DrawEmpty(p);
    else {
        if(selected<0||selected>=int(config.profiles.size()))selected=0;
        if(config.profiles.size()<=3) {
            const float width=644.f/float(config.profiles.size());p.Fill(24,72,652,44,c.soft,12);
            for(size_t i=0;i<config.profiles.size();++i){
                const float x=28+float(i)*width;const bool chosen=selected==int(i);const auto& name=config.profiles[i].name;
                if(chosen){p.Shadow(x,76,width-4,36,9,.8f);p.Fill(x,76,width-4,36,c.card,9);p.Border(x+.5f,76.5f,width-5,35,c.line,9);}
                else if(hoverAction==ProfileBase+int(i))p.Fill(x,76,width-4,36,c.card,9,.5f);
                float textWidth=std::min(p.Measure(name,13,chosen),width-60),textX=x+(width-4-textWidth-14)/2;
                if(int(i)==active)p.Circle(textX+3,94,3.5f,c.accent);else if(ProfileConnected(int(i)))p.Circle(textX+3,94,3.5f,c.muted);else p.Circle(textX+3,94,3,c.edge,false,1.2f);
                p.Text(name,textX+14,76,textWidth+2,36,13,chosen?c.ink:c.muted,chosen);AddHit(x,76,width-4,36,ProfileBase+int(i));
            }
        } else {
            bool hovered=hoverAction==ChooseDevice;p.Fill(24,72,652,44,hovered?c.soft:c.card,12);p.Border(24.5f,72.5f,651,43,hovered?c.edge:c.line,12);
            p.Circle(43,94,3.5f,selected==active?c.accent:c.muted);p.Text(config.profiles[selected].name,56,72,448,44,13,c.ink,true);
            p.Text(std::to_wstring(config.profiles.size())+L" 只鼠标",520,72,118,44,11,c.muted,false,Trailing);p.ChevronDown(648,92,c.muted);AddHit(24,72,652,44,ChooseDevice);
        }
        DrawDeviceCard(p);DrawBindings(p);
    }
    // Status banner coloured by severity.
    const auto severity=RoutingSeverity();
    const unsigned tint=severity==Severity::Ok?c.tint:severity==Severity::Info?c.soft:severity==Severity::Warning?c.warnTint:c.dangerTint;
    const unsigned tone=severity==Severity::Ok?c.accent:severity==Severity::Info?c.muted:severity==Severity::Warning?c.warn:c.danger;
    p.Fill(24,466,652,38,tint,10);
    if(severity==Severity::Ok)p.Check(39,481,tone,1.7f);else p.Circle(43,485,4,tone);
    p.Text(RoutingStatus(),58,466,600,38,12,severity==Severity::Ok||severity==Severity::Info?c.ink:tone);
    // Footer: last notice and the quick-view entry.
    p.Line(24,518,676,518,c.line);
    p.Text(notice.empty()?L"绑定保存在程序旁的 config.json":notice,24,518,404,60,11,notice.empty()?c.faint:c.muted);
    const float qx=446;const bool quickHovered=hoverAction==Quick;
    p.Fill(qx,530,230,38,quickHovered?c.soft:c.card,9);p.Border(qx+.5f,530.5f,229,37,quickHovered?c.edge:c.line,9);
    if(hotkeyReady||previewMode){p.Text(L"快捷键速览",qx+14,530,80,38,12,c.ink);p.Keycaps(QuickHotkey,qx+90,536,132,26);}
    else p.Text(L"快捷键速览",qx,530,230,38,12,c.ink,false,Center);
    AddHit(qx,530,230,38,Quick);
    if(modal!=Modal::None)DrawModal(p);
    DrawFocus(p);
}
void App::DrawFocus(Painter& p){
    if(keyboardFocus<0||keyboardFocus>=int(hits.size()))return;
    auto rect=hits[keyboardFocus].rect;p.Border(rect.left-3,rect.top-3,rect.right-rect.left+6,rect.bottom-rect.top+6,p.colors.accent,11,2);
}
void App::DrawEmpty(Painter& p){
    auto& c=p.colors;p.Card(24,72,652,384,14,c.card,c.line);
    p.Circle(350,214,78,c.tint);p.MouseIcon(327,170,3.9f,c.accent);
    p.Text(L"连接一只鼠标，开始设置",64,316,572,32,19,c.ink,true,Center);
    p.Text(L"设备识别后会自动出现在这里",64,354,572,24,13,c.muted,false,Center);
    p.Text(L"首次接入保留原功能，不会自动改键",64,380,572,22,12,c.faint,false,Center);
}
void App::DrawDeviceCard(Painter& p){
    auto& c=p.colors;const auto& profile=config.profiles[selected];
    p.Card(24,132,296,324,14,c.card,c.line);
    const float nameWidth=std::min(p.Measure(profile.name,16,true),150.f);p.Text(profile.name,44,148,nameWidth+2,28,16,c.ink,true);
    const float pencil=44+nameWidth+6;
    if(hoverAction==Rename)p.Fill(pencil,149,26,26,c.soft,7);
    p.Pencil(pencil+6.5f,155,c.muted);AddHit(pencil,149,26,26,Rename);
    // Status pill: in use / paused / connected / not connected.
    std::wstring status=selected==active&&config.enabled&&canWrite&&selectionConfirmed&&engineReady&&rawReady?L"当前使用":ProfileConnected(selected)?L"已连接":L"未连接";
    if(!config.enabled&&selected==active)status=L"映射暂停";
    const bool inUse=status==L"当前使用",paused=status==L"映射暂停";
    const float pill=p.Measure(status,11)+28;const unsigned pillInk=inUse?c.accent:paused?c.warn:c.muted;
    p.Fill(300-pill,150,pill,24,inUse?c.tint:paused?c.warnTint:c.soft,12);p.Circle(300-pill+12,162,3,pillInk);p.Text(status,300-pill+19,150,pill-22,24,11,pillInk);
    const std::wstring count=std::to_wstring(profile.sideCount)+L" 个侧键";const float chip=p.Measure(count,11)+40;
    p.Fill(44,182,chip,26,hoverAction==Count?c.line:c.soft,13);p.Text(count,56,182,chip-30,26,11,c.muted);p.ChevronDown(44+chip-22,192,c.muted);AddHit(44,182,chip,26,Count);
    // Illustration: the original mouse drawing, scaled into the card. Side buttons are clickable.
    const float cx=172,cy=318,scale=.78f;
    p.Circle(cx,cy+4,94,c.tint,true,1,c.dark?.55f:.75f);
    const auto transform=D2D1::Matrix3x2F::Rotation(-13,D2D1::Point2F(189,320))*D2D1::Matrix3x2F::Scale(D2D1::SizeF(scale,scale),D2D1::Point2F(189,320))*D2D1::Matrix3x2F::Translation(cx-189,cy-320);
    p.target->SetTransform(transform);
    p.Fill(132,219,128,211,c.shadow,58,c.dark?.25f:.07f);p.Fill(125,211,128,211,c.shell,58);p.Border(125.5f,211.5f,127,210,c.edge,58,1.2f);
    p.Line(189,212,189,290,c.edge,1.1f);p.Line(131,292,247,292,c.edge,1);
    p.Fill(182,236,14,33,c.card,7);p.Border(182.5f,236.5f,13,32,c.edge,7,1);for(int i=0;i<5;++i)p.Line(185.5f,243+4.f*i,192.5f,243+4.f*i,c.edge,1);
    std::vector<std::pair<D2D1_POINT_2F,int>> sideHits;
    for(int i=0;i<profile.sideCount;++i){
        const int button=PhysicalButton(profile,i);const int action=button==0?EditFirst:EditSecond;const float y=291+44.f*i;
        if(highlight==button||hoverAction==action)p.Border(111,y-5,30,47,c.accent,10,2.2f);
        p.Fill(116,y,20,37,config.enabled?c.accent:c.muted,7);p.Text(std::to_wstring(i+1),116,y,20,37,12,c.on,true,Center);
        sideHits.push_back({transform.TransformPoint(D2D1::Point2F(126,y+18.5f)),action});
    }
    p.target->SetTransform(D2D1::Matrix3x2F::Identity());
    for(const auto& [point,action]:sideHits)AddHit(point.x-17*scale-4,point.y-23*scale-4,34*scale+8,46*scale+8,action);
    p.Text(L"点击图中或右侧的侧键，设置快捷键",24,420,296,24,11,c.faint,false,Center);
}
void App::DrawBindings(Painter& p){
    auto& c=p.colors;const auto& profile=config.profiles[selected];const float x=336,w=340,h=104;
    const bool schemeHovered=hoverAction==ChooseScheme;
    p.Fill(x,132,240,36,schemeHovered?c.soft:c.card,10);p.Border(x+.5f,132.5f,239,35,schemeHovered?c.edge:c.line,10);
    p.Text(L"方案",x+14,132,40,36,12,c.muted);p.Text(CurrentScheme(profile).name,x+50,132,160,36,13,c.ink,true);p.ChevronDown(x+216,147,c.muted);AddHit(x,132,240,36,ChooseScheme);
    Button(p,L"＋ 新建",584,132,92,36,NewScheme);
    for(int i=0;i<profile.sideCount;++i){
        const int button=PhysicalButton(profile,i);const int action=button==0?EditFirst:EditSecond;const float y=184+116.f*i;
        const bool hovered=hoverAction==action,pulse=highlight==button;const auto& binding=CurrentBindings(profile)[button];
        p.Card(x,y,w,h,14,pulse?c.tint:c.card,pulse?c.accent:hovered?c.edge:c.line);if(pulse)p.Border(x+1,y+1,w-2,h-2,c.accent,13);
        p.Fill(x+16,y+20,34,34,pulse?c.card:c.tint,10);p.Text(std::to_wstring(i+1),x+16,y+20,34,34,14,c.accent,true,Center);
        p.Text(ActionName(binding),x+64,y+16,220,24,14,c.ink,true);
        p.Text(L"侧键 "+std::to_wstring(i+1)+L" · "+PhysicalName(button),x+64,y+40,220,18,11,c.muted);
        p.Keycaps(binding,x+64,y+66,236,26);
        p.Chevron(x+w-26,y+48,hovered?c.ink:c.faint);AddHit(x,y,w,h,action);
    }
    p.Text(L"在本窗口按一下鼠标侧键，可识别设备与按键",x,420,w,22,11,c.faint,false,Center);
}
void App::ModalFrame(Painter& p,float y,float height,const std::wstring& title,const std::wstring& subtitle){
    auto& c=p.colors;const float x=ModalX,w=ModalW;
    for(int i=1;i<=6;++i){float d=float(i);p.Fill(x-d,y-d+d*1.2f,w+2*d,height+2*d,c.shadow,16+d,(c.dark?.12f:.05f)/d);}
    p.Fill(x,y,w,height,c.card,16);p.Border(x+.5f,y+.5f,w-1,height-1,c.line,16);
    p.Text(title,x+24,y+18,270,28,17,c.ink,true);if(!subtitle.empty())p.Text(subtitle,x+24,y+46,300,18,11,c.muted);
    if(hoverAction==CloseModalAction)p.Fill(x+w-52,y+16,32,32,c.soft,9);
    p.Close(x+w-40.5f,y+27.5f,c.muted);AddHit(x+w-52,y+16,32,32,CloseModalAction);
}
void App::DrawModal(Painter& p) {
    auto& c=p.colors;p.Fill(0,0,W,H,c.shadow,0,c.dark?.45f:.28f);hits.clear();
    const float x=ModalX,w=ModalW;
    if(modal==Modal::Edit) {
        const auto& profile=config.profiles[selected];
        const float y=EditY,h=EditH;
        ModalFrame(p,y,h,L"设置侧键 "+std::to_wstring(profile.sideCount==1?1:editingButton+1),profile.name+L" · 方案 "+CurrentScheme(profile).name+L" · "+PhysicalName(editingButton));
        const float fx=x+24,fy=y+80,fw=332,fh=76;const bool hovered=hoverAction==Record;
        p.Fill(fx,fy,fw,fh,recording?c.tint:(hovered||!draft.key)?c.soft:c.canvas,12);p.Border(fx+.5f,fy+.5f,fw-1,fh-1,recording?c.accent:hovered?c.edge:c.line,12,recording?1.5f:1);
        if(recording){p.Circle(fx+fw/2-62,fy+fh/2,4.5f,c.accent);p.Text(L"请按下快捷键…",fx+fw/2-50,fy,160,fh,15,c.accent,true);}
        else if(!draft.key)p.Text(L"点击这里，然后按下快捷键",fx,fy,fw,fh,13,c.muted,false,Center);
        else{float width=p.KeycapsWidth(draft);if(width<=fw-32)p.Keycaps(draft,fx+(fw-width)/2,fy+16,fw-32,28);else p.Text(ShortcutText(draft),fx+16,fy+14,fw-32,30,15,c.ink,true,Center);p.Text(L"点击重新录制",fx,fy+48,fw,20,10,c.faint,false,Center);}
        AddHit(fx,fy,fw,fh,Record);
        p.Text(recording?L"按 Esc 取消；Win+L 等系统保留组合无法录入":L"也可以直接选择一个常用操作",x+24,y+162,332,22,11,c.muted);
        for(size_t i=0;i<presets.size();++i){
            const float px=x+24+float(i%2)*170,py=y+192+float(i/2)*44;const bool chosen=draft==presets[i],over=hoverAction==PresetBase+int(i);
            p.Fill(px,py,162,36,chosen?c.tint:over?c.soft:c.card,9);p.Border(px+.5f,py+.5f,161,35,chosen?c.accent:over?c.edge:c.line,9);
            p.Text(ActionName(presets[i]),px+12,py,74,36,12,chosen?c.accent:c.ink,chosen);p.Text(ShortcutText(presets[i]),px+76,py,76,36,10,c.muted,false,Trailing);AddHit(px,py,162,36,PresetBase+int(i));
        }
        const float by=y+h-58;const bool restoreHovered=hoverAction==Restore;
        if(restoreHovered)p.Fill(x+16,by,104,34,c.soft,9);
        p.Text(L"恢复原功能",x+16,by,104,34,12,restoreHovered?c.ink:c.muted,false,Center);AddHit(x+16,by,104,34,Restore);
        Button(p,L"取消",x+w-24-108-8-76,by,76,34,CancelModal);Button(p,L"保存绑定",x+w-24-108,by,108,34,SaveBinding,true);
    }else if(modal==Modal::Settings) {
        const float y=SettingsY,h=SettingsH;ModalFrame(p,y,h,L"设置",L"");
        float row=y+62;
        auto toggleRow=[&](const std::wstring& title,const std::wstring& detail,bool on,int action){
            if(hoverAction==action)p.Fill(x+12,row+2,w-24,60,c.soft,10);
            p.Text(title,x+24,row+11,260,22,13,c.ink);p.Text(detail,x+24,row+33,270,18,11,c.muted);p.Toggle(x+w-24-38,row+21,on);AddHit(x+12,row+2,w-24,60,action);
            row+=64;p.Line(x+24,row-1,x+w-24,row-1,c.line);
        };
        toggleRow(L"开机时启动",L"登录 Windows 后在托盘中静默运行",previewMode?false:IsStartupEnabled(executable.wstring()),Startup);
        toggleRow(L"接入鼠标后显示绑定提示",L"接入鼠标后，在屏幕右下角短暂显示速览",config.showOnConnect,ConnectNotice);
        p.Text(L"快捷键速览",x+24,row+11,200,22,13,c.ink);
        if(hotkeyReady||previewMode){p.Text(L"随时查看当前鼠标的绑定",x+24,row+33,220,18,11,c.muted);p.Keycaps(QuickHotkey,x+w-24-128,row+19,140,26);}
        else p.Text(L"快捷键被占用，可从托盘菜单打开",x+24,row+33,300,18,11,c.warn);
        Button(p,L"打开配置文件夹",x+24,y+h-58,140,34,OpenFolder);p.Text(L"侧映 0.3.0",x+w-24-120,y+h-58,120,34,11,c.faint,false,Trailing);
    }else {
        const float y=RenameY,h=RenameH;const bool device=namePurpose==NamePurpose::Device;
        ModalFrame(p,y,h,device?L"给鼠标起个名字":namePurpose==NamePurpose::NewScheme?L"新建方案":L"重命名方案",L"");
        p.Text(device?L"设备名称":L"方案名称",x+24,y+60,332,20,11,c.muted);
        // The native EDIT control sits inside this box (see NameEditRect).
        p.Fill(x+24,y+84,332,42,c.card,10);p.Border(x+24.75f,y+84.75f,330.5f,40.5f,c.accent,10,1.5f);
        p.Text(device?L"名称只用于方便区分，不改变设备身份":namePurpose==NamePurpose::NewScheme?L"复制当前绑定，新方案的修改互不影响":L"仅修改名称，保留此方案全部绑定",x+24,y+132,332,22,11,c.muted);
        const float by=y+h-58;Button(p,L"取消",x+w-24-108-8-76,by,76,34,CancelModal);Button(p,L"保存名称",x+w-24-108,by,108,34,SaveName,true);
    }
}
void App::DrawQuick(Painter& p) {
    auto& c=p.colors;p.Fill(0,0,QW,QH,c.card);
    p.Fill(18,16,30,30,c.accent,9);p.MouseIcon(28,23,.85f,c.on);
    p.Text(L"快捷键速览",58,13,200,20,14,c.ink,true);
    p.Text(active>=0?config.profiles[active].name+L" · "+CurrentScheme(config.profiles[active]).name:L"未连接鼠标",58,33,220,16,11,c.muted);
    p.Close(296,26,c.muted);
    if(active>=0) {
        const auto& profile=config.profiles[active];
        const bool effective=config.enabled&&selectionConfirmed&&canWrite&&engineReady&&rawReady;
        for(int i=0;i<profile.sideCount;++i){
            const int button=PhysicalButton(profile,i);const float y=62+i*62.f;const auto& binding=CurrentBindings(profile)[button];
            p.Fill(14,y,300,54,c.soft,11);
            p.Text(L"侧键 "+std::to_wstring(i+1),28,y+8,90,18,10,c.muted);p.Text(effective?ActionName(binding):L"原始功能",28,y+25,110,22,13,c.ink,true);
            if(effective&&binding.key){float width=p.KeycapsWidth(binding);if(width<=160)p.Keycaps(binding,300-width,y+14,160,26,true);else p.Text(ShortcutText(binding),140,y,160,54,11,c.ink,false,Trailing);}
            else p.Text(effective?L"保留原功能":L"映射已暂停",140,y,160,54,11,c.muted,false,Trailing);
        }
    } else p.Text(L"连接鼠标后显示它的绑定",14,72,300,80,13,c.muted,false,Center);
    const bool shortcut=hotkeyReady||previewMode;
    const std::wstring footer=quickAutomatic&&!previewMode?(shortcut?L"几秒后自动收起 · Ctrl + Alt + M 随时查看":L"几秒后自动收起 · 可从托盘再次打开"):
        (escapeHotkey||previewMode)?(shortcut?L"Esc 收起 · 也可再次按 Ctrl + Alt + M":L"Esc 收起 · 可从托盘再次打开"):
        shortcut?L"再次按 Ctrl + Alt + M 收起":L"点击右上角关闭 · 可从托盘再次打开";
    p.Text(footer,14,204,300,20,10,c.faint,false,Center);
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
    const int oldActive=active,oldSelected=selected;const auto oldDevices=devices;devices=EnumerateMice();
    devices.erase(std::remove_if(devices.begin(),devices.end(),[](const Device& d){return !IsMouseCandidate(d);}),devices.end());
    bool changed=false;int newlyConnected=-1;int newCount=0;
    for(const auto& device:devices) {
        int existing=FindProfile(config,device);
        if(existing<0&&config.profiles.size()<100) {
            Profile p;p.id=device.id;p.hardwareId=device.hardwareId;p.name=device.name==L"鼠标"?L"鼠标 "+std::to_wstring(config.profiles.size()+1):device.name;
            if(p.name.size()>60){size_t length=60;if(IS_HIGH_SURROGATE(p.name[length-1]))--length;p.name.resize(length);}  // never split a surrogate pair
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
    // Unsaved edits belong to a specific mouse: drop them only if the hardware or the edited mouse really changed.
    const bool deviceSetChanged=devices.size()!=oldDevices.size()||!std::equal(devices.begin(),devices.end(),oldDevices.begin(),[](const Device& a,const Device& b){return a.id==b.id;});
    if(!initial&&(modal==Modal::Edit||modal==Modal::Rename)&&(deviceSetChanged||selected!=oldSelected)){CloseModal();Notify(L"设备发生变化，已取消未保存的编辑");}
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
    if(active>=0){HMENU schemes=CreatePopupMenu();const auto& profile=config.profiles[active];for(size_t i=0;i<profile.schemes.size();++i)AppendMenuW(schemes,MF_STRING|(i==profile.activeScheme?MF_CHECKED:0),SchemeBase+i,MenuLabel(profile.schemes[i].name).c_str());InsertMenuW(menu,2,MF_BYPOSITION|MF_POPUP,reinterpret_cast<UINT_PTR>(schemes),L"切换方案");}
    POINT point{};GetCursorPos(&point);SetForegroundWindow(window);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);DestroyMenu(menu);PostMessageW(window,WM_NULL,0,0);
    if(result==1)ShowMain();else if(result==2)ShowQuick();else if(result==3)Activate(Enable);else if(result==4)Activate(OpenFolder);else if(result==5){exiting=true;DestroyWindow(window);}else if(result>=SchemeBase&&result<SchemeBase+32&&active>=0){SwitchScheme(active,size_t(result-SchemeBase));ShowQuick(true);}
}
void App::SchemeMenu(){
    if(selected<0)return;
    HMENU menu=CreatePopupMenu();const auto& profile=config.profiles[selected];
    for(size_t i=0;i<profile.schemes.size();++i)AppendMenuW(menu,MF_STRING|(i==profile.activeScheme?MF_CHECKED:0),SchemeBase+i,MenuLabel(profile.schemes[i].name).c_str());
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,NewScheme,L"新建方案…");AppendMenuW(menu,MF_STRING,RenameScheme,L"重命名当前方案…");AppendMenuW(menu,MF_STRING|(profile.schemes.size()==1?MF_GRAYED:0),DeleteScheme,L"删除当前方案…");
    POINT point=MenuAnchor(ChooseScheme);
    int result=TrackPopupMenu(menu,TPM_RETURNCMD,point.x,point.y,0,window,nullptr);DestroyMenu(menu);if(result)Activate(result);
}
void App::SwitchScheme(int profileIndex,size_t schemeIndex){
    if(profileIndex<0||profileIndex>=int(config.profiles.size())||schemeIndex>=config.profiles[profileIndex].schemes.size())return;
    if(modal!=Modal::None)CloseModal();
    auto& profile=config.profiles[profileIndex];auto previous=profile.activeScheme;
    profile.activeScheme=schemeIndex;
    if(!Save()){profile.activeScheme=previous;UpdateMapping();return;}
    keyboardFocus=-1;UpdateMapping();Notify(L"已切换："+profile.name+L" · "+CurrentScheme(profile).name);
}
void App::OpenNameDialog(NamePurpose purpose){
    if(selected<0)return;
    HideQuick();namePurpose=purpose;modal=Modal::Rename;keyboardFocus=-1;
    const auto& profile=config.profiles[selected];std::wstring value;
    if(purpose==NamePurpose::Device)value=profile.name;
    else if(purpose==NamePurpose::Scheme)value=CurrentScheme(profile).name;
    else{for(int i=1;;++i){value=L"新方案 "+std::to_wstring(i);if(std::none_of(profile.schemes.begin(),profile.schemes.end(),[&](const MappingScheme& s){return s.name==value;}))break;}}
    const UINT dpi=GetDpiForWindow(window);const RECT box=NameEditRect(dpi);
    // Borderless EDIT inside the Direct2D-drawn input box; colours come from WM_CTLCOLOREDIT.
    nameEdit=CreateWindowExW(0,L"EDIT",value.c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,box.left,box.top,box.right-box.left,box.bottom-box.top,window,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!nameEdit){CloseModal();Notify(L"无法打开名称输入框");return;}
    SendMessageW(nameEdit,EM_SETLIMITTEXT,40,0);UpdateEditFont(dpi);SendMessageW(nameEdit,EM_SETSEL,0,-1);SetFocus(nameEdit);InvalidateRect(window,nullptr,FALSE);
}
void App::ShowQuick(bool automatic) {
    if(!quickWindow)return;
    if(quickVisible&&!automatic){HideQuick();return;}
    // An automatic popup must not steal Esc from whatever the user is doing; only a manual open claims it.
    if(!quickVisible)quickAutomatic=automatic;else if(!automatic)quickAutomatic=false;
    if(!quickAutomatic&&!escapeHotkey)escapeHotkey=RegisterHotKey(window,2,MOD_NOREPEAT,VK_ESCAPE)!=FALSE;
    quickVisible=true;
    POINT cursor{};GetCursorPos(&cursor);PlaceQuick(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),true);
    InvalidateRect(quickWindow,nullptr,FALSE);if(quickAutomatic)SetTimer(window,QuickTimer,4500,nullptr);else KillTimer(window,QuickTimer);
}
// Size the popup with the DPI of the monitor it appears on, not the DPI of the main window.
void App::PlaceQuick(HMONITOR monitor,bool show) {
    MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return;
    UINT dpi=0,dpiY=0;if(FAILED(GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dpi,&dpiY))||!dpi)dpi=GetDpiForWindow(window);
    const int width=MulDiv(int(QW),int(dpi),96),height=MulDiv(int(QH),int(dpi),96),margin=MulDiv(16,int(dpi),96);
    if(graphics.quickTarget)graphics.quickTarget->SetDpi(float(dpi),float(dpi));
    SetWindowPos(quickWindow,HWND_TOPMOST,info.rcWork.right-width-margin,info.rcWork.bottom-height-margin,width,height,SWP_NOACTIVATE|(show?SWP_SHOWWINDOW:0));
}
void App::HideQuick(){if(!quickVisible)return;ShowWindow(quickWindow,SW_HIDE);quickVisible=false;quickAutomatic=false;if(escapeHotkey)UnregisterHotKey(window,2);escapeHotkey=false;KillTimer(window,QuickTimer);}
POINT App::MenuAnchor(int action) const {
    POINT point{};
    for(const auto& hit:hits)if(hit.action==action){const float scale=GetDpiForWindow(window)/96.f;point={LONG(hit.rect.left*scale),LONG((hit.rect.bottom+4)*scale)};ClientToScreen(window,&point);return point;}
    GetCursorPos(&point);return point;
}
RECT App::NameEditRect(UINT dpi) const {
    const float scale=float(dpi)/96.f;
    return RECT{LONG((ModalX+38)*scale),LONG((RenameY+94)*scale),LONG((ModalX+38+304)*scale),LONG((RenameY+94+22)*scale)};
}
void App::UpdateEditFont(UINT dpi) {
    if(!nameEdit)return;
    HFONT next=CreateFontW(-MulDiv(14,int(dpi),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft YaHei UI");
    if(next){SendMessageW(nameEdit,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);if(editFont)DeleteObject(editFont);editFont=next;}
    SendMessageW(nameEdit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
}
// Dark title bar and a caption colour matching the canvas (Windows 11; ignored on older systems).
void App::ApplyWindowTheme() {
    if(!window)return;
    const auto& c=graphics.palette;BOOL dark=c.dark;COLORREF caption=ToColorRef(c.canvas),text=ToColorRef(c.ink);
    DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    DwmSetWindowAttribute(window,35,&caption,sizeof(caption));
    DwmSetWindowAttribute(window,36,&text,sizeof(text));
    if(editBrush){DeleteObject(editBrush);editBrush=nullptr;}
    if(nameEdit)InvalidateRect(nameEdit,nullptr,TRUE);
}
void App::OpenEdit(int index) {
    if(selected<0||index<0||index>1||(config.profiles[selected].sideCount==1&&index!=config.profiles[selected].singleSideIndex))return;
    HideQuick();editingButton=index;draft=CurrentBindings(config.profiles[selected])[index];modal=Modal::Edit;recording=false;keyboardFocus=-1;InvalidateRect(window,nullptr,FALSE);
}
void App::CloseModal(){input.CancelRecording();recording=false;modal=Modal::None;if(nameEdit){DestroyWindow(nameEdit);nameEdit=nullptr;}if(editFont){DeleteObject(editFont);editFont=nullptr;}keyboardFocus=-1;InvalidateRect(window,nullptr,FALSE);SetFocus(window);}
void App::Activate(int action) {
    if(action==Enable){
        if(!canWrite){Notify(L"配置读取失败，无法开启映射；请检查 config.json");return;}
        config.enabled=!config.enabled;
        if(!Save()){UpdateMapping();return;}  // Save() already paused mapping and reported the error
        UpdateMapping();Notify(config.enabled?L"映射已恢复":L"已暂停，原始功能生效");
    }
    else if(action==Settings){HideQuick();modal=Modal::Settings;keyboardFocus=-1;InvalidateRect(window,nullptr,FALSE);}
    else if(action==Quick)ShowQuick();
    else if(action==ChooseScheme)SchemeMenu();
    else if(action==NewScheme)OpenNameDialog(NamePurpose::NewScheme);
    else if(action==RenameScheme)OpenNameDialog(NamePurpose::Scheme);
    else if(action>=SchemeBase&&action<SchemeBase+32)SwitchScheme(selected,size_t(action-SchemeBase));
    else if(action==DeleteScheme&&selected>=0){auto& profile=config.profiles[selected];if(profile.schemes.size()==1)return;std::wstring question=L"删除方案“"+CurrentScheme(profile).name+L"”？\n此方案的绑定也会删除，其他方案不受影响。";if(MessageBoxW(window,question.c_str(),L"侧映",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;Profile previous=profile;std::wstring error;if(!DeleteCurrentScheme(profile,error)){Notify(error);return;}if(!Save()){profile=std::move(previous);UpdateMapping();return;}UpdateMapping();Notify(L"当前方案："+CurrentScheme(profile).name);}
    else if(action==EditFirst||action==EditSecond)OpenEdit(action==EditFirst?0:1);
    else if(action>=ProfileBase&&action<ProfileBase+100){selected=action-ProfileBase;highlight=-1;InvalidateRect(window,nullptr,FALSE);}
    else if(action==ChooseDevice){HMENU menu=CreatePopupMenu();for(size_t i=0;i<config.profiles.size();++i)AppendMenuW(menu,MF_STRING|(int(i)==selected?MF_CHECKED:0),UINT_PTR(ProfileBase+i),MenuLabel(config.profiles[i].name).c_str());POINT pt=MenuAnchor(ChooseDevice);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(result)Activate(result);}
    else if(action==Count&&selected>=0){
        auto& profile=config.profiles[selected];HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|(profile.sideCount==1?MF_CHECKED:0),1,L"1 个侧键");AppendMenuW(menu,MF_STRING|(profile.sideCount==2?MF_CHECKED:0),2,L"2 个侧键");
        POINT pt=MenuAnchor(Count);int count=TrackPopupMenu(menu,TPM_RETURNCMD,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);
        if(count&&count!=profile.sideCount){const int previous=profile.sideCount;profile.sideCount=count;if(!Save())profile.sideCount=previous;UpdateMapping();}
    }
    else if(action==CloseModalAction||action==CancelModal)CloseModal();
    else if(action==Record){
        if(!engineReady){Notify(L"输入监听不可用，暂时无法录制快捷键");return;}  // otherwise the field would wait forever
        recording=true;input.BeginRecording();InvalidateRect(window,nullptr,FALSE);
    }
    else if(action==Restore){input.CancelRecording();recording=false;draft={};InvalidateRect(window,nullptr,FALSE);}
    else if(action>=PresetBase&&action<PresetBase+int(presets.size())){input.CancelRecording();recording=false;draft=presets[action-PresetBase];InvalidateRect(window,nullptr,FALSE);}
    else if(action==SaveBinding&&modal==Modal::Edit&&!recording){if(!ValidBinding(draft))return;auto& binding=CurrentBindings(config.profiles[selected])[editingButton];const auto previous=binding;binding=draft;if(Save()){CloseModal();UpdateMapping();Notify(L"已保存 · "+config.profiles[selected].name+L" / 侧键 "+std::to_wstring(config.profiles[selected].sideCount==1?1:editingButton+1));}else{binding=previous;UpdateMapping();}}
    else if(action==Startup){std::wstring error;if(!SetStartupEnabled(!IsStartupEnabled(executable.wstring()),executable.wstring(),error))Notify(error,true);InvalidateRect(window,nullptr,FALSE);}
    else if(action==ConnectNotice){config.showOnConnect=!config.showOnConnect;if(!Save())config.showOnConnect=!config.showOnConnect;InvalidateRect(window,nullptr,FALSE);}
    else if(action==OpenFolder)ShellExecuteW(window,L"open",executable.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    else if(action==Rename)OpenNameDialog(NamePurpose::Device);
    else if(action==SaveName&&nameEdit){wchar_t name[81]{};GetWindowTextW(nameEdit,name,81);std::wstring value=name;auto first=value.find_first_not_of(L" \t\r\n"),last=value.find_last_not_of(L" \t\r\n");if(first==std::wstring::npos){MessageBoxW(window,L"名称不能为空。",L"侧映",MB_OK|MB_ICONINFORMATION);return;}value=value.substr(first,last-first+1);auto& profile=config.profiles[selected];Profile previous=profile;std::wstring error;bool valid=true;if(namePurpose==NamePurpose::Device)profile.name=value;else if(namePurpose==NamePurpose::NewScheme)valid=AddScheme(profile,value,error);else valid=RenameCurrentScheme(profile,value,error);if(!valid){MessageBoxW(window,error.c_str(),L"侧映",MB_OK|MB_ICONINFORMATION);return;}if(Save()){CloseModal();UpdateMapping();Notify(L"已保存："+profile.name+L" · "+CurrentScheme(profile).name);}else{profile=std::move(previous);UpdateMapping();}}
}
void App::ResizeForDpi(UINT dpi,const RECT* suggested) {
    RECT rect{0,0,MulDiv(int(W),dpi,96),MulDiv(int(H),dpi,96)};
    AdjustWindowRectExForDpi(&rect,DWORD(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE)),dpi);
    SetWindowPos(window,nullptr,suggested?suggested->left:0,suggested?suggested->top:0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOZORDER|(suggested?0:SWP_NOMOVE));
    if(graphics.mainTarget)graphics.mainTarget->SetDpi(float(dpi),float(dpi));  // the quick window follows its own monitor's DPI
    if(nameEdit){const RECT box=NameEditRect(dpi);SetWindowPos(nameEdit,nullptr,box.left,box.top,box.right-box.left,box.bottom-box.top,SWP_NOZORDER);UpdateEditFont(dpi);}
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
    case WM_DPICHANGED:{UINT dpi=HIWORD(wp);if(app->graphics.quickTarget)app->graphics.quickTarget->SetDpi(float(dpi),float(dpi));app->PlaceQuick(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),false);return 0;}
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
    case WM_SETTINGCHANGE:graphics.RefreshPalette();ApplyWindowTheme();Release(graphics.mainTarget);Release(graphics.quickTarget);InvalidateRect(window,nullptr,FALSE);if(quickWindow)InvalidateRect(quickWindow,nullptr,FALSE);return 0;
    case WM_CTLCOLOREDIT:
        if(reinterpret_cast<HWND>(lp)==nameEdit){HDC dc=reinterpret_cast<HDC>(wp);const auto& c=graphics.palette;SetTextColor(dc,ToColorRef(c.ink));SetBkColor(dc,ToColorRef(c.card));if(!editBrush)editBrush=CreateSolidBrush(ToColorRef(c.card));return reinterpret_cast<LRESULT>(editBrush);}
        break;
    case WM_DISPLAYCHANGE:HideQuick();return 0;
    case WM_SETCURSOR:if(LOWORD(lp)==HTCLIENT&&hoverAction){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_MOUSEMOVE:{float scale=GetDpiForWindow(window)/96.f;int next=HitTest(GET_X_LPARAM(lp)/scale,GET_Y_LPARAM(lp)/scale);if(next!=hoverAction){hoverAction=next;InvalidateRect(window,nullptr,FALSE);}TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);return 0;}
    case WM_MOUSELEAVE:hoverAction=0;InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_LBUTTONUP:{float scale=GetDpiForWindow(window)/96.f;int action=HitTest(GET_X_LPARAM(lp)/scale,GET_Y_LPARAM(lp)/scale);if(action)Activate(action);return 0;}
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){if(modal!=Modal::None){if(recording){recording=false;input.CancelRecording();InvalidateRect(window,nullptr,FALSE);}else CloseModal();}else if(quickVisible)HideQuick();return 0;}
        if(wp==VK_TAB&&!recording&&!hits.empty()){int step=(GetKeyState(VK_SHIFT)&0x8000)?-1:1;keyboardFocus=(keyboardFocus+step+int(hits.size()))%int(hits.size());InvalidateRect(window,nullptr,FALSE);return 0;}
        if((wp==VK_RETURN||wp==VK_SPACE)&&!recording&&keyboardFocus>=0&&keyboardFocus<int(hits.size())){Activate(hits[keyboardFocus].action);return 0;}
        if(wp==VK_RETURN&&!recording&&modal==Modal::Edit){Activate(SaveBinding);return 0;}
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
            if(index>=0&&sideDown&&GetForegroundWindow()==window&&modal==Modal::None){bool changed=active!=index||!selectionConfirmed;int button=(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)?0:1;auto& profile=config.profiles[index];if(LearnSingleSideButton(profile,button))changed=true;active=selected=index;selectionConfirmed=true;config.preferredProfile=profile.id;if(changed){if(Save())Notify(L"已识别当前鼠标："+profile.name);UpdateMapping();}}
            else if(index>=0&&active>=0&&index!=active&&(mouse.lLastX||mouse.lLastY||sideDown)&&GetForegroundWindow()!=window){active=-1;selectionConfirmed=false;config.preferredProfile.clear();UpdateMapping();Notify(L"检测到另一鼠标输入，映射暂停；打开侧映并按侧键确认",true);}
            if(index==selected&&sideDown){highlight=(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)?0:1;SetTimer(window,HighlightTimer,650,nullptr);InvalidateRect(window,nullptr,FALSE);}break;
        }}}}
        return DefWindowProcW(window,message,wp,lp);
    }
    case WM_POWERBROADCAST:if(wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND)SetTimer(window,RefreshTimer,900,nullptr);return TRUE;
    case WM_TIMER:
        if(wp==500&&smokeMode){
            KillTimer(window,500);bool ok=engineReady&&rawReady&&IsWindow(window)&&IsWindow(quickWindow);
            ok=ok&&trayReady;
            std::vector<std::pair<const char*,bool>> regressions;
            auto check=[&](const char* name,bool passed){regressions.emplace_back(name,passed);ok=passed&&ok;};
            ok=ok&&((GetWindowLongPtrW(quickWindow,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0);
            config.enabled=false;
            if(config.profiles.empty()){Profile fixture;fixture.id=L"smoke";fixture.hardwareId=L"smoke";fixture.name=L"自检鼠标";config.profiles.push_back(fixture);}
            if(!config.profiles.empty()){
                selected=0;auto button=PhysicalButton(config.profiles[0],0);auto originalScheme=config.profiles[0].activeScheme;OpenEdit(button);Activate(PresetBase);Activate(SaveBinding);
                auto previousCount=config.profiles[0].schemes.size();
                Activate(NewScheme);if(nameEdit){SetWindowTextW(nameEdit,L"集成测试方案");Activate(SaveName);}else ok=false;
                ok=ok&&config.profiles[0].schemes.size()==previousCount+1;
                auto addedScheme=config.profiles[0].activeScheme;
                OpenEdit(button);Activate(PresetBase+1);Activate(SaveBinding);
                Activate(SchemeBase+int(originalScheme));ok=ok&&CurrentBindings(config.profiles[0])[button]==presets[0];
                SwitchScheme(0,addedScheme);ok=ok&&CurrentBindings(config.profiles[0])[button]==presets[1];
                Activate(RenameScheme);if(nameEdit){SetWindowTextW(nameEdit,L"办公测试");Activate(SaveName);}else ok=false;
                Config verification;std::wstring error;ok=ok&&LoadConfig(configPath,verification,error)&&!verification.enabled&&!verification.profiles.empty()&&CurrentBindings(verification.profiles[0])[button]==presets[1]&&CurrentScheme(verification.profiles[0]).name==L"办公测试"&&verification.profiles[0].activeScheme==addedScheme;
                keyboardFocus=1;Activate(Settings);
                check("settings clears keyboard focus",modal==Modal::Settings&&keyboardFocus==-1);
                RefreshDevices();check("unchanged refresh keeps settings",modal==Modal::Settings);CloseModal();
                OpenEdit(button);draft=presets[2];RefreshDevices();
                check("unchanged refresh preserves unsaved binding",modal==Modal::Edit&&draft==presets[2]);CloseModal();
                const bool ready=engineReady;engineReady=false;OpenEdit(button);Activate(Record);
                check("unavailable hook does not start recording",!recording&&notice.find(L"输入监听不可用")!=std::wstring::npos);engineReady=ready;CloseModal();
                OpenEdit(button);draft=presets[2];Handle(WM_KEYDOWN,VK_RETURN,0);
                check("Enter saves a binding",modal==Modal::None&&CurrentBindings(config.profiles[0])[button]==presets[2]);
                OpenNameDialog(NamePurpose::Scheme);
                LOGFONTW font{};RECT actual{};if(nameEdit)GetWindowRect(nameEdit,&actual);
                const RECT expected=NameEditRect(GetDpiForWindow(window));
                check("name edit uses themed borderless DPI-sized control",nameEdit&&editFont&&GetObjectW(editFont,sizeof(font),&font)&&font.lfHeight==-MulDiv(14,GetDpiForWindow(window),96)&&(actual.right-actual.left)==expected.right-expected.left&&!(GetWindowLongPtrW(nameEdit,GWL_EXSTYLE)&WS_EX_CLIENTEDGE)&&(GetWindowLongPtrW(window,GWL_STYLE)&WS_CLIPCHILDREN));
                const auto savedPalette=graphics.palette;
                for(const bool dark:{false,true}){graphics.palette=dark?DarkPalette():LightPalette();ApplyWindowTheme();HDC dc=GetDC(nameEdit);Handle(WM_CTLCOLOREDIT,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(nameEdit));check(dark?"dark native edit colours":"light native edit colours",editBrush&&GetTextColor(dc)==ToColorRef(graphics.palette.ink)&&GetBkColor(dc)==ToColorRef(graphics.palette.card));ReleaseDC(nameEdit,dc);}
                graphics.palette=savedPalette;ApplyWindowTheme();CloseModal();
                check("name edit releases its font",!nameEdit&&!editFont);
                const auto workingPath=configPath;
                configPath=workingPath.parent_path()/L"blocked-config.json";std::filesystem::create_directories(configPath);
                const auto previousBinding=CurrentBindings(config.profiles[0])[button];
                OpenEdit(button);draft=presets[3];Activate(SaveBinding);
                check("failed save rolls binding back and keeps error",CurrentBindings(config.profiles[0])[button]==previousBinding&&modal==Modal::Edit&&!config.enabled&&notice.find(L"保存失败")!=std::wstring::npos);CloseModal();
                const bool previousNotice=config.showOnConnect;Activate(ConnectNotice);
                check("failed save rolls connection notice back",config.showOnConnect==previousNotice);
                Activate(Enable);check("failed save cannot report mapping restored",!config.enabled&&notice.find(L"保存失败")!=std::wstring::npos);
                configPath=workingPath;canWrite=false;Activate(Enable);
                check("unreadable config cannot report mapping restored",!config.enabled&&notice.find(L"配置读取失败")!=std::wstring::npos);canWrite=true;
            }
            HideQuick();ShowQuick(true);check("automatic quick view leaves Esc alone",quickVisible&&quickAutomatic&&!escapeHotkey);HideQuick();
            ShowQuick();check("manual quick view is untimed",quickVisible&&!quickAutomatic);HideQuick();
            if(devices.size()>1){active=-1;selectionConfirmed=false;config.preferredProfile.clear();RefreshDevices();check("ambiguous device refresh stays paused",active<0&&!selectionConfirmed);}
            const Config savedConfig=config;const int savedSelected=selected;config.profiles.clear();selected=-1;modal=Modal::Settings;
            check("settings renders without any device",graphics.SaveImage(configPath.parent_path()/L"native-empty-settings.png",UINT(W),UINT(H),[&](Painter& p){DrawMain(p);}));
            config=savedConfig;selected=savedSelected;CloseModal();
            smokeResult=ok?0:3;
            std::ofstream report(configPath.parent_path()/L"native-smoke-result.txt");
            report<<(ok?"PASS":"FAIL")<<": native window, hooks, tray, scheme create/rename/edit/switch, independent bindings, persisted selection, isolated config, and clean shutdown.\n";
            report<<"hook_ready="<<engineReady<<" raw_ready="<<rawReady<<" tray_ready="<<trayReady<<" candidates="<<devices.size()<<"\n";
            for(const auto& [name,passed]:regressions)report<<(passed?"PASS: ":"FAIL: ")<<name<<"\n";
            report.close();exiting=true;DestroyWindow(window);return 0;
        }
        if(wp==RefreshTimer){KillTimer(window,RefreshTimer);RefreshDevices();}else if(wp==HighlightTimer){KillTimer(window,HighlightTimer);highlight=-1;InvalidateRect(window,nullptr,FALSE);}else if(wp==NoticeTimer){KillTimer(window,NoticeTimer);notice.clear();InvalidateRect(window,nullptr,FALSE);}else if(wp==QuickTimer)HideQuick();return 0;
    case WM_HOTKEY:if(wp==1)ShowQuick();else if(wp==2)HideQuick();return 0;
    case WM_TRAY:if(LOWORD(lp)==WM_CONTEXTMENU||LOWORD(lp)==WM_RBUTTONUP)TrayMenu();else if(LOWORD(lp)==NIN_SELECT||LOWORD(lp)==NIN_KEYSELECT||LOWORD(lp)==WM_LBUTTONDBLCLK)ShowMain();return 0;
    case WM_SHOW_MAIN:ShowMain();return 0;
    case WM_CLOSE:if(trayReady&&!exiting){CloseModal();ShowWindow(window,SW_HIDE);}else DestroyWindow(window);return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_DESTROY:{HideQuick();if(nameEdit){DestroyWindow(nameEdit);nameEdit=nullptr;}if(editFont){DeleteObject(editFont);editFont=nullptr;}if(editBrush){DeleteObject(editBrush);editBrush=nullptr;}UnregisterHotKey(window,1);input.Stop();NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;Shell_NotifyIconW(NIM_DELETE,&data);if(quickWindow){DestroyWindow(quickWindow);quickWindow=nullptr;}PostQuitMessage(0);return 0;}
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
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;  // keep Direct2D from painting over the name box
    UINT dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(int(W),dpi,96),MulDiv(int(H),dpi,96)};AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);
    window=CreateWindowExW(0,MainClass,L"侧映 · 鼠标快捷键",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,this);
    if(!window){DestroyIcon(icon);return 1;}
    quickWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,QuickClass,L"侧映 · 快捷键速览",WS_POPUP|WS_BORDER,0,0,int(QW),int(QH),window,nullptr,instance,this);
    DWORD corner=2;DwmSetWindowAttribute(window,33,&corner,sizeof(corner));if(quickWindow)DwmSetWindowAttribute(quickWindow,33,&corner,sizeof(corner));
    ApplyWindowTheme();
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");AddTray();
    hotkeyReady=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'M')!=FALSE;
    RAWINPUTDEVICE mouse{0x01,0x02,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window};rawReady=RegisterRawInputDevices(&mouse,1,sizeof(mouse))!=FALSE;
    engineReady=input.Start(window);if(smokeMode){config=Config{};config.enabled=false;canWrite=true;}RefreshDevices(true);
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
    Profile a;a.id=L"demo-a";a.hardwareId=L"demo-a";a.name=L"鼠标 A";a.sideCount=1;CurrentBindings(a)[0]={ModAlt,VK_TAB};
    Profile b;b.id=L"demo-b";b.hardwareId=L"demo-b";b.name=L"鼠标 B";b.sideCount=2;CurrentBindings(b)={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'}};
    CurrentScheme(b).name=L"办公";std::wstring schemeError;AddScheme(b,L"浏览",schemeError);CurrentBindings(b)={Binding{ModCtrl,VK_TAB},Binding{ModCtrl,'W'}};b.activeScheme=0;
    config.profiles={a,b};selected=active=1;Device d;d.id=b.id;d.hardwareId=b.hardwareId;devices={d};
    std::filesystem::create_directories(folder);bool ok=true;
    // Render every screen in both themes so light and dark layouts can be reviewed together.
    for(const bool dark:{false,true}){
        graphics.palette=dark?DarkPalette():LightPalette();const std::wstring prefix=dark?L"native-dark-":L"native-";
        auto save=[&](const wchar_t* name,bool quick){return graphics.SaveImage(folder/(prefix+name+L".png"),UINT(quick?QW:W),UINT(quick?QH:H),[&](Painter& p){if(quick)DrawQuick(p);else DrawMain(p);});};
        config.profiles={a,b};selected=active=1;config.profiles[1].activeScheme=0;modal=Modal::None;hoverAction=0;
        ok=save(L"main",false)&&ok;
        modal=Modal::Edit;editingButton=0;draft=CurrentBindings(config.profiles[1])[0];ok=save(L"editor",false)&&ok;
        recording=true;ok=save(L"editor-recording",false)&&ok;recording=false;
        modal=Modal::Settings;ok=save(L"settings",false)&&ok;
        modal=Modal::None;ok=save(L"quick",true)&&ok;
        config.profiles[1].activeScheme=1;ok=save(L"browsing",false)&&ok;ok=save(L"browsing-quick",true)&&ok;
        selected=0;ok=save(L"one-button",false)&&ok;
        selected=1;config.profiles[1].name=L"我的办公与剪辑专用无线鼠标 🖱";
        CurrentScheme(config.profiles[1]).name=L"浏览器与剪辑软件之间切换时使用的长名称方案";
        CurrentBindings(config.profiles[1])[0]={ModCtrl|ModAlt|ModShift|ModWin,VK_SNAPSHOT};
        ok=save(L"long-names",false)&&ok;ok=save(L"long-names-quick",true)&&ok;
        config.profiles.push_back(a);config.profiles.push_back(a);ok=save(L"many-devices",false)&&ok;
        config.profiles.clear();selected=active=-1;ok=save(L"empty",false)&&ok;
        modal=Modal::Settings;ok=save(L"empty-settings",false)&&ok;
    }
    graphics.RefreshPalette();
    return ok;
}
}
