# winlinux

[![CI](https://github.com/your-org/winlinux/actions/workflows/ci.yml/badge.svg)](https://github.com/your-org/winlinux/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#)
[![C11](https://img.shields.io/badge/standard-C11-5B5B5B.svg)](#)

原生 Win32 编写的 **Git Bash 命令外壳**：输入 Linux 命令，由 Git for Windows 自带的 GNU 工具层执行，输出回流到窗口。专为「习惯于 Linux 命令、却在 PowerShell 里无法直接使用」的 AI 大模型与开发者设计。

> [!TIP]
> 你输入的是 `ls -la`、`grep foo`、`rm -rf`——它们被**原样**交给 `bash` 执行，不经过 PowerShell，不做命令翻译。语义 100% 等价于真实 shell。

## 特性

- 纯 C11 + Win32 API，零第三方运行时依赖
- 复用已安装的 Git Bash 工具层（`usr/bin` 下的 GNU 工具）
- 异步管道 I/O：stdout/stderr 实时回流，命令可交互（`ping`、`vi`、长任务均支持）
- 原生窗口 UI：输入框 + 滚动输出区，Enter 快捷执行
- 后台线程 → UI 线程消息封送，控件访问线程安全

## 快速开始

### 安装依赖

- [Git for Windows](https://git-scm.com/download/win)（提供 `usr/bin` 工具层）
- 任一 C 编译器（MinGW-w64 / MSVC）+ [CMake](https://cmake.org/) ≥ 3.16

### 构建

```powershell
# Windows PowerShell
.\build.ps1                    # Release 构建 + 单元测试
.\build.ps1 -Run               # 构建并启动
.\build.ps1 -Config Debug      # Debug 构建
.\build.ps1 -Clean             # 清除构建缓存后重构建
```

产物输出到 `build-release/winlinux.exe`（或 `build-debug/`）。

### 手动构建

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### 使用

启动 winlinux.exe，在输入框键入 Linux 命令（如 `uname -a`、`ls -lh`、`cat file.txt`、`ps aux`），回车执行。输出实时显示在下方的滚动区。关闭窗口即退出。

> [!NOTE]
> 若 Git 安装于非标准路径，可用环境变量 `WINLINUX_GIT_BASH` 显式指定 Git 根目录，例如：
> ```powershell
> $env:WINLINUX_GIT_BASH = "D:\Git"
> .\winlinux.exe
> ```

## 命令模式（CLI）

winlinux 除 GUI 外还提供无头 CLI 模式，供脚本、自动化与 AI Agent 直接调用：

```powershell
# 执行一条 Linux 命令，输出原样回流到 stdout，退出码与 bash 一致
winlinux.exe -c "ls -la"
winlinux.exe -c "grep winlinux README.md"
winlinux.exe -c "exit 42"    # 退出码 42
```

- `winlinux.exe -c "<command>"`：拉起 Git Bash 执行命令，完成后进程退出
- 退出码 = 命令的退出码（`0`/非 0），可用 `$LASTEXITCODE` 或 `Start-Process` 的 `ExitCode` 判断成败
- 复杂命令可含管道/替换：`winlinux.exe -c "ps aux | grep vim"`

> [!TIP]
> 由于 winlinux.exe 是 GUI 子系统，在 PowerShell 中获取退出码推荐用：
> ```powershell
> $p = Start-Process -FilePath .\winlinux.exe -ArgumentList '-c','ls -la' -Wait -PassThru -NoNewWindow
> $p.ExitCode
> ```

## 全局命令（Global Install）

一键把 winlinux 与整套 Linux 命令装进系统（用户级）PATH，让 `ls`、`grep`、`cat`、`ps` 等同名 Linux 命令在 **PowerShell / cmd / 任意终端** 里直接可用：

```powershell
.\install.ps1              # 安装，无需管理员
.\install.ps1 -GitRoot "D:\Git"   # 指定 Git 安装根
.\install.ps1 -Force       # 重新生成与覆盖
```

安装到 `%LOCALAPPDATA%\winlinux\bin` 会执行：
- 复制 `winlinux.exe`
- 生成 90+ 个 `.cmd` shim（`ls.cmd`、`grep.cmd`、…），经 Git Bash 转发 GNU 工具，保留 glob / 管道 / 退出码
- 将 bin 目录写入**用户级** `PATH`（改环境变量即可生效，无需 UAC）

> [!NOTE]
> 需在**新开**的终端中生效。若 `ls` 等命令在你的 shell 中之前被别名/内建遮蔽，优先用完整路径调试。

## 接入 AI 对话工具 / Agent

让 ChatGPT、Copilot、Cursor 等 AI 使用 Linux 命令的策略：

1. **推荐：全局安装后直接给 AI 一段约定**。AI 在终端里能直接敲 Linux 命令（无需前缀）：
   ```text
   这是 Windows 环境，但 Linux 命令 ls/grep/cat/ps 等已经全局可用（经 Git Bash 转发）。
   请直接用 Linux 语义执行命令。等价于真正的 Linux shell。
   例：列目录 -> ls -la        查看文件 -> cat README.md    过滤 -> grep winlinux README.md
   ```
2. **需要显式调用时**，用 winlinux 包裹：
   ```text
   winlinux -c "<linux命令>"    # 退出码 0=成功 / 非0=失败
   例：winlinux -c "ls -la"   winlinux -c "cat README.md"   winlinux -c "wc -l file.txt"
   ```
3. **自己写的脚本 / Agent**：`CreateProcess` / `Start-Process` 调用 `winlinux -c "<命令>"`，读 stdout 与退出码即可。

## 架构

```
+-------------+     WriteFile     +-------------------------------------+
| Win32 UI    |   (stdin)        |  Git Bash process (bash.exe -s)     |
| (window.c)  | ---------------> |   --noprofile --norc                 |
|  输入框/输出  |                  |  GNU tools: ls, grep, cat, ...     |
+------+------+     ReadFile      +-------------------------------------+
       ^       <---------------+
       |  PostMessage(WM_APP+1)         后台读线程 (stdout/stderr)
       +---------- app 回调封送------------+  (pipeline.c)
```

| 模块 | 职责 |
|------|------|
| [`src/core/backend.c`](src/core/backend.c) | Git Bash 定位（多候选路径探测 + 环境变量）、`CreateProcess` 拉起、匿名管道与句柄继承控制 |
| [`src/core/pipeline.c`](src/core/pipeline.c) | 异步管道：stdout/stderr 两个后台读线程 + 数据回调 |
| [`src/core/executor.c`](src/core/executor.c) | 命令执行状态机（IDLE/READY/DEAD），stdin 写入与回显 |
| [`src/ui/window.c`](src/ui/window.c) | 原生 Win32 窗口，后台回调经临界区 + `WM_APP` 封送到 UI 线程 |

## 测试

```bash
ctest --test-dir build --output-on-failure
```

`tests/` 内含进程拉起与管道回环的真实集成测试（对 Git Bash 发起 `echo`/`pwd` 并验证回流）。

## 命令接口参考（典型）

| Linux | 语义 |
|-------|------|
| `ls [opts]` | 列目录 |
| `grep PATTERN [FILE]` | 文本过滤 |
| `cat FILE` | 输出文件 |
| `pwd` / `uname` | 路径 / 系统信息 |
| `ps aux` / `top` | 进程查看 |

上述与全部 GNU 工具均由 Git Bash 提供，无需本应用内置实现。

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE)

## 版本历史

见 [CHANGELOG.md](CHANGELOG.md)。
