# Sekiro-Debug-Patch 项目分析文档

## 1. 项目概述

[`Sekiro-Debug-Patch`](README.md) 是一个面向《只狼：影逝二度》的调试菜单补丁项目，核心形式是一个名为 [`dinput8.dll`](x64/Release/dinput8.dll) 的代理 DLL。该 DLL 会在游戏启动时被加载，并继续转发系统原始 [`dinput8.dll`](dinput8/dinput8.def) 的导出函数，同时在进程内执行额外初始化逻辑，以启用开发者调试菜单并绘制一个基于 ImGui 的覆盖层。

从工程结构看，该项目是一个典型的 Windows 原生注入/代理 DLL 工程，主要由以下几类内容组成：

- 入口与代理层：[`dinput8/dinput8.cpp`](dinput8/dinput8.cpp)
- 调试菜单总控层：[`dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp)
- 内存签名扫描层：[`dinput8/SekiroDebugMenu/Signature/Signature.cpp`](dinput8/SekiroDebugMenu/Signature/Signature.cpp)
- 内存补丁与钩子层：[`dinput8/SekiroDebugMenu/Hooks/Hooks.cpp`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp)
- 调试菜单绘制层：[`dinput8/SekiroDebugMenu/Graphics/Graphics.cpp`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp)
- 第三方依赖：[`dinput8/ImGui`](dinput8/ImGui)、[`dinput8/MinHook`](dinput8/MinHook)
- 构建配置：[`dinput8.sln`](dinput8.sln)、[`dinput8/dinput8.vcxproj`](dinput8/dinput8.vcxproj)

## 2. 项目目标与实现思路

根据 [`README.md`](README.md) 的说明，这个项目的目标是：

1. 利用代理 DLL 机制让自定义 [`dinput8.dll`](x64/Release/dinput8.dll) 被游戏优先加载。
2. 在代理层加载系统目录下真正的 [`dinput8.dll`](dinput8/dinput8.cpp)，避免破坏正常输入相关功能。
3. 在游戏进程中通过特征码扫描找到关键逻辑地址。
4. 对目标地址进行字节级补丁，启用开发者菜单、修复参数缺失、解锁更多调试功能。
5. 通过 Direct3D 11 Present 钩子拦截渲染流程，将游戏原本输出的调试菜单字符串重新绘制到自定义覆盖层上。

简而言之，该工程不是“从零创建一个菜单”，而是“恢复/放大游戏中原有的开发菜单能力，并通过自绘覆盖层把内容正确显示出来”。

## 3. 构建与工程配置分析

### 3.1 解决方案与项目

解决方案文件 [`dinput8.sln`](dinput8.sln) 中只包含一个项目 [`dinput8`](dinput8/dinput8.vcxproj)。该项目支持：

- Debug / Release
- Win32 / x64

但从目录中的产物 [`x64/Release/dinput8.dll`](x64/Release/dinput8.dll) 与注入目标游戏的现实场景看，实际使用重点显然是 x64 Release。

### 3.2 工程类型

[`dinput8/dinput8.vcxproj`](dinput8/dinput8.vcxproj) 中配置显示：

- 项目类型是 `DynamicLibrary`
- 链接子系统为 `Windows`
- 使用模块定义文件 [`dinput8/dinput8.def`](dinput8/dinput8.def)
- 启用了 MASM 构建扩展以编译 [`dinput8/dinput8_asm.asm`](dinput8/dinput8_asm.asm)

说明该项目最终输出的是一个原生 Windows DLL，而不是控制台程序或 EXE。

### 3.3 依赖组成

编译源文件包括：

- 代理层：[`dinput8/dinput8.cpp`](dinput8/dinput8.cpp)
- 菜单逻辑：[`dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp)
- 图形层：[`dinput8/SekiroDebugMenu/Graphics/Graphics.cpp`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp)
- Hook 层：[`dinput8/SekiroDebugMenu/Hooks/Hooks.cpp`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp)
- Patch 回调层：[`dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp)
- Signature 扫描层：[`dinput8/SekiroDebugMenu/Signature/Signature.cpp`](dinput8/SekiroDebugMenu/Signature/Signature.cpp)
- ImGui 全量实现：[`dinput8/ImGui/imgui.cpp`](dinput8/ImGui/imgui.cpp) 等
- MinHook 实现：[`dinput8/MinHook/src/hook.c`](dinput8/MinHook/src/hook.c) 等

说明项目是静态包含第三方源码进行编译，而非通过包管理器动态拉取依赖。

## 4. 整体运行流程

项目的完整运行链路可概括如下：

1. 游戏加载本地 [`dinput8.dll`](x64/Release/dinput8.dll)。
2. [`DllMain`](dinput8/dinput8.cpp) 在进程附加时启动线程执行初始化。
3. 初始化线程在 [`Begin()`](dinput8/dinput8.cpp:19) 中加载系统目录下真正的 [`dinput8.dll`](dinput8/dinput8.cpp)。
4. 代理 DLL 保存真实导出函数地址，确保 DirectInput 行为可继续转发。
5. 之后调用 [`Initialise()`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:8) 初始化 SekiroDebugMenu 核心对象。
6. 初始化阶段首先执行 [`CHooks::Patch()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:4)，通过签名扫描查找函数/代码片段并写入补丁字节。
7. 然后执行 [`CGraphics::HookD3D11Present()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:26)，安装 DX11 Present 钩子。
8. 当游戏渲染帧到达 Present 时，钩子中初始化 ImGui，并在每帧调用 [`CGraphics::DrawMenu()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:184) 绘制收集到的调试菜单文本。
9. 游戏原始菜单绘制逻辑会被 [`DrawMenuHook()`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp:3) 截获，其文本和坐标被存入共享缓冲区，随后在 ImGui 覆盖层中渲染。

