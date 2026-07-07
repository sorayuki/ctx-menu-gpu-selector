#include "SubCommand.h"
#include "RegistryUtil.h"
#include <atlbase.h>
#include <atlcom.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace
{
    bool GetItemPath(IShellItemArray* itemArray, DWORD index, std::wstring& path)
    {
        path.clear();
        if (!itemArray)
            return false;

        ATL::CComPtr<IShellItem> item;
        if (FAILED(itemArray->GetItemAt(index, &item)))
            return false;

        LPWSTR rawPath = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)))
            return false;

        path = rawPath;
        CoTaskMemFree(rawPath);
        return true;
    }

    bool GetSelectionPreference(
        IShellItemArray* itemArray,
        RegistryUtil::GpuPreference& selectionPreference,
        bool& hasPreferenceEntry)
    {
        selectionPreference = RegistryUtil::GpuNotSet;
        hasPreferenceEntry = false;
        if (!itemArray)
            return false;

        DWORD count = 0;
        if (FAILED(itemArray->GetCount(&count)) || count == 0)
            return false;

        std::wstring filePath;
        if (!GetItemPath(itemArray, 0, filePath))
            return false;

        hasPreferenceEntry = RegistryUtil::TryGetPreference(filePath, selectionPreference);

        for (DWORD index = 1; index < count; ++index)
        {
            if (!GetItemPath(itemArray, index, filePath))
                return false;

            RegistryUtil::GpuPreference currentPreference = RegistryUtil::GpuNotSet;
            bool currentHasPreferenceEntry = RegistryUtil::TryGetPreference(filePath, currentPreference);
            if (currentHasPreferenceEntry != hasPreferenceEntry || currentPreference != selectionPreference)
                return false;
        }

        return true;
    }

    bool IsCommandSelected(SubCommandType type, RegistryUtil::GpuPreference preference, bool hasPreferenceEntry)
    {
        switch (type)
        {
        case SubCommandType::Clear:
            return hasPreferenceEntry && preference == RegistryUtil::GpuNotSet;
        case SubCommandType::Default:
            return preference == RegistryUtil::GpuDefault;
        case SubCommandType::PowerSaving:
            return preference == RegistryUtil::GpuPowerSaving;
        case SubCommandType::HighPerf:
            return preference == RegistryUtil::GpuHighPerf;
        }

        return false;
    }
}

// 从 IShellItemArray 提取第一个文件路径
HRESULT CSubCommand::GetFirstFilePath(IShellItemArray* psiItemArray, std::wstring& path)
{
    path.clear();
    if (!psiItemArray)
        return E_FAIL;

    DWORD count = 0;
    HRESULT hr = psiItemArray->GetCount(&count);
    if (FAILED(hr) || count == 0)
        return E_FAIL;

    ATL::CComPtr<IShellItem> spItem;
    hr = psiItemArray->GetItemAt(0, &spItem);
    if (FAILED(hr))
        return hr;

    LPWSTR pszPath = nullptr;
    hr = spItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (FAILED(hr))
        return hr;

    path = pszPath;
    CoTaskMemFree(pszPath);
    return S_OK;
}

// 工厂方法
HRESULT CSubCommand::Create(SubCommandType type, IExplorerCommand** ppCommand)
{
    if (!ppCommand)
        return E_POINTER;
    *ppCommand = nullptr;

    ATL::CComObject<CSubCommand>* pObj = nullptr;
    HRESULT hr = ATL::CComObject<CSubCommand>::CreateInstance(&pObj);
    if (FAILED(hr))
        return hr;

    pObj->m_type = type;
    return pObj->QueryInterface(IID_IExplorerCommand, reinterpret_cast<void**>(ppCommand));
}

// IExplorerCommand 实现

