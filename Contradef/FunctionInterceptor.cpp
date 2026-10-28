#include "FunctionInterceptor.h"
#include "Notifier.h"
#include "SectionTracker.h"

static std::ofstream MainOutFile;
Notifier globalNotifier;
FunctionInterceptor fcnInterceptor; // As estratégias já estão adicionadas no Interceptor

FunctionInterceptor::FunctionInterceptor() {
    // Contrutor
}

FunctionInterceptor::~FunctionInterceptor() {
    // Não é necessário deletar os ponteiros de função

    strategyMap.clear();  // Limpa o mapa de estratégias
}

VOID HandleExecutionCompletedEvent(const EventData* data, void* context) {
	if (data->type != EventData::ExecutionCompleted) {
		return;
	}
	if (!KnobFunctionInterceptor || !MainOutFile.is_open()) {
		return;
	}

	auto executionEvent = static_cast<const ExecutionEventData*>(data);
	auto executionOutputText = executionEvent->executionInformation.outputText;
	CallContextLockGuard guard;
	MainOutFile << executionOutputText;
	MainOutFile.flush();  // Esvazia o buffer de saída
}

void FunctionInterceptor::InitStrategies() {
	if (IsStringInStrategyScope("Obsidium")) {
		std::cout << "[CONTRADEF] Instrumentando o Obsidium" << std::endl;
		strategyMap["CreateToolhelp32Snapshot"] = &InstCreateToolhelp32Snapshot::InstrumentFunction; // Precisa ser invocada para o correto funcionamento com o Obsidium
		strategyMap["CreateFileW"] = &InstCreateFileW::InstrumentFunction;
		strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
		strategyMap["ZwQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
		strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
		strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
		strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
		strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
		strategyMap["GetSystemFirmwareTable"] = &InstGetSystemFirmwareTable::InstrumentFunction;

	}

	if (IsStringInStrategyScope("Themida")) {
		std::cout << "[CONTRADEF] Instrumentando o Themida" << std::endl;
		strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
		strategyMap["ZwQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
		strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
		strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
		strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
		strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
		strategyMap["GetSystemFirmwareTable"] = &InstGetSystemFirmwareTable::InstrumentFunction;
		strategyMap["CreateWindowExA"] = &InstCreateWindowExA::InstrumentFunction;
		strategyMap["GetMessageA"] = &InstGetMessageA::InstrumentFunction;
		strategyMap["PostQuitMessage"] = &InstPostQuitMessage::InstrumentFunction;
		strategyMap["NtUserPostQuitMessage"] = &InstPostQuitMessage::InstrumentFunction;

	}

	if (!KnobFunctionInterceptor) {
		return;
	}

	// Com Problema, não chama o after -> strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado, já tentei removendo os parametros. O CB After funciona no modo Probed
    strategyMap["VirtualProtect"] = &InstVirtualProtect::InstrumentFunction;
    strategyMap["CreateFileW"] = &InstCreateFileW::InstrumentFunction;
    strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
    strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["GetModuleHandleW"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
    strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    strategyMap["ZwQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    strategyMap["GetCurrentProcess"] = &InstGetCurrentProcess::InstrumentFunction;
    strategyMap["GetCurrentThread"] = &InstGetCurrentThread::InstrumentFunction;
    strategyMap["NtSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
    strategyMap["ZwSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
    strategyMap["RtlGetNativeSystemInformation"] = &InstRtlGetNativeSystemInformation::InstrumentFunction;
    strategyMap["OpenThread"] = &InstOpenThread::InstrumentFunction;
    strategyMap["RtlInstallFunctionTableCallback"] = &InstRtlInstallFunctionTableCallback::InstrumentFunction;
    strategyMap["CreateEventW"] = &InstCreateEventW::InstrumentFunction;
    strategyMap["CreateThread"] = &InstCreateThread::InstrumentFunction;
    strategyMap["WaitForSingleObject"] = &InstWaitForSingleObject::InstrumentFunction;
    strategyMap["SuspendThread"] = &InstSuspendThread::InstrumentFunction;
    strategyMap["GetThreadContext"] = &InstGetThreadContext::InstrumentFunction;
	strategyMap["SetThreadContext"] = &InstSetThreadContext::InstrumentFunction;
    strategyMap["ResumeThread"] = &InstResumeThread::InstrumentFunction;
    strategyMap["SetEvent"] = &InstSetEvent::InstrumentFunction;
    strategyMap["CloseHandle"] = &InstCloseHandle::InstrumentFunction;
    strategyMap["ResetEvent"] = &InstResetEvent::InstrumentFunction;
    strategyMap["RtlAddFunctionTable"] = &InstRtlAddFunctionTable::InstrumentFunction;
    strategyMap["RtlDeleteFunctionTable"] = &InstRtlDeleteFunctionTable::InstrumentFunction;
    strategyMap["VirtualAlloc"] = &InstVirtualAlloc::InstrumentFunction;
    strategyMap["VirtualFree"] = &InstVirtualFree::InstrumentFunction;
    strategyMap["ReadFile"] = &InstReadFile::InstrumentFunction;
    strategyMap["GetProcAddress"] = &InstGetProcAddress::InstrumentFunction;
    strategyMap["QueueUserAPC2"] = &InstQueueUserAPC2::InstrumentFunction;
    strategyMap["SleepEx"] = &InstSleepEx::InstrumentFunction;
    strategyMap["RtlAddVectoredExceptionHandler"] = &InstRtlAddVectoredExceptionHandler::InstrumentFunction;
    strategyMap["RaiseException"] = &InstRaiseException::InstrumentFunction; 
    strategyMap["RtlRemoveVectoredExceptionHandler"] = &InstRtlRemoveVectoredExceptionHandler::InstrumentFunction;
    strategyMap["GetVersionExW"] = &InstGetVersionExW::InstrumentFunction;
    strategyMap["NtdllDefWindowProc_W"] = &InstNtdllDefWindowProc_W::InstrumentFunction;
    strategyMap["CreateToolhelp32Snapshot"] = &InstCreateToolhelp32Snapshot::InstrumentFunction;
    strategyMap["GetFileSizeEx"] = &InstGetFileSizeEx::InstrumentFunction;
    strategyMap["SetFilePointerEx"] = &InstSetFilePointerEx::InstrumentFunction;
    strategyMap["QueryDosDeviceW"] = &InstQueryDosDeviceW::InstrumentFunction;
    strategyMap["GetSystemFirmwareTable"] = &InstGetSystemFirmwareTable::InstrumentFunction;
    strategyMap["InstGetVolumePathNameW"] = &InstGetVolumePathNameW::InstrumentFunction;
    strategyMap["DeviceIoControl"] = &InstDeviceIoControl::InstrumentFunction;
    strategyMap["RtlQueryPerformanceCounter"] = &InstRtlQueryPerformanceCounter::InstrumentFunction;
    strategyMap["GetCommandLineA"] = &InstGetCommandLineA::InstrumentFunction;
    strategyMap["GetCommandLineW"] = &InstGetCommandLineW::InstrumentFunction;
    strategyMap["GetComputerNameA"] = &InstGetComputerNameA::InstrumentFunction;
    strategyMap["LCMapStringEx"] = &InstLCMapStringEx::InstrumentFunction;
    strategyMap["GetEnvironmentStringsW"] = &InstGetEnvironmentStringsW::InstrumentFunction;
    strategyMap["GetEnvironmentVariableW"] = &InstGetEnvironmentVariableW::InstrumentFunction;
	strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
	strategyMap["ZwQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
    strategyMap["NtQuerySystemInformationEx"] = &InstNtQuerySystemInformationEx::InstrumentFunction;
    strategyMap["ZwQuerySystemInformationEx"] = &InstNtQuerySystemInformationEx::InstrumentFunction;
    strategyMap["RtlGetNativeSystemInformation"] = &InstRtlGetNativeSystemInformation::InstrumentFunction;
	strategyMap["WriteFile"] = &InstWriteFile::InstrumentFunction;
	strategyMap["CompareStringEx"] = &InstCompareStringEx::InstrumentFunction;
	strategyMap["lstrcpynA"] = &InstLstrcpynA::InstrumentFunction;
	strategyMap["lstrcpy"] = &InstLstrcpy::InstrumentFunction;
	strategyMap["WriteConsoleW"] = &InstWriteConsoleW::InstrumentFunction;
	strategyMap["ExitProcess"] = &InstExitProcess::InstrumentFunction;
	strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
	strategyMap["DeleteFileA"] = &InstDeleteFileA::InstrumentFunction;
	strategyMap["ShellExecute"] = &InstShellExecute::InstrumentFunction;
	strategyMap["ShellExecuteEx"] = &InstShellExecuteEx::InstrumentFunction;
	strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
	strategyMap["BaseRegEnumKey"] = &InstBaseRegEnumKey::InstrumentFunction;
	strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
	strategyMap["RegEnumKeyA"] = &InstRegEnumKeyA::InstrumentFunction;
	strategyMap["RegQueryValueA"] = &InstRegQueryValueA::InstrumentFunction;
	strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;
	strategyMap["RegEnumKeyW"] = &InstRegEnumKeyW::InstrumentFunction;
	strategyMap["RegQueryValueW"] = &InstRegQueryValueW::InstrumentFunction;
	strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
	strategyMap["RegEnumKeyExA"] = &InstRegEnumKeyExA::InstrumentFunction;
	strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
	strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
	strategyMap["RegEnumKeyExW"] = &InstRegEnumKeyExW::InstrumentFunction;
	strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
	strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
	strategyMap["ZwOpenKey"] = &InstNtOpenKey::InstrumentFunction;
	strategyMap["NtEnumKey"] = &InstNtEnumKey::InstrumentFunction;
	strategyMap["ZwEnumKey"] = &InstNtEnumKey::InstrumentFunction;
	strategyMap["NtQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
	strategyMap["ZwQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
	strategyMap["NtQueryKey"] = &InstNtQueryKey::InstrumentFunction;
	strategyMap["ZwQueryKey"] = &InstNtQueryKey::InstrumentFunction;
	strategyMap["CoCreateInstance"] = &InstWmi::InstrumentFunction;
	strategyMap["ConnectServer"] = &InstWmi::InstrumentFunction;
	strategyMap["ExecQuery"] = &InstWmi::InstrumentFunction;
	strategyMap["CoCreateInstanceEx"] = &InstWmiEx::InstrumentFunction;
	strategyMap["ConnectServer"] = &InstWmiEx::InstrumentFunction;
	strategyMap["ExecQuery"] = &InstWmiEx::InstrumentFunction;
	strategyMap["GetObject"] = &InstWmiEx::InstrumentFunction;
	strategyMap["GetObjectA"] = &InstWmiEx::InstrumentFunction;
	strategyMap["GetObjectW"] = &InstWmiEx::InstrumentFunction;
	strategyMap["GetWriteWatch"] = &InstGetWriteWatch::InstrumentFunction;
	strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
	strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction;
	strategyMap["GetModuleHandleExW"] = &InstGetModuleHandleExW::InstrumentFunction;
	strategyMap["GetModuleFileNameExA"] = &InstGetModuleFileNameExA::InstrumentFunction;
	strategyMap["GetModuleFileNameExW"] = &InstGetModuleFileNameExW::InstrumentFunction;
	strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
	strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
	strategyMap["GetWindowTextA"] = &InstGetWindowTextA::InstrumentFunction;
	strategyMap["GetWindowTextW"] = &InstGetWindowTextW::InstrumentFunction;
	strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
	strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
	strategyMap["OutputDebugStringW"] = &InstOutputDebugStringW::InstrumentFunction;
	strategyMap["GetSystemTime"] = &InstGetSystemTime::InstrumentFunction;
	strategyMap["OutputDebugStringA"] = &InstOutputDebugStringA::InstrumentFunction;
	strategyMap["BlockInput"] = &InstBlockInput::InstrumentFunction;
	strategyMap["OpenProcess"] = &InstOpenProcess::InstrumentFunction;
	strategyMap["CreateProcess"] = &InstCreateProcess::InstrumentFunction;
	strategyMap["CreateProcessAsUser"] = &InstCreateProcessAsUser::InstrumentFunction;
	strategyMap["LoadLibraryEx"] = &InstLoadLibraryEx::InstrumentFunction;
	strategyMap["LoadLibraryExA"] = &InstLoadLibraryEx::InstrumentFunction;
	strategyMap["LoadLibraryExW"] = &InstLoadLibraryEx::InstrumentFunction;
	strategyMap["NtOpenProcess"] = &InstNtOpenProcess::InstrumentFunction;
	strategyMap["NtSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
	strategyMap["ZwSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
	strategyMap["NtSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
	strategyMap["ZwSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
	strategyMap["NtQueryObject"] = &InstNtQueryObject::InstrumentFunction;
	strategyMap["ZwQueryObject"] = &InstNtQueryObject::InstrumentFunction;
	strategyMap["NtYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
	strategyMap["ZwYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
	strategyMap["NtCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
	strategyMap["ZwCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
	strategyMap["NtCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
	strategyMap["ZwCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
	strategyMap["NtUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
	strategyMap["ZwUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
	/*strategyMap["NtUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;
	strategyMap["ZwUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;*/
	strategyMap["NtUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
	strategyMap["ZwUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
	strategyMap["NtSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
	strategyMap["ZwSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
	strategyMap["NtClose"] = &InstNtClose::InstrumentFunction;
	strategyMap["ZwClose"] = &InstNtClose::InstrumentFunction;
	strategyMap["NtGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
	strategyMap["ZwGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
	strategyMap["NtSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
	strategyMap["ZwSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
	strategyMap["NtContinue"] = &InstNtContinue::InstrumentFunction;
	strategyMap["ZwContinue"] = &InstNtContinue::InstrumentFunction;
	strategyMap["GetTickCount64"] = &InstGetTickCount64::InstrumentFunction;
	strategyMap["GetLocalTime"] = &InstGetLocalTime::InstrumentFunction;
	strategyMap["ZwQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
	strategyMap["NtQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;
	strategyMap["ZwQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;
	strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
	strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
	strategyMap["KiUserExceptionDispatcher"] = &InstKiUserExceptionDispatcher::InstrumentFunction;
	strategyMap["ShellExecute"] = &InstShellExecute::InstrumentFunction;
	strategyMap["WinExec"] = &InstWinExec::InstrumentFunction;
	strategyMap["CreateService"] = &InstCreateService::InstrumentFunction;
	strategyMap["StartService"] = &InstStartService::InstrumentFunction;
	strategyMap["CoCreateInstance"] = &InstCoCreateInstance::InstrumentFunction;
	strategyMap["QueryPerformanceCounter"] = &InstQueryPerformanceCounter::InstrumentFunction;
	strategyMap["EnumSystemFirmwareTables"] = &InstEnumSystemFirmwareTables::InstrumentFunction;
	strategyMap["NtQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
	strategyMap["ZwQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
	strategyMap["NtQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
	strategyMap["VirtualProtect"] = &InstVirtualProtect::InstrumentFunction;
	strategyMap["VirtualProtectEx"] = &InstVirtualProtectEx::InstrumentFunction;
	strategyMap["VirtualQuery"] = &InstVirtualQuery::InstrumentFunction;
	strategyMap["ShellExecuteEx"] = &InstShellExecuteEx::InstrumentFunction;
	strategyMap["VirtualQueryEx"] = &InstVirtualQueryEx::InstrumentFunction;
	strategyMap["NtQueryPerformanceCounter"] = &InstNtQueryPerformanceCounter::InstrumentFunction;
	strategyMap["ZwQueryPerformanceCounter"] = &InstNtQueryPerformanceCounter::InstrumentFunction;
	strategyMap["AllocateHeap"] = &InstRtlAllocateHeap::InstrumentFunction;
	strategyMap["FindWindowW"] = &InstFindWindowW::InstrumentFunction;
	strategyMap["LoadLibrary"] = &InstLoadLibrary::InstrumentFunction;
	strategyMap["RtlAllocateHeap"] = &InstRtlAllocateHeap::InstrumentFunction;
	strategyMap["FindWindowA"] = &InstFindWindowA::InstrumentFunction;
	strategyMap["LoadLibraryA"] = &InstLoadLibrary::InstrumentFunction;
	strategyMap["CreateWindowExA"] = &InstCreateWindowExA::InstrumentFunction;
	strategyMap["GetMessageA"] = &InstGetMessageA::InstrumentFunction;
	strategyMap["PostQuitMessage"] = &InstPostQuitMessage::InstrumentFunction;
	strategyMap["NtUserPostQuitMessage"] = &InstPostQuitMessage::InstrumentFunction;

}

void FunctionInterceptor::ExecuteStrategy(const std::string& name, RTN rtn, Notifier& globalNotifier) {
    auto it = strategyMap.find(name);
    if (it != strategyMap.end()) {
        (*it->second)(rtn, globalNotifier);
    }
}

bool FunctionInterceptor::HasStrategy(const std::string& name) const {
    return strategyMap.find(name) != strategyMap.end();
}

void FunctionInterceptor::ExecuteAllStrategies(IMG img, Notifier& globalNotifier) {
    ModuleRange range;
    range.start = IMG_LowAddress(img);
    range.end = IMG_HighAddress(img);
    range.name = IMG_Name(img);

    moduleRanges.push_back(range);

    std::string moduleName = IMG_Name(img);
    for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {
        RTN rtn = RTN_FindByAddress(IMG_LowAddress(img) + SYM_Value(sym));
        if (RTN_Valid(rtn)) {
			std::string funcName = RTN_Name(rtn);
            for (const auto& pair : strategyMap) {
                std::string strategyName = pair.first.c_str();
                if (strategyName == funcName) {
                    RTNFunction func = pair.second;
                    func(rtn, globalNotifier);
                }
            }
        }
    }

}


VOID InstrumentFunctionInterceptor(IMG img, VOID* v) {
	fcnInterceptor.ExecuteAllStrategies(img, globalNotifier);
}

int InitFunctionInterceptor() {
    InitCallContextStorage();
	// Criando observadores dinamicamente com o contexto nulo
	auto* executionCompleted = new Observer(HandleExecutionCompletedEvent, NULL);
	// Anexa os observers ao notificador
	globalNotifier.Attach(executionCompleted);

	SectionTracker::Init(&globalNotifier);

	if (KnobFunctionInterceptor) {
		string pid = decstr(WindowsAPI::getpid());
		string logsName = KnobOutputFile.Value();
		string logfilename = logsName + "." + pid + ".FunctionInterceptor.cdf";
		MainOutFile.open(logfilename.c_str(), std::ios::binary);
		MainOutFile.imbue(std::locale(""));
		MainOutFile << "\xEF\xBB\xBF"; // Adiciona BOM para indicar UTF-8
	}

	fcnInterceptor.InitStrategies();
	IMG_AddInstrumentFunction(InstrumentFunctionInterceptor, 0);
	return 1;
}