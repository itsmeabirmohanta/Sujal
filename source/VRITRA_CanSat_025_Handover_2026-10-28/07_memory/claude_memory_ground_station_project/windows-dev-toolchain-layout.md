---
name: windows-dev-toolchain-layout
description: "Where the C/C++/Python toolchain lives on this machine and the two traps (GCC hates spaces in its path, MinGW DLLs at runtime)."
metadata: 
  node_type: memory
  type: project
  originSessionId: ef6d6688-7035-4d49-b6c5-2351d42d315c
  modified: 2026-08-11T08:05:10.600Z
---

Installed 2026-08-11 by `D:\Software\VS Code\Scripts\Setup-DevEnv.ps1` (idempotent, re-runnable).

- VS Code 1.132.0 **portable** at `D:\Software\VS Code\VSCode` — the `data\` folder next to `Code.exe` holds all settings/extensions; nothing in `C:\Users`.
- CMake 4.4.2, Ninja 1.13.2, Python 3.14.7 under `D:\Software\VS Code\Toolchains\`.
- **GCC 16.2.0 / GDB / make at `D:\Software\MinGW64` — deliberately NOT under the install root.**

**Why:** GCC derives its internal library paths from its own location and passes them to `ld` unquoted, so a space in the toolchain path breaks every *link* (`ld.exe: cannot find D:/Software/VS: Permission denied`) while compiling still succeeds. Junctions and 8.3 short names don't help — GCC canonicalises them back to the spaced path.

**How to apply:** never relocate MinGW under a path with a space. Build tasks pass `-static` on purpose: without it, anything using `<iostream>` or threads needs libstdc++-6.dll/libgcc_s_seh-1.dll/libwinpthread-1.dll and dies with `0xC0000135` outside a configured shell.

Also on this machine: the python.org installer cannot run (fails extracting its own MSI payloads, `MSI 2203 / 0x80070003`, `/layout` produces nothing — AV interception), so Python is the core team's NuGet distribution — **no tkinter**; PyQt5 is what [[cansat-handoff-doc]]'s GCS uses anyway. pip intermittently hits an untrusted TLS root from an HTTPS-inspecting proxy; retry.
