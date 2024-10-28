#include "Instrumentation.h"

BOOL IsMainExecutable(ADDRINT address) {
    PIN_LockClient();
    IMG img = IMG_FindByAddress(address);
    if (IMG_Valid(img)) {
        if (IMG_IsMainExecutable(img)) {
            PIN_UnlockClient();
            return TRUE;
        }
    }
    PIN_UnlockClient();
    return FALSE;
}

ADDRINT GetRtnAddr(ADDRINT instAddress) {
    PIN_LockClient();
    RTN rtnCall = RTN_FindByAddress(instAddress);
    ADDRINT rtnAddress = 0;
    if (RTN_Valid(rtnCall)) {
        rtnAddress = RTN_Address(rtnCall);
    }
    PIN_UnlockClient();
    return rtnAddress;
}
VOID GetSectionInfo(IMG img, std::ofstream& OutFile)
{
    OutFile << std::endl << "[+] Informação de seções..." << std::endl;
    OutFile << "    Nome da imagem: " << std::string(IMG_Name(img)) << std::endl;

    // Índice da seção
    int index = 0;

    // Itera sobre as seções da imagem
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec), ++index)
    {
        std::string secName = SEC_Name(sec);
        secName = secName.empty() ? "[VAZIO]" : secName;
        ADDRINT secAddr = SEC_Address(sec);
        UINT64 secSize = SEC_Size(sec);

        OutFile << "    Seção [" << index << "]" << std::endl;
        OutFile << "        Nome da seção: " << secName << std::endl;
        OutFile << "        Endereço: 0x" << std::hex << static_cast<uint64_t>(secAddr) << std::dec << std::endl;
        OutFile << "        Tamanho: " << static_cast<uint64_t>(secSize) << " bytes" << std::endl;

        OutFile << "        Características: " << std::endl;
        if (SEC_IsExecutable(sec))
        {
            OutFile << "            A seção é executável." << std::endl;
        }
        if (SEC_IsReadable(sec))
        {
            OutFile << "            A seção é legível." << std::endl;
        }
        if (SEC_IsWriteable(sec))
        {
            OutFile << "            A seção é gravável." << std::endl;
        }
    }

    OutFile << "[*] Concluído" << std::endl << std::endl;

    OutFile.flush();  // Esvazia o buffer de saída
}