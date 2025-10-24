#include "YaraContradef.h"

static std::ofstream YaraOutFile;

int RunYara(std::string _rules_file, std::string _target_file, std::ofstream& OutFile, std::vector<std::string>& matched)
{
    // Carregando a API do YARA de forma dinâmica para evitar erros ou conflitos da biblioteca windows.h com pin.h
    using namespace YaraAPI;

    // Converter as strings para wchar_t*
    const wchar_t* rules_file = StringToWString(_rules_file);
    const wchar_t* target_file = StringToWString(_target_file);

    // LoadLibrary e obter ponteiros para funções do YARA
#ifdef HOST_IA32
    HMODULE hYaraLib = LoadLibrary(L"yaraCdefWin32.dll");
#else
    HMODULE hYaraLib = LoadLibrary(L"yaraCdefx64.dll");
#endif // HOST_IA32

    if (hYaraLib == NULL) {
        std::cout << "Error loading YARA API." << std::endl;
        delete[] rules_file;
        delete[] target_file;
        return 1;
    }

    typedef int (*RunYaraFunc)(const wchar_t*, const wchar_t*, YARA_OUTPUT*);
    RunYaraFunc dynamic_run_yara = (RunYaraFunc)GetProcAddress(hYaraLib, "run_yara");
    if (dynamic_run_yara == NULL) {
        std::cout << "Error loading run_yara function from YARA API." << std::endl;
        if (hYaraLib)
            FreeLibrary(hYaraLib);
        delete[] rules_file;
        delete[] target_file;
        return 1;
    }

    typedef void (*FreeYaraOutputFunc)(YARA_OUTPUT* output);
    FreeYaraOutputFunc dynamic_free_yara_output = (FreeYaraOutputFunc)GetProcAddress(hYaraLib, "free_yara_output");
    if (dynamic_free_yara_output == NULL) {
        std::cout << "Error loading free_yara_output function from YARA API." << std::endl;
        if (hYaraLib)
            FreeLibrary(hYaraLib);
        delete[] rules_file;
        delete[] target_file;
        return 1;
    }

    YARA_OUTPUT output;
    int result = dynamic_run_yara(rules_file, target_file, &output);

    if (result == ERROR_SUCCESS)
    {
        OutFile << std::endl << "[+] YARA Rules..." << std::endl;
        OutFile << "    Matches found: " << output.match_count << std::endl;
        OutFile << "    Rules matched: " << std::endl;

        // Itera sobre as correspondências de regras
        RULE_MATCH* rule = output.rule_matches;
        while (rule)
        {
            std::string ruleid = rule->identifier;
            matched.push_back(rule->identifier);

            OutFile << "        " << rule->identifier << std::endl;
            STRING_MATCH* sm = rule->string_matches;
            OutFile << std::hex << std::uppercase;
            while (sm)
            {
                OutFile << "            String: " << sm->identifier << " is on offset 0x" << sm->offset << std::endl;
                sm = sm->next;
            }
            OutFile << std::dec << std::nouppercase;
            rule = rule->next;
        }
        OutFile << "[*] Completed" << std::endl << std::endl;

        // Libera a memória alocada pela API
        dynamic_free_yara_output(&output);
    }
    else
    {
        std::cerr << "Error running YARA scan: " << result << std::endl;

        // Lida com erros de compilação
        ERROR_MESSAGE* error_msg = output.compiler_results.error_list;
        while (error_msg)
        {
            std::cerr << "Compiler error in file " << error_msg->file_name << " on line " << error_msg->line_number << L": " << error_msg->message << std::endl;
            error_msg = error_msg->next;
        }

        // Lida com erros de execução
        EXECUTION_ERROR* exec_error = output.execution_errors;
        while (exec_error)
        {
            std::cerr << "Execution error: " << exec_error->message << std::endl;
            exec_error = exec_error->next;
        }

        // Libera a memória alocada pela API
        dynamic_free_yara_output(&output);
    }

    if (hYaraLib)
        FreeLibrary(hYaraLib);

    // Libera a memória dos wide strings
    delete[] rules_file;
    delete[] target_file;

    return 0;
}

static bool g_yaraInitialized = false;

VOID initYara()
{
    if (g_yaraInitialized) return; // idempotente
    g_yaraInitialized = true;

    // 1) Resolver arquivo de regras
    std::string rules_file = KnobYaraRulesFile.Value();

    if (rules_file.empty()) {
        // fallback: contradef.yar no diretório do pintool
        const std::string toolPath = GetContradefToolPath();
        const std::string toolDir = Dirname(toolPath);
        const std::string candidate = JoinPath(toolDir, "contradef.yar");
        if (FileExistsA(candidate)) {
            rules_file = candidate;
        }
    }

    if (rules_file.empty() || !FileExistsA(rules_file)) {
        std::cerr << "[yara] Nenhum arquivo de regras encontrado (use -yara ou coloque contradef.yar na mesma pasta do pintool).\n";
        return;
    }

    // 2) Preparar arquivo de log com BOM
    const std::string baseLog = KnobOutputFile.Value().empty()
        ? std::string("Contradef") : KnobOutputFile.Value();
    const std::string pid = decstr(WindowsAPI::getpid());
    const std::string logName = baseLog + "." + pid + ".Yara.cdf";

    YaraOutFile.open(logName.c_str(), std::ios::binary | std::ios::out);
    if (!YaraOutFile) {
        std::cerr << "[yara] Falha ao abrir log: " << logName << "\n";
        return;
    }
    // BOM UTF-8
    YaraOutFile << "\xEF\xBB\xBF";

    // 3) Alvo = executável principal
    const std::string execName = WStringToString(GetMainExePath());

    // 4) Rodar YARA (sua RunYara já inicializa/finaliza se necessário)
    std::vector<std::string> matched;
    RunYara(rules_file, execName, YaraOutFile, matched);

    // 5) Atualizar strategyScope sem duplicatas
    std::sort(matched.begin(), matched.end());
    matched.erase(std::unique(matched.begin(), matched.end()), matched.end());
    strategyScope.assign(matched.begin(), matched.end());

    YaraOutFile.flush();

    //std::cerr << "[yara] OK. regras=" << rules_file
    //    << " | log=" << logName
    //    << " | scopes=" << strategyScope.size() << "\n";
}
