#include "model.h"
#include "json.hpp"
#include <hidsdi.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>

namespace side {
using nlohmann::json;
std::string Utf8(const std::wstring& text) {
    if(text.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    if(!n) return {};
    std::string result(n,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),n,nullptr,nullptr);
    return result;
}
std::wstring Wide(const std::string& text) {
    if(text.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
    if(!n) return {};
    std::wstring result(n,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),n);
    return result;
}
static bool IsModifier(unsigned key) {
    return key==VK_CONTROL||key==VK_LCONTROL||key==VK_RCONTROL||key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT||key==VK_MENU||key==VK_LMENU||key==VK_RMENU||key==VK_LWIN||key==VK_RWIN;
}
bool ValidBinding(const Binding& b) {
    return b.modifiers<=15 && b.key<=254 && (b.key==0 ? b.modifiers==0 : (b.key>=VK_BACK && !IsModifier(b.key) && b.key!=VK_PACKET && b.key!=VK_PROCESSKEY));
}
int PhysicalButton(const Profile& profile,int displayedIndex){return profile.sideCount==1?profile.singleSideIndex:displayedIndex;}
MappingScheme& CurrentScheme(Profile& p){return p.schemes.at(p.activeScheme);}
const MappingScheme& CurrentScheme(const Profile& p){return p.schemes.at(p.activeScheme);}
std::array<Binding,2>& CurrentBindings(Profile& p){return CurrentScheme(p).bindings;}
const std::array<Binding,2>& CurrentBindings(const Profile& p){return CurrentScheme(p).bindings;}
std::array<Binding,2> EffectiveBindings(const Profile& profile){auto bindings=CurrentBindings(profile);if(profile.sideCount==1)bindings[1-profile.singleSideIndex]={};return bindings;}
static std::wstring TrimName(const std::wstring& name){auto start=name.find_first_not_of(L" \t\r\n"),end=name.find_last_not_of(L" \t\r\n");return start==std::wstring::npos?L"":name.substr(start,end-start+1);}
static bool CheckSchemeName(const Profile& p,const std::wstring& name,size_t except,std::wstring& error){
    if(name.empty()||name.size()>40){error=L"方案名称需要 1 至 40 个字符。";return false;}
    if(name.find_first_of(L"\r\n\t")!=std::wstring::npos){error=L"方案名称不能包含换行或制表符。";return false;}
    for(size_t i=0;i<p.schemes.size();++i)if(i!=except&&CompareStringOrdinal(name.c_str(),-1,p.schemes[i].name.c_str(),-1,TRUE)==CSTR_EQUAL){error=L"已经有同名方案，请换一个名称。";return false;}
    return true;
}
bool AddScheme(Profile& p,const std::wstring& name,std::wstring& error){error.clear();if(p.schemes.size()>=32){error=L"每只鼠标最多保存 32 套方案。";return false;}auto trimmed=TrimName(name);if(!CheckSchemeName(p,trimmed,size_t(-1),error))return false;MappingScheme added{trimmed,CurrentBindings(p)};p.schemes.push_back(std::move(added));p.activeScheme=p.schemes.size()-1;return true;}
bool RenameCurrentScheme(Profile& p,const std::wstring& name,std::wstring& error){error.clear();auto trimmed=TrimName(name);if(!CheckSchemeName(p,trimmed,p.activeScheme,error))return false;CurrentScheme(p).name=trimmed;return true;}
bool DeleteCurrentScheme(Profile& p,std::wstring& error){error.clear();if(p.schemes.size()<=1){error=L"至少需要保留一套方案。";return false;}p.schemes.erase(p.schemes.begin()+p.activeScheme);p.activeScheme=std::min(p.activeScheme,p.schemes.size()-1);return true;}
std::wstring KeyName(unsigned key) {
    if(key==0) return L"原始功能";
    switch(key) {
    case VK_CONTROL:return L"Ctrl";case VK_MENU:return L"Alt";case VK_SHIFT:return L"Shift";
    case VK_LWIN:case VK_RWIN:return L"Win";case VK_RETURN:return L"Enter";case VK_ESCAPE:return L"Esc";
    case VK_BACK:return L"Backspace";case VK_SPACE:return L"Space";case VK_DELETE:return L"Delete";
    case VK_TAB:return L"Tab";case VK_LEFT:return L"←";case VK_RIGHT:return L"→";case VK_UP:return L"↑";case VK_DOWN:return L"↓";
    }
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9')) return std::wstring(1,wchar_t(key));
    UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);
    if(key==VK_INSERT||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||key==VK_NUMLOCK||key==VK_DIVIDE) scan|=0x100;
    wchar_t buffer[80]{};
    if(GetKeyNameTextW(LONG(scan<<16),buffer,80)) return buffer;
    return L"Key "+std::to_wstring(key);
}
std::wstring ShortcutText(const Binding& b) {
    if(!b.key) return L"保留鼠标原功能";
    std::wstring result;
    if(b.modifiers&ModCtrl)result+=L"Ctrl + ";if(b.modifiers&ModAlt)result+=L"Alt + ";
    if(b.modifiers&ModShift)result+=L"Shift + ";if(b.modifiers&ModWin)result+=L"Win + ";
    return result+KeyName(b.key);
}
std::wstring ActionName(const Binding& b) {
    if(!b.key)return L"原始功能";
    if(b.modifiers==ModCtrl) {
        switch(b.key){case 'C':return L"复制";case 'V':return L"粘贴";case 'Z':return L"撤销";case 'X':return L"剪切";case 'S':return L"保存";case 'A':return L"全选";case 'F':return L"查找";}
    }
    if(b.modifiers==ModAlt&&b.key==VK_TAB)return L"切换窗口";
    if(b.modifiers==ModWin&&b.key=='D')return L"显示桌面";
    return L"自定义快捷键";
}
bool LoadConfig(const std::filesystem::path& path, Config& config, std::wstring& error) {
    error.clear();
    try {
        if(!std::filesystem::exists(path))return true;
        if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("config too large");
        std::ifstream stream(path,std::ios::binary);
        if(!stream)throw std::runtime_error("cannot read config");
        json root=json::parse(stream);
        int version=root.is_object()?root.value("version",0):0;
        if(version!=1&&version!=2)throw std::runtime_error("unsupported config version");
        Config loaded;
        loaded.needsMigrationBackup=version==1;
        loaded.enabled=root.value("enabled",true);
        loaded.showOnConnect=root.value("show_on_connect",true);
        loaded.preferredProfile=Wide(root.value("preferred_profile",std::string{}));
        if(!root.contains("profiles")||!root["profiles"].is_array()||root["profiles"].size()>100)throw std::runtime_error("invalid profiles");
        std::set<std::wstring> ids;
        for(const auto& item:root["profiles"]) {
            Profile profile;
            profile.id=Wide(item.at("id").get<std::string>());
            profile.hardwareId=Wide(item.at("hardware_id").get<std::string>());
            profile.name=Wide(item.at("name").get<std::string>());
            profile.sideCount=item.at("side_count").get<int>();
            profile.singleSideIndex=item.value("single_side_index",0);
            if(profile.id.empty()||profile.id.size()>2048||profile.hardwareId.empty()||profile.hardwareId.size()>2048||profile.name.empty()||profile.name.size()>80||profile.sideCount<1||profile.sideCount>2||profile.singleSideIndex<0||profile.singleSideIndex>1||!ids.insert(profile.id).second)throw std::runtime_error("invalid device profile");
            auto readBindings=[](const json& bindings){std::array<Binding,2> result{};if(!bindings.is_array()||bindings.size()!=2)throw std::runtime_error("invalid bindings");for(int i=0;i<2;++i){result[i]={bindings[i].at("modifiers").get<unsigned>(),bindings[i].at("key").get<unsigned>()};if(!ValidBinding(result[i]))throw std::runtime_error("invalid shortcut");}return result;};
            if(version==1)CurrentBindings(profile)=readBindings(item.at("bindings"));
            else {
                const auto& schemes=item.at("schemes");
                if(!schemes.is_array()||schemes.empty()||schemes.size()>32)throw std::runtime_error("invalid schemes");
                profile.schemes.clear();
                for(const auto& entry:schemes){auto name=Wide(entry.at("name").get<std::string>());std::wstring reason;if(name!=TrimName(name)||!CheckSchemeName(profile,name,size_t(-1),reason))throw std::runtime_error("invalid scheme name");profile.schemes.push_back({name,readBindings(entry.at("bindings"))});}
                profile.activeScheme=item.at("active_scheme").get<size_t>();
                if(profile.activeScheme>=profile.schemes.size())throw std::runtime_error("invalid active scheme");
            }
            loaded.profiles.push_back(std::move(profile));
        }
        config=std::move(loaded);
        return true;
    } catch(const std::exception& e) { error=L"配置读取失败："+Wide(e.what())+L"。原文件未被覆盖。";return false; }
}
bool SaveConfig(const std::filesystem::path& path,const Config& config,std::wstring& error) {
    error.clear();
    std::filesystem::path temporary=path;temporary+=L".tmp";
    try {
        json root={{"version",2},{"enabled",config.enabled},{"show_on_connect",config.showOnConnect},{"preferred_profile",Utf8(config.preferredProfile)},{"profiles",json::array()}};
        for(const auto& p:config.profiles) {
            if(p.schemes.empty()||p.schemes.size()>32||p.activeScheme>=p.schemes.size())throw std::runtime_error("invalid schemes");
            json schemes=json::array();
            for(size_t i=0;i<p.schemes.size();++i){const auto& scheme=p.schemes[i];std::wstring reason;if(scheme.name!=TrimName(scheme.name)||!CheckSchemeName(p,scheme.name,i,reason))throw std::runtime_error("invalid scheme name");json bindings=json::array();for(const auto& b:scheme.bindings){if(!ValidBinding(b))throw std::runtime_error("invalid shortcut");bindings.push_back({{"modifiers",b.modifiers},{"key",b.key}});}schemes.push_back({{"name",Utf8(scheme.name)},{"bindings",bindings}});}
            root["profiles"].push_back({{"id",Utf8(p.id)},{"hardware_id",Utf8(p.hardwareId)},{"name",Utf8(p.name)},{"side_count",p.sideCount},{"single_side_index",p.singleSideIndex},{"active_scheme",p.activeScheme},{"schemes",schemes}});
        }
        if(config.needsMigrationBackup&&std::filesystem::exists(path)){auto backup=path;backup+=L".v1.bak";if(!CopyFileW(path.c_str(),backup.c_str(),TRUE)&&GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("cannot back up legacy config");}
        std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
        if(!stream)throw std::runtime_error("cannot create temporary config");
        stream<<root.dump(2)<<'\n';stream.flush();
        if(!stream)throw std::runtime_error("config write failed");
        stream.close();
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cannot replace config");
        return true;
    }catch(const std::exception& e){error=L"配置保存失败："+Wide(e.what());return false;}
}
static std::wstring Lower(std::wstring text){std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c){return std::towlower(c);});return text;}
std::wstring HardwareIdentity(const std::wstring& path,const std::wstring& product) {
    const auto lower=Lower(path);
    auto vendor=lower.find(L"vid_");auto pid=lower.find(L"pid_");
    if(vendor!=std::wstring::npos&&pid!=std::wstring::npos&&vendor+8<=lower.size()&&pid+8<=lower.size())
        return lower.substr(vendor,8)+L"&"+lower.substr(pid,8)+L"|"+Lower(product);
    // Unknown devices use the complete path, never an empty/shared fallback identity.
    return lower;
}
std::vector<Device> EnumerateMice() {
    UINT count=0;
    if(GetRawInputDeviceList(nullptr,&count,sizeof(RAWINPUTDEVICELIST))!=0)return {};
    std::vector<RAWINPUTDEVICELIST> list(count);
    UINT actual=GetRawInputDeviceList(list.data(),&count,sizeof(RAWINPUTDEVICELIST));
    if(actual==UINT(-1))return {};
    std::vector<Device> devices;
    for(UINT i=0;i<actual;++i) {
        if(list[i].dwType!=RIM_TYPEMOUSE)continue;
        UINT length=0;
        if(GetRawInputDeviceInfoW(list[i].hDevice,RIDI_DEVICENAME,nullptr,&length)==UINT(-1)||!length)continue;
        std::wstring path(length+1,L'\0');
        if(GetRawInputDeviceInfoW(list[i].hDevice,RIDI_DEVICENAME,path.data(),&length)==UINT(-1))continue;
        path.resize(wcslen(path.c_str()));
        RID_DEVICE_INFO info{};info.cbSize=sizeof(info);UINT bytes=sizeof(info);
        GetRawInputDeviceInfoW(list[i].hDevice,RIDI_DEVICEINFO,&info,&bytes);
        std::wstring product;
        HANDLE file=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file!=INVALID_HANDLE_VALUE){wchar_t name[128]{};if(HidD_GetProductString(file,name,sizeof(name)))product=name;CloseHandle(file);}
        Device device;
        device.handle=list[i].hDevice;device.path=path;device.id=Lower(path);
        device.hardwareId=HardwareIdentity(path,product);device.name=product.empty()?L"鼠标":product;
        device.reportedButtons=info.mouse.dwNumberOfButtons;
        devices.push_back(std::move(device));
    }
    return devices;
}
int FindProfile(const Config& config,const Device& device) {
    for(size_t i=0;i<config.profiles.size();++i)if(config.profiles[i].id==device.id)return int(i);
    int result=-1;
    for(size_t i=0;i<config.profiles.size();++i)if(config.profiles[i].hardwareId==device.hardwareId){if(result!=-1)return -1;result=int(i);}
    return result;
}
bool IsMouseCandidate(const Device& device) {
    const auto path=Lower(device.path);
    if(path.find(L"hid_device_system_vhf")!=std::wstring::npos||path.find(L"rdp_mou")!=std::wstring::npos)return false;
    return device.reportedButtons==0||device.reportedButtons>=4;
}
bool IsStartupEnabled() {
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;
    LONG result=RegQueryValueExW(key,L"SideMapping",nullptr,nullptr,nullptr,nullptr);RegCloseKey(key);return result==ERROR_SUCCESS;
}
bool SetStartupEnabled(bool enabled,const std::wstring& executable,std::wstring& error) {
    HKEY key=nullptr;
    LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(result==ERROR_SUCCESS) {
        const std::wstring command=L"\""+executable+L"\" --background";
        result=enabled?RegSetValueExW(key,L"SideMapping",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"SideMapping");
        RegCloseKey(key);
    }
    if(result==ERROR_SUCCESS||(!enabled&&result==ERROR_FILE_NOT_FOUND))return true;
    error=L"无法修改开机启动设置（错误 "+std::to_wstring(result)+L"）。";return false;
}
}
