#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace side {
constexpr unsigned ModCtrl=1, ModAlt=2, ModShift=4, ModWin=8;
struct Binding {
    unsigned modifiers=0;
    unsigned key=0; // Zero preserves the original mouse action.
    bool operator==(const Binding&) const = default;
};
struct Profile {
    std::wstring id, hardwareId, name;
    int sideCount=2;
    int singleSideIndex=0;
    std::array<Binding,2> bindings{};
};
struct Config {
    bool enabled=true, showOnConnect=true;
    std::wstring preferredProfile;
    std::vector<Profile> profiles;
};
struct Device {
    HANDLE handle=nullptr;
    std::wstring path, id, hardwareId, name;
    unsigned reportedButtons=0;
};
std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);
std::wstring KeyName(unsigned key);
std::wstring ShortcutText(const Binding& binding);
std::wstring ActionName(const Binding& binding);
bool ValidBinding(const Binding& binding);
int PhysicalButton(const Profile&,int displayedIndex);
std::array<Binding,2> EffectiveBindings(const Profile&);
bool LoadConfig(const std::filesystem::path&, Config&, std::wstring& error);
bool SaveConfig(const std::filesystem::path&, const Config&, std::wstring& error);
std::vector<Device> EnumerateMice();
bool IsMouseCandidate(const Device&);
std::wstring HardwareIdentity(const std::wstring& path, const std::wstring& product);
int FindProfile(const Config&, const Device&);
bool IsStartupEnabled();
bool SetStartupEnabled(bool enabled, const std::wstring& executable, std::wstring& error);
}
