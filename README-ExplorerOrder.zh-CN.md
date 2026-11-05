# JPEGView：普通文件夹打开时读取资源管理器排序

基于官方 `sylikc/jpegview` 的 `v1.3.46`，基线提交：
`af075dbd1918554ea4491b42646f03286749a0e6`。

本扩展按上游的 GPL-2.0-or-later 许可证提供，保留上游 LICENSE/COPYING 文件。

## 本版实现的行为

从 Windows 资源管理器的普通文件夹双击图片后：

1. JPEGView 按原流程打开被双击的那张图。
2. 后台线程读取来源文件夹当前视图中的文件顺序。
3. 首张图片首次绘制以后，在界面线程中应用这份顺序；保持当前图片不变。
4. 上一张、下一张、Home、End 随后使用这份顺序。
5. 读取失败、来源不符或查询未在约 2 秒预算内完成时，使用原来的 `FileDisplayOrder`。

**只读取一次。** 在资源管理器里重新排序，不会实时改变已打开的 JPEGView。
重新从资源管理器打开图片会建立新快照，包括交给已有单实例窗口打开的情况。

查询尚未完成时，翻页操作暂不执行，避免按临时顺序跳错；无需阻塞界面等待。
完成很快的查询可在首图绘制后立即使用；若首张大图解码超过 2 秒，及时完成的快照仍会保留。

本机独立探针测试的 3 张图文件夹读取耗时约 31–32 ms；这不是重新编译后的完整看图程序启动耗时。

## 设置

新增 INI 选项，默认开启；旧 INI 没有此项也会使用默认值：

```ini
FollowExplorerOrder=true
```

设为 `false` 可关闭扩展，完全按原来的排序规则浏览。
在 JPEGView 中主动选择排序方式／升降序，会退出本次快照排序。
显式命令行 `/order` 或自动播放 `/slideshow` 的新进程启动保持原命令行为，不启用本扩展。

## 范围与边界

- 支持实际文件系统中的普通文件夹；只接受来源 Explorer 窗口、实际文件夹路径和被打开图片相符的结果。
- 搜索结果、库、主页、桌面、压缩包视图及其他虚拟文件夹不在本版范围内。
- 文件对话框、命令行直接启动、拖入 JPEGView 的文件使用原排序；不猜测其他已打开文件夹的顺序。
- Windows 11 有限度检查活动视图的可见性，但不承诺复杂多标签页／同目录多视图组合；本次没有做这些场景的兼容验证。
- 沿用上游的常规路径限制，超过 `MAX_PATH` 的路径回退，不宣称增加长路径支持。
- 删除文件／目录刷新会保留已有快照相对顺序；新增文件排在快照条目之后，使用原排序的相对顺序。
- Shell COM 调用不在界面线程执行。超时后界面放弃结果，不强制终止线程；同一进程最多允许两个读取工作线程，避免异常 Shell 服务导致无限累积。

## 源码结构

- `src/JPEGView/ExplorerOrder.cpp/.h`：独立 STA 线程，读取来源 Explorer 视图，使用 `SVGIO_FLAG_VIEWORDER` 枚举真实显示顺序。
- `src/JPEGView/ExplorerOrderPolicy.h`：按快照重排现有链表节点，保持当前图片的迭代器和对象地址。
- `src/JPEGView/FileList.cpp/.h`：应用快照、刷新后保留快照、显式排序时退出快照。
- `src/JPEGView/JPEGView.cpp`：在获得焦点前记录来源窗口；复用已有实例时转发来源窗口。
- `src/JPEGView/MainDlg.cpp/.h`：首次绘制后的定时轮询、在界面线程应用、取消过期任务和导航保护。
- `src/JPEGView/SettingsProvider.cpp/.h`、`Config/JPEGView.ini*`：开关。

扩展只改文件列表和启动接线，没有修改图片解码／缩放／绘制算法。

## 完整程序的编译

