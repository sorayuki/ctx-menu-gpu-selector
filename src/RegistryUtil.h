#pragma once

#include <string>
#include <Windows.h>

/// 使用 ATL::CRegKey 封装 GPU 偏好设置的注册表操作
class RegistryUtil
{
public:
    // GPU 偏好值
    enum GpuPreference
    {
        GpuNotSet = 0,       // 未设置 / 已清除
        GpuPowerSaving = 1,  // 节能
        GpuHighPerf = 2      // 高性能
    };

    /// 尝试读取指定 EXE 的 GPU 偏好。返回 false 表示注册表中不存在该 EXE 项。
    static bool TryGetPreference(const std::wstring& exePath, GpuPreference& pref);

    /// 读取指定 EXE 的 GPU 偏好设置。未设置则返回 GpuNotSet
    static GpuPreference GetPreference(const std::wstring& exePath);

    /// 设置指定 EXE 的 GPU 偏好（1=节能, 2=高性能）
    static bool SetPreference(const std::wstring& exePath, GpuPreference pref);

    /// 删除指定 EXE 的 GPU 偏好设置
    static bool RemovePreference(const std::wstring& exePath);

private:
    static const wchar_t* const kRegistryPath;
};
