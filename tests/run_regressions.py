"""Exercise production safety routines with a small Pin API test double.

Run with Python 3 and a C++17 compiler, e.g.:
    python tests/run_regressions.py --compiler clang++
    python tests/run_regressions.py --compiler C:/path/to/zig.exe

This does not build the pintool or validate Pin's JIT integration.
"""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def source(name):
    data = (ROOT / "Contradef" / name).read_bytes()
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("cp1252")


def braced(text, marker):
    """Copy a production definition, including its balanced body."""
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if not depth:
                return text[start:pos + 1]
    raise AssertionError(f"Unterminated definition: {marker}")


PIN_STUB = r'''
#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

using ADDRINT = std::uintptr_t;
using UINT32 = std::uint32_t;
using INT32 = std::int32_t;
using THREADID = UINT32;
using BOOL = int;
using VOID = void;
using TLS_KEY = UINT32;
using IMG = int;
struct CONTEXT {};
struct EXCEPTION_INFO {};
struct PHYSICAL_CONTEXT {};
enum class EXCEPT_HANDLING_RESULT { EHR_HANDLED, EHR_UNHANDLED, EHR_CONTINUE_SEARCH };
constexpr TLS_KEY INVALID_TLS_KEY = UINT32(-1);
constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;
#define STATIC static
#define ASSERTX(value) assert(value)

struct PIN_MUTEX { std::mutex* native = nullptr; };
inline BOOL PIN_MutexInit(PIN_MUTEX* m) { m->native = new std::mutex; return TRUE; }
inline void PIN_MutexLock(PIN_MUTEX* m) { assert(m->native); m->native->lock(); }
inline void PIN_MutexUnlock(PIN_MUTEX* m) { m->native->unlock(); }

inline std::recursive_mutex clientMutex;
inline thread_local int clientDepth = 0;
inline void PIN_LockClient() { clientMutex.lock(); ++clientDepth; }
inline void PIN_UnlockClient() { --clientDepth; clientMutex.unlock(); }
inline IMG IMG_FindByAddress(ADDRINT address) { return address ? 1 : 0; }
inline BOOL IMG_Valid(IMG img) { return img != 0; }

inline thread_local THREADID currentThread = 0;
inline std::atomic<TLS_KEY> nextKey{0};
inline thread_local std::map<std::pair<TLS_KEY, THREADID>, void*> tls;
inline TLS_KEY PIN_CreateThreadDataKey(void*) { return nextKey++; }
inline THREADID PIN_ThreadId() { return currentThread; }
inline void* PIN_GetThreadData(TLS_KEY key, THREADID tid) { return tls[{key, tid}]; }
inline BOOL PIN_SetThreadData(TLS_KEY key, void* value, THREADID tid) {
    tls[{key, tid}] = value; return TRUE;
}

inline thread_local ADDRINT readableBase = 0;
inline thread_local std::size_t readableSize = 0;
inline thread_local ADDRINT deniedWrite = 0;
inline std::size_t PIN_SafeCopy(void* dest, const void* src, std::size_t size) {
    const ADDRINT input = reinterpret_cast<ADDRINT>(src);
    const ADDRINT output = reinterpret_cast<ADDRINT>(dest);
    if (input < 0x10000 || output < 0x10000 || output == deniedWrite) return 0;
    if (readableBase && input >= readableBase && input <= readableBase + readableSize) {
        size = std::min(size, readableSize - (input - readableBase));
    }
    if (size) std::memcpy(dest, src, size);
    return size;
}
'''


PRELUDE = r'''
#include "pin.H"
#include "CallContext.h"
#include "SafeMemory.h"
#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <thread>
#include <vector>

using DWORD = std::uint32_t;
constexpr UINT32 ProcessDebugFlags = 31;
struct QueryArgs {
    ADDRINT ProcessInformation;
    UINT32 ProcessInformationLength;
    UINT32 ProcessInformationClass;
};
'''


DETECTOR_FIXTURE = r'''
struct InstrumentedInstruction {
    static int live;
    ADDRINT instAddr = 0;
    int insClass = 0;
    InstrumentedInstruction() { ++live; }
    ~InstrumentedInstruction() { --live; }
};
int InstrumentedInstruction::live = 0;
struct InstructionToMatch { bool isAnyInst = false; int insClass = 0; };
struct SequenceInstructions {
    std::vector<std::vector<InstructionToMatch*>> instruction_sequences;
    int matchCount = 0;
    int matchOcurrenceIndex = -1;
    int matchOcurrenceIndexEnd = -1;
    void (*match_callback)(THREADID, ADDRINT, CONTEXT*, SequenceInstructions&, void*) = nullptr;
    void* v = nullptr;
};
bool InstMatch(const InstrumentedInstruction&, const InstructionToMatch&) { return false; }
class InstructionSequenceDetector {
public:
    // PRODUCTION_THREAD_DATA
    ThreadData data;
    std::size_t max_sequence_length = 2;
    std::vector<SequenceInstructions*> instruction_sequences;
    ThreadData* GetThreadData() { return &data; }
    void DisassembledInstruction(ADDRINT, InstrumentedInstruction*);
};
'''


