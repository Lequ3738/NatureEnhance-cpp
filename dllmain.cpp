// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "Gmapi.h"
#include "NatureEnhance/ErrorTraceback.h"

gm::CGMAPI* gmapi = nullptr;

BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            // 应对 vs 内联 sprintf 导致 d3d8.lib 找不到 _sprintf 的问题
            char f__kInline[1];
            sprintf(f__kInline, "");

            DWORD result = 0;
            gmapi = gm::CGMAPI::Create(&result);
            // Check the initialization
            if (result == gm::GMAPI_INITIALIZATION_FAILED)
            {
                MessageBox(NULL, L"Unable to initialize GMAPI.", NULL, MB_SYSTEMMODAL | MB_ICONERROR);
                return FALSE;
            }

            // GM8.0 报错信息增强（签名不匹配的 runner 上自动禁用）
            errtrace::Install();
        }
        break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            gm::CGMAPI::Destroy();
            break;
    }

    return TRUE;
}

