#include "model.h"
#include "input.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "json.hpp"

using namespace side;
static int checks=0;
static void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv) {
    try {
        std::filesystem::path folder=argc>1?argv[1]:L"test-data";
        std::filesystem::create_directories(folder);
        auto path=folder/L"配置测试.json";
        Config original;Profile a;a.id=L"mouse-a";a.hardwareId=L"vid_0001&pid_0001";a.name=L"单侧键鼠标";a.sideCount=1;CurrentBindings(a)[0]={ModAlt,VK_TAB};
        Profile b;b.id=L"mouse-b";b.hardwareId=L"vid_0002&pid_0002";b.name=L"双侧键鼠标 🖱";b.sideCount=2;CurrentBindings(b)={Binding{ModCtrl,'C'},Binding{ModCtrl,'V'}};
        original.profiles={a,b};std::wstring error;
        Check(SaveConfig(path,original,error),"save Unicode config");Config loaded;
        Check(LoadConfig(path,loaded,error),"load Unicode config");
        Check(loaded.profiles.size()==2&&loaded.profiles[1].name==b.name,"Unicode roundtrip");
        Check(loaded.profiles[0].sideCount==1&&CurrentBindings(loaded.profiles[1])[1]==Binding{ModCtrl,'V'},"independent mappings persist");
        CurrentBindings(loaded.profiles[0])[0]={ModCtrl,'Z'};
        Check(CurrentBindings(loaded.profiles[1])[0]==Binding{ModCtrl,'C'},"profile edits isolated");
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
        Config plans=original;
        auto& mouse=plans.profiles[1];
        Check(CurrentScheme(mouse).name==L"默认方案"&&mouse.activeScheme==0,"new device has a default scheme");
        Check(AddScheme(mouse,L"  办公  ",error)&&CurrentScheme(mouse).name==L"办公","create scheme with trimmed name");
        Check(CurrentBindings(mouse)[0]==Binding{ModCtrl,'C'},"new scheme copies the current bindings");
        CurrentBindings(mouse)[0]={ModCtrl,'Z'};
        mouse.activeScheme=0;
        Check(CurrentBindings(mouse)[0]==Binding{ModCtrl,'C'},"editing one scheme never changes another");
        Check(CurrentBindings(plans.profiles[0])[0]==Binding{ModAlt,VK_TAB},"schemes remain independent across mice");
        Check(!AddScheme(mouse,L"办公",error)&&mouse.schemes.size()==2,"duplicate scheme rejected without mutation");
        Check(!AddScheme(mouse,L"  ",error)&&!AddScheme(mouse,std::wstring(41,L'a'),error),"blank and oversized scheme names rejected");
        Check(AddScheme(mouse,L"Browser",error)&&!AddScheme(mouse,L"browser",error),"case-insensitive duplicate names rejected");
        Check(RenameCurrentScheme(mouse,L"浏览 & 阅读",error),"scheme rename accepts descriptive names");
        Check(!RenameCurrentScheme(mouse,L"办公",error)&&CurrentScheme(mouse).name==L"浏览 & 阅读","duplicate rename preserves the old name");
        mouse.activeScheme=1;
        auto plansPath=folder/L"schemes.json";
        Check(SaveConfig(plansPath,plans,error),"multiple schemes saved");
        Config restored;
        Check(LoadConfig(plansPath,restored,error)&&restored.profiles[1].schemes.size()==3,"multiple schemes restored");
        Check(restored.profiles[1].activeScheme==1&&CurrentBindings(restored.profiles[1])[0]==Binding{ModCtrl,'Z'},"selected scheme and its bindings survive restart");
        Check(restored.profiles[0].activeScheme==0,"different mice retain independent selections");
        Check(DeleteCurrentScheme(mouse,error)&&mouse.schemes.size()==2&&mouse.activeScheme<mouse.schemes.size(),"delete selects a valid remaining scheme");
        Check(DeleteCurrentScheme(mouse,error)&&!DeleteCurrentScheme(mouse,error)&&mouse.schemes.size()==1,"cannot delete the final scheme");
        Profile many=b;
        for(int i=1;i<32;++i)Check(AddScheme(many,L"方案 "+std::to_wstring(i),error),"create up to scheme limit");
        Check(!AddScheme(many,L"超出上限",error)&&many.schemes.size()==32,"scheme count is bounded");
        const std::string legacyText=R"({"version":1,"enabled":true,"profiles":[{"id":"old","hardware_id":"old-device","name":"old mouse","side_count":1,"single_side_index":1,"bindings":[{"modifiers":0,"key":0},{"modifiers":1,"key":67}]}]})";
        auto legacyPath=folder/L"legacy.json";
        {std::ofstream legacy(legacyPath,std::ios::binary|std::ios::trunc);legacy<<legacyText;}
        Config migrated;
        Check(LoadConfig(legacyPath,migrated,error)&&migrated.needsMigrationBackup,"version 1 migration detected");
        Check(CurrentScheme(migrated.profiles[0]).name==L"默认方案"&&CurrentBindings(migrated.profiles[0])[1]==Binding{ModCtrl,'C'},"old mappings migrate unchanged into the default scheme");
        Check(migrated.profiles[0].singleSideIndex==1&&migrated.enabled,"legacy physical button and enable state retained");
        Check(SaveConfig(legacyPath,migrated,error),"upgraded configuration saved");
        auto backupPath=legacyPath;backupPath+=L".v1.bak";
        std::ifstream backup(backupPath,std::ios::binary);std::string backupText((std::istreambuf_iterator<char>(backup)),{});
        Check(backupText==legacyText,"legacy config backup is byte-for-byte identical");
        Check(LoadConfig(legacyPath,migrated,error)&&!migrated.needsMigrationBackup,"upgraded file reads as version 2");
        Check(AddScheme(migrated.profiles[0],L"新方案",error)&&SaveConfig(legacyPath,migrated,error),"migrated device can add schemes");
        std::ifstream backupAgain(backupPath,std::ios::binary);Check(std::string((std::istreambuf_iterator<char>(backupAgain)),{})==legacyText,"subsequent saves do not overwrite migration backup");
        nlohmann::json malformed;
        {std::ifstream saved(plansPath);saved>>malformed;}
        auto invalidPath=folder/L"invalid-schemes.json";
        auto invalid=malformed;invalid["profiles"][1]["active_scheme"]=999;
        {std::ofstream saved(invalidPath);saved<<invalid;}
        Check(!LoadConfig(invalidPath,restored,error),"out-of-range selected scheme is rejected");
        invalid=malformed;invalid["profiles"][1]["schemes"]=nlohmann::json::array();
        {std::ofstream saved(invalidPath);saved<<invalid;}
        Check(!LoadConfig(invalidPath,restored,error),"empty scheme collection is rejected");
        invalid=malformed;invalid["profiles"][1]["schemes"][1]["name"]=invalid["profiles"][1]["schemes"][0]["name"];
        {std::ofstream saved(invalidPath);saved<<invalid;}
        Check(!LoadConfig(invalidPath,restored,error),"duplicate persisted scheme names are rejected");
        Profile routed=b;AddScheme(routed,L"另一套",error);CurrentBindings(routed)[0]={ModCtrl,'Z'};
        MappingLogic switchedLogic;MappingState switchedState;switchedState.enabled=true;switchedState.bindings=EffectiveBindings(routed);
        Check(switchedLogic.Process(0,true,false,false,switchedState).binding==Binding{ModCtrl,'Z'},"selected scheme drives actual mapping decisions");
        routed.activeScheme=0;switchedState.bindings=EffectiveBindings(routed);
        auto stillHeld=switchedLogic.Process(0,true,false,false,switchedState);
        Check(stillHeld.block&&!stillHeld.fire&&switchedLogic.Process(0,false,false,false,switchedState).block,"switching while held keeps the original down-up pair balanced");
        Check(switchedLogic.Process(0,true,false,false,switchedState).binding==Binding{ModCtrl,'C'},"next press uses the newly selected scheme");
        auto blockedPath=folder/L"backup-blocked.json";{std::ofstream file(blockedPath,std::ios::binary);file<<legacyText;}
        auto blockedBackup=blockedPath;blockedBackup+=L".v1.bak";std::filesystem::create_directories(blockedBackup);
        Config blockedConfig;LoadConfig(blockedPath,blockedConfig,error);
        Check(!SaveConfig(blockedPath,blockedConfig,error),"backup failure stops migration");
        std::ifstream untouched(blockedPath,std::ios::binary);Check(std::string((std::istreambuf_iterator<char>(untouched)),{})==legacyText,"backup failure preserves the original legacy file");
        MappingState state;state.enabled=true;state.bindings=CurrentBindings(b);
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
        Profile reversed=a;reversed.singleSideIndex=1;CurrentBindings(reversed)[1]=CurrentBindings(a)[0];
        Check(PhysicalButton(reversed,0)==1,"single side button may report XButton2");
        auto effective=EffectiveBindings(reversed);
        Check(!effective[0].key&&effective[1]==CurrentBindings(a)[0],"only the learned physical side button is mapped");
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
