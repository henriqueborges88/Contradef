// utils.cpp
#include "utils.h"

bool IsPointer(ADDRINT value) {
    // Verifica se o valor é um ponteiro acessível
    return PIN_CheckReadAccess(reinterpret_cast<void*>(value));
}


std::string GetNumericValue(ADDRINT ea, UINT32 size) {
    if (size == 0) {
        return "0"; // Retorna "0" para tamanho inválido
    }

    std::stringstream ss;

    switch (size) {
    case 1:
    {
        UINT8 value = static_cast<UINT8>(ea);
        ss << std::dec << static_cast<unsigned int>(value);
    }
    break;

    case 2:
    {
        UINT16 value = static_cast<UINT16>(ea);
        ss << std::dec << value;
    }
    break;

    case 4:
    {
        UINT32 value = static_cast<UINT32>(ea);
        ss << std::dec << value;
    }
    break;

    case 8:
    {
        UINT64 value = static_cast<UINT64>(ea);
        ss << std::dec << value;
    }
    break;

    default:
        // Trata outros casos como um ponteiro de 64 bits
        ss << std::dec << static_cast<UINT64>(ea);
        break;
    }

    return ss.str();
}

std::string GetNumericHexValue(ADDRINT ea, UINT32 size) {
    if (size == 0) {
        return "0"; // Retorna "0" para tamanho inválido
    }

    std::stringstream ss;
    ss << "0x"; // Prefixo hexadecimal para maior clareza

    switch (size) {
    case 1:
    {
        UINT8 value = static_cast<UINT8>(ea);
        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(value);
    }
    break;

    case 2:
    {
        UINT16 value = static_cast<UINT16>(ea);
        ss << std::setfill('0') << std::setw(4) << std::hex << value;
    }
    break;

    case 4:
    {
        UINT32 value = static_cast<UINT32>(ea);
        ss << std::setfill('0') << std::setw(8) << std::hex << value;
    }
    break;

    case 8:
    {
        UINT64 value = static_cast<UINT64>(ea);
        ss << std::setfill('0') << std::setw(16) << std::hex << value;
    }
    break;

    default:
        // Trata outros casos como um ponteiro de 64 bits
        ss << std::hex << static_cast<UINT64>(ea);
        break;
    }

    return ss.str();
}

std::string GetNumericValueFromRef(VOID* ea, UINT32 size) {
    if (!IsPointer(reinterpret_cast<ADDRINT>(ea))) {
        return ""; // Retorna um valor padrão se o ponteiro for inválido
    }

    std::stringstream ss;
    ss << "0x"; // Prefixo hexadecimal para maior clareza

    switch (size) {
    case 0:
        return ""; // Tamanho inválido
        break;

    case 1:
    {
        UINT8 x;
        PIN_SafeCopy(&x, static_cast<UINT8*>(ea), 1);
        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(x) << " (" << std::dec << static_cast<unsigned int>(x) << ")"; // Converte UINT8 para string
    }
    break;

    case 2:
    {
        UINT16 x;
        PIN_SafeCopy(&x, static_cast<UINT16*>(ea), 2);
        ss << std::setfill('0') << std::setw(4) << std::hex << x << " (" << std::dec << x << ")"; // Converte UINT16 para string
    }
    break;

    case 4:
    {
        UINT32 x;
        PIN_SafeCopy(&x, static_cast<UINT32*>(ea), 4);
        ss << std::setfill('0') << std::setw(8) << std::hex << x << " (" << std::dec << x << ")"; // Converte UINT32 para string
    }
    break;

    case 8:
    {
        UINT64 x;
        PIN_SafeCopy(&x, static_cast<UINT64*>(ea), 8);
        ss << std::setfill('0') << std::setw(16) << std::hex << x << " (" << std::dec << x << ")"; // Converte UINT64 para string
    }
    break;

    default:
        // Converte o ponteiro em string hexadecimal
        ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(ea);
        break;
    }

    return ss.str();
}

