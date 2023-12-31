#include "InstNtQueryInformationProcess.h"


/*
A função NtQueryInformationProcess é uma chamada de sistema do Windows, e a instrumentação para ela pode ser mais complexa do que para funções de API padrão.

Info sobre a função:
https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FProcess%2FNtQueryInformationProcess.html
https://pubhtml5.com/dtiq/ufpk/Windows_Kernel_Programming/274
https://forums.codeguru.com/showthread.php?371338-debugger-check-via-PEB
https://copyprogramming.com/howto/ntqueryinformationprocess-function-winternl-h
https://anti-debug.checkpoint.com/techniques/debug-flags.html
https://www.youtube.com/watch?v=WlE8abc8V-4
https://community.osr.com/discussion/228192

Verificar se pode ser utilizado para obter os argumentos de execução do processo e ver se está sendo debugado:
https://www.bordergate.co.uk/argument-spoofing/

*/


// Definição do PROCESSINFOCLASS e ProcessBasicInformation
// Mais informação sobre classes de comportamento evasivo -> https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess
/*
Value	Meaning
ProcessBasicInformation
0
Retrieves a pointer to a PEB structure that can be used to determine whether the specified process is being debugged, and a unique value used by the system to identify the specified process.
Use the CheckRemoteDebuggerPresent and GetProcessId functions to obtain this information.

ProcessDebugPort
7
Retrieves a DWORD_PTR value that is the port number of the debugger for the process. A nonzero value indicates that the process is being run under the control of a ring 3 debugger.
Use the CheckRemoteDebuggerPresent or IsDebuggerPresent function.

ProcessWow64Information
26
Determines whether the process is running in the WOW64 environment (WOW64 is the x86 emulator that allows Win32-based applications to run on 64-bit Windows).
Use the IsWow64Process2 function to obtain this information.

ProcessImageFileName
27
Retrieves a UNICODE_STRING value containing the name of the image file for the process.
Use the QueryFullProcessImageName or GetProcessImageFileName function to obtain this information.

ProcessBreakOnTermination
29
Retrieves a ULONG value indicating whether the process is considered critical.
Note  This value can be used starting in Windows XP with SP3. Starting in Windows 8.1, IsProcessCritical should be used instead.

ProcessTelemetryIdInformation
64
Retrieves a PROCESS_TELEMETRY_ID_INFORMATION_TYPE value that contains metadata about a process.

ProcessSubsystemInformation
75
Retrieves a SUBSYSTEM_INFORMATION_TYPE value indicating the subsystem type of the process. The buffer pointed to by the ProcessInformation parameter should be large enough to hold a single SUBSYSTEM_INFORMATION_TYPE enumeration.


Extraído de:
https://github.com/winsiderss/systeminformer/blob/ae0cdac96f9c752c881a801af03f5b74deb51fa9/phnt/include/ntpsapi.h#L110
https://ntdoc.m417z.com/processinfoclass

Outras estruturas: https://www.cin.ufpe.br/~frsn/arquivos/GnuWin32/include/ddk/ntapi.h
*/
typedef enum _PROCESSINFOCLASS
{
    ProcessBasicInformation, // q: PROCESS_BASIC_INFORMATION, PROCESS_EXTENDED_BASIC_INFORMATION // Themida antianalisys, é antidebug?
    ProcessQuotaLimits, // qs: QUOTA_LIMITS, QUOTA_LIMITS_EX // Themida antianalisys, é antidebug?
    ProcessIoCounters, // q: IO_COUNTERS
    ProcessVmCounters, // q: VM_COUNTERS, VM_COUNTERS_EX, VM_COUNTERS_EX2 // Themida antianalisys, é antidebug?
    ProcessTimes, // q: KERNEL_USER_TIMES
    ProcessBasePriority, // s: KPRIORITY
    ProcessRaisePriority, // s: ULONG
    ProcessDebugPort, // q: HANDLE // Debug present
    ProcessExceptionPort, // s: PROCESS_EXCEPTION_PORT (requires SeTcbPrivilege)
    ProcessAccessToken, // s: PROCESS_ACCESS_TOKEN
    ProcessLdtInformation, // qs: PROCESS_LDT_INFORMATION // 10
    ProcessLdtSize, // s: PROCESS_LDT_SIZE
    ProcessDefaultHardErrorMode, // qs: ULONG
    ProcessIoPortHandlers, // (kernel-mode only) // s: PROCESS_IO_PORT_HANDLER_INFORMATION
    ProcessPooledUsageAndLimits, // q: POOLED_USAGE_AND_LIMITS
    ProcessWorkingSetWatch, // q: PROCESS_WS_WATCH_INFORMATION[]; s: void
    ProcessUserModeIOPL, // qs: ULONG (requires SeTcbPrivilege)
    ProcessEnableAlignmentFaultFixup, // s: BOOLEAN
    ProcessPriorityClass, // qs: PROCESS_PRIORITY_CLASS
    ProcessWx86Information, // qs: ULONG (requires SeTcbPrivilege) (VdmAllowed)
    ProcessHandleCount, // q: ULONG, PROCESS_HANDLE_INFORMATION // 20
    ProcessAffinityMask, // (q >WIN7)s: KAFFINITY, qs: GROUP_AFFINITY
    ProcessPriorityBoost, // qs: ULONG
    ProcessDeviceMap, // qs: PROCESS_DEVICEMAP_INFORMATION, PROCESS_DEVICEMAP_INFORMATION_EX
    ProcessSessionInformation, // q: PROCESS_SESSION_INFORMATION // Themida antianalisys, é antidebug?
    ProcessForegroundInformation, // s: PROCESS_FOREGROUND_BACKGROUND
    ProcessWow64Information, // q: ULONG_PTR // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessImageFileName, // q: UNICODE_STRING // Themida antianalisys, é antidebug? Pesquisar mais e ver o processamento posterior se tem algum tratamento para nomes de arquivos ou processos 
    ProcessLUIDDeviceMapsEnabled, // q: ULONG
    ProcessBreakOnTermination, // qs: ULONG
    ProcessDebugObjectHandle, // q: HANDLE // 30 // Debug present
    ProcessDebugFlags, // qs: ULONG // Debug present
    ProcessHandleTracing, // q: PROCESS_HANDLE_TRACING_QUERY; s: PROCESS_HANDLE_TRACING_ENABLE[_EX] or void to disable
    ProcessIoPriority, // qs: IO_PRIORITY_HINT
    ProcessExecuteFlags, // qs: ULONG (MEM_EXECUTE_OPTION_*)
    ProcessTlsInformation, // PROCESS_TLS_INFORMATION // ProcessResourceManagement
    ProcessCookie, // q: ULONG
    ProcessImageInformation, // q: SECTION_IMAGE_INFORMATION // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessCycleTime, // q: PROCESS_CYCLE_TIME_INFORMATION // since VISTA
    ProcessPagePriority, // qs: PAGE_PRIORITY_INFORMATION
    ProcessInstrumentationCallback, // s: PVOID or PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION // 40  // Debug present
    ProcessThreadStackAllocation, // s: PROCESS_STACK_ALLOCATION_INFORMATION, PROCESS_STACK_ALLOCATION_INFORMATION_EX
    ProcessWorkingSetWatchEx, // q: PROCESS_WS_WATCH_INFORMATION_EX[]; s: void
    ProcessImageFileNameWin32, // q: UNICODE_STRING
    ProcessImageFileMapping, // q: HANDLE (input)
    ProcessAffinityUpdateMode, // qs: PROCESS_AFFINITY_UPDATE_MODE
    ProcessMemoryAllocationMode, // qs: PROCESS_MEMORY_ALLOCATION_MODE
    ProcessGroupInformation, // q: USHORT[]
    ProcessTokenVirtualizationEnabled, // s: ULONG  // pode ser antidebug? Pesquisar mais
    ProcessConsoleHostProcess, // qs: ULONG_PTR // ProcessOwnerInformation
    ProcessWindowInformation, // q: PROCESS_WINDOW_INFORMATION // 50
    ProcessHandleInformation, // q: PROCESS_HANDLE_SNAPSHOT_INFORMATION // since WIN8
    ProcessMitigationPolicy, // s: PROCESS_MITIGATION_POLICY_INFORMATION // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessDynamicFunctionTableInformation, // s: PROCESS_DYNAMIC_FUNCTION_TABLE_INFORMATION
    ProcessHandleCheckingMode, // qs: ULONG; s: 0 disables, otherwise enables
    ProcessKeepAliveCount, // q: PROCESS_KEEPALIVE_COUNT_INFORMATION
    ProcessRevokeFileHandles, // s: PROCESS_REVOKE_FILE_HANDLES_INFORMATION
    ProcessWorkingSetControl, // s: PROCESS_WORKING_SET_CONTROL (requires SeDebugPrivilege)
    ProcessHandleTable, // q: ULONG[] // since WINBLUE
    ProcessCheckStackExtentsMode, // qs: ULONG // KPROCESS->CheckStackExtents (CFG)
    ProcessCommandLineInformation, // q: UNICODE_STRING // 60
    ProcessProtectionInformation, // q: PS_PROTECTION
    ProcessMemoryExhaustion, // s: PROCESS_MEMORY_EXHAUSTION_INFO // since THRESHOLD
    ProcessFaultInformation, // s: PROCESS_FAULT_INFORMATION
    ProcessTelemetryIdInformation, // q: PROCESS_TELEMETRY_ID_INFORMATION
    ProcessCommitReleaseInformation, // qs: PROCESS_COMMIT_RELEASE_INFORMATION
    ProcessDefaultCpuSetsInformation, // qs: SYSTEM_CPU_SET_INFORMATION[5]
    ProcessAllowedCpuSetsInformation, // qs: SYSTEM_CPU_SET_INFORMATION[5]
    ProcessSubsystemProcess,
    ProcessJobMemoryInformation, // q: PROCESS_JOB_MEMORY_INFO
    ProcessInPrivate, // q: BOOLEAN; s: void // ETW // since THRESHOLD2 // 70
    ProcessRaiseUMExceptionOnInvalidHandleClose, // qs: ULONG; s: 0 disables, otherwise enables
    ProcessIumChallengeResponse,
    ProcessChildProcessInformation, // q: PROCESS_CHILD_PROCESS_INFORMATION
    ProcessHighGraphicsPriorityInformation, // qs: BOOLEAN (requires SeTcbPrivilege)
    ProcessSubsystemInformation, // q: SUBSYSTEM_INFORMATION_TYPE // since REDSTONE2
    ProcessEnergyValues, // q: PROCESS_ENERGY_VALUES, PROCESS_EXTENDED_ENERGY_VALUES
    ProcessPowerThrottlingState, // qs: POWER_THROTTLING_PROCESS_STATE
    ProcessReserved3Information, // ProcessActivityThrottlePolicy // PROCESS_ACTIVITY_THROTTLE_POLICY
    ProcessWin32kSyscallFilterInformation, // q: WIN32K_SYSCALL_FILTER
    ProcessDisableSystemAllowedCpuSets, // s: BOOLEAN // 80
    ProcessWakeInformation, // q: PROCESS_WAKE_INFORMATION
    ProcessEnergyTrackingState, // qs: PROCESS_ENERGY_TRACKING_STATE
    ProcessManageWritesToExecutableMemory, // MANAGE_WRITES_TO_EXECUTABLE_MEMORY // since REDSTONE3
    ProcessCaptureTrustletLiveDump,
    ProcessTelemetryCoverage, // q: TELEMETRY_COVERAGE_HEADER; s: TELEMETRY_COVERAGE_POINT
    ProcessEnclaveInformation, // Themida antianalisys, é antidebug? Pesquisar mais <-----------------
    ProcessEnableReadWriteVmLogging, // qs: PROCESS_READWRITEVM_LOGGING_INFORMATION
    ProcessUptimeInformation, // q: PROCESS_UPTIME_INFORMATION
    ProcessImageSection, // q: HANDLE
    ProcessDebugAuthInformation, // since REDSTONE4 // 90
    ProcessSystemResourceManagement, // s: PROCESS_SYSTEM_RESOURCE_MANAGEMENT
    ProcessSequenceNumber, // q: ULONGLONG
    ProcessLoaderDetour, // since REDSTONE5
    ProcessSecurityDomainInformation, // q: PROCESS_SECURITY_DOMAIN_INFORMATION
    ProcessCombineSecurityDomainsInformation, // s: PROCESS_COMBINE_SECURITY_DOMAINS_INFORMATION
    ProcessEnableLogging, // qs: PROCESS_LOGGING_INFORMATION
    ProcessLeapSecondInformation, // qs: PROCESS_LEAP_SECOND_INFORMATION
    ProcessFiberShadowStackAllocation, // s: PROCESS_FIBER_SHADOW_STACK_ALLOCATION_INFORMATION // since 19H1
    ProcessFreeFiberShadowStackAllocation, // s: PROCESS_FREE_FIBER_SHADOW_STACK_ALLOCATION_INFORMATION
    ProcessAltSystemCallInformation, // s: PROCESS_SYSCALL_PROVIDER_INFORMATION // since 20H1 // 100
    ProcessDynamicEHContinuationTargets, // s: PROCESS_DYNAMIC_EH_CONTINUATION_TARGETS_INFORMATION
    ProcessDynamicEnforcedCetCompatibleRanges, // s: PROCESS_DYNAMIC_ENFORCED_ADDRESS_RANGE_INFORMATION // since 20H2
    ProcessCreateStateChange, // since WIN11
    ProcessApplyStateChange,
    ProcessEnableOptionalXStateFeatures, // s: ULONG64 // optional XState feature bitmask
    ProcessAltPrefetchParam, // since 22H1
    ProcessAssignCpuPartitions,
    ProcessPriorityClassEx, // s: PROCESS_PRIORITY_CLASS_EX
    ProcessMembershipInformation, // q: PROCESS_MEMBERSHIP_INFORMATION
    ProcessEffectiveIoPriority, // q: IO_PRIORITY_HINT
    ProcessEffectivePagePriority, // q: ULONG
    MaxProcessInfoClass
} PROCESSINFOCLASS;

