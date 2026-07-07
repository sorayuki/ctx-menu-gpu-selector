#pragma once

#include <atlbase.h>
#include "GpuSelectGuids.h"
#include <shobjidl.h>
#include <atlcom.h>

/// 父菜单 "绑定显卡"，包含三个子命令
class ATL_NO_VTABLE CGpuSelectCommand :
    public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
    public ATL::CComCoClass<CGpuSelectCommand, &CLSID_GpuSelectCommand>,
    public IExplorerCommand
{
public:
    CGpuSelectCommand() = default;

    DECLARE_NO_REGISTRY()
    DECLARE_CLASSFACTORY()

    BEGIN_COM_MAP(CGpuSelectCommand)
        COM_INTERFACE_ENTRY(IExplorerCommand)
    END_COM_MAP()

    // ── IExplorerCommand ──────────────────────────────────
    STDMETHODIMP GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName) override;
    STDMETHODIMP GetIcon(IShellItemArray* psiItemArray, LPWSTR* ppszIcon) override;
    STDMETHODIMP GetToolTip(IShellItemArray* psiItemArray, LPWSTR* ppszToolTip) override;
    STDMETHODIMP GetCanonicalName(GUID* pguidCommandName) override;
    STDMETHODIMP GetState(IShellItemArray* psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override;
    STDMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx* pbc) override;
    STDMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override;
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override;
};

/// IEnumExplorerCommand 实现：枚举三个子命令
class ATL_NO_VTABLE CEnumSubCommands :
    public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
    public IEnumExplorerCommand
{
public:
    CEnumSubCommands();
    ~CEnumSubCommands();

    /// 初始化枚举器，传入三个子命令的 COM 指针
    HRESULT Init(IExplorerCommand* pClear, IExplorerCommand* pPowerSave, IExplorerCommand* pHighPerf);

    BEGIN_COM_MAP(CEnumSubCommands)
        COM_INTERFACE_ENTRY(IEnumExplorerCommand)
    END_COM_MAP()

    // ── IEnumExplorerCommand ──────────────────────────────
    STDMETHODIMP Next(ULONG celt, IExplorerCommand** ppl, ULONG* pceltFetched) override;
    STDMETHODIMP Skip(ULONG celt) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumExplorerCommand** ppEnum) override;

private:
    ATL::CComPtr<IExplorerCommand> m_commands[3];
    ULONG m_index;
};
