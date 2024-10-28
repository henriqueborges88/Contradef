
[+] Informação de seções...
    Nome da imagem: C:\Users\henri\OneDrive\Escritorio\VM_Compartilhada\hw_Obsidium-x64-1-4-5-build-31.exe
    Seção [0]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae61000
        Tamanho: 137472 bytes
        Características: 
            A seção é legível.
    Seção [1]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae83000
        Tamanho: 71574 bytes
        Características: 
            A seção é legível.
    Seção [2]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae95000
        Tamanho: 10568 bytes
        Características: 
            A seção é legível.
            A seção é gravável.
    Seção [3]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae98000
        Tamanho: 7896 bytes
        Características: 
            A seção é legível.
    Seção [4]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae9a000
        Tamanho: 348 bytes
        Características: 
            A seção é legível.
    Seção [5]
        Nome da seção: .rsrc
        Endereço: 0x7ff65ae9b000
        Tamanho: 840 bytes
        Características: 
            A seção é legível.
    Seção [6]
        Nome da seção: [VAZIO]
        Endereço: 0x7ff65ae9c000
        Tamanho: 62536 bytes
        Características: 
            A seção é executável.
            A seção é legível.
            A seção é gravável.
[*] Concluído


[+] Regras YARA...
    Correspondências encontradas: 3
    Regras correspondidas: 
        IsPacked
        IsPE64
        ObsidiumDemoX64Cdf
            String: $a1 no offset 0xAE8
            String: $a2 no offset 0xA15
            String: $a2 no offset 0xCC0
            String: $a3 no offset 0xA15
            String: $a3 no offset 0xCC0
            String: $a4 no offset 0xBFC
            String: $a5 no offset 0x9BA
            String: $a6 no offset 0x9C3
            String: $a7 no offset 0xC32
            String: $a8 no offset 0xCC9
            String: $a9 no offset 0xAF7
            String: $a9 no offset 0xBB7
            String: $a10 no offset 0x993
            String: $a11 no offset 0xB31
[*] Concluído

