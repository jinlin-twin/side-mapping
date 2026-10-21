#include "input.h"
#include <algorithm>

namespace side {
InputEngine* InputEngine::instance=nullptr;
MouseDecision MappingLogic::Process(int index,bool down,bool injected,bool ownWindow,const MappingState& state) {
    if(index<0||index>=2||injected)return {};
    if(!down){bool blocked=suppressed[index];suppressed[index]=false;return {blocked,false,{}};}
    if(suppressed[index])return {true,false,{}};
    if(ownWindow||!state.enabled||index>=state.sideCount||!state.bindings[index].key)return {};
    suppressed[index]=true;
    return {true,true,state.bindings[index]};
}
std::vector<INPUT> BuildShortcutInput(const Binding& b,unsigned held,bool keyDown) {
    std::vector<INPUT> inputs;
    if(!b.key||!ValidBinding(b)||keyDown)return inputs;
    auto append=[&](unsigned key,bool up){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=WORD(key);i.ki.dwFlags=up?KEYEVENTF_KEYUP:0;if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_INSERT||key==VK_DELETE||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||key==VK_DIVIDE||key==VK_NUMLOCK||key==VK_SNAPSHOT||key==VK_LWIN||key==VK_RWIN)i.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;i.ki.wScan=WORD(MapVirtualKeyW(key,MAPVK_VK_TO_VSC));i.ki.dwExtraInfo=0x53494445;inputs.push_back(i);};
    const std::array<std::pair<unsigned,unsigned>,4> mods={{{ModCtrl,VK_CONTROL},{ModAlt,VK_MENU},{ModShift,VK_SHIFT},{ModWin,VK_LWIN}}};
    for(auto [flag,key]:mods)if((b.modifiers&flag)&&!(held&flag))append(key,false);
    append(b.key,false);append(b.key,true);
    for(auto it=mods.rbegin();it!=mods.rend();++it)if((b.modifiers&it->first)&&!(held&it->first))append(it->second,true);
    return inputs;
}
static unsigned HeldModifiers() {
    unsigned result=0;
    if(GetAsyncKeyState(VK_CONTROL)&0x8000)result|=ModCtrl;if(GetAsyncKeyState(VK_MENU)&0x8000)result|=ModAlt;
    if(GetAsyncKeyState(VK_SHIFT)&0x8000)result|=ModShift;if((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)result|=ModWin;
    return result;
}
static unsigned ModifierFlag(unsigned key){switch(key){case VK_CONTROL:case VK_LCONTROL:case VK_RCONTROL:return ModCtrl;case VK_MENU:case VK_LMENU:case VK_RMENU:return ModAlt;case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:return ModShift;case VK_LWIN:case VK_RWIN:return ModWin;default:return 0;}}
bool InputEngine::Start(HWND window) {
    owner=window;ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!ready)return false;
    thread=std::thread([this]{Run();});
    WaitForSingleObject(ready,INFINITE);CloseHandle(ready);ready=nullptr;
    return running;
}
InputEngine::~InputEngine(){Stop();}
void InputEngine::Stop(){recording=false;{std::lock_guard lock(mutex);state.enabled=false;++state.generation;pending.clear();}DWORD id=threadId.load();if(id)PostThreadMessageW(id,WM_QUIT,0,0);if(thread.joinable())thread.join();}
void InputEngine::Update(const MappingState& next){std::lock_guard lock(mutex);state=next;}
void InputEngine::BeginRecording(){PostThreadMessageW(threadId,WM_APP+30,1,0);}
void InputEngine::CancelRecording(){recording=false;}
void InputEngine::Run() {
    instance=this;
    MSG msg{};PeekMessageW(&msg,nullptr,WM_USER,WM_USER,PM_NOREMOVE);threadId=GetCurrentThreadId();
    mouseHook=SetWindowsHookExW(WH_MOUSE_LL,MouseHook,GetModuleHandleW(nullptr),0);
    keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,KeyboardHook,GetModuleHandleW(nullptr),0);
    running=mouseHook&&keyboardHook;
    SetEvent(ready);
    if(running)while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_APP+30){recordMods=HeldModifiers();for(unsigned key:capturedKeys)recordMods|=ModifierFlag(key);recording=true;}else if(msg.message==WM_APP+31)DeliverPending();else{TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(mouseHook)UnhookWindowsHookEx(mouseHook);if(keyboardHook)UnhookWindowsHookEx(keyboardHook);
    mouseHook=nullptr;keyboardHook=nullptr;running=false;threadId=0;instance=nullptr;
}
LRESULT CALLBACK InputEngine::MouseHook(int code,WPARAM message,LPARAM data) {
    auto* self=instance;
    if(code==HC_ACTION&&self&&(message==WM_XBUTTONDOWN||message==WM_XBUTTONUP)) {
        auto* event=reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        int index=HIWORD(event->mouseData)==XBUTTON1?0:1;
        HWND foreground=GetForegroundWindow();
        bool own=foreground==self->owner||GetWindow(foreground,GW_OWNER)==self->owner||self->recording;
        std::lock_guard lock(self->mutex);
        auto decision=self->logic.Process(index,message==WM_XBUTTONDOWN,(event->flags&LLMHF_INJECTED)!=0,own,self->state);
        if(decision.fire){self->pending.push_back({decision.binding,foreground,self->state.generation});PostThreadMessageW(self->threadId,WM_APP+31,0,0);}
        if(decision.block)return 1;
    }
    return CallNextHookEx(nullptr,code,message,data);
}
void InputEngine::DeliverPending() {
    std::deque<Pending> jobs;
    {std::lock_guard lock(mutex);jobs.swap(pending);}
    for(const auto& job:jobs) {
        {std::lock_guard lock(mutex);if(!state.enabled||job.generation!=state.generation)continue;}
        if(job.foreground!=GetForegroundWindow())continue;
        auto inputs=BuildShortcutInput(job.binding,HeldModifiers(),(GetAsyncKeyState(job.binding.key)&0x8000)!=0);
        if(inputs.empty())continue;
        UINT sent=SendInput(UINT(inputs.size()),inputs.data(),sizeof(INPUT));
        if(sent!=inputs.size()){
            // Release only keys successfully introduced by this batch; never release a user's modifier.
            std::set<WORD> pressed;
            for(UINT i=0;i<sent;++i){if(inputs[i].ki.dwFlags&KEYEVENTF_KEYUP)pressed.erase(inputs[i].ki.wVk);else pressed.insert(inputs[i].ki.wVk);}
            for(WORD key:pressed){INPUT up{};up.type=INPUT_KEYBOARD;up.ki.wVk=key;up.ki.dwFlags=KEYEVENTF_KEYUP;up.ki.dwExtraInfo=0x53494445;SendInput(1,&up,sizeof(up));}
            PostMessageW(owner,WM_INPUT_ERROR,0,0);
        }
    }
}
LRESULT CALLBACK InputEngine::KeyboardHook(int code,WPARAM message,LPARAM data) {
    auto* self=instance;
    if(code==HC_ACTION&&self) {
        const auto* event=reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        unsigned key=event->vkCode;
        bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
        bool up=message==WM_KEYUP||message==WM_SYSKEYUP;
        if(event->flags&LLKHF_INJECTED)return CallNextHookEx(nullptr,code,message,data);
        if(up&&self->capturedKeys.erase(key)){unsigned flag=ModifierFlag(key);if(flag)self->recordMods&=~flag;return 1;}
        if(down&&self->capturedKeys.count(key))return 1;
        if(self->recording&&GetForegroundWindow()!=self->owner)self->recording=false;
        if(self->recording) {
            unsigned flag=ModifierFlag(key);
            if(flag){if(down){self->recordMods|=flag;self->capturedKeys.insert(key);return 1;}if(up)self->recordMods&=~flag;return CallNextHookEx(nullptr,code,message,data);}
            if(down){self->capturedKeys.insert(key);self->recording=false;if(key==VK_ESCAPE)PostMessageW(self->owner,WM_CAPTURE_CANCEL,0,0);else PostMessageW(self->owner,WM_CAPTURED,key,self->recordMods);return 1;}
        }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
}
