# winlinux

中文 · [English](README.en.md)

[![CI](https://github.com/boxiaolanya2008/winliunx/actions/workflows/ci.yml/badge.svg)](https://github.com/boxiaolanya2008/winliunx/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#)
[![C11](https://img.shields.io/badge/standard-C11-5B5B5B.svg)](#)

<!--
After pushing to GitHub, swap the CI badge above for your real repository badge:
[![CI](https://github.com/<owner>/<repo>/actions/workflows/ci.yml/badge.svg)](https://github.com/<owner>/<repo>/actions/workflows/ci.yml)
-->

A native Win32 shell that runs **Linux commands on Windows** through Git Bash. Type `ls -la`, `grep foo`, or `rm -rf`, and winlinux executes them with the GNU tooling bundled in Git for Windows, streaming the output back to a small native window.

It exists for anyone who thinks and writes in Linux — including AI coding agents that are prone to fumbling PowerShell syntax.

> [!TIP]
> `ls -la`, `grep foo`, `rm -rf` are passed **as-is** to `bash`. No PowerShell, no translation layer. The semantics match a real Linux shell 1:1.

## Why

- You know Linux, but your machine only speaks Windows.
- AI agents (ChatGPT, Copilot, opencode, etc.) are too often handed a PowerShell tool and fail.
- You want a single, predictable way to run Linux commands from Windows — interactively, from a script, or from an agent.

## Features

- Pure C11 + Win32 API — **zero third-party runtime dependencies**
- Reuses the GNU tool layer already shipped with Git for Windows (`usr/bin`)
- Asynchronous pipe I/O — stdout/stderr stream back live; interactive commands work (`ping`, `vi`, long-running jobs)
- Native window UI: command input + scrolling output area, Enter to run
- Background threads marshal output to the UI thread via a critical section + `WM_APP` message — safe control access
- Headless CLI mode (`winlinux -c "..."`) for scripts, automation, and agents, with bash exit codes preserved

## Quick Start

### Dependencies

- [Git for Windows](https://git-scm.com/download/win) (provides the `usr/bin` GNU tool layer)
- Any C compiler (MinGW-w64 or MSVC) + [CMake](https://cmake.org/) ≥ 3.16

### Build

```powershell
# Windows PowerShell
.\build.ps1                    # Release build + unit tests
.\build.ps1 -Run               # Build and launch
.\build.ps1 -Config Debug      # Debug build
.\build.ps1 -Clean             # Clean cache and rebuild
```

The binary lands at `build-release\winlinux.exe` (or `build-debug\`).

### Build Manually

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Use the GUI

Launch winlinux.exe, type a Linux command (`uname -a`, `ls -lh`, `cat file.txt`, `ps aux`), press Enter. Output appears in the scrolling area below. Closing the window exits cleanly.

> [!NOTE]
> If Git is installed somewhere unusual, point to its root with the `WINLINUX_GIT_BASH` environment variable:
> ```powershell
> $env:WINLINUX_GIT_BASH = "D:\Git"
> .\winlinux.exe
> ```

## CLI Mode (Headless)

winlinux also runs headless, for scripts, automation, and agents:

```powershell
winlinux.exe -c "ls -la"
winlinux.exe -c "grep winlinux README.md"
winlinux.exe -c "exit 42"    # exit code 42
```

- `winlinux.exe -c "<command>"` spawns Git Bash, runs the command, then exits.
- The process exit code equals the command's exit code (`0`/non-zero), so scripts and agents can judge success.
- Complex commands with pipes or substitution work: `winlinux.exe -c "ps aux | grep vim"`.

> [!TIP]
> winlinux.exe is a GUI-subsystem binary, so in PowerShell grab the exit code like this:
> ```powershell
> $p = Start-Process -FilePath .\winlinux.exe -ArgumentList '-c','ls -la' -Wait -PassThru -NoNewWindow
> $p.ExitCode
> ```

## Global Install (Linux Commands in Every Terminal)

One command puts winlinux plus a whole set of Linux commands into your **user-level** `PATH`, so `ls`, `grep`, `cat`, `ps`, and friends work directly in **PowerShell / cmd / any terminal**:

```powershell
.\install.ps1                    # install, no admin needed
.\install.ps1 -GitRoot "D:\Git"  # point to a custom Git root
.\install.ps1 -Force             # regenerate / overwrite shims
.\uninstall.ps1                  # remove shims and clean the user PATH
```

Installing into `%LOCALAPPDATA%\winlinux\bin`:

- Copies `winlinux.exe`
- Generates 90+ `.cmd` shims (`ls.cmd`, `grep.cmd`, …) that forward to Git Bash's GNU tools, preserving **glob expansion, pipes, and exit codes**
- Adds the bin directory to your **user** `PATH` (UAC not required)

> [!NOTE]
> Open a **new** terminal for the changes to take effect. If a shell previously shadowed `ls` (alias/builtin), debug with the full path.

## Using winlinux with Command-Line Agents

Many CLI agents (opencode, Claude- or Copilot-based CLIs, and the terminal-flavored ChatGPT) run against a restricted "whitelisted" shell that assumes plain Windows/PowerShell. Two rules get them using Linux commands:

### Option A — declare it in the agent's system prompt

Drop this line into the agent's tooling instructions. It makes the agent drop its PowerShell assumption and adopt Linux semantics:

```text
Although the user's system is Windows, the user has a way to make Windows run Linux commands.
```

If you also did the global install below, the same instruction lets the agent call `ls`, `grep`, `cat`, `ps` directly. For agents that need an explicit tool to invoke, add:

```text
To run a Linux command on Windows, use: winlinux -c "<linux command>"
Exit code 0 means success, non-zero means failure. Examples:
  winlinux -c "ls -la"
  winlinux -c "cat README.md"
  winlinux -c "grep winlinux README.md"
```

> [!TIP]
> Combine both into a single tool definition for the best result:
>
> > Although the user's system is Windows, the user has a way to make Windows run Linux commands. Run Linux commands with `winlinux -c "<command>"`, or use the globally installed `ls`/`grep`/`cat`/`ps`. Exit code 0 = success, non-zero = failure.

### Option B — let the agent call global commands directly

After `.\install.ps1`, any terminal the agent runs in can type Linux verbs straight up — no prompt injection needed:

```bash
ls -la
grep winlinux README.md
ps aux | grep vim
```

Use both together: **Option A** persuades the agent to allow / switch to Linux semantics; **Option B** makes sure the commands actually run.

## Connecting AI Chat Tools / Agents

1. **Recommended — global install + give the agent one instruction.** The agent can type Linux verbs with no prefix:
   ```text
   This is a Windows machine, but the Linux commands ls/grep/cat/ps are globally available
   (forwarded through Git Bash). Use Linux semantics directly. For example:
   ls -la        cat README.md        grep winlinux README.md
   ```
2. **For explicit invocation**, wrap with winlinux:
   ```text
   winlinux -c "<linux command>"    # 0 = success, non-zero = failure
   ```
3. **In your own scripts / agents**, spawn `winlinux -c "<command>"` via `CreateProcess` / `Start-Process` and read stdout plus the exit code.

## Architecture

```
+-------------+     WriteFile     +-------------------------------------+
| Win32 UI    |   (stdin)        |  Git Bash process (bash.exe -s)     |
| (window.c)  | ---------------> |   --noprofile --norc                 |
|  输入框/输出  |                  |  GNU tools: ls, grep, cat, ...     |
+------+------+     ReadFile      +-------------------------------------+
       ^       <---------------+
       |  PostMessage(WM_APP+1)          background readers (stdout/stderr)
       +---------- app callback marshalled -------------+  (pipeline.c)
```

| Module | Responsibility |
|--------|----------------|
| [`src/core/backend.c`](src/core/backend.c) | Locates Git Bash (path probing + env var), spawns it via `CreateProcess`, and controls anonymous pipes & handle inheritance |
| [`src/core/pipeline.c`](src/core/pipeline.c) | Async pipes: background reader threads for stdout/stderr + data callbacks |
| [`src/core/executor.c`](src/core/executor.c) | Command execution state machine (IDLE/READY/DEAD), stdin writes and echo |
| [`src/ui/window.c`](src/ui/window.c) | Native Win32 window; background callbacks marshalled via critical section + `WM_APP` |

## Tests

```bash
ctest --test-dir build --output-on-failure
```

`tests/` includes real integration tests that spawn Git Bash and verify the round-trip (`echo`, `pwd`, `ls`, and the headless `-c` mode with exit codes).

## Typical Commands

| Linux | Meaning |
|-------|---------|
| `ls [opts]` | list directory |
| `grep PATTERN [FILE]` | filter text |
| `cat FILE` | print a file |
| `pwd` / `uname` | path / system info |
| `ps aux` / `top` | inspect processes |

All of these—and the whole GNU toolbox—come from Git Bash; winlinux doesn't reimplement them.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)

## Version History

See [CHANGELOG.md](CHANGELOG.md).
