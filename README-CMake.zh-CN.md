# 在 CLion 中使用 CMake 构建 JPEGView

本适配为现有 Windows/MSVC 工程增加 CMake 目标，不替换 `.sln/.vcxproj`。
最低 CMake 版本为 3.24，使用 C++17。

## 完整应用需要 MSVC

JPEGView 主程序依赖 ATL/WTL 和仓库里的 MSVC 格式图片库，不能用 CLion 自带 MinGW 直接构建整个应用。

在 Visual Studio Installer 中为所用工具集安装：

- 使用 C++ 的桌面开发、MSVC x64/x86 编译器；
- 与该工具集匹配的 C++ ATL 组件；
- Windows SDK。

只有 Visual Studio IDE、.NET 工具或可运行 MSBuild，不能证明上述组件已经安装。
CMake 不再固定上游工程的 v142 工具集，会使用 CLion 选中的 MSVC；若希望与上游完全一致，可安装并选择 v142。

## CLion 操作

1. 打开工程根目录，或根目录的 `CMakeLists.txt`，作为 CMake Project 加载。
2. 在 **Settings → Build, Execution, Deployment → Toolchains** 添加 **Visual Studio** 工具链，名称保留为 `Visual Studio`，Architecture 选择 `amd64`。
3. 等待 CLion 检测到 C/C++ 编译器、Windows SDK、构建工具和调试器。
4. 导入 `CMakePresets.json`，选择 `msvc-x64-release` 或 `msvc-x64-relwithdebinfo`。
5. Reload CMake，选择运行目标 `JPEGView`。

Preset 中的工具链名称位于 `vendor.jetbrains.com/clion.toolchain`。如果本机使用了其他名称，修改这里，或手动创建对应 CMake Profile。
如果尚未创建名称匹配的工具链，某些 CLion 版本可能不显示该 preset；先完成第 2 步，再重新加载项目。
不使用 preset 也可以：创建 Visual Studio 工具链的 CMake Profile，Generator 选择 Ninja，Build type 选择 Release/RelWithDebInfo。

编译成功后，程序及所需文件位于：

```text
out/build/msvc-x64-release/bin/Release/JPEGView.exe
```

CLion 运行配置的工作目录建议设为可执行文件所在目录。

### 命令行

先启动 x64 Native Tools/Developer PowerShell，保证 `cl.exe`、Windows SDK 和 Ninja 可用：

```powershell
cmake --preset msvc-x64-release
cmake --build --preset msvc-x64-release
```

Preset 的 x64 架构由开发者终端／CLion 工具链提供；Ninja 本身不会初始化 MSVC 环境。

## CMake 目标与原工程设置

- `JPEGView`：主程序，包含主目录全部 `.cpp/.h`、图标、Windows 资源及 DPI manifest。
- `WICLoader`：WIC DLL，单独生成并与主程序放在同一运行目录。
- `ExplorerOrder`：有该源码时自动建立的独立静态库；不使用主程序的 XP 预编译头。
- `ExplorerOrderTests` / `ExplorerOrderProbe`：有对应测试源码时可构建的独立工具。
- 主程序使用 MSVC 静态 CRT、Unicode、C++17、SEH 异常处理及原有 delay-load DLL。
- 只有 AVX 实现文件启用 `/arch:AVX`，不把整个程序提高为 AVX 最低要求。
- `dcraw_mod.cpp` 不强制使用 CMake PCH，保留它的 Winsock/Windows 头文件顺序。
- 根据目标位数选择 `lib64/bin64` 或 `lib/bin` 的预编译库。
- 构建后复制配置模板、语言、文档与编解码 DLL；已有输出目录的 `.ini` 文件不会被覆盖。
- Release 和 RelWithDebInfo 使用发布版 CRT；Debug 按原工程使用调试 CRT。仓库第三方库主要是预编译版本，Debug 的完整链接仍需在实际 MSVC 环境验证。

## 依赖

WTL 必须存在，初次 Git 克隆后执行：

```powershell
git submodule update --init deps/WTL-sf
```

也可设置 `-DJPEGVIEW_WTL_INCLUDE_DIR=你的WTL/Include`。
PNG/JXL/HEIF/AVIF/WebP/RAW 等使用仓库自带的头文件、`.lib` 和 DLL，不自动下载或重新编译这些库。
当前仓库的 QOI 头文件已经在 `src/JPEGView/qoi/qoi.h`，完整应用不依赖额外下载 QOI 源码。

## 只构建排序模块测试（可用 MinGW）

这是独立模块的验证方式，不能代替完整应用的 MSVC 构建。
在 CLion 里选择 MinGW 工具链及 `mingw-explorer-tests` preset，或在已配置 MinGW/Ninja 的终端运行：

```powershell
cmake --preset mingw-explorer-tests
cmake --build --preset mingw-explorer-tests
ctest --preset mingw-explorer-tests
```

等效手动配置参数：

```text
-DJPEGVIEW_BUILD_APP=OFF -DJPEGVIEW_BUILD_EXPLORER_TESTS=ON
```

## 此工程里的排序功能状态

2026-10-30：已确认用户应用的排序补丁接入主程序、文件列表和设置；Release 构建会链接 ExplorerOrder 静态库。
默认 FollowExplorerOrder=true，从普通资源管理器文件夹打开图片时读取一次当前顺序。
排序模块的真实 Explorer 顺序读取已验证；主程序实际窗口中的连续翻图顺序仍需手动验证。

为兼容本机 MSVC 14.51，原来的非标准 hash_map 已替换为 std::unordered_map。
字符串键仍按内容比较并区分大小写，保留原哈希函数；已检查不同地址的相同文本、替换、大小写和碰撞行为。
主程序使用静态 CRT，但第三方解码 DLL 使用动态 CRT，因此输出目录额外复制 Visual Studio 官方可再分发运行库。

## 常见提示

- **主程序要求 MSVC**：当前选择了 MinGW；切换 Visual Studio 工具链。
- **找不到编译器**：先安装 MSVC C++ 组件，并确认工具链初始化脚本可用。
- **找不到 atlbase.h**：安装对应工具集的 ATL 组件，然后重新加载／重置 CMake 缓存。
- **找不到 atlapp.h**：初始化 WTL 子模块，或设置 WTL Include 路径。
- **找不到 codec .lib**：检查仓库依赖文件是否完整，以及架构是否选择了 x64/Win32。
- **仍提示不属于任何 target**：检查是否成功加载了完整应用的 MSVC CMake Profile。仅测试模式不会为整个主程序建立目标。

## 本机验证记录

- CMake preset 解析、MinGW 独立模块配置／生成／构建成功。
- `ExplorerOrderPolicy` CTest 测试通过。
- 已核对主程序 84 个原始 `.cpp`，CMake 的主程序源文件集合没有遗漏；新增排序 `.cpp` 属于独立库目标。
- 运行文件复制脚本检查通过：10 个 codec DLL 已复制，重复运行保留已有 INI 内容。
- 误用 MinGW 构建完整应用时，MSVC/ATL/Windows SDK 提示检查通过。
- 2026-10-30：MSVC 19.51 x64 Release 完整配置、编译、链接及运行文件复制通过。
- MSVC ExplorerOrderPolicy CTest：1/1 通过；MSVC 原生 Explorer 读取集成测试：2/2 通过。
- 新程序以独立 INI 启动后可响应，窗口标题为 sample.jpg - JPEGView；测试后已关闭。
- 隐藏窗口的幻灯片自动退出测试超时，随后已关闭测试进程；没有据此声称完整界面翻图测试通过。
- 未逐一测试所有第三方图片格式，也未验证 Debug 完整构建。