这条链路体现出项目的核心设计：**代理加载 + 内存扫描 + 字节补丁 + 渲染劫持 + 文本重绘**。

## 5. 关键源码文件分析

### 5.1 代理入口层

#### [`dinput8/dinput8.cpp`](dinput8/dinput8.cpp)

该文件承担整个补丁的入口职责。

核心逻辑：

- 定义了真实 [`dinput8.dll`](dinput8/dinput8.cpp) 的 6 个导出函数名数组 `mImportNames`
- 在 [`Begin()`](dinput8/dinput8.cpp:19) 中从系统目录加载原始 DLL
- 通过 `GetProcAddress` 获取导出地址并存入 `mProcs`
- 调用 [`Initialise()`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:8) 进入补丁初始化流程
- 在 [`DllMain`](dinput8/dinput8.cpp:38) 的 `DLL_PROCESS_ATTACH` 中创建线程执行初始化，避免直接在加载器锁上下文中做过多工作
- 在 `DLL_PROCESS_DETACH` 时释放已加载的原始 DLL

这是一种标准的代理 DLL 写法，目的在于兼容原有 API 行为的同时插入自定义逻辑。

#### [`dinput8/dinput8_asm.asm`](dinput8/dinput8_asm.asm)

该汇编文件实现了导出函数转发桩。当前只实现了：

- [`_DirectInput8Create`](dinput8/dinput8_asm.asm:5)

它通过 `jmp mProcs[0*8]` 直接跳转到真实 DLL 的目标函数地址，实现零额外包装的快速转发。

#### [`dinput8/dinput8.def`](dinput8/dinput8.def)

模块定义文件将导出符号 `DirectInput8Create` 映射到汇编实现 `_DirectInput8Create`，使生成的代理 DLL 对外保持与系统 [`dinput8.dll`](dinput8/dinput8.def) 一致的关键导出接口。

### 5.2 总控与单例访问层

#### [`dinput8/SekiroDebugMenu/SekiroDebugMenu.h`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h)
#### [`dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp)

