#include "tools.h"
#include <ntimage.h>


ULONG GetThreadModeOffset()
{
	static int offset = 0;
	if (offset) return offset;

	UNICODE_STRING funcName = { 0 };
	RtlInitUnicodeString(&funcName, L"ExGetPreviousMode");
	PUCHAR func = (PUCHAR)MmGetSystemRoutineAddress(&funcName);
	if (func == NULL) return 0;
	PUCHAR temp = func;

	for (int i = 10; i < 50; i++)
	{
		if (temp[i] == 0xC3)
		{
			if (temp[i + 1] == 0x90 || temp[i + 1] == 0x0 || temp[i + 1] == 0xcc)
			{
				temp += i;
				break;
			}
		}
	}

	if (temp != func)
	{
		temp -= 4;
		offset = *(PULONG)temp;
	}

	return offset;
}

BOOLEAN SetThreadMode(PETHREAD thread, MODE newMode, MODE * oldMode)
{
	ULONG offset = GetThreadModeOffset();
	if (offset == 0) return FALSE;

	MODE mode = (MODE)*((PUCHAR)thread + offset);
	*((PUCHAR)thread + offset) = newMode;
	if (oldMode) *oldMode = mode;
	return TRUE;
}





NTSTATUS GetNtModuleBaseAndSize(ULONG64 * pModule, ULONG64 * pSize)
{
	if (pModule == NULL || pSize == NULL) return STATUS_UNSUCCESSFUL;
	ULONG BufferSize = PAGE_SIZE * 64;
	PVOID Buffer = ExAllocatePool(PagedPool, BufferSize);
	ULONG ReturnLength;
	NTSTATUS Status = ZwQuerySystemInformation(SystemModuleInformation,
		Buffer,
		BufferSize,
		&ReturnLength
		);

	if (Status == STATUS_INFO_LENGTH_MISMATCH) {
		ExFreePool(Buffer);
		return STATUS_INFO_LENGTH_MISMATCH;
	}

	PRTL_PROCESS_MODULES            Modules;
	PRTL_PROCESS_MODULE_INFORMATION ModuleInfo;
	Modules = (PRTL_PROCESS_MODULES)Buffer;
	ModuleInfo = &(Modules->Modules[0]);
	*pModule = ModuleInfo->ImageBase;
	*pSize = ModuleInfo->ImageSize;

	ExFreePool(Buffer);

	return Status;
}


#define DEREF( name )*(UINT_PTR *)(name)
#define DEREF_64( name )*(unsigned __int64 *)(name)
#define DEREF_32( name )*(unsigned long *)(name)
#define DEREF_16( name )*(unsigned short *)(name)
#define DEREF_8( name )*(UCHAR *)(name)