本补丁接入上游当前的 `src/JPEGView.sln` / `JPEGView.vcxproj`。
未接入旧 VS2010/2013/2017 工程，也不支持 XP 构建。

按上游 `COMPILING.txt`，推荐 VS2019 Community/Professional，安装：

- 使用 C++ 的桌面开发工具；
- MSVC v142 x86/x64 工具链；
- 对应 C++ ATL 组件；
- Windows 10 SDK。

打开 `src/JPEGView.sln`，选择 `Release | x64`，生成 `JPEGView` 及其依赖 `WICLoader`。
输出目录为 `src/JPEGView/bin/x64/Release/`。
只构建看图程序不需要构建 `JPEGView.Setup`；WiX 只用于 MSI 安装包。

如果使用 VS2022，安装相应 C++/ATL/SDK，并安装 v142 兼容工具链，或自行将工程切换为已安装的工具集。
本次没有验证切换工具集后的完整构建。

源码 ZIP 已包含所需 WTL 与 QOI 子模块内容；补丁本身应用到全新 Git 克隆时，先运行：

```powershell
git checkout v1.3.46
git submodule update --init deps/WTL-sf extras/third_party/qoi
git apply --check ..\JPEGView-1.3.46-explorer-order.patch
git apply ..\JPEGView-1.3.46-explorer-order.patch
```

## 已执行的验证（2026-10-30）

1. **新增原生模块编译通过**：使用本机 CLion 随附的 MinGW GCC 15.2，C++17、`-Wall -Wextra -Werror`。
2. **实际重排代码测试通过**：任意视图顺序、当前节点保留、前后导航位置、Home/End、中文／空格路径、大小写、重复项、删除项、新增项和空列表。
3. **真实 Explorer 集成探针通过**：独立文件夹中按大小升序、按名称降序，都返回预期顺序；还验证了后台完成后延迟 2200 ms 消费仍有效。结果见 `tests/explorer-smoke-result.json`。
4. 工程与筛选 XML 解析、源码补丁空白检查通过；补丁还会对干净基线执行适用性检查。

**未完成的验证：** 完整 JPEGView 程序的编译和端到端双击／翻图测试。
本机 Visual Studio 缺少 C++ 构建工具，MSBuild 报缺少 `Microsoft.Cpp.Default.props`，因此没有生成或替换 JPEGView.exe。
新增模块的编译通过不等于整个应用已经编译通过。

### 用 MinGW 重跑原生模块测试

在仓库根目录执行，并将 MinGW `bin` 放入当前终端 PATH：

```powershell
New-Item -ItemType Directory -Path tests\bin -Force | Out-Null
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests\ExplorerOrderTests.cpp src\JPEGView\ExplorerOrder.cpp -o tests\bin\ExplorerOrderTests.exe -lole32 -loleaut32 -lshell32 -luuid -lshlwapi
.\tests\bin\ExplorerOrderTests.exe
```

重跑真实 Explorer 检查：

```powershell
.\tests\SmokeExplorer.ps1 -Compiler '你的 MinGW\bin\g++.exe'
```

该脚本只在 `tests/explorer-fixture` 创建测试 BMP，并打开、排序、关闭这个专用 Explorer 窗口；不会改默认打开方式或用户图片。

### 完整构建后的手动验收

1. 在普通文件夹放入三张图片，让名称顺序与大小顺序不同。
2. 在资源管理器按大小升序排列，双击中间一张。
3. 验证先显示被双击图片，左右／Home／End 与资源管理器顺序一致。
4. 在资源管理器改变排序，验证已经打开的看图会话保持旧顺序。
5. 重新双击图片，验证新会话使用新顺序；同时测试单实例复用。
6. 设置 `FollowExplorerOrder=false`，验证恢复原排序。
7. 在 JPEGView 中手动选择原排序，验证能退出快照。
8. 验证关闭、删除、重命名和目录刷新；最后以同图、同目录对照原版的首图启动速度。
