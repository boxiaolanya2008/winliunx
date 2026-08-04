# Contributing

欢迎为本项目贡献力量。请遵循以下约定，保证协作的规范性与可审查性。

## 分支与提交

- 使用 [Conventional Commits](https://www.conventionalcommits.org/zh-hans/) 规范：
  `feat:`、`fix:`、`docs:`、`refactor:`、`build:`、`ci:`、`test:`、`chore:`。
- 例：`feat: 支持管道输入`,`fix: 修复发布时关进程死锁`。
- 每个提交只做一件事，保持历史可读。

## 开发环境

- Windows + MinGW-w64（或 MSVC）+ CMake ≥ 3.16
- Git for Windows（运行期依赖，提供 GNU 工具层）
- 构建与测试：`.\build.ps1`

## 代码风格

- C11，遵守 `-Wall -Wextra -Wpedantic`，不允许隐式类型转换与未处理返回值
- 引入新模块时同步补充 `cmake/CompilerWarnings.cmake` 的编译选项（必要时）
- 新功能必须附带测试（`tests/`），并在本地 `.\build.ps1` 通过

## 测试

```bash
# 仓库根目录
.\build.ps1
```

所有改动合并前必须 `100% tests passed`。

## 许可证

为本仓库贡献即表示同意以 [MIT](LICENSE) 许可发布你的贡献。
