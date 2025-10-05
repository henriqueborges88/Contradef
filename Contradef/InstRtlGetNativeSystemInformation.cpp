#include "InstRtlGetNativeSystemInformation.h"
#include "HeapTracker.h"
#include "DynExecTracker.h"
#include "SectionTracker.h"


std::map<CallContextKey, CallContext*> InstRtlGetNativeSystemInformation::callContextMap;
UINT32 InstRtlGetNativeSystemInformation::imgCallId = 0;
UINT32 InstRtlGetNativeSystemInformation::fcnCallId = 0;
Notifier* InstRtlGetNativeSystemInformation::globalNotifierPtr;


VOID printParentProcess(DWORD parentProcessId) {
    using namespace WindowsAPI;

    // Abrir o processo pai para obter o nome
    HANDLE hParentProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, parentProcessId);
    if (hParentProcess) {

        WCHAR parentProcessName[MAX_PATH] = { 0 };
        if (GetModuleBaseName(hParentProcess, nullptr, parentProcessName, MAX_PATH)) {
            std::wstring ppName(parentProcessName);
            std::string pName = WStringToString(ppName);
            if (ppName == L"pin.exe") {
                std::cout << "[CONTRADEF]bInstrumentation PIN detected. (parent process name: " << pName << ")" << std::endl;
            }
            else {
                std::cout << "[CONTRADEF] Instrumentation PIN not detected. (parent process name: " << pName << ")" << std::endl;
            }
        }
        else {
            std::cerr << "[CONTRADEF] Error getting parent process name.\n";
        }
        CloseHandle(hParentProcess);
    }
    else {
        std::cerr << "[CONTRADEF] Error opening parent process.\n";
    }
}

VOID InstRtlGetNativeSystemInformation::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RtlGetNativeSystemInformationArgs* args = new RtlGetNativeSystemInformationArgs;
    args->SystemInformationClass = SystemInformationClass;
    args->SystemInformation = SystemInformation;
    args->SystemInformationLength = SystemInformationLength;
    args->ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;

    stringStream << std::endl << "[+] RtlGetNativeSystemInformation..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        SystemInformationClass: " << SystemInformationClass << std::endl;
    stringStream << "        SystemInformation: " << std::hex << SystemInformation << std::dec << std::endl;
    stringStream << "        SystemInformationLength: " << SystemInformationLength << " bytes" << std::endl;
    stringStream << "        ReturnLength (Address): " << std::hex << ReturnLength << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of obtaining system information" << std::endl;

}

