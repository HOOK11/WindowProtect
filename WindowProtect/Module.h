#pragma once
#include <ntifs.h>


typedef struct _PEB_LDR_DATA
{
	
	ULONG Length;                                                           //0x0
	UCHAR Initialized;                                                      //0x4
	VOID* SsHandle;                                                         //0x8
	LIST_ENTRY InLoadOrderModuleList;                               //0x10
	LIST_ENTRY InMemoryOrderModuleList;                             //0x20
	LIST_ENTRY InInitializationOrderModuleList;                     //0x30
	VOID* EntryInProgress;                                                  //0x40
	UCHAR ShutdownInProgress;                                               //0x48
	VOID* ShutdownThreadId;                                                 //0x50
}PEB_LDR_DATA,*PPEB_LDR_DATA;

typedef struct _PEB
{
	ULONG64 x;
	VOID* Mutant;                                                           //0x8
	VOID* ImageBaseAddress;                                                 //0x10
	PEB_LDR_DATA* Ldr;														 //0x18

}PEB,*PPEB;




ULONG_PTR GetModuleR3(HANDLE pid, char *moduleName, PULONG_PTR sizeImage);

