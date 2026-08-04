# 更新日志

本项目严格遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 与
[语义化版本](https://semver.org/lang/zh-CN/)。

## 0.1.0 - 2026-08-04

### 新增

- 原生 Win32 主窗口：命令输入框 + 滚动输出区，Enter 快速执行
- Git Bash 后端桥接：经匿名管道拉起 `bash.exe`，GNU 工具层执行 Linux 命令
- Git Bash 自动定位：多候选路径探测 + `WINLINUX_GIT_BASH` 环境变量显式指定
- 异步管道 I/O：stdout/stderr 双后台读线程，输出实时回流 UI
- 后台线程 → UI 线程消息封送（临界区 + `WM_APP`），控件访问线程安全
- 关闭时优雅终止：向 bash 发送 `exit` 并回收子进程
- CMake 工程化：MinGW / MSVC 双编译、严格编译警告、单元测试
- 初始化测试：backend 定位 + `echo` / `pwd` 管道回环集成测试
- 构建脚本 `build.ps1`（Release/Debug、Clean、Run）
- GitHub Actions CI 双矩阵（MSVC + MinGW）

### 文档

- README、CONTRIBUTING、LICENSE（MIT）
