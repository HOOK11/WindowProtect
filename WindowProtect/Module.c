#include "Module.h"
#include "tools.h"


ULONG_PTR GetModuleX86(PEPROCESS Process,PPEB32 peb32,PUNICODE_STRING moudleName,PULONG_PTR sizeImage)
{
	SIZE_T retSize = 0;
	MmCopyVirtualMemory(Process, peb32, Process, peb32, 0x1, UserMode, &retSize);
	PPEB_LDR_DATA32 pebldr = UlongToPtr(peb32->Ldr);

	PLIST_ENTRY32 pList32  = (PLIST_ENTRY32)&pebldr->InLoadOrderModuleList;
	PLDR_DATA_TABLE_ENTRY32 plistNext = (PLDR_DATA_TABLE_ENTRY32)UlongToPtr(pList32->Flink);
	
	ULONG_PTR module = 0;

	while (pList32 != plistNext)
	{

		PWCH baseDllName = plistNext->BaseDllName.Buffer;

		UNICODE_STRING uBaseName = {0};
		RtlInitUnicodeString(&uBaseName, baseDllName);

		if (RtlCompareUnicodeString(&uBaseName, moudleName, TRUE) == 0)
		{
			DbgPrintEx(77, 0, "[hotge]:imageBase = %llx,sizeofimage = %llx,%wZ\r\n", plistNext->DllBase, plistNext->SizeOfImage, &uBaseName);
			module = plistNext->DllBase;
			if (sizeImage) *sizeImage = plistNext->SizeOfImage;
			break;
		}


		plistNext = (PLDR_DATA_TABLE_ENTRY32)UlongToPtr(plistNext->InLoadOrderLinks.Flink);
	}

	return module;
}

ULONG_PTR GetModuleX64(PEPROCESS Process, PPEB peb, PUNICODE_STRING moudleName, PULONG_PTR sizeImage)
{
	SIZE_T retSize = 0;
	MmCopyVirtualMemory(Process, peb, Process, peb, 0x1, UserMode, &retSize);
	PPEB_LDR_DATA pebldr = peb->Ldr;

	PLIST_ENTRY pList = (PLIST_ENTRY)&pebldr->InLoadOrderModuleList;
	PLDR_DATA_TABLE_ENTRY plistNext = (PLDR_DATA_TABLE_ENTRY)(pList->Flink);

	ULONG_PTR module = 0;
	
	while (pList != plistNext)
	{
		if (RtlCompareUnicodeString(&plistNext->BaseDllName, moudleName, TRUE) == 0)
		{
			DbgPrintEx(77, 0, "[hotge]:imageBase = %llx,sizeofimage = %llx,%wZ\r\n", plistNext->DllBase, plistNext->SizeOfImage, &plistNext->BaseDllName);
			module = plistNext->DllBase;
			if (sizeImage) *sizeImage = plistNext->SizeOfImage;
			break;
		}


		plistNext = (PLDR_DATA_TABLE_ENTRY)plistNext->InLoadOrderLinks.Flink;
	}

	return module;
}

ULONG_PTR GetModuleR3(HANDLE pid, char *moduleName, PULONG_PTR sizeImage)
{
	if (!moduleName) return 0;

	PEPROCESS Process = NULL;
	KAPC_STATE kApcState = {0};
	ULONG_PTR moudule = 0;

	NTSTATUS status = PsLookupProcessByProcessId(pid, &Process);
	if (!NT_SUCCESS(status))
	{
		return 0;
	}

	STRING aModuleName = {0};
	RtlInitAnsiString(&aModuleName, moduleName);

	UNICODE_STRING uModuleName = {0};
	status = RtlAnsiStringToUnicodeString(&uModuleName, &aModuleName, TRUE);

	if (!NT_SUCCESS(status))
	{
		return 0;
	}

	
	_wcsupr(uModuleName.Buffer);

	

	KeStackAttachProcess(Process, &kApcState);

	PPEB peb = PsGetProcessPeb(Process);

	PPEB32 peb32 = (PPEB32)PsGetProcessWow64Process(Process);



	if (peb32)
	{
		moudule = GetModuleX86(Process, peb32, &uModuleName, sizeImage);
	}
	else
	{
		moudule = GetModuleX64(Process, peb, &uModuleName, sizeImage);
	}


	KeUnstackDetachProcess(&kApcState);

	RtlFreeUnicodeString(&uModuleName);

	return moudule;
}

