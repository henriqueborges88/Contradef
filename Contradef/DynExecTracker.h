#pragma once
#include "pin.H"   // ADDRINT, BOOL

namespace DynExecTracker {

    // Opções do rastreador de execução dinâmica
    struct Options {
        // Se true, só considera regiões MEM_PRIVATE (heap/VirtualAlloc) como candidatas;
        // o .cpp ainda trata como dinâmico MEM_IMAGE sem IMG (loader reflexivo) e
        // MEM_MAPPED executável (MapViewOfFile), independentemente deste flag.
        bool onlyPrivate;

        // Se true, páginas escritas (write) entram como sinal adicional.
        bool trackWrites;

        // Se true, instala hook também em NtProtectVirtualMemory (além de VirtualProtect).
        bool hookNtProtect;

        Options()
            : onlyPrivate(true)
            , trackWrites(false)
            , hookNtProtect(true) {
        }
    };

    // Define/atualiza as opções antes (ou depois) do Init.
    // Se não chamar, o Init usa os defaults do construtor acima.
    void Configure(const Options& cfg);

    // Inicializa o rastreador:
    // - descobre o tamanho de página
    // - registra hooks (ImageLoad) e instrumentação (INS/TRACE)
    // - registra ThreadFini para limpar TLS
    void Init();

    // Retorna TRUE se o endereço (IP) pertence a código considerado "dinâmico"/desempacotado:
    // - fora de qualquer IMG conhecido pelo PIN (IMG_FindByAddress falha)
    // - com permissão de execução
    // - e (pelo menos uma condição):
    //     * transição RW->RX na página,
    //     * página marcada como escrita (se trackWrites = true),
    //     * região previamente indexada por alocação/mapa (malloc/HeapAlloc/VirtualAlloc/MapViewOfFile etc.)
    // Além disso, trata como dinâmico:
    //     * MEM_IMAGE executável sem IMG (loader reflexivo);
    //     * MEM_MAPPED executável (MapViewOfFile/Ex).
    BOOL IsIpDynamic(ADDRINT ip);

} // namespace DynExecTracker