/*
classes usadas para detecção de debug e antianalise:
    ProcessBasicInformation, // q: PROCESS_BASIC_INFORMATION, PROCESS_EXTENDED_BASIC_INFORMATION // Themida antianalisys, é antidebug?
    ProcessQuotaLimits, // qs: QUOTA_LIMITS, QUOTA_LIMITS_EX // Themida antianalisys, é antidebug?
    ProcessVmCounters, // q: VM_COUNTERS, VM_COUNTERS_EX, VM_COUNTERS_EX2 // Themida antianalisys, é antidebug?
    ProcessDebugPort, // q: HANDLE // Debug present
    ProcessSessionInformation, // q: PROCESS_SESSION_INFORMATION // Themida antianalisys, é antidebug?
    ProcessWow64Information, // q: ULONG_PTR // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessImageFileName, // q: UNICODE_STRING // Themida antianalisys, é antidebug? Pesquisar mais e ver o processamento posterior se tem algum tratamento para nomes de arquivos ou processos
    ProcessDebugObjectHandle, // q: HANDLE // 30 // Debug present
    ProcessDebugFlags, // qs: ULONG // Debug present
    ProcessImageInformation, // q: SECTION_IMAGE_INFORMATION // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessInstrumentationCallback, // s: PVOID or PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION // 40  // Debug present
    ProcessTokenVirtualizationEnabled, // s: ULONG  // pode ser antidebug? Pesquisar mais
    ProcessMitigationPolicy, // s: PROCESS_MITIGATION_POLICY_INFORMATION // Themida antianalisys, é antidebug? Pesquisar mais
    ProcessEnclaveInformation, // Themida antianalisys, é antidebug? Pesquisar mais <-----------------

*/


