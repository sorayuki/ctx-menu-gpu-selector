// GPU 亲和性管理窗口程序（prd2）
// 列出注册表中所有已配置 GPU 亲和性的程序，支持多选、右键调整亲和性。
// 菜单栏第一级提供“刷新”、“清理”、“系统设置”；“右键”下拉提供安装/卸载资源管理器右键菜单。
// 注册表的读取与操作逻辑复用 RegistryUtil（与 Shell 扩展共享同一份实现）。

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
#include <set>

#include "RegistryUtil.h"

// 启用 ComCtl32 v6 视觉样式（主题化的 ListView / Button）
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace
{
    const wchar_t* const kWindowClass = L"GpuSelectUI.MainWindow";
    const wchar_t* const kWindowTitle = L"GPU 亲和性管理";

    // 控件 ID
    const int IDC_LISTVIEW = 1001;

    // 菜单栏命令 ID
    const int IDM_REFRESH   = 1010;
    const int IDM_CLEAN     = 1011;
    const int IDM_SETTINGS  = 1012;
    const int IDM_INSTALL   = 1013;
    const int IDM_UNINSTALL = 1014;

    // ListView 子类化 ID（用于接收拖放）
    const UINT_PTR kListSubclassId = 1;

    // 右键菜单命令 ID（DEFAULT..HIGHPERF 必须连续，供 CheckMenuRadioItem 使用）
    const int IDM_CTX_CLEAR      = 2000;
    const int IDM_CTX_DEFAULT    = 2001;
    const int IDM_CTX_POWERSAVE  = 2002;
    const int IDM_CTX_HIGHPERF   = 2003;

    // 列索引
    const int COL_PATH = 0;
    const int COL_PREF = 1;

    // 布局常量
    const int kMargin   = 8;
    const int kPrefColW = 150;

    // 设置 → 系统 → 屏幕 → 显示卡 页面（Windows 10 对应“显示 → 图形设置”）
    const wchar_t* const kGraphicsSettingsUri = L"ms-settings:display-advancedgraphics";

    struct UIState
    {
        HWND hMain    = nullptr;
        HWND hList    = nullptr;
        HFONT hFont   = nullptr;
        std::vector<RegistryUtil::GpuEntry> entries;
        std::set<int> redItems;   // 需标红的行索引（清理确认期间）
    };

    void RefreshList(UIState* state);

    // 计算所有选中项共同的亲和性；unified 为 true 表示选中项亲和性一致
    bool GetSelectionCommonPreference(UIState* state,
                                      RegistryUtil::GpuPreference& common,
                                      bool& unified)
    {
        common = RegistryUtil::GpuNotSet;
        unified = false;

        bool any = false;
        bool first = true;
        RegistryUtil::GpuPreference cur = RegistryUtil::GpuNotSet;

        int index = -1;
        while ((index = ListView_GetNextItem(state->hList, index, LVNI_SELECTED)) != -1)
        {
            if (index < 0 || static_cast<size_t>(index) >= state->entries.size())
                continue;

            any = true;
            RegistryUtil::GpuPreference p = state->entries[index].preference;
            if (first)
            {
                cur = p;
                first = false;
            }
            else if (p != cur)
            {
                common = cur;
                unified = false;
                return true;
            }
        }

        if (!any)
            return false;

        common = cur;
        unified = true;
        return true;
    }

    // 对当前所有选中项应用命令，然后刷新列表
    void ApplyContextCommand(UIState* state, int commandId)
    {
        bool isRemove = (commandId == IDM_CTX_CLEAR);
        RegistryUtil::GpuPreference setTo = RegistryUtil::GpuNotSet;

        switch (commandId)
        {
        case IDM_CTX_CLEAR:      break;
        case IDM_CTX_DEFAULT:    setTo = RegistryUtil::GpuDefault;     break;
        case IDM_CTX_POWERSAVE:  setTo = RegistryUtil::GpuPowerSaving; break;
        case IDM_CTX_HIGHPERF:   setTo = RegistryUtil::GpuHighPerf;    break;
        default:                 return;
        }

        int index = -1;
        while ((index = ListView_GetNextItem(state->hList, index, LVNI_SELECTED)) != -1)
        {
            if (index < 0 || static_cast<size_t>(index) >= state->entries.size())
                continue;

            const std::wstring& path = state->entries[index].exePath;
            if (isRemove)
                RegistryUtil::RemovePreference(path);
            else
                RegistryUtil::SetPreference(path, setTo);
        }

        // 用户执行清除（或调整）后刷新列表
        RefreshList(state);
    }

    // 处理 ListView 右键：修正选择集后弹出亲和性菜单
    void HandleListRightClick(UIState* state)
    {
        POINT ptScreen = {};
        GetCursorPos(&ptScreen);

        // 命中测试：若右键点击的项不在当前选择集内，则改为单选该项；
        // 若已属于多选集合，则保留整个选择集（资源管理器习惯）。
        POINT ptClient = ptScreen;
        ScreenToClient(state->hList, &ptClient);

        LVHITTESTINFO hit = {};
        hit.pt = ptClient;
        int hitIndex = ListView_HitTest(state->hList, &hit);

        if (hitIndex >= 0)
        {
            UINT selState = static_cast<UINT>(
                ListView_GetItemState(state->hList, hitIndex, LVIS_SELECTED));
            if ((selState & LVIS_SELECTED) == 0)
            {
                ListView_SetItemState(state->hList, -1, 0, LVIS_SELECTED);
                ListView_SetItemState(state->hList, hitIndex, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
        else
        {
            ListView_SetItemState(state->hList, -1, 0, LVIS_SELECTED);
        }

        int selectedCount = static_cast<int>(
            SendMessageW(state->hList, LVM_GETSELECTEDCOUNT, 0, 0));
        if (selectedCount <= 0)
            return;

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        AppendMenuW(menu, MF_STRING, IDM_CTX_CLEAR, L"清除设置");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDM_CTX_DEFAULT, L"默认");
        AppendMenuW(menu, MF_STRING, IDM_CTX_POWERSAVE, L"节能");
        AppendMenuW(menu, MF_STRING, IDM_CTX_HIGHPERF, L"高性能");

        RegistryUtil::GpuPreference common = RegistryUtil::GpuNotSet;
        bool unified = false;
        if (GetSelectionCommonPreference(state, common, unified) && unified)
        {
            int radioId = 0;
            switch (common)
            {
            case RegistryUtil::GpuDefault:     radioId = IDM_CTX_DEFAULT;   break;
            case RegistryUtil::GpuPowerSaving: radioId = IDM_CTX_POWERSAVE; break;
            case RegistryUtil::GpuHighPerf:    radioId = IDM_CTX_HIGHPERF;  break;
            default: break;
            }
            if (radioId != 0)
            {
                CheckMenuRadioItem(menu, IDM_CTX_DEFAULT, IDM_CTX_HIGHPERF,
                                   radioId, MF_BYCOMMAND);
            }
        }

        int commandId = static_cast<int>(TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            ptScreen.x, ptScreen.y, 0, state->hMain, nullptr));

        DestroyMenu(menu);

        if (commandId != 0)
            ApplyContextCommand(state, commandId);
    }

    // 依据客户区尺寸布局：ListView 填满整个客户区
    void LayoutControls(UIState* state, int clientW, int clientH)
    {
        int listTop   = kMargin;
        int listWidth = clientW - kMargin * 2;
        int listHeight = clientH - kMargin * 2;
        if (listWidth < 0)  listWidth = 0;
        if (listHeight < 0) listHeight = 0;

        MoveWindow(state->hList, kMargin, listTop, listWidth, listHeight, TRUE);

        // 让“程序路径”列填满可用宽度，“亲和性”列固定宽度
        int gutter = GetSystemMetrics(SM_CXVSCROLL) + 4;
        int pathWidth = listWidth - gutter - kPrefColW;
        if (pathWidth < 60) pathWidth = 60;

        ListView_SetColumnWidth(state->hList, COL_PATH, pathWidth);
        ListView_SetColumnWidth(state->hList, COL_PREF, kPrefColW);
    }

    // 将 ListView 顶层项恢复到指定索引，从而还原刷新前的滚动条位置。
    // 索引超出范围时（如清理后条目变少）夹取到有效范围内。
    void RestoreListTopIndex(HWND hList, int savedTop)
    {
        int count = ListView_GetItemCount(hList);
        if (count <= 0)
            return;

        if (savedTop < 0)
            savedTop = 0;
        if (savedTop >= count)
            savedTop = count - 1;

        // 表头占据客户区顶部，计算目标项相对表头底部的偏移后滚动
        int headerHeight = 0;
        HWND hHeader = ListView_GetHeader(hList);
        RECT rcHeader = {};
        if (hHeader && GetClientRect(hHeader, &rcHeader))
            headerHeight = rcHeader.bottom - rcHeader.top;

        RECT rcItem = {};
        if (ListView_GetItemRect(hList, savedTop, &rcItem, LVIR_BOUNDS))
        {
            int delta = rcItem.top - headerHeight;
            if (delta != 0)
                ListView_Scroll(hList, 0, delta);
        }
    }

    void RefreshList(UIState* state)
    {
        const int savedTop = ListView_GetTopIndex(state->hList);

        state->redItems.clear();
        state->entries = RegistryUtil::EnumerateAll();
        std::sort(state->entries.begin(), state->entries.end(),
                  [](const RegistryUtil::GpuEntry& a, const RegistryUtil::GpuEntry& b)
                  {
                      return _wcsicmp(a.exePath.c_str(), b.exePath.c_str()) < 0;
                  });

        ListView_DeleteAllItems(state->hList);

        for (int i = 0; i < static_cast<int>(state->entries.size()); ++i)
        {
            LVITEMW item = {};
            item.mask    = LVIF_TEXT;
            item.iItem   = i;
            item.iSubItem = COL_PATH;
            item.pszText = const_cast<LPWSTR>(state->entries[i].exePath.c_str());
            SendMessageW(state->hList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));

            const wchar_t* label = RegistryUtil::PreferenceLabel(state->entries[i].preference);
            ListView_SetItemText(state->hList, i, COL_PREF, const_cast<LPWSTR>(label));
        }

        RestoreListTopIndex(state->hList, savedTop);
    }

    // 刷新后选中与指定路径匹配的条目（用于拖放后的选中反馈）
    void SelectEntriesByPath(UIState* state, const std::vector<std::wstring>& paths)
    {
        std::vector<int> matches;
        for (int i = 0; i < static_cast<int>(state->entries.size()); ++i)
        {
            for (const auto& path : paths)
            {
                if (_wcsicmp(state->entries[i].exePath.c_str(), path.c_str()) == 0)
                {
                    matches.push_back(i);
                    break;
                }
            }
        }

        if (matches.empty())
            return;

        ListView_SetItemState(state->hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        for (int index : matches)
            ListView_SetItemState(state->hList, index, LVIS_SELECTED, LVIS_SELECTED);
        ListView_SetItemState(state->hList, matches.front(), LVIS_FOCUSED, LVIS_FOCUSED);
    }

    // 处理拖放到 ListView 的文件：仅接受 .exe 文件，将其亲和性设为“默认”后刷新列表
    void HandleDroppedFiles(UIState* state, HDROP drop)
    {
        std::vector<std::wstring> exePaths;

        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            UINT len = DragQueryFileW(drop, i, nullptr, 0);
            if (len == 0)
                continue;

            std::wstring path(len + 1, L'\0');
            UINT copied = DragQueryFileW(drop, i, path.data(), len + 1);
            if (copied == 0)
                continue;
            path.resize(copied);

            const wchar_t* ext = PathFindExtensionW(path.c_str());
            if (ext && _wcsicmp(ext, L".exe") == 0)
                exePaths.push_back(std::move(path));
        }

        DragFinish(drop);

        if (exePaths.empty())
            return;

        for (const auto& path : exePaths)
            RegistryUtil::SetPreference(path, RegistryUtil::GpuDefault);

        RefreshList(state);
        SelectEntriesByPath(state, exePaths);
    }

    // ListView 子类化：拦截 WM_DROPFILES 以支持作为拖放目标
    LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        switch (msg)
        {
        case WM_DROPFILES:
            HandleDroppedFiles(reinterpret_cast<UIState*>(dwRefData),
                               reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ListViewSubclassProc, uIdSubclass);
            break;
        }

        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }

    // 打开 Windows 的“显示卡”（图形）设置页面，交给用户在系统界面里继续操作
    void OpenGraphicsSettings(UIState* state)
    {
        const INT_PTR result = reinterpret_cast<INT_PTR>(::ShellExecuteW(
            state->hMain, L"open", kGraphicsSettingsUri, nullptr, nullptr, SW_SHOWNORMAL));

        if (result <= 32)
        {
            const std::wstring message = std::wstring(
                L"无法打开显示卡设置页面（") + kGraphicsSettingsUri +
                L"）。\n请手动进入 设置 → 系统 → 屏幕 → 显示卡。";
            MessageBoxW(state->hMain, message.c_str(), kWindowTitle, MB_OK | MB_ICONWARNING);
        }
    }

    // 调用 GpuSelect.dll 导出的 DllRegisterServer / DllUnregisterServer，
    // 安装或卸载资源管理器中的“绑定显卡”右键菜单
    void RunDllRegistration(UIState* state, bool install)
    {
        wchar_t exePath[MAX_PATH] = {};
        if (!::GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        {
            MessageBoxW(state->hMain, L"无法定位程序目录。", kWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }
        ::PathRemoveFileSpecW(exePath);
        const std::wstring dllPath = std::wstring(exePath) + L"\\GpuSelect.dll";

        HMODULE hDll = ::LoadLibraryW(dllPath.c_str());
        if (!hDll)
        {
            const std::wstring message = L"无法加载 " + dllPath +
                L"。\n请确认 GpuSelect.dll 与本程序位于同一目录。";
            MessageBoxW(state->hMain, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }

        using RegisterFn = HRESULT(WINAPI*)();
        RegisterFn registerFn = reinterpret_cast<RegisterFn>(
            ::GetProcAddress(hDll, install ? "DllRegisterServer" : "DllUnregisterServer"));
        if (!registerFn)
        {
            ::FreeLibrary(hDll);
            MessageBoxW(state->hMain, L"GpuSelect.dll 未导出注册接口。",
                        kWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }

        const HRESULT hr = registerFn();
        ::FreeLibrary(hDll);

        if (SUCCEEDED(hr))
        {
            MessageBoxW(state->hMain,
                        install ? L"安装成功：已在资源管理器右键菜单中添加“绑定显卡”。"
                                : L"卸载成功：已移除资源管理器右键菜单中的“绑定显卡”。",
                        kWindowTitle, MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            wchar_t code[32] = {};
            swprintf_s(code, L"0x%08X", static_cast<unsigned int>(hr));
            const std::wstring message = std::wstring(install ? L"安装失败，HRESULT = " : L"卸载失败，HRESULT = ") +
                code + L"。";
            MessageBoxW(state->hMain, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
        }
    }

    // 判断是否为文件系统路径（含反斜杠）。UWP 等以 AppUserModelID 形式存在的键
    // （如 "OpenAI.Codex_xxx!App"）不是文件路径，无法用文件存在性判断，故跳过。
    bool IsFileSystemPath(const std::wstring& path)
    {
        return path.find(L'\\') != std::wstring::npos;
    }

    // 清理：先把"文件已不存在"的项在列表中标红，确认后再删除；取消则恢复原颜色
    void CleanInvalidEntries(UIState* state)
    {
        // 依据当前列表计算无效项索引（列表与 state->entries 始终同步）
        state->redItems.clear();
        for (int i = 0; i < static_cast<int>(state->entries.size()); ++i)
        {
            const std::wstring& path = state->entries[i].exePath;
            if (!IsFileSystemPath(path))
                continue; // 非文件路径键（如 UWP）不处理
            if (!::PathFileExistsW(path.c_str()))
                state->redItems.insert(i);
        }

        if (state->redItems.empty())
        {
            MessageBoxW(state->hMain, L"未发现文件已不存在的无效项。",
                        kWindowTitle, MB_OK | MB_ICONINFORMATION);
            return;
        }

        // 确认前先标红并重绘，让用户看清将要删除的是哪些项
        InvalidateRect(state->hList, nullptr, TRUE);
        UpdateWindow(state->hList);

        std::wstring prompt = L"发现 " + std::to_wstring(state->redItems.size()) +
            L" 个文件已不存在的无效项（已在列表中标红）。\n\n确定要删除标红的项吗？";
        int answer = MessageBoxW(state->hMain, prompt.c_str(),
                                 L"清理", MB_YESNO | MB_ICONQUESTION);

        if (answer != IDYES)
        {
            // 用户取消：恢复原有颜色
            state->redItems.clear();
            InvalidateRect(state->hList, nullptr, TRUE);
            return;
        }

        std::vector<std::wstring> toDelete;
        toDelete.reserve(state->redItems.size());
        for (int i : state->redItems)
            toDelete.push_back(state->entries[i].exePath);

        int removed = 0;
        for (const auto& path : toDelete)
        {
            if (RegistryUtil::RemovePreference(path))
                ++removed;
        }

        std::wstring result = L"已清理 " + std::to_wstring(removed) + L" 个无效项。";
        MessageBoxW(state->hMain, result.c_str(), kWindowTitle, MB_OK | MB_ICONINFORMATION);

        RefreshList(state);
    }

    // ListView 自绘：将 redItems 中的行文字标红
    LRESULT OnListCustomDraw(UIState* state, LPNMLVCUSTOMDRAW cd)
    {
        switch (cd->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
            return CDRF_NOTIFYSUBITEMDRAW;
        case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):
            if (state->redItems.find(static_cast<int>(cd->nmcd.dwItemSpec)) != state->redItems.end())
                cd->clrText = RGB(192, 0, 0);
            return CDRF_DODEFAULT;
        }
        return CDRF_DODEFAULT;
    }

    LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        UIState* state = reinterpret_cast<UIState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            auto* s = new (std::nothrow) UIState();
            if (!s)
                return -1;

            s->hMain = hWnd;
            s->hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
            state = s;

            HINSTANCE hInst = reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance;

            // 菜单栏：“刷新”“清理”“系统设置”为第一级菜单，“右键”下拉包含安装/卸载
            HMENU hMenuBar = CreateMenu();
            AppendMenuW(hMenuBar, MF_STRING, IDM_REFRESH, L"刷新");
            AppendMenuW(hMenuBar, MF_STRING, IDM_CLEAN, L"清理");
            AppendMenuW(hMenuBar, MF_STRING, IDM_SETTINGS, L"系统设置");

            HMENU hExtMenu = CreatePopupMenu();
            AppendMenuW(hExtMenu, MF_STRING, IDM_INSTALL, L"安装");
            AppendMenuW(hExtMenu, MF_STRING, IDM_UNINSTALL, L"卸载");
            AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hExtMenu), L"右键");

            SetMenu(hWnd, hMenuBar);

            // ListView：不带 LVS_SINGLE_SEL 以支持多选；
            // LVS_SHOWSELALWAYS 保证拖放后（控件可能未获得焦点）选中行仍可见
            s->hList = CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LISTVIEW)),
                hInst, nullptr);

            SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->hFont), TRUE);

            ListView_SetExtendedListViewStyle(s->hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            // 让 ListView 作为拖放目标接收 exe 文件
            DragAcceptFiles(s->hList, TRUE);
            SetWindowSubclass(s->hList, ListViewSubclassProc, kListSubclassId,
                              reinterpret_cast<DWORD_PTR>(s));

            LVCOLUMNW col = {};
            col.mask  = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            col.fmt   = LVCFMT_LEFT;

            col.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(L"程序路径"));
            col.cx      = 600;
            SendMessageW(s->hList, LVM_INSERTCOLUMNW, COL_PATH, reinterpret_cast<LPARAM>(&col));

            col.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(L"GPU 亲和性"));
            col.cx      = kPrefColW;
            SendMessageW(s->hList, LVM_INSERTCOLUMNW, COL_PREF, reinterpret_cast<LPARAM>(&col));

            RefreshList(s);
            return 0;
        }

        case WM_SIZE:
            if (state)
                LayoutControls(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_GETMINMAXINFO:
        {
            // 设置最小尺寸，避免控件被压缩为负值
            LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lParam);
            mmi->ptMinTrackSize.x = 320;
            mmi->ptMinTrackSize.y = 200;
            return 0;
        }

        case WM_COMMAND:
            if (state)
            {
                switch (LOWORD(wParam))
                {
                case IDM_REFRESH:
                    RefreshList(state);
                    return 0;
                case IDM_CLEAN:
                    CleanInvalidEntries(state);
                    return 0;
                case IDM_SETTINGS:
                    OpenGraphicsSettings(state);
                    return 0;
                case IDM_INSTALL:
                    RunDllRegistration(state, true);
                    return 0;
                case IDM_UNINSTALL:
                    RunDllRegistration(state, false);
                    return 0;
                }
            }
            break;

        case WM_NOTIFY:
        {
            LPNMHDR hdr = reinterpret_cast<LPNMHDR>(lParam);
            if (state && hdr && hdr->hwndFrom == state->hList)
            {
                if (hdr->code == NM_RCLICK)
                {
                    HandleListRightClick(state);
                    return 0;
                }
                if (hdr->code == NM_CUSTOMDRAW)
                {
                    return OnListCustomDraw(state, reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam));
                }
            }
            break;
        }

        case WM_DESTROY:
            if (state)
            {
                if (state->hList)
                {
                    DragAcceptFiles(state->hList, FALSE);
                    RemoveWindowSubclass(state->hList, ListViewSubclassProc, kListSubclassId);
                }
                SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                delete state;
            }
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
        return 0;

    HWND hWnd = CreateWindowExW(
        0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 560,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
        return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