M2_FIXTURE = r'''
namespace M2Test {
// PRODUCTION_TRACE_THREAD_STATE
static TLS_KEY gThreadKey = PIN_CreateThreadDataKey(nullptr);
static BOOL enableIdent = false;
inline bool IsOn() { return true; }
inline bool WantCall(ADDRINT, ADDRINT) { return true; }
// PRODUCTION_THREAD_STATE_ACCESS
// PRODUCTION_THREAD_FINI
static void LogFuncCall(THREADID tid, ADDRINT, ADDRINT, BOOL, ADDRINT, ADDRINT,
                       ADDRINT, ADDRINT, ADDRINT, ADDRINT) {
    if (enableIdent) ++ThreadState(tid).indent;
}
// PRODUCTION_DIRECT_CALL
// PRODUCTION_INDIRECT_CALL
}
'''


TESTS = r'''
void test_context_pairing() {
    InitCallContextStorage();
    CallContextMap contexts;
    CallSequenceNumber counter;
    std::vector<std::thread> workers;
    constexpr int threads = 8, iterations = 4000;
    for (THREADID tid = 0; tid < threads; ++tid) workers.emplace_back([&, tid] {
        for (int i = 0; i < iterations; ++i) {
            CallContext outer(1, tid, 0, nullptr), inner(2, tid, 0, nullptr);
            SaveCallContext(contexts, {10, tid}, &outer);
            SaveCallContext(contexts, {10, tid}, &inner);
            counter++;
            assert(TakeCallContext(contexts, {11, tid}) == nullptr);
            assert(TakeCallContext(contexts, {10, tid}) == &inner);
            assert(TakeCallContext(contexts, {10, tid}) == &outer);
            assert(TakeCallContext(contexts, {10, tid}) == nullptr);
        }
    });
    for (auto& worker : workers) worker.join();
    assert(contexts.empty());
    assert(UINT32(counter) == threads * iterations);
    std::ostringstream log;
    log << counter;
    assert(log.str() == "32000");
    std::cout << "PASS: concurrent/reentrant contexts and completion counter\n";
}

void test_wide_strings() {
    for (std::size_t n : {0u, 5u, 1024u, 1026u}) {
        std::wstring input(n, L'A');
        readableBase = reinterpret_cast<ADDRINT>(input.c_str());
        readableSize = (input.size() + 1) * sizeof(wchar_t);
        assert(ConvertAddrToWideStringSafe(readableBase, 1024) == input.substr(0, 1024));
    }
    wchar_t noTerminator[] = {L'A', L'B', L'C'};
    readableBase = reinterpret_cast<ADDRINT>(noTerminator);
    readableSize = sizeof(noTerminator);
    assert(ConvertAddrToWideStringSafe(readableBase, 1024) == L"ABC");
    readableBase = 0;
    assert(ConvertAddrToWideStringSafe(0, 1024).empty());
    std::cout << "PASS: bounded, long, unterminated and inaccessible strings\n";
}

void test_debug_flags() {
    DWORD flags = 0;
    QueryArgs args{reinterpret_cast<ADDRINT>(&flags), sizeof(flags), ProcessDebugFlags};
    apply_debug_flags(-1, &args);
    assert(flags == 0);
    apply_debug_flags(0, &args);
    assert(flags == 1);
    flags = 2;
    apply_debug_flags(0, &args);
    assert(flags == 2);
    flags = 0;
    args.ProcessInformationLength = 1;
    apply_debug_flags(0, &args);
    assert(flags == 0);
    args.ProcessInformationLength = sizeof(flags);
    deniedWrite = args.ProcessInformation;
    apply_debug_flags(0, &args);
    assert(flags == 0);
    deniedWrite = 0;
    args.ProcessInformation = 0;
    apply_debug_flags(0, &args);
    args.ProcessInformation = 1;
    apply_debug_flags(0, &args);
    std::cout << "PASS: original debug-flags countermeasure and invalid buffers\n";
}

void test_lock_and_exception() {
    assert(IsFromUnknownImg(0) == TRUE);
    assert(clientDepth == 0);
    assert(IsFromUnknownImg(1) == FALSE);
    assert(clientDepth == 0);
    assert(ExceptionHandler(0, nullptr, nullptr, nullptr) ==
           EXCEPT_HANDLING_RESULT::EHR_CONTINUE_SEARCH);
    std::cout << "PASS: balanced client lock and exception propagation\n";
}

void test_detector_ownership() {
    {
        InstructionSequenceDetector detector;
        for (int i = 0; i < 20000; ++i) {
            detector.DisassembledInstruction(i, new InstrumentedInstruction);
            assert(InstrumentedInstruction::live <= 2);
        }
    }
    assert(InstrumentedInstruction::live == 0);
    {
        InstructionSequenceDetector detector;
        detector.max_sequence_length = 0;
        detector.DisassembledInstruction(0, new InstrumentedInstruction);
    }
    assert(InstrumentedInstruction::live == 0);
    std::cout << "PASS: bounded instruction ownership and thread-data cleanup\n";
}

void test_m2_thread_state() {
    std::vector<std::thread> workers;
    for (THREADID tid = 0; tid < 8; ++tid) workers.emplace_back([tid] {
        currentThread = tid;
        for (int i = 0; i < 1000; ++i) {
            M2Test::EmitDirectCall(tid, tid + 10, 100, FALSE, 0, 0, 0, 0, 0, 0);
            auto& calls = M2Test::ThreadState(tid).calls;
            assert(calls.size() == 1 && calls.top() == tid + 10);
            M2Test::EmitDirectCall(tid, 200, 300, TRUE, 0, 0, 0, 0, 0, 0);
            assert(calls.size() == 2 && calls.top() == 200);
            calls.pop();
            assert(calls.top() == tid + 10);
            calls.pop();
            M2Test::EmitIndirectCall(tid, tid + 20, 100, 0, 0, 0, 0, 0, 0);
            assert(calls.size() == 1 && calls.top() == tid + 20);
            calls.pop();
        }
        M2Test::ThreadFini(tid, nullptr, 0, nullptr);
        assert(PIN_GetThreadData(M2Test::gThreadKey, tid) == nullptr);
    });
    for (auto& worker : workers) worker.join();
    std::cout << "PASS: independent M2 stacks, tail calls and thread cleanup\n";
}

int main() {
    test_context_pairing();
    test_wide_strings();
    test_debug_flags();
    test_lock_and_exception();
    test_detector_ownership();
    test_m2_thread_state();
}
'''


