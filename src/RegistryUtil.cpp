#include "RegistryUtil.h"
#include <atlbase.h>

const wchar_t* const RegistryUtil::kRegistryPath =
    L"Software\\Microsoft\\DirectX\\UserGpuPreferences";

bool RegistryUtil::TryGetPreference(const std::wstring& exePath, GpuPreference& pref)
{
    pref = GpuNotSet;

    ATL::CRegKey key;
    LONG lr = key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
    if (lr != ERROR_SUCCESS)
        return false;

    wchar_t buffer[256] = {};
    ULONG chars = _countof(buffer);
    lr = key.QueryStringValue(exePath.c_str(), buffer, &chars);
    if (lr != ERROR_SUCCESS)
        return false;

    std::wstring value(buffer);
    size_t pos = value.find(L"GpuPreference=");
    if (pos == std::wstring::npos)
        return true;

    pos += 14;
    int num = _wtoi(value.c_str() + pos);
    if (num == 1)
        pref = GpuPowerSaving;
    else if (num == 2)
        pref = GpuHighPerf;

    return true;
}

RegistryUtil::GpuPreference RegistryUtil::GetPreference(const std::wstring& exePath)
{
    GpuPreference pref = GpuNotSet;
    TryGetPreference(exePath, pref);
    return pref;
}

bool RegistryUtil::SetPreference(const std::wstring& exePath, GpuPreference pref)
{
    if (pref != GpuPowerSaving && pref != GpuHighPerf)
        return false;

    // 确保注册表项存在
    ATL::CRegKey key;
    LONG lr = key.Create(HKEY_CURRENT_USER, kRegistryPath);
    if (lr != ERROR_SUCCESS)
        return false;

    wchar_t value[64];
    swprintf_s(value, L"GpuPreference=%d;", static_cast<int>(pref));

    lr = key.SetStringValue(exePath.c_str(), value);
    return (lr == ERROR_SUCCESS);
}

bool RegistryUtil::RemovePreference(const std::wstring& exePath)
{
    ATL::CRegKey key;
    LONG lr = key.Open(HKEY_CURRENT_USER, kRegistryPath, KEY_SET_VALUE);
    if (lr != ERROR_SUCCESS)
        return true; // 项不存在，等同于已清除

    lr = key.DeleteValue(exePath.c_str());
    return (lr == ERROR_SUCCESS || lr == ERROR_FILE_NOT_FOUND);
}
