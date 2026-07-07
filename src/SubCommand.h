#pragma once

#include <atlbase.h>
#include <shobjidl.h>
#include <atlcom.h>
#include <string>

/// 子命令类型
enum class SubCommandType
{
    Clear,        // 清除设置
    Default,      // 默认
    PowerSaving,  // 节能
    HighPerf      // 高性能
};

/// 单个子菜单命令，实现 IExplorerCommand
/// 由父命令枚举子命令时创建，不从外部 CoCreateInstance
class ATL_NO_VTABLE CSubCommand :
    public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
    public IExplorerCommand
{
public:
    CSubCommand() : m_type(SubCommandType::Clear) {}

    static HRESULT Create(SubCommandType type, IExplorerCommand** ppCommand);

    BEGIN_COM_MAP(CSubCommand)
        COM_INTERFACE_ENTRY(IExplorerCommand)
    END_COM_MAP()

    // IExplorerCommand
    STDMETHODIMP GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName) override;
    STDMETHODIMP GetIcon(IShellItemArray* psiItemArray, LPWSTR* ppszIcon) override;
    STDMETHODIMP GetToolTip(IShellItemArray* psiItemArray, LPWSTR* ppszToolTip) override;
    STDMETHODIMP GetCanonicalName(GUID* pguidCommandName) override;
    STDMETHODIMP GetState(IShellItemArray* psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override;
    STDMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx* pbc) override;
    STDMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override;
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override;

private:
    SubCommandType m_type;

    /// 从 IShellItemArray 获取第一个文件的完整路径
    static HRESULT GetFirstFilePath(IShellItemArray* psiItemArray, std::wstring& path);
};