bool IsStringPointer(UINT64 addr) {
    if (!IsPointer(addr)) {
        return false; // Ponteiro inválido
    }

    char* str = reinterpret_cast<char*>(addr);
    for (int i = 0; i < 256; ++i) { // Limite para evitar leitura fora do contexto
        if (!PIN_CheckReadAccess(&str[i])) {
            return false; // Parte da string não é acessível
        }
        if (str[i] == '\0') {
            return true; // Encontrou o final da string
        }
        if (str[i] < 0x20 || str[i] > 0x7E) {
            return false; // Não é um caractere imprimível
        }
    }
    return false; // Não encontrou um final de string válido
}


bool IsValidWideString(const std::wstring& str) {
    // Verifique primeiro o tamanho mínimo
    if (str.size() < 3) {
        return false;
    }

    for (wchar_t ch : str) {
        // Permitir apenas caracteres ASCII alfanuméricos e os permitidos extras
        if (!((ch >= L'0' && ch <= L'9') || // Números
            (ch >= L'A' && ch <= L'Z') || // Letras maiúsculas
            (ch >= L'a' && ch <= L'z') || // Letras minúsculas
            ch == L'.' || ch == L'-' || ch == L'_' ||
            ch == L':' || ch == L'\\' || ch == L'/')) {
            return false; // Caractere inválido encontrado
        }
    }

    return true; // Todos os caracteres são válidos
}


bool IsValidString(const std::string& str) {
    // Verifique primeiro o tamanho mínimo
    if (str.size() < 3) {
        return false;
    }

    for (char ch : str) {
        // Permitir apenas caracteres ASCII alfanuméricos e os extras permitidos
        if (!((ch >= '0' && ch <= '9') || // Números
            (ch >= 'A' && ch <= 'Z') || // Letras maiúsculas
            (ch >= 'a' && ch <= 'z') || // Letras minúsculas
            ch == '.' || ch == '-' || ch == '_' ||
            ch == ':' || ch == '\\' || ch == '/')) {
            return false; // Caractere inválido encontrado
        }
    }

    return true; // Todos os caracteres são válidos
}

std::string InterpretAddrIntAsASCII(ADDRINT value) {
    if (value == 0)
        return "";

    std::string result;
    for (std::size_t i = 0; i < sizeof(ADDRINT); ++i) {
        char c = static_cast<char>((value >> (i * 8)) & 0xFF);
        if (std::isprint(static_cast<unsigned char>(c)) || c == '\0') {
            result += c;
        }
        else {
            break; // interrompe no primeiro caractere não imprimível
        }
    }
    return TrimAsciiEdges(result);
}

// ---------------------------------------------------------------------------
//  Lê string ASCII terminada em '\0' a partir de `addr`.
//  • maxLen:  limite (em bytes) a copiar; padrão = 800.
// ---------------------------------------------------------------------------
std::string CopyLPCSTR(ADDRINT addr, size_t maxLen)
{
    if (maxLen == 0) return "";

    std::vector<char> buf(maxLen + 1, 0);                // +1 p/ terminador
    size_t copied = 0;

    for (size_t i = 0; i < maxLen; ++i)
    {
        if (PIN_SafeCopy(&buf[i], reinterpret_cast<const void*>(addr + i), 1) != 1)
            break;                                       // falha de leitura

        if (buf[i] == '\0') break;                       // fim da string
        ++copied;
    }
    return std::string(&buf[0], copied);
}

// ---------------------------------------------------------------------------
//  Lê string UTF-16 terminada em L'\0' a partir de `addr`.
//  • maxLen:  limite (em wchar_t) a copiar; padrão = 800.
// ---------------------------------------------------------------------------
std::wstring CopyLPCWSTR(ADDRINT addr, size_t maxLen)
{
    if (maxLen == 0) return L"";

    std::vector<wchar_t> buf(maxLen + 1, 0);             // +1 p/ terminador
    size_t copied = 0;

    for (size_t i = 0; i < maxLen; ++i)
    {
        if (PIN_SafeCopy(&buf[i],
            reinterpret_cast<const void*>(addr + i * sizeof(wchar_t)),
            sizeof(wchar_t)) != sizeof(wchar_t))
            break;                                       // falha de leitura

        if (buf[i] == L'\0') break;                      // fim da string
        ++copied;
    }
    return std::wstring(&buf[0], copied);
}

