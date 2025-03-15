#include "pin.H"
#include <iostream>
#include <string>
#include "YaraContradef.h"
#include "Instrumentation.h"


INT32 Usage()
{
    cerr << "Esta ferramenta PIN fornece informação sobre possíveis comportamentos evasivos (antianálise) presentes em executáveis\n"
        "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

int main(int argc, char* argv[]) {

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    InitInstrumentation();

    return 0;
}