VOID InstRtlGetNativeSystemInformation::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnVal, ADDRINT* returnAddress, ADDRINT ReturnLength) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }


    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;
        const RtlGetNativeSystemInformationArgs* args = reinterpret_cast<const RtlGetNativeSystemInformationArgs*>(callContext->functionArgs);

        ULONG returnLengthValue = 0;
        if (ReturnLength != 0) {
            PIN_SafeCopy(&returnLengthValue, reinterpret_cast<ULONG*>(args->ReturnLength), sizeof(ULONG));
        }

        stringStream << "    Return Value: " << std::hex << returnVal << std::dec << std::endl;
        if (ReturnLength != 0) {
            stringStream << "    ReturnLength: " << returnLengthValue << " bytes" << std::endl;
        }
        stringStream << "  [-] Obtaining system information completed" << std::endl;

        // CONTRAMEDIDA
        if (args->SystemInformationClass == 5) {
            using namespace WindowsAPI;
            DWORD currentProcessId = GetCurrentProcessId();

            PSYSTEM_PROCESS_INFORMATION processInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(args->SystemInformation);
            while (processInfo) {
                if (reinterpret_cast<DWORD>(processInfo->UniqueProcessId) == currentProcessId) {
                    stringStream << "  [-] Applying countermeasure" << std::endl;

                    DWORD* parentProcessId = reinterpret_cast<DWORD*>(&(processInfo->Reserved2));
                    DWORD fakePID = GetProcessIdByName("explorer.exe");
                    stringStream << "    Replacing the PID of the parent process (pin.exe - " << *parentProcessId << ") with that of explorer.exe (" << fakePID << ")" << std::endl;
                    *parentProcessId = fakePID;
                    stringStream << "  [-] Countermeasure applied" << std::endl;

                    break;
                }

                // Avançar para o próximo processo
                if (processInfo->NextEntryOffset == 0) {
                    break;
                }
                processInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
                    reinterpret_cast<BYTE*>(processInfo) + processInfo->NextEntryOffset
                    );
            }
        }
        else if (args->SystemInformationClass == 11) {
            stringStream << "  [-] Decoding / rewriting SYSTEM_MODULE_INFORMATION -> one entry (kd.dll)" << std::endl;

            // Ponteiro para o buffer no processo-alvo
            PSYSTEM_MODULE_INFORMATION moduleInfo =
                reinterpret_cast<PSYSTEM_MODULE_INFORMATION>(args->SystemInformation);

            // Leitura segura do NumberOfModules para log
            ULONG originalCount = 0;
            if (PIN_SafeCopy(&originalCount, &moduleInfo->NumberOfModules, sizeof(ULONG)) != sizeof(ULONG)) {
                stringStream << "    ! Failed to read NumberOfModules" << std::endl;
                goto done_modules;
            }
            stringStream << "    Original modules: " << originalCount << std::endl;

            // Prepara a entrada fake "kd.dll"
            SYSTEM_MODULE_INFORMATION_ENTRY fake{};
            // (Campos opcionais, só para não parecer tudo zero)
            fake.ImageBase = nullptr;
            fake.ImageSize = 0;
            fake.Flags = 0;
            fake.LoadOrderIndex = 0;
            fake.InitOrderIndex = 0;
            fake.LoadCount = 1;

            // Caminho e nome
            const char fullPath[] = "\\SystemRoot\\System32\\kd.dll";
            // fullPath deve caber em 256 bytes (cabe folgado)
            size_t pathLen = strlen(fullPath);
            if (pathLen >= sizeof(fake.FullPathName)) {
                stringStream << "    ! Path too long for FullPathName" << std::endl;
                goto done_modules;
            }
            // Copia o caminho completo
            memset(fake.FullPathName, 0, sizeof(fake.FullPathName));
            memcpy(fake.FullPathName, fullPath, pathLen);

            // Offset pro nome base dentro de FullPathName
            // (posição do 'k' em kd.dll)
            const char* base = strrchr(fullPath, '\\');
            USHORT baseOffset = 0;
            if (base != nullptr) {
                baseOffset = static_cast<USHORT>(base - fullPath + 1);
            }
            fake.OffsetToFileName = baseOffset;

            // Verifica se o buffer retornado pelo sistema suporta pelo menos 1 entrada
            ULONG neededLen = static_cast<ULONG>(sizeof(ULONG) + sizeof(SYSTEM_MODULE_INFORMATION_ENTRY)); // count + 1 entry
            ULONG retLenFromApi = 0;
            if (ReturnLength != 0 && args->ReturnLength) {
                PIN_SafeCopy(&retLenFromApi, reinterpret_cast<const VOID*>(args->ReturnLength), sizeof(ULONG));
            }
            // Se não temos ReturnLength do chamador, usamos um valor "alto" para seguir.
            ULONG effectiveBufLen = (retLenFromApi != 0) ? retLenFromApi : (neededLen + 4096);

            if (effectiveBufLen < neededLen) {
                stringStream << "    ! Caller buffer too small (" << effectiveBufLen
                    << " < " << neededLen << "), skipping rewrite." << std::endl;
                goto done_modules;
            }

            // Escreve NumberOfModules = 1
            {
                ULONG one = 1;
                if (PIN_SafeCopy(&moduleInfo->NumberOfModules, &one, sizeof(ULONG)) != sizeof(ULONG)) {
                    stringStream << "    ! Failed to write NumberOfModules=1" << std::endl;
                    goto done_modules;
                }
            }

            // Escreve a primeira entrada com nosso módulo fake
            {
                SYSTEM_MODULE_INFORMATION_ENTRY* firstEntry =
                    reinterpret_cast<SYSTEM_MODULE_INFORMATION_ENTRY*>(
                        reinterpret_cast<BYTE*>(&moduleInfo->Modules[0]) + 0 * sizeof(SYSTEM_MODULE_INFORMATION_ENTRY));

                if (PIN_SafeCopy(firstEntry, &fake, sizeof(SYSTEM_MODULE_INFORMATION_ENTRY))
                    != sizeof(SYSTEM_MODULE_INFORMATION_ENTRY)) {
                    stringStream << "    ! Failed to write fake module entry" << std::endl;
                    goto done_modules;
                }
            }

            // Zera o restante do buffer (higieniza)
            {
                ULONG toZero = 0;
                if (effectiveBufLen > neededLen) {
                    toZero = effectiveBufLen - neededLen;
                }
                if (toZero > 0) {
                    BYTE* tail =
                        reinterpret_cast<BYTE*>(moduleInfo) + neededLen;
                    // zera em blocos para evitar grandes cópias de uma vez
                    const size_t CHUNK = 1024;
                    BYTE zeros[CHUNK] = { 0 };
                    while (toZero > 0) {
                        size_t now = (toZero > CHUNK) ? CHUNK : toZero;
                        if (PIN_SafeCopy(tail, zeros, now) != now) {
                            // se falhar, apenas para de zerar; já temos a 1ª entrada válida
                            break;
                        }
                        tail += now;
                        toZero -= static_cast<ULONG>(now);
                    }
                }
            }

            // Ajusta ReturnLength do chamador (se houver)
            if (ReturnLength != 0 && args->ReturnLength) {
                PIN_SafeCopy(reinterpret_cast<VOID*>(args->ReturnLength), &neededLen, sizeof(ULONG));
            }

            stringStream << "    Rewritten to 1 module: kd.dll" << std::endl;

        done_modules:
            stringStream << "  [-] SYSTEM_MODULE_INFORMATION rewrite completed" << std::endl;
        }
        /////

        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<RtlGetNativeSystemInformationArgs*>(callContext->functionArgs);
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstRtlGetNativeSystemInformation::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RtlGetNativeSystemInformation") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                        // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // SystemInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // SystemInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // SystemInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // ReturnLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) -> RSP/ESP atual
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // ReturnLength
            IARG_END);

        RTN_Close(rtn);
    }
}
