#pragma once
#include "model.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <set>

namespace side {
constexpr UINT WM_CAPTURED=WM_APP+10,WM_CAPTURE_CANCEL=WM_APP+11,WM_INPUT_ERROR=WM_APP+12;
struct MappingState { bool enabled=false; std::array<Binding,2> bindings{}; int sideCount=2; unsigned generation=0; };
struct MouseDecision { bool block=false; bool fire=false; Binding binding{}; };
class MappingLogic {
    std::array<bool,2> suppressed{};
public:
    MouseDecision Process(int side,bool down,bool injected,bool ownWindow,const MappingState& state);
};
std::vector<INPUT> BuildShortcutInput(const Binding&,unsigned heldModifiers,bool keyAlreadyDown);
class InputEngine {
    HWND owner=nullptr;
    std::thread thread;
    std::mutex mutex;
    MappingState state;
    struct Pending{Binding binding;HWND foreground;unsigned generation;};
    std::deque<Pending> pending;
    std::atomic<DWORD> threadId{0};
    std::atomic<bool> running{false},recording{false};
    HANDLE ready=nullptr;
    HHOOK mouseHook=nullptr,keyboardHook=nullptr;
    MappingLogic logic;
    std::set<unsigned> capturedKeys;
    unsigned recordMods=0;
    static InputEngine* instance;
    static LRESULT CALLBACK MouseHook(int,WPARAM,LPARAM);
    static LRESULT CALLBACK KeyboardHook(int,WPARAM,LPARAM);
    void Run();
    void DeliverPending();
public:
    ~InputEngine();
    bool Start(HWND window);
    void Stop();
    void Update(const MappingState& next);
    void BeginRecording();
    void CancelRecording();
};
}