ULONG_PTR GetProcAddressR(ULONG_PTR hModule, const char* lpProcName, BOOLEAN x64Module)
{
	UINT_PTR uiLibraryAddress = 0;
	ULONG_PTR fpResult = NULL;

	if (hModule == NULL)
		return NULL;

	// a module handle is really its base address
	uiLibraryAddress = (UINT_PTR)hModule;

	__try
	{
		UINT_PTR uiAddressArray = 0;
		UINT_PTR uiNameArray = 0;
		UINT_PTR uiNameOrdinals = 0;
		PIMAGE_NT_HEADERS32 pNtHeaders32 = NULL;
		PIMAGE_NT_HEADERS64 pNtHeaders64 = NULL;
		PIMAGE_DATA_DIRECTORY pDataDirectory = NULL;
		PIMAGE_EXPORT_DIRECTORY pExportDirectory = NULL;

		// get the VA of the modules NT Header
		pNtHeaders32 = (PIMAGE_NT_HEADERS32)(uiLibraryAddress + ((PIMAGE_DOS_HEADER)uiLibraryAddress)->e_lfanew);
		pNtHeaders64 = (PIMAGE_NT_HEADERS64)(uiLibraryAddress + ((PIMAGE_DOS_HEADER)uiLibraryAddress)->e_lfanew);
		if (x64Module)
		{
			pDataDirectory = (PIMAGE_DATA_DIRECTORY)&pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		}
		else
		{
			pDataDirectory = (PIMAGE_DATA_DIRECTORY)&pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		}


		// get the VA of the export directory
		pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(uiLibraryAddress + pDataDirectory->VirtualAddress);

		// get the VA for the array of addresses
		uiAddressArray = (uiLibraryAddress + pExportDirectory->AddressOfFunctions);

		// get the VA for the array of name pointers
		uiNameArray = (uiLibraryAddress + pExportDirectory->AddressOfNames);

		// get the VA for the array of name ordinals
		uiNameOrdinals = (uiLibraryAddress + pExportDirectory->AddressOfNameOrdinals);

		// test if we are importing by name or by ordinal...
		if ((PtrToUlong(lpProcName) & 0xFFFF0000) == 0x00000000)
		{
			// import by ordinal...

			// use the import ordinal (- export ordinal base) as an index into the array of addresses
			uiAddressArray += ((IMAGE_ORDINAL(PtrToUlong(lpProcName)) - pExportDirectory->Base) * sizeof(unsigned long));

			// resolve the address for this imported function
			fpResult = (ULONG_PTR)(uiLibraryAddress + DEREF_32(uiAddressArray));
		}
		else
		{
			// import by name...
			unsigned long dwCounter = pExportDirectory->NumberOfNames;
			while (dwCounter--)
			{
				char * cpExportedFunctionName = (char *)(uiLibraryAddress + DEREF_32(uiNameArray));

				// test if we have a match...
				if (strcmp(cpExportedFunctionName, lpProcName) == 0)
				{
					// use the functions name ordinal as an index into the array of name pointers
					uiAddressArray += (DEREF_16(uiNameOrdinals) * sizeof(unsigned long));

					// calculate the virtual address for the function
					fpResult = (ULONG_PTR)(uiLibraryAddress + DEREF_32(uiAddressArray));

					// finish...
					break;
				}

				// get the next exported function name
				uiNameArray += sizeof(unsigned long);

				// get the next exported function name ordinal
				uiNameOrdinals += sizeof(unsigned short);
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		fpResult = NULL;
	}

	return fpResult;
}


UCHAR charToHex(UCHAR * ch)
{
	unsigned char temps[2] = { 0 };
	for (int i = 0; i < 2; i++)
	{
		if (ch[i] >= '0' && ch[i] <= '9')
		{
			temps[i] = (ch[i] - '0');
		}
		else if (ch[i] >= 'A' && ch[i] <= 'F')
		{
			temps[i] = (ch[i] - 'A') + 0xA;
		}
		else if (ch[i] >= 'a' && ch[i] <= 'f')
		{
			temps[i] = (ch[i] - 'a') + 0xA;
		}
	}
	return ((temps[0] << 4) & 0xf0) | (temps[1] & 0xf);
}




void initFindCodeStruct(PFindCode findCode, PCHAR code, ULONG64 offset, ULONG64 lastAddrOffset)
{

	memset(findCode, 0, sizeof(FindCode));

	findCode->lastAddressOffset = lastAddrOffset;
	findCode->offset = offset;

	PCHAR pTemp = code;
	ULONG64 i = 0;
	for (i = 0; *pTemp != '\0'; i++)
	{
		if (*pTemp == '*' || *pTemp == '?')
		{
			findCode->code[i] = *pTemp;
			pTemp++;
			continue;
		}

		findCode->code[i] = charToHex(pTemp);
		pTemp += 2;

	}

	findCode->len = i;
}


ULONG64 findAddressByCode(ULONG64 beginAddr, ULONG64 endAddr, PFindCode  findCode, ULONG size)
{
	ULONG64 j = 0;
	LARGE_INTEGER rtna = { 0 };

	for (ULONG64 i = beginAddr; i <= endAddr; i++)
	{
		if (!MmIsAddressValid((PVOID)i))continue;


		for (j = 0; j < size; j++)
		{
			FindCode  fc = findCode[j];
			ULONG64 tempAddress = i;

			UCHAR * code = (UCHAR *)(tempAddress + fc.offset);
			BOOLEAN isFlags = FALSE;

			for (ULONG64 k = 0; k < fc.len; k++)
			{
				if (!MmIsAddressValid((PVOID)(code + k)))
				{
					isFlags = TRUE;
					break;
				}

				if (fc.code[k] == '*' || fc.code[k] == '?') continue;

				if (code[k] != fc.code[k])
				{
					isFlags = TRUE;
					break;
				}
			}

			if (isFlags) break;

		}

		//ур╣╫ак
		if (j == size)
		{
			rtna.QuadPart = i;
			rtna.LowPart += findCode[0].lastAddressOffset;
			break;
		}

	}

	return rtna.QuadPart;
}



ULONG GetWindowsVersionNumber()
{
	static ULONG gNumber = 0;
	if (gNumber != 0) return gNumber;

	RTL_OSVERSIONINFOW version = { 0 };
	RtlGetVersion(&version);

	if (version.dwMajorVersion <= 6) return 7;

	if (version.dwBuildNumber == 9600)
	{
		gNumber = 8;
	}
	else if (version.dwBuildNumber == 10240)
	{
		gNumber = 1507;
	}
	else if (version.dwBuildNumber == 10586)
	{
		gNumber = 1511;
	}
	else if (version.dwBuildNumber == 14393)
	{
		gNumber = 1607;
	}
	else if (version.dwBuildNumber == 15063)
	{
		gNumber = 1703;
	}
	else if (version.dwBuildNumber == 16299)
	{
		gNumber = 1709;
	}
	else if (version.dwBuildNumber == 17134)
	{
		gNumber = 1803;
	}
	else if (version.dwBuildNumber == 17763)
	{
		gNumber = 1809;
	}
	else if (version.dwBuildNumber == 18362)
	{
		gNumber = 1903;
	}
	else if (version.dwBuildNumber == 18363)
	{
		gNumber = 1909;
	}
	else if (version.dwBuildNumber == 19041)
	{
		gNumber = 2004;
	}
	else if (version.dwBuildNumber == 19042)
	{
		gNumber = 2009;
	}
	else if (version.dwBuildNumber == 19043)
	{
		gNumber = 2011;
	}
	else if (version.dwBuildNumber == 22200)
	{
		gNumber = 2012;
	}


	return gNumber;
}

PEPROCESS FindProcess(char * processName)
{
	PEPROCESS eprocess = NULL;
	KAPC_STATE kapc = { 0 };
	for (int i = 8; i < 0x10000; i += 4)
	{
		PEPROCESS tempProcess = NULL;
		NTSTATUS status = PsLookupProcessByProcessId((HANDLE)i, &tempProcess);
		if (NT_SUCCESS(status))
		{
			char * name = PsGetProcessImageFileName(tempProcess);
			if (name && _stricmp(name, processName) == 0)
			{
				eprocess = tempProcess;
				break;
			}
			ObDereferenceObject(tempProcess);

		}
	}

	return eprocess;
}



void KernelSleep(ULONG ms, BOOLEAN alert)
{
	#define DELAY_ONE_MICROSECOND 	(-10)
	#define DELAY_ONE_MILLISECOND	(DELAY_ONE_MICROSECOND*1000)
	LARGE_INTEGER my_interval;
	my_interval.QuadPart = DELAY_ONE_MILLISECOND;
	my_interval.QuadPart *= ms;
	KeDelayExecutionThread(KernelMode, alert, &my_interval);
}

LONG SafeSearchString(IN PUNICODE_STRING source, IN PUNICODE_STRING target, IN BOOLEAN CaseInSensitive)
{
	ASSERT(source != NULL && target != NULL);
	if (source == NULL || target == NULL || source->Buffer == NULL || target->Buffer == NULL)
		return STATUS_INVALID_PARAMETER;

	// Size mismatch
	if (source->Length < target->Length)
		return -1;

	USHORT diff = source->Length - target->Length;
	for (USHORT i = 0; i <= (diff / sizeof(WCHAR)); i++)
	{
		if (RtlCompareUnicodeStrings(
			source->Buffer + i,
			target->Length / sizeof(WCHAR),
			target->Buffer,
			target->Length / sizeof(WCHAR),
			CaseInSensitive
		) == 0)
		{
			return i;
		}
	}

	return -1;
}