这两个文件定义了系统的总控对象 [`CSekiroDebugMenu`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:17) 以及若干全局访问函数。

主要内容：

- `CSekiroDebugMenu` 内部持有三个核心组件：
  - [`CHooks`](dinput8/SekiroDebugMenu/Hooks/Hooks.h:7)
  - [`CSignature`](dinput8/SekiroDebugMenu/Signature/Signature.h:11)
  - [`CGraphics`](dinput8/SekiroDebugMenu/Graphics/Graphics.h:18)
- [`Initialise()`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp:8) 负责创建总控对象，并顺序执行补丁安装和图形钩子安装
- `GetHooks()` / `GetSignature()` / `GetGraphics()` 提供全局访问入口
- `InitDebug()`、[`DebugPrint()`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp:53)、`Panic()` 是调试辅助接口

设计上它更接近一种轻量级“全局服务定位器”。项目规模较小，因此使用全局对象简化了跨模块访问。

### 5.3 签名扫描层

#### [`dinput8/SekiroDebugMenu/Signature/Signature.h`](dinput8/SekiroDebugMenu/Signature/Signature.h)
#### [`dinput8/SekiroDebugMenu/Signature/Signature.cpp`](dinput8/SekiroDebugMenu/Signature/Signature.cpp)

该模块负责在目标模块 [`sekiro.exe`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:20) 的镜像内查找字节特征码。

数据结构：

- [`SSignature`](dinput8/SekiroDebugMenu/Signature/Signature.h:5)：保存签名字节串、掩码和长度
- [`CSignature`](dinput8/SekiroDebugMenu/Signature/Signature.h:11)：保存模块范围并执行遍历匹配

工作方式：

1. 构造函数中通过 `GetModuleHandleA` 获取目标模块基址。
2. 使用 `VirtualQuery` 与 PE 头解析镜像大小。
3. 保存镜像起止地址，供后续全量扫描。
4. [`GetSignature()`](dinput8/SekiroDebugMenu/Signature/Signature.cpp:3) 使用线性扫描比较目标字节与 mask，支持 `?` 通配。

该模块是整个补丁成功与否的基础，因为游戏版本变化后，硬编码偏移通常会失效，而签名扫描可以在一定程度上适配不同构建版本。

### 5.4 Hook 与补丁层

#### [`dinput8/SekiroDebugMenu/Hooks/Hooks.h`](dinput8/SekiroDebugMenu/Hooks/Hooks.h)
#### [`dinput8/SekiroDebugMenu/Hooks/Hooks.cpp`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp)
#### [`dinput8/SekiroDebugMenu/Hooks/HookSites.h`](dinput8/SekiroDebugMenu/Hooks/HookSites.h)

该模块是项目最核心的功能层。

##### 5.4.1 主要职责

- 初始化 MinHook
- 根据特征码定位关键代码位置
- 对内存地址直接写入字节补丁
- 安装必要函数钩子
- 汇总“启用调试菜单”所需的一系列补丁行为

##### 5.4.2 关键方法

[`CHooks::Patch()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:4) 是总入口，流程如下：

1. 检查 `MH_Initialize()` 是否成功。
2. 循环扫描 [`sActivateDebugMenu`](dinput8/SekiroDebugMenu/Hooks/HookSites.h:6) 直到找到。
3. 对找到的位置偏移后写入 `mov al, 1`，强制激活调试菜单。
4. 调用 [`CHooks::GetHookSites()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:47) 获取其他补丁位点。
5. 对菜单绘制函数安装 [`DrawMenuHook()`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp:3) 并写入 `call` 字节。
6. 通过写入 `ret`、`xor al, al`、`mov al, 1` 等机器码，修复或开启不同调试功能。
7. 对自由视角、冻结摄像机相关逻辑写入更大块的字节序列。

##### 5.4.3 特征码定义

[`HookSites.h`](dinput8/SekiroDebugMenu/Hooks/HookSites.h) 中定义了一组全局 [`SSignature`](dinput8/SekiroDebugMenu/Signature/Signature.h:5) 对象，例如：

