# Agent 开发指南

本文件是仓库内的 AGENTS.md，供智能体（AI 编码助手）在改动本仓库时遵循。

## 构建与验证

一切改动后必须执行以下命令确保通过：

```powershell
.\build.ps1
```

这条命令完成：CMake 配置 → 编译 → 单元测试（ctest）。要求输出 `100% tests passed`。

## 运行实测

```powershell
.\build.ps1 -Run
```

## 关键架构不变量

- **后端必须是 Git Bash**，不是 PowerShell。任何命令都经 `bash.exe` 执行，禁止命令翻译层。
- 管道 I/O 走 `backend`（句柄）/ `pipeline`（双读线程）/ `executor`（状态机）三层，修改时保持分层不变量。
- 后台线程不得直接访问 Win32 控件，必须经临界区 + `WM_APP` 封送到 UI 线程。
- 新增源文件：C11，遵守 `cmake/CompilerWarnings.cmake` 的严格警告选项。
- 不改动 `LICENSE`，保持 MIT。

## 提交规范

Conventional Commits：`feat:` / `fix:` / `docs:` / `refactor:` / `build:` / `ci:` / `test:` / `chore:`。

## 运行依赖

Git for Windows（提供 `usr/bin` 的 GNU 工具）。构建依赖：MinGW-w64 或 MSVC + CMake ≥ 3.16。
