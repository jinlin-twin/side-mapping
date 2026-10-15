#include "model.h"
#include "input.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace side;
static int checks=0;
static void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv) {
    try {
        std::filesystem::path folder=argc>1?argv[1]:L"test-data";
        std::filesystem::create_directories(folder);
        auto path=folder/L"配置测试.json";
        Config original;Profile a;a.id=L"mouse-a";a.hardwareId=L"vid_0001&pid_0001";a.name=L"单侧键鼠标";a.sideCount=1;a.bindings[0]={ModAlt,VK_TAB};
        Profile b;b.id=L"mouse-b";b.hardwareId=L"vid_0002&pid_0002";b.name=L"双侧键鼠标 🖱";b.sideCount=2;b.bindings={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'}};
        original.profiles={a,b};std::wstring error;
        Check(SaveConfig(path,original,error),"save Unicode config");Config loaded;
        Check(LoadConfig(path,loaded,error),"load Unicode config");
        Check(loaded.profiles.size()==2&&loaded.profiles[1].name==b.name,"Unicode roundtrip");
        Check(loaded.profiles[0].sideCount==1&&loaded.profiles[1].bindings[1]==Binding{ModCtrl,'V'},"independent mappings persist");
        loaded.profiles[0].bindings[0]={ModCtrl,'Z'};
        Check(loaded.profiles[1].bindings[0]==Binding{ModCtrl,'C'},"profile edits isolated");
        Check(SaveConfig(path,loaded,error)&&LoadConfig(path,loaded,error),"atomic replacement");
        Check(!std::filesystem::exists(path.wstring()+L".tmp"),"temporary file replaced");
        Device d;d.id=a.id;d.hardwareId=a.hardwareId;
        Check(FindProfile(loaded,d)==0,"exact device match");d.id=L"different-usb-port";
        Check(FindProfile(loaded,d)==0,"unique model follows USB port");
        loaded.profiles.push_back(loaded.profiles[0]);loaded.profiles.back().id=L"another-identical-mouse";
        Check(FindProfile(loaded,d)==-1,"ambiguous identity is not guessed");
        Check(HardwareIdentity(L"\\\\?\\HID#VID_046D&PID_C077#port-one",L"USB Mouse")==HardwareIdentity(L"\\\\?\\HID#VID_046D&PID_C077#port-two",L"USB Mouse"),"hardware identity independent of USB port");
        Check(HardwareIdentity(L"unknown-one",L"")!=HardwareIdentity(L"unknown-two",L""),"unknown devices never share an empty identity");
        Check(Wide(Utf8(b.name))==b.name,"UTF-8 conversion");
        Check(ValidBinding({})&&ValidBinding({ModCtrl|ModShift,'K'}),"valid bindings");
        Check(!ValidBinding({32,'C'})&&!ValidBinding({ModCtrl,0})&&!ValidBinding({0,VK_CONTROL})&&!ValidBinding({0,999}),"invalid bindings rejected");
        auto corrupt=folder/L"corrupt.json";{std::ofstream file(corrupt);file<<"{malformed";}
        Config survivor=original;
        Check(!LoadConfig(corrupt,survivor,error)&&!error.empty(),"malformed config reported");
        Check(survivor.profiles.size()==2&&survivor.profiles[0].id==a.id,"failed load preserves memory");
        std::ifstream preserved(corrupt);std::string text((std::istreambuf_iterator<char>(preserved)),{});
        Check(text=="{malformed","failed load never overwrites original");
        auto future=folder/L"future.json";{std::ofstream file(future);file<<"{\"version\":999,\"profiles\":[]}";}
        Check(!LoadConfig(future,survivor,error),"unsupported config version rejected");
        Config missing;Check(LoadConfig(folder/L"missing.json",missing,error),"missing config starts safely");
        Check(missing.profiles.empty(),"missing config has no automatic mappings");
        MappingState state;state.enabled=true;state.bindings=b.bindings;
        MappingLogic logic;
        auto down=logic.Process(0,true,false,false,state);
        Check(down.block&&down.fire&&down.binding==Binding{ModCtrl,'C'},"mapped down swallowed and triggers correct shortcut");
        auto repeat=logic.Process(0,true,false,false,state);
        Check(repeat.block&&!repeat.fire,"repeat down never repeats a shortcut");
        auto up=logic.Process(0,false,false,false,state);
        Check(up.block&&!up.fire,"matching up swallowed");
        Check(!logic.Process(0,false,false,false,state).block,"orphan up passes through");
        Check(!logic.Process(0,true,true,false,state).block,"injected mouse input ignored");
        Check(!logic.Process(0,true,false,true,state).block,"settings window never triggers mappings");
        state.enabled=false;Check(!logic.Process(1,true,false,false,state).block,"pause preserves original function");
        state.enabled=true;state.sideCount=1;Check(!logic.Process(1,true,false,false,state).block,"absent side button never mapped");
        state.sideCount=2;state.bindings[1]={};Check(!logic.Process(1,true,false,false,state).block,"passthrough binding untouched");
        state.bindings[1]={ModCtrl,'V'};Check(logic.Process(1,true,false,false,state).block,"second button down swallowed");
        state.enabled=false;Check(logic.Process(1,false,false,false,state).block,"release remains swallowed after pause or unplug");
        state.enabled=true;logic.Process(0,true,false,false,state);Check(!logic.Process(0,false,true,false,state).block,"injected up cannot release physical state");
        Check(logic.Process(0,false,false,false,state).block,"real up still paired after injected event");
        auto batch=BuildShortcutInput({ModCtrl,'C'},0,false);
        Check(batch.size()==4&&batch.front().ki.wVk==VK_CONTROL&&batch.back().ki.wVk==VK_CONTROL,"shortcut modifiers bracket main key");
        Check(!(batch[0].ki.dwFlags&KEYEVENTF_KEYUP)&&(batch[3].ki.dwFlags&KEYEVENTF_KEYUP),"modifier down/up balanced");
        auto held=BuildShortcutInput({ModCtrl,'C'},ModCtrl,false);
        Check(held.size()==2&&held[0].ki.wVk=='C'&&held[1].ki.wVk=='C',"physically held modifiers are not released");
        Check(BuildShortcutInput({ModCtrl,'C'},0,true).empty(),"physically held target key untouched");
        Check(BuildShortcutInput({},0,false).empty(),"original action produces no keyboard events");
        Profile reversed=a;reversed.singleSideIndex=1;reversed.bindings[1]=a.bindings[0];
        Check(PhysicalButton(reversed,0)==1,"single side button may report XButton2");
        auto effective=EffectiveBindings(reversed);
        Check(!effective[0].key&&effective[1]==a.bindings[0],"only the learned physical side button is mapped");
        Check(BuildShortcutInput({ModCtrl|ModAlt|ModShift|ModWin,'K'},0,false).size()==10,"four modifiers produce balanced input");
        auto arrow=BuildShortcutInput({ModCtrl,VK_RIGHT},0,false);
        Check((arrow[1].ki.dwFlags&KEYEVENTF_EXTENDEDKEY)!=0&&(arrow[2].ki.dwFlags&KEYEVENTF_EXTENDEDKEY)!=0,"navigation keys retain extended-key flags");
        Check(ActionName({ModCtrl,'C'})==L"复制"&&ShortcutText({ModCtrl|ModShift,'K'})==L"Ctrl + Shift + K","human-readable shortcut labels");
        auto mice=EnumerateMice();
        Device virtualDevice;virtualDevice.path=L"HID_DEVICE_SYSTEM_VHF&COL04";virtualDevice.reportedButtons=5;
        Check(!IsMouseCandidate(virtualDevice),"system virtual mice excluded from automatic selection");
        virtualDevice.path=L"touchpad";virtualDevice.reportedButtons=2;Check(!IsMouseCandidate(virtualDevice),"two-button touchpads excluded");
        virtualDevice.path=L"HID#VID_046D&PID_C548";virtualDevice.reportedButtons=16;Check(IsMouseCandidate(virtualDevice),"physical receiver is a candidate");
        std::cout<<"PASS: "<<checks<<" checks\n";
        std::cout<<"Detected mouse input sources: "<<mice.size()<<"\n";
        for(const auto& mouse:mice)std::cout<<"  "<<Utf8(mouse.name)<<" | "<<Utf8(mouse.hardwareId)<<" | buttons="<<mouse.reportedButtons<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