// ---------------------------------------------------------------
// TRUE se c é espaço, TAB, CR/LF, NUL ou outro byte de controlo.
// ---------------------------------------------------------------
static inline bool IsWsOrCtrl(unsigned char c)
{
    return (c <= 0x20) || (c == 0x7F);      // 0x00-0x20 + DEL
}

/*===============================================================
 *  TrimAsciiEdges  (C++11)
 *    • Remove lixo ASCII ≤0x20 (e DEL) no início/fim da string
 *    • Mantém o “miolo” intacto
 *===============================================================*/
std::string TrimAsciiEdges(const std::string& s)
{
    size_t first = 0;
    while (first < s.size() && IsWsOrCtrl(static_cast<unsigned char>(s[first])))
        ++first;

    if (first == s.size())          // só havia espaços/controlo
        return std::string();

    size_t last = s.size() - 1;
    while (last > first && IsWsOrCtrl(static_cast<unsigned char>(s[last])))
        --last;

    return s.substr(first, last - first + 1);
}

static inline bool IsPrintableA(char c)
{
    return (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\r' || c == '\n';
}
static inline bool IsPrintableW(wchar_t w)
{
    return (w >= 0x20 && w < 0x7F) || w == L'\t' || w == L'\r' || w == L'\n';
}

std::string ExtractStringAt(ADDRINT addr, size_t maxLen)
{
    if (addr == 0 || maxLen == 0) return "";

    const size_t HARD_CAP = 0x10000;
    if (maxLen > HARD_CAP) maxLen = HARD_CAP;

    /* ----------------------------------------------------------------
       1) Copiamos, de forma segura, até `maxLen` bytes a partir de addr
          – sem parar no primeiro NUL.
    -----------------------------------------------------------------*/
    std::vector<char> raw(maxLen, 0);
    size_t got = PIN_SafeCopy(&raw[0],
        reinterpret_cast<const void*>(addr),
        maxLen);
    if (got == 0) return "";

    /* ----------------------------------------------------------------
       2) Construímos, em paralelo, dois candidatos:
          – ASCII  / UTF-8  (1 byte)
          – UTF-16 / wide    (2 bytes)
    -----------------------------------------------------------------*/
    std::string   asciiCand;
    std::wstring  wideCand;

    /* --- ASCII pass ------------------------------------------------ */
    for (size_t i = 0; i < got; ++i)
    {
        char c = raw[i];
        if (c == '\0') break;                        // NUL => fim
        if (!IsPrintableA(c)) { asciiCand.clear(); break; }
        asciiCand.push_back(c);
    }

    /* --- UTF-16 pass ---------------------------------------------- */
    for (size_t i = 0; i + 1 < got; i += 2)
    {
        wchar_t w = *reinterpret_cast<wchar_t*>(&raw[i]);
        if (w == L'\0') break;                       // NUL wide
        if (!IsPrintableW(w)) { wideCand.clear(); break; }
        wideCand.push_back(w);
    }

    /* ----------------------------------------------------------------
       3) Heurística de escolha:
          – se apenas um candidato tem tamanho razoável (>=1), use-o;
          – se ambos existem, escolha o mais longo;
          – se none parece bom, devolva vazio.
    -----------------------------------------------------------------*/
    const size_t MIN_LEN = 1;

    bool asciiOk = asciiCand.length() >= MIN_LEN;
    bool wideOk = wideCand.length() >= MIN_LEN;

    if (asciiOk && !wideOk)         return asciiCand;
    if (!asciiOk && wideOk)         return WStringToString(wideCand);
    if (asciiOk && wideOk)
        return (asciiCand.length() >= wideCand.length())
        ? asciiCand
        : WStringToString(wideCand);

    std::ostringstream oss;
    oss << "";
    return oss.str();
}

//------------------------------------------------------------------
// Formata um argumento que *pode* ser ponteiro-para-string.
//  – Se for plausível, devolve  “0x1234 -> "Hello" ”
//  – Caso contrário devolve      “0x1234”
//------------------------------------------------------------------
std::string FormatPtrOrValue(ADDRINT val, size_t maxLen, size_t minChars)
{
    const ADDRINT MIN_PTR = 0x10000;           // limiar “plausível”

    if (val >= MIN_PTR && PIN_CheckReadAccess(reinterpret_cast<void*>(val)))
    {
        std::string s = ExtractStringAt(val, maxLen);
        if (s.length() >= minChars)
        {
            std::ostringstream oss;
            oss << s;
            return oss.str();
        }
    }

    std::ostringstream oss;
    oss << "";
    return oss.str();
}

std::string GetStringValueFromRegister(UINT64 value, UINT32 size) {
    std::ostringstream result;

    // Caso de um único caractere (1 byte)
    if (size == 1) {
        char c = static_cast<char>(value & 0xFF); // Apenas o byte menos significativo
        if (c >= 0x20 && c <= 0x7E) { // Verifica se é imprimível
            result << "'" << c << "'";
        }
        else {
            result << "";
        }
        return result.str();
    }

    // Caso de sequência de caracteres (até 8 bytes)
    const char* str = reinterpret_cast<const char*>(&value);
    bool isString = true;
    for (UINT32 i = 0; i < size && i < 8; ++i) { // Limite do registrador é 8 bytes
        if (str[i] == '\0') { // Terminação de string
            break;
        }
        if (str[i] < 0x20 || str[i] > 0x7E) { // Não imprimível
            isString = false;
            break;
        }
    }

    if (isString) {
        result << std::string(str, size).c_str(); // Converte para string
    }
    else {
        result << "";
    }

    return result.str();
}

size_t GetSafeWStringLength(ADDRINT addr, size_t maxLen) {
    size_t length = 0;
    wchar_t ch = 0;

    for (; length < maxLen; ++length) {
        // Copia apenas 1 wchar_t por vez de forma segura
        if (PIN_SafeCopy(&ch, reinterpret_cast<void*>(addr + length * sizeof(wchar_t)), sizeof(wchar_t)) != sizeof(wchar_t)) {
            break; // Falha ao acessar a memória -> parar
        }

        if (ch == L'\0') {
            break; // Fim da string
        }
    }

    return length;
}

std::wstring ConvertAddrToWideStringSafe(ADDRINT addr, size_t maxLen) {
    std::wstring result;

    size_t length = GetSafeWStringLength(addr, maxLen);
    if (length == 0) return result;

    // Buffer temporário com terminador adicional
    std::vector<wchar_t> buffer(length + 1, L'\0');

    // Use &buffer[0] no lugar de buffer.data() — compatível com C++11
    size_t copied = PIN_SafeCopy(&buffer[0], reinterpret_cast<void*>(addr), (length + 1) * sizeof(wchar_t));
    if (copied >= length * sizeof(wchar_t)) {
        result = std::wstring(&buffer[0]);
    }

    return result;
}


std::wstring ConvertAddrToWideString(ADDRINT addr) {
    if (addr != 0) {
        wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(addr);
        return std::wstring(wideCharStr);
    }
    else {
        return std::wstring();  // Retorna uma string vazia se o endereço for nulo
    }
}

std::string ConvertAddrToAnsiString(ADDRINT addr) {
    if (addr != 0) {
        const char* charStr = reinterpret_cast<const char*>(addr);
        return std::string(charStr);
    }
    else {
        return std::string();  // Retorna uma string vazia se o endereço for nulo
    }
}

std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
    {
        return std::string();
    }

    // Obtém o tamanho necessário do buffer para a string convertida (em UTF-8)
    int sizeNeeded = WindowsAPI::WideCharToMultiByte(
        CP_UTF8,          // Código de página para UTF-8
        0,                // none flag extra
        wstr.data(),      // Ponteiro para a string wide char
        static_cast<int>(wstr.size()), // Comprimento da string wide char
        nullptr,          // Sem buffer de saída por enquanto (queremos saber o tamanho necessário)
        0,
        nullptr,          // Sem uso de caractere de substituição
        nullptr           // Sem retorno de caractere substituto usado
    );

    if (sizeNeeded <= 0)
    {
        return std::string();
    }

    // Cria um buffer de tamanho adequado para a string resultante
    std::string result(sizeNeeded, 0);

    // Converte de wide char para string em UTF-8
    WindowsAPI::WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        static_cast<int>(wstr.size()),
        &result[0],
        sizeNeeded,
        nullptr,
        nullptr
    );

    return result;
}