- `sActivateDebugMenu`
- `sMenuDrawHook`
- `sDisableMissingParam1`
- `sDisableMissingParam2`
- `sDisableRemnantMenu`
- `sSigEnable3Areas`
- `sSigDisableFeature1`
- `sSigEnableFreezeCam`

这些名称已经很好地暴露出每个签名背后的用途：启用菜单、修复参数、禁用遗影菜单限制、启用若干功能区、开启冻结镜头等。

##### 5.4.4 底层能力函数

- [`Hook()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:100)：基于 MinHook 创建并启用函数钩子
- [`Tweak()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:107)：通过 `VirtualProtect` 修改内存页权限后覆盖字节
- [`Hookless()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:114)：直接替换函数指针，主要用于虚表项或函数指针表的简单位点替换

可以看出，这个模块混合使用了两类技术：

1. 标准函数 detour（MinHook）
2. 直接字节补丁 / 指针替换

这种方式对于游戏逆向补丁项目非常常见，因为不同目标点适合不同手段。

### 5.5 菜单绘制劫持层

#### [`dinput8/SekiroDebugMenu/Hooks/Patches/Patches.h`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.h)
#### [`dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp)

该模块的核心函数是 [`DrawMenuHook()`](dinput8/SekiroDebugMenu/Hooks/Patches/Patches.cpp:3)。

其功能并不是直接绘图，而是：

1. 接收游戏原始菜单绘制调用传入的坐标结构和宽字符文本。
2. 从图形模块获取一块 [`SDebugPrintStruct`](dinput8/SekiroDebugMenu/Graphics/Graphics.h:11) 数组作为共享缓冲。
3. 在临界区保护下，将文本和坐标写入第一个空闲槽位。
4. 留待渲染线程中的 ImGui 层统一消费并绘制。

这意味着游戏逻辑线程与渲染线程之间通过“共享队列/缓冲池”进行协作，是一个比较简洁的跨线程通信方案。

### 5.6 图形与覆盖层模块

#### [`dinput8/SekiroDebugMenu/Graphics/Graphics.h`](dinput8/SekiroDebugMenu/Graphics/Graphics.h)

#### [`dinput8/SekiroDebugMenu/Graphics/Graphics.cpp`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp)

该模块负责将捕获到的调试菜单文本实际显示出来。

##### 5.6.1 Present 钩子安装

[`CGraphics::HookD3D11Present()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:26) 的逻辑：

1. 等待 [`dxgi.dll`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp) 被加载。
2. 创建一个临时窗口与 D3D11 设备/交换链。
3. 获取交换链虚表地址。
4. 使用 [`Hookless()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:114) 将虚表中的 Present 指针替换为 [`CGraphics::D3D11PresentHook()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:99)。

该方案的优点是无需事先知道游戏交换链实例地址，只需通过本地构造一个同类型对象，拿到标准虚表布局后即可定位 Present 槽位。

##### 5.6.2 首帧初始化

在 [`D3D11PresentHook()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:99) 中，首次进入时会：

- 从真实交换链中获取游戏设备与上下文
- 创建 ImGui 上下文
- 初始化 Win32 与 DX11 backend
- 替换游戏窗口过程为自定义 [`hWndProc`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:231)
- 创建渲染目标视图
- 加载默认字体和 [`debugfont.ttf`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:153)
- 初始化临界区与窗口尺寸缓存

初始化完成后，每帧都会：

