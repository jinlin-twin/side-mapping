#include "app.h"
#include <fstream>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    int count=0;LPWSTR* arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    bool background=false;std::filesystem::path preview,smoke;
    for(int i=1;i<count;++i){if(wcscmp(arguments[i],L"--background")==0)background=true;else if(wcscmp(arguments[i],L"--render-preview")==0&&i+1<count)preview=arguments[++i];else if(wcscmp(arguments[i],L"--smoke-test")==0&&i+1<count)smoke=arguments[++i];}
    LocalFree(arguments);
    int result=0;
    {
        side::App app(executable,background);
        if(!preview.empty())result=app.RenderPreviews(preview)?0:2;
        else if(!smoke.empty())result=app.Run(instance,smoke);
        else {
            HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\SideMapping.Native.Singleton");
            if(mutex&&GetLastError()==ERROR_ALREADY_EXISTS){HWND existing=FindWindowW(L"SideMapping.Native.Main",nullptr);if(existing){AllowSetForegroundWindow(ASFW_ANY);PostMessageW(existing,side::WM_SHOW_MAIN,0,0);}}
            else result=app.Run(instance);
            if(mutex)CloseHandle(mutex);
        }
    }
    CoUninitialize();return result;
}
