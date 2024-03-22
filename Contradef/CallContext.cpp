#include "CallContext.h"
#include <iostream>

VOID CheckPrintConditionsCC(CallContext* callContext, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr) {

    if (!(callContext->defaultPrintReady == true && callContext->countInstructionsAfterCall >= 20 && callContext->countFunctionsCalledAfterCall >= 20)) {
        return;
    }

    std::stringstream& stringStream = callContext->stringStream;

    if (callContext->instructionsAfterCall.size() > 0) {
        stringStream << "    Instruções executadas na rotina chamadora:" << std::endl;
        for (const auto& instructionInfo : callContext->instructionsAfterCall) {
            //criar metodo para marcar a instrução que chama a função
            stringStream << "        " << std::hex << instructionInfo.address << std::dec << " | " << instructionInfo.disassembledInstruction << std::endl;
        }
    }

    if (callContext->functionsCalledAfterCall.size() > 0) {
        stringStream << "    Pila de chamadas:" << std::endl;
        for (const auto& callFunctionInfo : callContext->functionsCalledAfterCall) {
            if (callFunctionInfo.address == callContext->rtnAddress) {
                stringStream << "        " << std::hex << callFunctionInfo.address << std::dec << " | --> " << callFunctionInfo.functionName << " (chamada por: " << callFunctionInfo.rtnCallerName << ")" << std::endl;
            }
            else {
                stringStream << "        " << std::hex << callFunctionInfo.address << std::dec << " |     " << callFunctionInfo.functionName << " (chamada por: " << callFunctionInfo.rtnCallerName << ")" << std::endl;
            }
        }
    }

    stringStream << "[*] Concluído" << std::endl << std::endl;

    ExecutionInformation executionCompletedInfo = { stringStream.str() };
    // Cria evento
    ExecutionEventData executionEvent(executionCompletedInfo);
    // Notifica os observers
    notifierPtr->NotifyAll(&executionEvent);

    // Limpa memoria
    CallContextKey key = { callContext->callId, callContext->threadId };
    callContextMap->erase(key);

    delete callContext;
}


// Callbacks que serão chamados quando um evento for notificado
VOID HandleInstructionEventCC(const EventData* data, void* context, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr) {
    // Verifica o tipo de evento e processa conforme necessário
    CallContext* callContext = reinterpret_cast<CallContext*>(context);
    if (data->type == EventData::Instruction) {
        if (callContext->defaultPrintReady == true && callContext->countInstructionsAfterCall >= 20) {
            auto instructionEvent = static_cast<const InstructionEventData*>(data);
            auto iTrace = instructionEvent->instuctionInformation.instructionTrace;

            // Copia a fila original para uma fila temporária para não alterar a fila original. std::queue não fornece acesso direto aos seus elementos além da frente e do fim da fila.
            std::queue<InstructionInformation> tempQueue = *iTrace;

            // Desenfileira cada elemento da fila temporária e imprime
            while (!tempQueue.empty()) {
                InstructionInformation instructionInfo = tempQueue.front();
                tempQueue.pop();
                callContext->instructionsAfterCall.push_back(instructionInfo);
            }

            notifierPtr->Detach(callContext->instructionObserver);
            CheckPrintConditionsCC(callContext, callContextMap, notifierPtr);
        }
        else {
            callContext->countInstructionsAfterCall++;
        }
    }
}

VOID HandleTraceEventCC(const EventData* data, void* context, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr) {
    CallContext* callContext = reinterpret_cast<CallContext*>(context);
    if (data->type == EventData::CallTrace) {
        if (callContext->defaultPrintReady == true && callContext->countFunctionsCalledAfterCall >= 20) {
            UINT32 specificThreadId = callContext->threadId;
            auto traceEvent = static_cast<const TraceEventData*>(data);
            std::stringstream& stringStream = callContext->stringStream;
            auto threadCallTracePtr = traceEvent->callTraceInfo.threadCallTraces;
            const std::map<THREADID, std::deque<CallRtnInformation>> threadCallTraces = *threadCallTracePtr;
            auto it = threadCallTraces.find(specificThreadId);
            if (it != threadCallTraces.end()) {
                const std::deque<CallRtnInformation>& callTraces = it->second;
                for (const CallRtnInformation& callInfo : callTraces) {
                    callContext->functionsCalledAfterCall.push_back(callInfo);
                }
            }
            notifierPtr->Detach(callContext->traceObserver);
            CheckPrintConditionsCC(callContext, callContextMap, notifierPtr);
        }
        else {
            callContext->countFunctionsCalledAfterCall++;
        }
    }
}