- 调用 ImGui 新帧
- 执行 [`DrawMenu()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:184)
- 渲染绘制数据
- 回调原始 Present

##### 5.6.3 菜单绘制

[`DrawMenu()`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:184) 的行为是：

- 打开一个全屏、无背景、无标题的 ImGui 窗口
- 遍历共享缓冲区中的有效文本项
- 将宽字符转换为 UTF-8 字符串
- 使用 `AddText` 按原始坐标绘制文字
- 将消费后的项标记为无效

因此它本质上是一个“文本重放器”，并不承担菜单逻辑，只负责把游戏提供的菜单文本绘制出来。

##### 5.6.4 输入处理

[`hWndProc`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:231) 负责：

- 同步鼠标坐标到 ImGui IO
- 监听 `VK_INSERT`
- 将消息继续交给 ImGui Win32 backend 和原窗口过程

当前代码中 `ShowMenu` 变量与鼠标绘制开关之间的关系并不完整，更像是预留了菜单显隐控制但尚未真正实现切换状态。

## 6. 文件与目录职责梳理

### 6.1 根目录

- [`README.md`](README.md)：项目简述与鸣谢
- [`dinput8.sln`](dinput8.sln)：Visual Studio 解决方案
- [`x64/Release`](x64/Release)：最终编译产物及打包结果

### 6.2 [`dinput8`](dinput8)

项目主代码目录。

- [`dinput8/dinput8.cpp`](dinput8/dinput8.cpp)：代理 DLL 入口
- [`dinput8/dinput8_asm.asm`](dinput8/dinput8_asm.asm)：导出函数转发桩
- [`dinput8/dinput8.def`](dinput8/dinput8.def)：DLL 导出定义
- [`dinput8/dinput8.vcxproj`](dinput8/dinput8.vcxproj)：工程构建配置

### 6.3 [`dinput8/SekiroDebugMenu`](dinput8/SekiroDebugMenu)

项目业务核心目录。

- [`SekiroDebugMenu.cpp`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp)：初始化与全局访问
- [`Graphics`](dinput8/SekiroDebugMenu/Graphics)：渲染覆盖层
- [`Hooks`](dinput8/SekiroDebugMenu/Hooks)：签名定位后的 Hook 与 patch
- [`Signature`](dinput8/SekiroDebugMenu/Signature)：模块内签名扫描

### 6.4 第三方源码

- [`dinput8/ImGui`](dinput8/ImGui)：UI 绘制框架，负责叠加层渲染能力
- [`dinput8/MinHook`](dinput8/MinHook)：轻量级 API Hook 库，负责函数 detour

### 6.5 构建产物目录

- [`dinput8/x64/Release`](dinput8/x64/Release)
- [`x64/Release`](x64/Release)

这些目录包含对象文件、PDB、DLL、ZIP 等，不属于源码逻辑本身，但可以帮助确认项目已成功构建并打包。

## 7. 功能点分析

结合源码可归纳出项目的主要功能：

### 7.1 代理系统 dinput8

通过本地同名 [`dinput8.dll`](x64/Release/dinput8.dll) 劫持加载顺序，再把真实系统 DLL 转发出去，是本项目得以注入游戏的基础能力。

### 7.2 启用开发者调试菜单

[`CHooks::Patch()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:4) 会对特定逻辑写入 `mov al, 1` 等字节，强制相关判断返回真，从而激活隐藏的调试菜单。

### 7.3 修复菜单参数缺失与子项不可用

对 `qMissingParamHook1`、`qMissingParamHook2`、`qDisableRemnantMenuHook` 等位置写入 `ret` 或寄存器逻辑变更，目的是修复原菜单在零售版下缺失参数、残影菜单异常或某些子菜单不可进入的问题。

### 7.4 启用更多调试特性

`sSigEnable3Areas`、`sSigDisableFeature1`、`sSigEnableFreezeCam` 等相关补丁表明，该项目不仅开启菜单本体，还扩展开启了更多功能区以及相机相关调试能力，例如：

- 额外 debug feature 开关
- GUI 相关限制移除
- Freeze Camera
- Pan Camera

### 7.5 覆盖层重绘菜单文本

游戏内部菜单文本原本可能并不会在零售版环境下完整正确显示，项目通过劫持绘制调用，把文本收集后使用 ImGui 按原坐标重绘，从而恢复可视化展示效果。

## 8. 设计特点与优点

