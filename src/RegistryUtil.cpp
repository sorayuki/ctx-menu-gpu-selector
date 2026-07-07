#include "RegistryUtil.h"
#include <atlbase.h>

const wchar_t* const RegistryUtil::kRegistryPath =
    L"Software\\Microsoft\\DirectX\\UserGpuPreferences";

const std::wregex RegistryUtil::kGpuPreferenceRegex(
    L"GpuPreference\\s*=\\s*([0-9]+)\\s*;",
    std::regex_constants::icase);

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

    std::wcmatch match = {};
    if (!std::regex_search(buffer, match, kGpuPreferenceRegex) || match.size() < 2)
        return true;

    int num = _wtoi(match[1].str().c_str());
    if (num == 0)
        pref = GpuDefault;
    else if (num == 1)
        pref = GpuPowerSaving;
    else if (num == 2)
        pref = GpuHighPerf;
    else
        pref = GpuDefault;

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
    if (pref != GpuDefault && pref != GpuPowerSaving && pref != GpuHighPerf)
        return false;

    // 确保注册表项存在
    ATL::CRegKey key;
    LONG lr = key.Create(HKEY_CURRENT_USER, kRegistryPath);
    if (lr != ERROR_SUCCESS)
        return false;

    wchar_t buffer[256] = {};
    ULONG chars = _countof(buffer);
    lr = key.QueryStringValue(exePath.c_str(), buffer, &chars);

    std::wstring currentValue;
    if (lr == ERROR_SUCCESS)
        currentValue = buffer;

    const std::wstring replacement = L"GpuPreference=" + std::to_wstring(static_cast<int>(pref)) + L";";
    std::wstring updatedValue = std::regex_replace(currentValue, kGpuPreferenceRegex, replacement);
    if (updatedValue == currentValue)
    {
        if (!updatedValue.empty() && updatedValue.back() != L';')
            updatedValue += L';';
        updatedValue += replacement;
    }

    lr = key.SetStringValue(exePath.c_str(), updatedValue.c_str());
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