STDMETHODIMP CSubCommand::GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName)
{
    if (!ppszName)
        return E_POINTER;

    const wchar_t* title = L"";
    switch (m_type)
    {
    case SubCommandType::Clear:       title = L"清除设置(&C)";  break;
    case SubCommandType::Default:     title = L"默认(&D)";      break;
    case SubCommandType::PowerSaving: title = L"节能(&S)";      break;
    case SubCommandType::HighPerf:    title = L"高性能(&H)";    break;
    }

    RegistryUtil::GpuPreference selectionPreference = RegistryUtil::GpuNotSet;
    bool hasPreferenceEntry = false;
    if (!GetSelectionPreference(psiItemArray, selectionPreference, hasPreferenceEntry) || !hasPreferenceEntry)
        return SHStrDup(title, ppszName);

    if (IsCommandSelected(m_type, selectionPreference, hasPreferenceEntry))
    {
        std::wstring markedTitle = std::wstring(L"〇 ") + title;
        return SHStrDup(markedTitle.c_str(), ppszName);
    }

    std::wstring unmarkedTitle = std::wstring(L"　 ") + title;
    return SHStrDup(unmarkedTitle.c_str(), ppszName);
}

STDMETHODIMP CSubCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    *ppszIcon = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP CSubCommand::GetToolTip(IShellItemArray*, LPWSTR* ppszToolTip)
{
    *ppszToolTip = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP CSubCommand::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = GUID_NULL;
    return E_NOTIMPL;
}

STDMETHODIMP CSubCommand::GetState(IShellItemArray* psiItemArray, BOOL /*fOkToBeSlow*/, EXPCMDSTATE* pCmdState)
{
    if (!pCmdState)
        return E_POINTER;

    *pCmdState = ECS_ENABLED;

    RegistryUtil::GpuPreference selectionPreference = RegistryUtil::GpuNotSet;
    bool hasPreferenceEntry = false;
    if (!GetSelectionPreference(psiItemArray, selectionPreference, hasPreferenceEntry))
        return S_OK;

    if (m_type == SubCommandType::Clear && !hasPreferenceEntry)
    {
        *pCmdState = ECS_HIDDEN;
        return S_OK;
    }

    if (IsCommandSelected(m_type, selectionPreference, hasPreferenceEntry))
        *pCmdState = static_cast<EXPCMDSTATE>(ECS_ENABLED | ECS_CHECKED);

    return S_OK;
}

STDMETHODIMP CSubCommand::Invoke(IShellItemArray* psiItemArray, IBindCtx* /*pbc*/)
{
    if (!psiItemArray)
        return E_INVALIDARG;

    DWORD count = 0;
    HRESULT hr = psiItemArray->GetCount(&count);
    if (FAILED(hr))
        return hr;

    // 对每个选中文件执行操作
    for (DWORD i = 0; i < count; ++i)
    {
        ATL::CComPtr<IShellItem> spItem;
        hr = psiItemArray->GetItemAt(i, &spItem);
        if (FAILED(hr))
            continue;

        LPWSTR pszPath = nullptr;
        hr = spItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
        if (FAILED(hr))
            continue;

        std::wstring filePath(pszPath);
        CoTaskMemFree(pszPath);

        switch (m_type)
        {
        case SubCommandType::Clear:
            RegistryUtil::RemovePreference(filePath);
            break;
        case SubCommandType::Default:
            RegistryUtil::SetPreference(filePath, RegistryUtil::GpuDefault);
            break;
        case SubCommandType::PowerSaving:
            RegistryUtil::SetPreference(filePath, RegistryUtil::GpuPowerSaving);
            break;
        case SubCommandType::HighPerf:
            RegistryUtil::SetPreference(filePath, RegistryUtil::GpuHighPerf);
            break;
        }
    }

    return S_OK;
}

STDMETHODIMP CSubCommand::GetFlags(EXPCMDFLAGS* pFlags)
{
    if (!pFlags)
        return E_POINTER;
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

STDMETHODIMP CSubCommand::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    *ppEnum = nullptr;
    return E_NOTIMPL;
}
