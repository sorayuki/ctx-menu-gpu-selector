#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <shlwapi.h>
#include <string>
#include "GpuSelectGuids.h"
#include "GpuSelectCommand.h"

namespace
{
    HMODULE g_moduleHandle = nullptr;
}

// ATL 模块
class CGpuSelectModule : public ATL::CAtlDllModuleT<CGpuSelectModule>
{
};

CGpuSelectModule _AtlModule;

// 模块中的类对象映射
OBJECT_ENTRY_AUTO(CLSID_GpuSelectCommand, CGpuSelectCommand)

// DLL 入口
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
        g_moduleHandle = hInstance;

    return _AtlModule.DllMain(dwReason, lpReserved);
}

// COM 导出
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return _AtlModule.DllCanUnloadNow();
}

// 辅助：写注册表字符串
static HRESULT SetRegString(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, const wchar_t* data)
{
    ATL::CRegKey key;
    LONG lr = key.Create(hKeyRoot, subKey);
    if (lr != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(lr);

    lr = key.SetStringValue(valueName, data);
    return HRESULT_FROM_WIN32(lr);
}

// 辅助：删除注册表树
static HRESULT DeleteRegKey(HKEY hKeyRoot, const wchar_t* subKey)
{
    LONG lr = ::SHDeleteKeyW(hKeyRoot, subKey);
    if (lr == ERROR_FILE_NOT_FOUND || lr == ERROR_SUCCESS)
        return S_OK;
    return HRESULT_FROM_WIN32(lr);
}

// 注册 COM 类和 shell 入口到当前用户
STDAPI DllRegisterServer()
{
    if (!g_moduleHandle)
        return E_UNEXPECTED;

    // 获取本 DLL 路径
    wchar_t dllPath[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_moduleHandle, dllPath, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());

    // 将 CLSID 转为字符串
    LPOLESTR clsidStr = nullptr;
    HRESULT hr = StringFromCLSID(CLSID_GpuSelectCommand, &clsidStr);
    if (FAILED(hr))
        return hr;

    std::wstring guid(clsidStr);
    CoTaskMemFree(clsidStr);

    std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + guid;
    hr = SetRegString(HKEY_CURRENT_USER, clsidKey.c_str(), nullptr, L"GPU Select Shell Extension");
    if (FAILED(hr))
        return hr;

    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    hr = SetRegString(HKEY_CURRENT_USER, inprocKey.c_str(), nullptr, dllPath);
    if (FAILED(hr))
        return hr;

    hr = SetRegString(HKEY_CURRENT_USER, inprocKey.c_str(), L"ThreadingModel", L"Apartment");
    if (FAILED(hr))
        return hr;

    std::wstring verbKey = L"Software\\Classes\\exefile\\shell\\GpuSelect";
    hr = SetRegString(HKEY_CURRENT_USER, verbKey.c_str(), L"MUIVerb", L"绑定显卡");
    if (FAILED(hr))
        return hr;

    hr = SetRegString(HKEY_CURRENT_USER, verbKey.c_str(), L"ExplorerCommandHandler", guid.c_str());
    if (FAILED(hr))
        return hr;

    hr = SetRegString(HKEY_CURRENT_USER, verbKey.c_str(), L"CommandStateSync", L"");
    if (FAILED(hr))
        return hr;

    return S_OK;
}

// 反注册
STDAPI DllUnregisterServer()
{
    LPOLESTR clsidStr = nullptr;
    HRESULT hr = StringFromCLSID(CLSID_GpuSelectCommand, &clsidStr);
    if (SUCCEEDED(hr))
    {
        std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(clsidStr);
        DeleteRegKey(HKEY_CURRENT_USER, clsidKey.c_str());
        CoTaskMemFree(clsidStr);
    }

    DeleteRegKey(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\GpuSelect");
    return S_OK;
}

// 导出通过 .def 文件完成