// Função para converter std::string para const wchar_t*
const wchar_t* StringToWString(const std::string& str)
{
    using namespace WindowsAPI;
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* wideStr = new wchar_t[size_needed];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, size_needed);
    return wideStr;
}

void LogWString(const std::wstring& wstr) {
    std::string str = WStringToString(wstr);
    LOG("Converted String: %s\n", str.c_str());
}

std::string ExtractModuleName(const std::string& fullPath) {
    size_t lastSlashPos = fullPath.find_last_of("\\/");
    if (lastSlashPos != std::string::npos) {
        return fullPath.substr(lastSlashPos + 1);
    }
    else {
        return fullPath; // Retorna o fullPath original se não houver separador de diretório
    }
}

std::string toUpperCase(const std::string& str) {
    std::string upperCaseStr = str; // Cria uma cópia da string original
    for (char& c : upperCaseStr) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return upperCaseStr;
}

VOID PrintTrace(std::deque<std::string>& threadTraces) {
    std::cout << "Routine Trace:" << std::endl;
    for (const auto& name : threadTraces) {
        std::cout << "  " << name << std::endl;
    }
}


void PrintCallContextMap(std::map<CallContextKey, CallContext*>& callContextMap) {

    for (const auto& pair : callContextMap) {
        const CallContextKey& key = pair.first;
        const CallContext* context = pair.second;

        std::cout << "CallContextKey: (callCtxId: " << key.callId
            << ", tid: " << key.threadId << ")" << std::endl;

        std::cout << "  CallContext: " << std::endl;
        std::cout << "    callCtxId: " << context->callId << std::endl;
        std::cout << "    tid: " << context->threadId << std::endl;
        std::cout << "    rtnAddress: " << std::hex << context->rtnAddress << std::dec << std::endl;
        std::cout << "    stringStream: " << context->stringStream.str() << std::endl;
    }
}


