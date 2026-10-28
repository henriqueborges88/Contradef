# Contradef

**Dynamic binary instrumentation for investigating evasive Windows executables.**

Contradef is a C++ analysis tool built on [Intel Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html). It observes native Windows programs at runtime and records API activity, function calls, memory accesses, and instruction traces for later inspection. It helps malware analysts and reverse engineers investigate anti-analysis techniques, packed code, and program behavior without requiring the target's source code.

Tracing modules can be enabled individually or combined. Optional YARA scanning provides static context before execution and selects built-in instrumentation strategies for recognized protectors.

- [Capabilities](#capabilities)
- [Requirements](#requirements)
- [Build from source](#build-from-source)
- [Quick start](#quick-start)
- [Usage and options](#usage-and-options)
- [YARA integration](#yara-integration)
- [Output logs](#output-logs)
- [Architecture and source layout](#architecture-and-source-layout)
- [Scope and limitations](#scope-and-limitations)
- [Research publications](#research-publications)
- [License](#license)

## Capabilities

| Capability | What it provides |
| --- | --- |
| **API interception** | Parameters, return values, and contextual information for Windows APIs related to debugging, processes, threads, memory, files, the registry, firmware, and environment inspection. |
| **Function-call tracing** | Two complementary tracers for calls and returns, including direct and indirect control transfers and tail-call handling in the M2 tracer. |
| **Memory tracing** | Read and write addresses, accessed values, and string interpretation where available. |
| **Instruction tracing** | Runtime instruction flow with register values, flags, call/return information, and optional memory details. |
| **Disassembly logging** | Addresses and disassembled instructions encountered by Pin during instrumentation. |
| **Instruction-sequence detection** | Built-in patterns for suspicious instruction sequences, including complementary conditional jumps to the same target and constant-condition branches. |
| **YARA integration** | Rule matches and offsets from a scan of the main executable, with rule identifiers used to select protector-specific strategies. |
| **Heap and section tracking** | Allocation and PE section information used to recognize target-related code regions, including tracked heap allocations. |

## Requirements

The primary analysis target is **native Windows x64 executables**. Use a Windows x64 environment for the workflow below.

| Dependency | Configuration |
| --- | --- |
| **Intel Pin** | **3.31, Windows MSVC kit**. Extract it into `pin/` at the repository root. Pin is downloaded separately and is excluded from version control. |
| **Visual Studio 2022** | Install the **Desktop development with C++** workload, the **v143** build tools, and the optional **MSVC v142** build tools. `Contradef` uses v142; `libyara` and `yaraCdef` use v143. |
| **Windows SDK** | Install a Windows 10/11 SDK and check the SDK path described under [Build configuration notes](#build-configuration-notes). |
| **YARA 4.5.2** | Source is included in `yara/`; the solution builds it with the `yaraCdef` integration library. |
| **NuGet CLI** | Restores the Jansson and OpenSSL packages referenced by `yaracontradef/libyara/packages.config`. |

Use the **MSVC** kit from the [Intel Pin downloads page](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html). Intel lists Pin 3.31 as end of life. Pin 4.x uses a different Windows toolchain (LLVM/clang-cl), so the current Visual Studio projects require adaptation before using that series.

Contradef runs the target program. Analyze untrusted executables in a dedicated, isolated Windows environment with a recoverable snapshot and controlled network access. Use a trusted executable for the quick start below.

## Build from source

Clone the repository and extract the Pin kit so that `pin/pin.exe`, `pin/source/`, and `pin/intel64/` exist directly under the repository root:

```powershell
git clone https://github.com/henriqueborges88/Contradef.git
Set-Location .\Contradef
```

From a **Developer PowerShell for Visual Studio 2022**, restore the native dependencies:

```powershell
nuget restore .\Contradef.sln -ConfigFile .\yaracontradef\NuGet.Config -PackagesDirectory .\yaracontradef\packages
```

The package directory matters: the YARA project resolves its Jansson and OpenSSL headers and libraries relative to `yaracontradef/packages/`.

Review the configuration notes below, then open `Contradef.sln`, select **Release | x64**, and build the solution. The equivalent command is:

```powershell
MSBuild.exe .\Contradef.sln /m /p:Configuration=Release /p:Platform=x64
```

The root solution builds `libyara`, `yaraCdef`, and `Contradef` in dependency order. The main runtime files are:

```text
x64/Release/
├── Contradef.dll
└── yaraCdefx64.dll
```

Keep the matching `yaraCdefx64.dll` available when using YARA. Build outputs are local artifacts excluded from version control.

### Build configuration notes

The checked-in projects contain settings that need attention on a fresh machine:

- `Contradef.vcxproj` defines `_WINDOWS_H_PATH_` with an absolute path to Windows SDK **10.0.22621.0**. Adjust it to the SDK installed on your machine, including when Visual Studio retargets the project.
- Compiler options reference `/FIinclude/msvc_compat.h`, but that header is absent from the tracked source. Resolve this forced-include setting before building: provide the intended compatibility header or revise the setting for your build environment.
- Keep both v142 and v143 installed for the current solution. Changing only the solution's selected platform does not change the toolset requirements of its projects.

## Quick start

After building, run this from the repository root in PowerShell. It instruments the trusted Windows `whoami.exe` utility and records API activity and function calls:

```powershell
New-Item -ItemType Directory -Force .\logs | Out-Null

& .\pin\pin.exe -t .\x64\Release\Contradef.dll `
    -o .\logs\whoami `
    -intercept_fcn 1 `
    -trace_exfcn 1 `
    -- "$env:WINDIR\System32\whoami.exe"

Get-ChildItem .\logs\whoami.*.cdf
```

The command should produce `FunctionInterceptor`, `TraceFcnCall.M1`, and `TraceFcnCall.M2` logs with a process ID in their filenames. The instruction-sequence detector is also enabled by default and can print matches to the console.

If no YARA rules are available, Contradef prints a message and continues with dynamic instrumentation. See [YARA integration](#yara-integration) to configure scanning.

## Usage and options

```text
pin.exe [Pin options] -t <Contradef.dll> [Contradef options] -- <target.exe> [target arguments]
```

Place Contradef options **after `-t <Contradef.dll>` and before `--`**. Everything after `--` belongs to the target program. Boolean options accept `0` or `1`.

### Analysis modules

| Option | Default | Effect |
| --- | --- | --- |
| `-o <prefix>` | `contradef` | Sets the prefix for log filenames. A directory can be included; create it before running. |
| `-intercept_fcn <0\|1>` | `0` | Enables the general API interceptor and its log. Protector-specific hooks may also be selected by YARA matches. |
| `-trace_exfcn <0\|1>` | `0` | Enables both function-call tracers, producing M1 and M2 logs. |
| `-trace_mem <0\|1>` | `0` | Logs memory reads and writes. |
| `-trace_instr <0\|1>` | `0` | Logs executed instructions and associated state. |
| `-trace_dasm <0\|1>` | `0` | Logs disassembly as Pin instruments instructions. |
| `-detect_seq <0\|1>` | `1` | Enables instruction-sequence detection. |
| `-yara <rules.yar>` | Empty | Selects a YARA source rules file. If omitted, Contradef looks for `contradef.yar` beside the pintool DLL. |

The five tracing/logging modules default to off; enable the modules needed for each analysis. Sequence detection defaults to on independently of those modules.

### Instruction-trace controls

These controls apply to `-trace_instr 1`. Address-formatting options are also used by the M2 call tracer.

| Option | Default | Effect |
| --- | --- | --- |
| `-watch_thread <id>` | `-1` | Filters instruction-trace output by Pin thread ID; `-1` includes all threads. |
| `-only_main <0\|1>` | `0` | Applies the instruction tracer's target-focused filter. It also recognizes tracked heap and section ranges and can retain routine-entry instructions outside those ranges. |
| `-atmemory <0\|1>` | `0` | Adds memory-access details to the instruction trace. |
| `-call <0\|1>` | `1` | Includes call/return details in the instruction trace. |
| `-symbols <0\|1>` | `1` | Includes available symbol information. |
| `-full_img_name <0\|1>` | `0` | Uses full image paths in formatted addresses. |
| `-lines <0\|1>` | `1` | Includes source locations when debug information is available. |
| `-flush <0\|1>` | `0` | Flushes instruction-trace output after each instruction, increasing I/O cost. |
| `-silent <0\|1>` | `0` | Suppresses instruction-trace output while retaining its instrumentation. Other modules still write their logs. |

`-memory-only-str` (default `1`) and `-early_out` (default `0`) are declared in the option parser but are not consumed by the current implementation. They do not currently filter memory logs or stop tracing early.

### Example: instruction and memory analysis

Replace the example target path with a native x64 executable you intend to analyze:

```powershell
& .\pin\pin.exe -t .\x64\Release\Contradef.dll `
    -o .\logs\target `
    -trace_instr 1 -only_main 1 -atmemory 1 `
    -trace_mem 1 `
    -- "C:\Analysis\target.exe"
```

For broader collection, combine `-intercept_fcn 1`, `-trace_exfcn 1`, `-trace_mem 1`, `-trace_instr 1`, and `-trace_dasm 1` in one invocation. Full instruction and memory collection can produce large logs and substantial runtime overhead; start with the modules needed for your investigation.

## YARA integration

Contradef scans the main executable before starting the instrumented program. Provide a rules file explicitly:

```powershell
$env:PATH = "$(Resolve-Path .\x64\Release);$env:PATH"

& .\pin\pin.exe -t .\x64\Release\Contradef.dll `
    -yara "C:\Analysis\rules.yar" `
    -o .\logs\target `
    -intercept_fcn 1 `
    -- "C:\Analysis\target.exe"
```

The PATH entry makes `yaraCdefx64.dll` discoverable by the Windows loader. Alternatively, arrange for the matching wrapper DLL to be in the loader's search path for your deployment.

If `-yara` is omitted, the fallback location is **beside `Contradef.dll`**, for example `x64/Release/contradef.yar`. The repository does not ship that rules file; provide your own.

Matched rule identifiers populate the instrumentation strategy scope. The current code checks identifiers containing the case-sensitive strings `Obsidium` and `Themida` to select corresponding hooks and, where implemented, instruction-sequence callbacks. An arbitrary rule match is logged but does not automatically enable every tracing module. Protector-specific strategies can alter execution context or API results, so account for them when interpreting behavior.

## Output logs

Logs use the filename pattern `<prefix>.<PID>.<module>.cdf`. The default prefix is `contradef`; relative paths are resolved from the current working directory.

| Filename suffix | Contents |
| --- | --- |
| `FunctionInterceptor.cdf` | Intercepted API events and main-image section information. |
| `TraceFcnCall.M1.cdf` | Call/return records from the first function-call tracer. |
| `TraceFcnCall.M2.cdf` | Call/return and tail-call records from the second tracer. |
| `TraceMemory.cdf` | Instrumented memory reads and writes, values, and interpreted strings. |
| `TraceInstructions.cdf` | Executed instructions and associated register/control-flow information. |
| `TraceDisassembly.cdf` | Instruction addresses and disassembly recorded at instrumentation time. |
| `Yara.cdf` | YARA scan matches, rule identifiers, and string offsets. |

Only enabled modules create their corresponding trace logs. The YARA log is opened when an existing rules file is found. Sequence-detection messages are printed to the console rather than to a dedicated `.cdf` file.

The `.cdf` files are text logs. Inspect them with a text editor or search them with tools that can handle large files. Correlate available addresses, thread IDs, API names, and values across logs. `TraceDisassembly` reflects instrumentation order; use `TraceInstructions` to inspect runtime execution flow. Some outputs are buffered, so allow the target to finish normally when possible.

## Architecture and source layout

`Contradef.cpp` initializes Pin and delegates setup to `Instrumentation.cpp`. Initialization attempts YARA scanning, configures symbols and heap tracking, registers API interception and section tracking, and enables the selected trace modules. It then starts the target through `PIN_StartProgram()`.

API handlers implement individual instrumentation strategies. A notifier/observer mechanism forwards their events to the function-interceptor log. The sequence detector and its callbacks provide instruction-pattern matching and specialized handling.

```text
Contradef/
├── Contradef.sln                 # Main Visual Studio solution
├── Contradef/                    # Pintool source
│   ├── Contradef.cpp             # Entry point
│   ├── InitParams.cpp            # Command-line options and defaults
│   ├── Instrumentation.cpp       # Initialization and module registration
│   ├── FunctionInterceptor.cpp   # API strategy registration and event logging
│   ├── Inst*.cpp / Inst*.h        # Individual Windows API handlers
│   ├── Trace*.cpp / Trace*.h      # Call, memory, instruction, and disassembly logs
│   ├── InstructionSequence*.cpp  # Sequence detector and pattern definitions
│   ├── HeapTracker.cpp           # Allocation tracking
│   ├── SectionTracker.cpp        # Main executable's PE sections
│   └── YaraContradef.cpp         # YARA loading, scanning, and strategy scope
├── yaracontradef/                # YARA wrapper and library build projects
├── yara/                        # YARA 4.5.2 source and license
├── pin/                         # Separately downloaded Intel Pin kit (local)
└── LICENSE
```

## Scope and limitations

- The documented workflow targets native Windows x64 programs. The solution also contains x86/Win32 configurations; their presence alone does not establish feature parity. Managed runtimes and scripts have no dedicated analysis support.
- Memory and disassembly collection, and many API handlers, use a target-focused filter defined in `CallContext.h`. That filter includes the main image, tracked heap allocations, and known main-image sections. `-only_main` controls the instruction tracer separately; it is not a global switch for all modules.
- `DynExecTracker` and `ContextMonitor` source files exist, but their initialization calls are disabled in `Instrumentation.cpp`.
- Instrumentation and protector-specific handling can affect timing and program behavior. A trace describes the observed execution under the selected configuration; it does not guarantee complete detection or bypass of every evasion technique.

## Research publications

Contradef has been used in the following research works. Publication titles are retained in their original language, with English translations for reference.

1. **Henrique B. Campelo, Francisco S. S. Neto, and Eduardo L. Feitosa.** [Contradef: Uma Ferramenta de Instrumentação Binária Dinâmica para Análise de Malware Evasivo](https://doi.org/10.5753/sbseg_estendido.2025.10498). *Anais Estendidos do XXV Simpósio Brasileiro de Cibersegurança (SBSeg 2025)*, pp. 11–19, 2025. English title: *Contradef: A Dynamic Binary Instrumentation Tool for Evasive Malware Analysis*.
2. **Henrique Borges Campelo.** [Detecção e Identificação de Comportamento Evasivo em Executáveis para Windows Usando Instrumentação Binária Dinâmica](https://tede.ufam.edu.br/handle/tede/11741). Master's dissertation, Graduate Program in Informatics, Universidade Federal do Amazonas (UFAM), 2026. English title: *Detection and Identification of Evasive Behavior in Windows Executables Using Dynamic Binary Instrumentation*.

To cite the tool's paper:

```bibtex
@inproceedings{campelo2025contradef,
  author    = {Henrique B. Campelo and Francisco S. S. Neto and Eduardo L. Feitosa},
  title     = {Contradef: Uma Ferramenta de Instrumentação Binária Dinâmica para Análise de Malware Evasivo},
  booktitle = {Anais Estendidos do XXV Simpósio Brasileiro de Cibersegurança (SBSeg 2025)},
  pages     = {11--19},
  year      = {2025},
  publisher = {Sociedade Brasileira de Computação},
  doi       = {10.5753/sbseg_estendido.2025.10498},
  url       = {https://doi.org/10.5753/sbseg_estendido.2025.10498}
}
```

## License

Contradef is distributed under the [MIT License](LICENSE). Intel Pin and bundled third-party components retain their own license terms; see the Pin kit's licensing files and [YARA's license](yara/COPYING).
