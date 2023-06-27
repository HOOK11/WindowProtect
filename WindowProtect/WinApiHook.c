#include "WinApiHook.h"
#include "ssdt.h"
#include"Module.h"
#include "tools.h"

int ReadIndex(ULONG64 addr, int offset)
{
	PUCHAR base = NULL;
	SIZE_T size = PAGE_SIZE;
	NTSTATUS status = ZwAllocateVirtualMemory(NtCurrentProcess(), &base, 0, &size, MEM_COMMIT, PAGE_READWRITE);

	if (!NT_SUCCESS(status)) return -1;

	ULONG proc = NULL;

	memset(base, 0, size);

	status = MmCopyVirtualMemory(IoGetCurrentProcess(), addr, IoGetCurrentProcess(), base, 0x300, UserMode, &proc);

	if (!NT_SUCCESS(status))
	{
		ZwFreeVirtualMemory(NtCurrentProcess(), &base, &size, MEM_RELEASE);
		return -1;
	}

	//int index = *(int*)(base + 0x4);
	ULONG number = GetWindowsVersionNumber();
	int index = 0;
	if (!offset)
	{
		index = *(int*)(base + 0x4);
	}
	else
	{
		if (number != 7)
		{
			index = *(int*)(base + 0x4);
		}
		else
		{
			PUCHAR temp = base + offset;
			for (int i = 0; i < 200; i++)
			{
				if (temp[i] == 0x4C && temp[i + 1] == 0x8B && temp[i + 2] == 0xD1)
				{
					index = *(int*)(temp + i + 4);
					break;
				}
			}
		}
	}

	ZwFreeVirtualMemory(NtCurrentProcess(), &base, &size, MEM_RELEASE);

	return index;
}


char* GetSearchModule()
{
	ULONG number = GetWindowsVersionNumber();
	char* moudleName = NULL;

	if (number == 7)
	{
		moudleName = "user32.dll";
	}
	else
	{
		moudleName = "win32u.dll";
	}

	return moudleName;
}


int GetFuncIndexPlus(char* funcName, int subAddr, int offset)
{
	int index = -1;
	PEPROCESS Process = FindProcess("explorer.exe");
	if (Process == NULL) return index;

	ULONG number = GetWindowsVersionNumber();
	char* moudleName = GetSearchModule();

	char* FuncName = funcName;

	ULONG_PTR imageBase = 0;
	ULONG_PTR imageSize = 0;
	KAPC_STATE kApcState = { 0 };

	KeStackAttachProcess(Process, &kApcState);

	do
	{

		imageBase = GetModuleR3(PsGetProcessId(Process), moudleName, &imageSize);

		if (!imageBase) break;

		ULONG_PTR funcAddr = GetProcAddressR(imageBase, FuncName, TRUE);

		if (!funcAddr) break;

		if (subAddr)
		{
			funcAddr -= subAddr;
		}

		index = ReadIndex(funcAddr, offset);

	} while (0);



	KeUnstackDetachProcess(&kApcState);

	ObDereferenceObject(Process);

	return index;
}

int GetFuncIndex(char* funcName, int offset)
{
	return GetFuncIndexPlus(funcName, 0, offset);
}



int GetUserGetForegroundWindowIndex()
{
	static int index = -1;
	if (index != -1) return index;

	ULONG number = GetWindowsVersionNumber();
	char* FuncName = NULL;

	if (number == 7)
	{
		FuncName = "GetForegroundWindow";
	}
	else
	{
		FuncName = "NtUserGetForegroundWindow";
	}

	index = GetFuncIndex(FuncName, 0);
	return index;
}

int GetUserFindWindowExIndex()
{
	static int index = -1;
	if (index != -1) return index;

	ULONG number = GetWindowsVersionNumber();
	char* FuncName = NULL;

	if (number == 7)
	{
		FuncName = "SetThreadDesktop";
	}
	else
	{
		FuncName = "NtUserFindWindowEx";
	}

	index = GetFuncIndex(FuncName, 11);
	return index;
}


int GetUserWindowFromPointIndex()
{
	static int index = -1;
	if (index != -1) return index;

	ULONG number = GetWindowsVersionNumber();
	char* FuncName = NULL;

	if (number == 7)
	{
		FuncName = "WindowFromPoint";
	}
	else
	{
		FuncName = "NtUserWindowFromPoint";
	}

	index = GetFuncIndex(FuncName, 0);
	return index;
}

int GetUserBuildHwndListIndex()
{
	static int index = -1;
	if (index != -1) return index;

	ULONG number = GetWindowsVersionNumber();
	char* FuncName = NULL;

	if (number == 7)
	{
		FuncName = "EnumDisplayMonitors";
	}
	else
	{
		FuncName = "NtUserBuildHwndList";
	}

	index = GetFuncIndex(FuncName, 11);
	return index;
}

int GetUserQueryWindowIndex() //重点怀疑对象
{
	static int index = -1;
	if (index != -1) return index;

	ULONG number = GetWindowsVersionNumber();
	char* FuncName = NULL;

	if (number == 7)
	{
		FuncName = "GetWindowLongW";
	}
	else
	{
		FuncName = "NtUserQueryWindow";
	}

	//index = GetFuncIndexPlus(FuncName, 0x65, 1);
	index =  GetFuncIndex(FuncName, 11); //win10 
	return index;
}

ULONG_PTR GetSSSDTFuncByIndex(LONG index)
{

	if (index == -1) return 0;

	PEPROCESS Process = FindProcess("explorer.exe");
	if (Process == NULL) return 0;

	KAPC_STATE kApcState = { 0 };

	KeStackAttachProcess(Process, &kApcState);

	SSDTStruct* sssdt = SSSDTFind();

	if (index >= 0x1000) index -= 0x1000;

	LONG offset = sssdt->pServiceTable[index];

	offset = (offset >> 4); // 使用无符号右移

	ULONG64 func = ((ULONG64)sssdt->pServiceTable + offset);

	KeUnstackDetachProcess(&kApcState);

	ObDereferenceObject(Process);

	return func;
	

}

ULONG_PTR GetUserFindWindowEx()
{
	static ULONG64 func = 0;
	if (func) return func;

	LONG index = GetUserFindWindowExIndex();
	func = GetSSSDTFuncByIndex(index);

	return func;
}

ULONG_PTR GetUserGetForegroundWindow()
{
	static ULONG64 func = 0;

	if (func) return func;

	LONG index = GetUserGetForegroundWindowIndex();

	func = GetSSSDTFuncByIndex(index);

	return func;
}

ULONG_PTR GetUserBuildHwndList()
{
	static ULONG64 func = 0;
	if (func) return func;

	LONG index = GetUserBuildHwndListIndex();
	func = GetSSSDTFuncByIndex(index);

	return func;
}


ULONG_PTR GetUserQueryWindow() //-------
{
	static ULONG64 func = 0;
	if (func) return func;

	LONG index = GetUserQueryWindowIndex();
	func = GetSSSDTFuncByIndex(index);

	return func;
}

ULONG_PTR GetUserWindowFromPoint()
{
	static ULONG64 func = 0;
	if (func) return func;

	LONG index = GetUserWindowFromPointIndex();
	func = GetSSSDTFuncByIndex(index);

	return func;
}


VOID InitHook()
{
	GetUserGetForegroundWindow();
	GetUserFindWindowEx();
	GetUserBuildHwndList();
	GetUserQueryWindow();
	GetUserWindowFromPoint();
}
