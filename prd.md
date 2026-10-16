# 软件功能

一个能通过 Windows 11 新右键菜单选择指定程序在哪个显卡上执行的小工具

# 功能设计

在资源管理器中，如果用户右键点击某个EXE文件，那么在弹出的菜单中添加一项“绑定显卡”，该项目有二级菜单

二级菜单中包含三个选项：

清除设置
节能
高性能

选项对应注册表的这个位置
```HKCU\Software\Microsoft\DirectX\UserGpuPreferences```

通过Powershell添加键值的示例脚本如下
```
$exe = "D:\Games\Example\game.exe"
New-Item -Path "HKCU:\Software\Microsoft\DirectX\UserGpuPreferences" -Force | Out-Null
New-ItemProperty `
  -Path "HKCU:\Software\Microsoft\DirectX\UserGpuPreferences" `
  -Name $exe `
  -Value "GpuPreference=2;" `
  -PropertyType String `
  -Force
```

读取注册表，如果该 EXE 已经设置成了 节能 或者 高性能，则在对应选项前面加上一个 〇 符号。

其中 2 是高性能， 1 是节能。如果用户点击了清除设置，则如果存在就删除对应 EXE 的项

# 实现要求

1. 尽量使用ATL中的资源管理类来替代裸的 HKEY 等
2. 使用Unicode字符集
3. 使用CMake构建，Generator为 Visual Studio 2026
4. MSVCRT使用静态链接
