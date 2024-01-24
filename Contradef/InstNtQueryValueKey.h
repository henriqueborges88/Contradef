#pragma once
#ifndef INST_NT_QUERY_VALUE_KEY_H
#define INST_NT_QUERY_VALUE_KEY_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "NtStructures.h"
#include  "CallContextKey.h"

// Baseado em https://github.com/3gstudent/HiddenNtRegistry/blob/master/HiddenNtRegistry.h

#define STDAPICALLTYPE          __stdcall
#define RtlZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) == STATUS_SUCCESS)
#define OBJ_CASE_INSENSITIVE	0x00000040L
#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );        \
    (p)->RootDirectory = r;                           \
    (p)->Attributes = a;                              \
    (p)->ObjectName = n;                              \
    (p)->SecurityDescriptor = s;                      \
    (p)->SecurityQualityOfService = NULL;             \
    }

typedef struct _STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING;
typedef STRING* PSTRING;
typedef STRING OEM_STRING;
typedef STRING* POEM_STRING;
typedef STRING ANSI_STRING;
typedef STRING* PANSI_STRING;

typedef enum _KEY_INFORMATION_CLASS
{
    KeyBasicInformation,
    KeyNodeInformation,
    KeyFullInformation,
    KeyNameInformation
} KEY_INFORMATION_CLASS;

typedef struct _KEY_BASIC_INFORMATION
{
    LARGE_INTEGER LastWriteTime;// The last time the key or any of its values changed.
    ULONG TitleIndex;			// Device and intermediate drivers should ignore this member.
    ULONG NameLength;			// The size in bytes of the following name, including the zero-terminating character.
    WCHAR Name[1];				// A zero-terminated Unicode string naming the key.
} KEY_BASIC_INFORMATION;
typedef KEY_BASIC_INFORMATION* PKEY_BASIC_INFORMATION;


typedef struct _KEY_VALUE_BASIC_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG NameLength;
    WCHAR Name[1]; 
} KEY_VALUE_BASIC_INFORMATION, * PKEY_VALUE_BASIC_INFORMATION;


typedef struct _KEY_VALUE_PARTIAL_INFORMATION
{
    ULONG TitleIndex;	// Device and intermediate drivers should ignore this member.
    ULONG Type;			// The system-defined type for the registry value in the 
    // Data member (see the values above).
    ULONG DataLength;	// The size in bytes of the Data member.
    UCHAR Data[1];		// A value entry of the key.
} KEY_VALUE_PARTIAL_INFORMATION;
typedef KEY_VALUE_PARTIAL_INFORMATION* PKEY_VALUE_PARTIAL_INFORMATION;


struct KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1]; // Nome do valor (de tamanho variável)
};

typedef enum _KEY_VALUE_INFORMATION_CLASS
{
    KeyValueBasicInformation,
    KeyValueFullInformation,
    KeyValuePartialInformation,
} KEY_VALUE_INFORMATION_CLASS;

typedef NTSTATUS(STDAPICALLTYPE NTQUERYVALUEKEY)
(
    IN HANDLE			KeyHandle,
    IN PUNICODE_STRING	ValueName,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    OUT PVOID			KeyValueInformation,
    IN ULONG			Length,
    OUT PULONG			ResultLength
    );
typedef NTQUERYVALUEKEY FAR* LPNTQUERYVALUEKEY;


struct InstNtQueryValueKeyArgs {
    ADDRINT KeyHandle;
    ADDRINT ValueName;
    USHORT KeyValueInformationClass;
    ADDRINT KeyValueInformation;
    UINT Length;
    UINT ResultLength;
};

class InstNtQueryValueKey : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstNtQueryValueKeyArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID InterpretKeyValueInformationFull(BYTE* keyValueInformation);
    static VOID InterpretKeyValueInformationPartial(BYTE* keyValueInformation);
    static VOID InterpretKeyValueInformationBasic(BYTE* keyValueInformation);
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT KeyHandle, ADDRINT ValueName, USHORT KeyValueInformationClass, ADDRINT KeyValueInformation, UINT Length, UINT ResultLength);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT ValueName, USHORT KeyValueInformationClass, ADDRINT KeyValueInformation, UINT Length, UINT ResultLength);
};

#endif // INST_NT_QUERY_VALUE_KEY_H