void GetParentProcessName() {
    using namespace WindowsAPI;
    DWORD currentProcessId = GetCurrentProcessId();
    DWORD parentProcessId = 0;

    // Tirar um snapshot de todos os processos
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Erro ao criar snapshot.\n";
        return;
    }

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                parentProcessId = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (parentProcessId != 0) {
        // Tirar outro snapshot para procurar o processo pai
        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            std::cerr << "Erro ao criar snapshot.\n";
            return;
        }

        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == parentProcessId) {
                    std::wcout << L"Nome do processo pai: " << pe32.szExeFile << L"\n";
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }
    else {
        std::cerr << "Processo pai não encontrado.\n";
    }
}

DWORD GetProcessIdByName(std::string processName) {
    using namespace WindowsAPI;
    DWORD processId = 0;

    // Cria um snapshot de todos os processos
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Erro ao criar snapshot.\n";
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Percorre os processos no snapshot
    if (Process32First(hSnapshot, &pe32)) {
        do {
            // Compara o nome do executável
            std::transform(processName.begin(), processName.end(), processName.begin(), ::tolower);
            std::string pName = WStringToString(pe32.szExeFile);
            std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
            if (processName == pName) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    else {
        std::cerr << "Erro ao percorrer os processos.\n";
    }

    CloseHandle(hSnapshot);
    return processId;
}

std::string getFileName(const std::string& filePath) {
    // Encontra a última ocorrência de '\' ou '/'
    size_t pos = filePath.find_last_of("\\/");

    if (pos != std::string::npos) {
        return filePath.substr(pos + 1);
    }

    return filePath;
}

std::string Hex(UINT64 v, UINT32 bytes)
{
    std::ostringstream oss;
    oss << "0x" << std::setfill('0')
        << std::setw(bytes * 2) << std::hex << v;
    return oss.str();
}

static inline std::string RtnNameAt(ADDRINT addr) {
    std::string name;
    PIN_LockClient();
    RTN r = RTN_FindByAddress(addr);
    if (RTN_Valid(r)) name = RTN_Name(r);
    PIN_UnlockClient();
    return name;
}

static inline std::string ImgNameAt(ADDRINT addr) {
    PIN_LockClient();
    IMG img = IMG_FindByAddress(addr);
    std::string n = IMG_Valid(img) ? IMG_Name(img) : std::string();
    PIN_UnlockClient();
    return n;
}