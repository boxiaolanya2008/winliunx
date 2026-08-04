# 更新日志

本项目严格遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 与
[语义化版本](https://semver.org/lang/zh-CN/)。

## 0.1.2 - 2026-08-04

### 新增

- 冷启动动画：启动闪屏（渐变背景 + "winlinux" 字标），`AnimateWindow` 淡入淡出后自动消失
- 运行动画：命令执行期间在底部状态条显示旋转活动指示器，命令完成后停止
- 增加底部状态条区域，为活动指示器预留展示空间

### 修复

- 恢复窗口默认尺寸 720×480（保留 Per-Monitor DPI 感知避免模糊）

## 0.1.1 - 2026-08-04

### 修复

- 修复 Git Bash 非交互模式下 `ls` 等命令无法找到：启动时注入环境块，前置
  `<git_root>\usr\bin` 到 `PATH` 并设置 `MSYSTEM`/`CHERE_INVOKING`
- 修复 2K 高分辨率屏窗口模糊与过小：声明 Per-Monitor DPI awareness，
  初始窗口尺寸自适应屏幕（宽 65% / 高 75%）
- 消除函数指针强转编译警告（union 拷贝）

### 测试

- 新增 `ls -la` 回归测试，覆盖 PATH 注入后外置命令解析

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