def build_fixture():
    utils = source("utils.cpp")
    nt = source("InstNtQueryInformationProcess.cpp")
    guard = nt[nt.index("        // Preserve the debug-flags"):nt.index(
        "        if (false && ProcessInformationClass")]
    functions = "\n".join([
        braced(utils, "size_t GetSafeWStringLength("),
        braced(utils, "std::wstring ConvertAddrToWideStringSafe("),
        braced(source("InstrumentationUtils.cpp"), "BOOL IsFromUnknownImg("),
        braced(source("Instrumentation.cpp"), "EXCEPT_HANDLING_RESULT ExceptionHandler("),
        "void apply_debug_flags(int status, QueryArgs* args) {\n" + guard + "}\n",
    ])
    detector = DETECTOR_FIXTURE.replace(
        "// PRODUCTION_THREAD_DATA",
        braced(source("InstructionSequenceDetector.h"), "struct ThreadData") + ";",
    )
    detector += braced(source("InstructionSequenceDetector.cpp"),
                       "VOID InstructionSequenceDetector::DisassembledInstruction(")
    m2 = source("TraceFcnCallM2.cpp")
    tracer = M2_FIXTURE
    for tag, marker, suffix in [
        ("TRACE_THREAD_STATE", "struct TraceThreadState", ";"),
        ("THREAD_STATE_ACCESS", "static TraceThreadState& ThreadState(", ""),
        ("THREAD_FINI", "static VOID ThreadFini(", ""),
        ("DIRECT_CALL", "static VOID EmitDirectCall(", ""),
        ("INDIRECT_CALL", "static VOID EmitIndirectCall(", ""),
    ]:
        tracer = tracer.replace("// PRODUCTION_" + tag, braced(m2, marker) + suffix)
    return PRELUDE + functions + detector + tracer + TESTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("clang++") or shutil.which("g++"))
    args = parser.parse_args()
    if not args.compiler:
        parser.error("Provide a C++17 compiler using --compiler")
    with tempfile.TemporaryDirectory(prefix="contradef-regressions-") as folder:
        directory = Path(folder)
        (directory / "pin.H").write_text(PIN_STUB, encoding="utf-8")
        cpp = directory / "regressions.cpp"
        cpp.write_text(build_fixture(), encoding="utf-8")
        executable = directory / "regressions.exe"
        compiler = [args.compiler]
        if Path(args.compiler).stem.lower() == "zig":
            compiler += ["c++"]
        subprocess.run(compiler + ["-std=c++17", "-O1", "-g", "-pthread",
                       "-I" + str(directory), "-I" + str(ROOT / "Contradef"),
                       str(cpp), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True, timeout=60)


if __name__ == "__main__":
    main()