### 8.1 模块划分清晰

虽然项目体量不大，但已经分为：

- 初始化
- 签名扫描
- Hook / Patch
- 图形渲染
- 第三方依赖

结构上较清楚，便于逆向研究和功能增补。

### 8.2 兼顾稳定性与适配性

相比硬编码地址，签名扫描提高了版本适配能力；相比只做内存 patch，不做 UI 处理，ImGui 重绘让菜单显示更可控。

### 8.3 工程实现直接高效

项目避免过度抽象，大部分逻辑围绕“找到地址、改字节、截获绘制、重放文本”展开，符合游戏补丁项目强调结果导向的特点。

## 9. 已观察到的问题与潜在改进点

在阅读代码后，也可以发现一些实现上的不足：

### 9.1 资源释放与生命周期管理较弱

- [`CSekiroDebugMenu`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:17) 中多个成员使用 `new` 创建，但没有对应释放逻辑。
- 图形资源、窗口类、临时窗口、ImGui 上下文的销毁流程缺失。
- [`CSignature`](dinput8/SekiroDebugMenu/Signature/Signature.h:15) 中对 `GetModuleHandleA` 得到的句柄调用 `CloseHandle` 并不合理。

### 9.2 错误处理不完整

- [`Panic()`](dinput8/SekiroDebugMenu/SekiroDebugMenu.cpp:64) 内部逻辑明显未完成，`pBuffer` 未格式化就直接输出。
- 多个 `if (!Hooks)` / `if (!Signature)` 分支为空。
- [`CHooks::Patch()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:44) 最终始终返回 `true`，没有返回 `bReturn`，导致部分补丁失败无法上抛。

### 9.3 线程与同步模型较粗糙

- 通过轮询 `Sleep()` 等待模块或签名出现，属于简单但不够优雅的同步方式。
- 文本缓冲区是固定大小数组，极端情况下可能丢失条目。

### 9.4 显示控制尚未完善

- [`hWndProc`](dinput8/SekiroDebugMenu/Graphics/Graphics.cpp:231) 中 `ShowMenu` 没有真正切换值。
- 菜单显隐逻辑、输入穿透和鼠标捕获策略还比较粗糙。

### 9.5 对游戏版本强相关

虽然使用了签名扫描，但签名本身仍依赖目标版本代码布局。若游戏更新幅度较大，仍需要重新修订 [`HookSites.h`](dinput8/SekiroDebugMenu/Hooks/HookSites.h) 中的特征码。

## 10. 适合继续扩展的方向

若后续要继续开发，这个项目适合从以下方向增强：

1. 完善文档，记录每个签名与补丁用途。
2. 将补丁项做成可配置表，而不是在 [`CHooks::Patch()`](dinput8/SekiroDebugMenu/Hooks/Hooks.cpp:4) 中硬编码。
3. 增加版本检测与签名校验日志。
4. 修复资源释放、错误处理和返回值问题。
5. 将菜单显隐、输入控制、字体路径等做成配置项。
6. 增强导出转发完整性，目前仅显式实现了 [`DirectInput8Create`](dinput8/dinput8.def:5)。

## 11. 结论

[`Sekiro-Debug-Patch`](README.md) 是一个面向 Sekiro 的原生 DLL 代理补丁项目，技术路线非常明确：通过代理 [`dinput8.dll`](x64/Release/dinput8.dll) 注入游戏，在运行时对 [`sekiro.exe`](dinput8/SekiroDebugMenu/SekiroDebugMenu.h:20) 做签名扫描和字节补丁，激活调试菜单相关功能，再通过 DX11 Present Hook 与 ImGui 覆盖层把调试文本绘制出来。

从逆向补丁工程角度看，它具备完整而实用的最小闭环：

- 有注入入口
- 有签名定位
- 有内存修补
- 有渲染接管
- 有最终可视化结果

这使它既可以直接作为玩家/研究者的调试工具，也可以作为 Sekiro Mod 制作的技术参考基础。