std::map<ADDRINT, NtQueryInformationProcessArgs> InstNtQueryInformationProcess::argsMap;

VOID InstNtQueryInformationProcess::CallbackBefore(ADDRINT instrPtr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength) {
    NtQueryInformationProcessArgs args;
    args.ProcessHandle = ProcessHandle;
    args.ProcessInformationClass = ProcessInformationClass;
    args.ProcessInformation = ProcessInformation;
    args.ProcessInformationLength = ProcessInformationLength;
    args.ReturnLength = ReturnLength;

    argsMap[instrPtr] = args;

    std::cout << "[Contradef] NtQueryInformationProcess será chamada com os seguintes parâmetros:" << std::endl;
    std::cout << "[Contradef] ProcessHandle: " << ProcessHandle << std::endl;
    std::cout << "[Contradef] ProcessInformationClass: " << ProcessInformationClass << std::endl;
    std::cout << "[Contradef] ProcessInformation: " << ProcessInformation << std::endl;
    std::cout << "[Contradef] ProcessInformationLength: " << ProcessInformationLength << std::endl;
    std::cout << "[Contradef] ReturnLength (Endereço): " << ReturnLength << std::endl;

}

VOID InstNtQueryInformationProcess::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const NtQueryInformationProcessArgs& args = it->second;

        // Utilizar os valores armazenados
        std::cout << "[Contradef] NtQueryInformationProcess foi chamada com ProcessInformationClass: " << args.ProcessInformationClass << std::endl;

        // Limpeza após o uso
        argsMap.erase(it);
    }

    std::cout << "[Contradef] Valor de retorno de NtQueryInformationProcess: " << *retValAddr << std::endl;

}

VOID InstNtQueryInformationProcess::InstrumentFunction(RTN rtn) {
    PROTO protoNtQueryInformationProcess = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "NtQueryInformationProcess",
        PIN_PARG(ADDRINT),   // ProcessHandle
        PIN_PARG(ADDRINT),   // ProcessInformationClass
        PIN_PARG(ADDRINT),   // ProcessInformation
        PIN_PARG(ADDRINT),   // ProcessInformationLength
        PIN_PARG(ADDRINT),   // ReturnLength
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoNtQueryInformationProcess,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoNtQueryInformationProcess,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
        IARG_END);

    PROTO_Free(protoNtQueryInformationProcess);
}
