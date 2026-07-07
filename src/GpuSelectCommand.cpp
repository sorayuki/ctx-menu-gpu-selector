#include "GpuSelectCommand.h"
#include "SubCommand.h"
#include <shlwapi.h>

// CGpuSelectCommand: 父菜单 "绑定显卡"

STDMETHODIMP CGpuSelectCommand::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    if (!ppszName)
        return E_POINTER;
    return SHStrDup(L"绑定显卡(&G)", ppszName);
}

STDMETHODIMP CGpuSelectCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    *ppszIcon = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP CGpuSelectCommand::GetToolTip(IShellItemArray*, LPWSTR* ppszToolTip)
{
    if (!ppszToolTip)
        return E_POINTER;
    return SHStrDup(L"选择此程序使用的显卡", ppszToolTip);
}

STDMETHODIMP CGpuSelectCommand::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = GUID_NULL;
    return E_NOTIMPL;
}

STDMETHODIMP CGpuSelectCommand::GetState(IShellItemArray*, BOOL /*fOkToBeSlow*/, EXPCMDSTATE* pCmdState)
{
    if (!pCmdState)
        return E_POINTER;
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

STDMETHODIMP CGpuSelectCommand::Invoke(IShellItemArray*, IBindCtx*)
{
    // 父命令本身没有点击行为，子命令处理
    return S_OK;
}

STDMETHODIMP CGpuSelectCommand::GetFlags(EXPCMDFLAGS* pFlags)
{
    if (!pFlags)
        return E_POINTER;
    *pFlags = ECF_HASSUBCOMMANDS;
    return S_OK;
}

STDMETHODIMP CGpuSelectCommand::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    if (!ppEnum)
        return E_POINTER;
    *ppEnum = nullptr;

    // 创建四个子命令
    ATL::CComPtr<IExplorerCommand> spClear, spDefault, spPowerSave, spHighPerf;

    HRESULT hr = CSubCommand::Create(SubCommandType::Clear, &spClear);
    if (FAILED(hr)) return hr;
    hr = CSubCommand::Create(SubCommandType::Default, &spDefault);
    if (FAILED(hr)) return hr;
    hr = CSubCommand::Create(SubCommandType::PowerSaving, &spPowerSave);
    if (FAILED(hr)) return hr;
    hr = CSubCommand::Create(SubCommandType::HighPerf, &spHighPerf);
    if (FAILED(hr)) return hr;

    // 创建枚举器
    ATL::CComObject<CEnumSubCommands>* pEnum = nullptr;
    hr = ATL::CComObject<CEnumSubCommands>::CreateInstance(&pEnum);
    if (FAILED(hr)) return hr;

    hr = pEnum->Init(spClear, spDefault, spPowerSave, spHighPerf);
    if (FAILED(hr))
    {
        delete pEnum;
        return hr;
    }

    return pEnum->QueryInterface(IID_IEnumExplorerCommand, reinterpret_cast<void**>(ppEnum));
}

// CEnumSubCommands: 子命令枚举器

CEnumSubCommands::CEnumSubCommands() : m_index(0)
{
}

CEnumSubCommands::~CEnumSubCommands()
{
}

HRESULT CEnumSubCommands::Init(IExplorerCommand* pClear, IExplorerCommand* pDefault, IExplorerCommand* pPowerSave, IExplorerCommand* pHighPerf)
{
    m_commands[0] = pClear;
    m_commands[1] = pDefault;
    m_commands[2] = pPowerSave;
    m_commands[3] = pHighPerf;
    m_index = 0;
    return S_OK;
}

STDMETHODIMP CEnumSubCommands::Next(ULONG celt, IExplorerCommand** ppIExplorerCommand, ULONG* pceltFetched)
{
    if (!ppIExplorerCommand)
        return E_POINTER;

    ULONG fetched = 0;
    while (fetched < celt && m_index < 4)
    {
        HRESULT hr = m_commands[m_index].CopyTo(&ppIExplorerCommand[fetched]);
        if (FAILED(hr))
            break;
        ++fetched;
        ++m_index;
    }

    if (pceltFetched)
        *pceltFetched = fetched;

    return (fetched == celt) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumSubCommands::Skip(ULONG celt)
{
    m_index += celt;
    if (m_index > 4)
        m_index = 4;
    return (m_index <= 4) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumSubCommands::Reset()
{
    m_index = 0;
    return S_OK;
}

STDMETHODIMP CEnumSubCommands::Clone(IEnumExplorerCommand** ppEnum)
{
    if (!ppEnum)
        return E_POINTER;
    *ppEnum = nullptr;

    ATL::CComObject<CEnumSubCommands>* pClone = nullptr;
    HRESULT hr = ATL::CComObject<CEnumSubCommands>::CreateInstance(&pClone);
    if (FAILED(hr)) return hr;

    hr = pClone->Init(m_commands[0], m_commands[1], m_commands[2], m_commands[3]);
    if (FAILED(hr))
    {
        delete pClone;
        return hr;
    }

    pClone->m_index = m_index;
    return pClone->QueryInterface(IID_IEnumExplorerCommand, reinterpret_cast<void**>(ppEnum));
}
