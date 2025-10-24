#include "InstrumentationUtils.h"
#include "Utils.h"
#include "HeapTracker.h"
#include "SectionTracker.h"
#include "DynExecTracker.h"

std::vector<std::string> strategyScope;

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

    if (HeapTracker::IsInTrackedHeap(address)) {
        //std::cout << "Tracked IsInTrackedHeap " << std::hex << address << std::dec << std::endl;
        return TRUE;
    }
    
    //std::cout << "Checking IsInKnownSection " << std::hex << address << std::dec << std::endl;
    if (SectionTracker::IsInKnownSection(address)) {
        //std::cout << "Tracked IsInKnownSection " << std::hex << address << std::dec << std::endl;
        return TRUE;
    }

    ////std::cout << "Checking IsIpDynamic " << std::hex << address << std::dec << std::endl;
    //if (DynExecTracker::IsIpDynamic(address)) {
    //    //std::cout << "Tracked IsIpDynamic " << std::hex << address << std::dec << std::endl;
    //    return TRUE;
    //}

    return FALSE;
}

BOOL IsFromUnknownImg(ADDRINT address) {
    PIN_LockClient();
    IMG img = IMG_FindByAddress(address);
    if (IMG_Valid(img)) {
        return FALSE;
    }
    return TRUE;
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


VOID PauseAtAddress(ADDRINT address) {
    PIN_LockClient();
    std::cout << "[CONTRADEF] O Contradef pausou no endereço " << std::hex << address << std::endl;
    std::cout << "[CONTRADEF] Anexe o depurador ao processo agora." << std::endl;
    std::cout << "[CONTRADEF] Pressione Enter para continuar a execucao..." << std::endl;

    std::cin.get(); // Aguarda entrada do usuário para continuar
    PIN_UnlockClient();
    PIN_Detach();
}


std::string FormatAddress(ADDRINT address, RTN rtn, BOOL showSymbols, BOOL showFullImgName, BOOL showLines)
{
    std::string s = StringFromAddrint(address) + " |";

    if (showSymbols) {
        if (RTN_Valid(rtn))
        {
            IMG img = SEC_Img(RTN_Sec(rtn));
            s += " ";
            if (IMG_Valid(img))
            {
                if (showFullImgName)
                {
                    s += IMG_Name(img) + ":";
                }
                else
                {
                    s += getFileName(IMG_Name(img)) + ":";
                }
            }
            else
            {
                s += " UNKNOWN-IMG";
            }

            s += RTN_Name(rtn);

            ADDRINT delta = address - RTN_Address(rtn);
            if (delta != 0)
            {
                s += "+" + hexstr(delta, 4);
            }
            s += " (rtn-id: " + decstr(rtn.index) + ')';
        }
        else
        {
            s += " UNKNOWN-IMG";
            if (IsMainExecutable(address)) {
                s += ":main (rtn-id: " + decstr(rtn.index) + ')';
            }
        }
    }
    

    if (showLines)
    {
        INT32 line;
        std::string file;
        PIN_LockClient();
        PIN_GetSourceLocation(address, NULL, &line, &file);
        PIN_UnlockClient();

        if (file != "")
        {
            s += " (" + file + ":" + decstr(line) + ")";
        }
    }
    return s;
}

bool IsStringInStrategyScope(const std::string& str) {
    for (const std::string& scope : strategyScope) {
        if (scope.find(str) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::wstring GetMainExePath()
{
    wchar_t buf[MAX_PATH];
    DWORD n = WindowsAPI::GetModuleFileNameW(NULL, buf, MAX_PATH); // NULL => módulo principal
    return std::wstring(buf, buf + n);
}