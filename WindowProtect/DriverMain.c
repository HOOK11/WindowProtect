#include"../EtwHook/hook.h"
#include"ssdt.h"
#include"WinApiHook.h"

#pragma comment(lib, "EtwHook.lib") 

#ifdef __cplusplus
extern "C" {
#endif
	void checkLogger();

#ifdef __cplusplus
}
#endif

PVOID ghwnd = NULL;

PVOID MyNtUserGetForegroundWindow()
{
	typedef PVOID(NTAPI* NtUserGetForegroundWindowProc)(VOID);

	NtUserGetForegroundWindowProc NtUserGetForegroundWindowFunc = GetUserGetForegroundWindow();

	PVOID hwnd = NtUserGetForegroundWindowFunc();

	if (ghwnd == hwnd)
	{
		//DbgBreakPoint();
		return NULL;
	}

	return hwnd;
}

PVOID MyNtUserFindWindowEx(PVOID desktop1, PVOID desktop2, PUNICODE_STRING tName, PUNICODE_STRING tclassName, ULONG64 x)
{
	typedef PVOID(NTAPI* MyUserFindWindowExProc)(PVOID desktop1, PVOID desktop2, PUNICODE_STRING tName, PUNICODE_STRING tclassName, ULONG64 x);

	MyUserFindWindowExProc MyUserFindWindowExFunc = GetUserFindWindowEx();

	PVOID hwnd = MyUserFindWindowExFunc(desktop1, desktop2, tName, tclassName, x);

	if (ghwnd == hwnd)
	{
		//DbgBreakPoint();
		return NULL;
	}

	return hwnd;
}

ULONG64 MyNtUserQueryWindow(PVOID Hwnd, int flags)
{
	typedef ULONG64(NTAPI* MyNtUserQueryWindowProc)(PVOID Hwnd, int flags);

	MyNtUserQueryWindowProc MyNtUserQueryWindowFunc = GetUserQueryWindow();

	if (Hwnd == ghwnd) return 0;

	return MyNtUserQueryWindowFunc(Hwnd, flags);
}

PVOID MyNtUserWindowFromPoint(PVOID Point)
{
	typedef PVOID(NTAPI* NtUserWindowFromPointProc)(PVOID Point);

	NtUserWindowFromPointProc NtUserWindowFromPointFunc = GetUserWindowFromPoint();

	PVOID Hwnd = NtUserWindowFromPointFunc(Point);

	if (Hwnd == ghwnd) return 0;

	return Hwnd;
}

NTSTATUS MyNtUserBuildHwndList(PVOID a1, PVOID a2, PVOID Address, unsigned int a4, ULONG count, PVOID Addressa, PULONG pretCount)
{
	typedef NTSTATUS(NTAPI* MyNtUserBuildHwndListProc)(PVOID a1, PVOID a2, PVOID Address, unsigned int a4, ULONG count, PVOID Addressa, PULONG pretCount);

	MyNtUserBuildHwndListProc MyNtUserBuildHwndListFunc = GetUserBuildHwndList();

	NTSTATUS status = MyNtUserBuildHwndListFunc(a1, a2, Address, a4, count, Addressa, pretCount);


	if (NT_SUCCESS(status))
	{

		if (MmIsAddressValid(pretCount) && MmIsAddressValid(Addressa))
		{
			int scount = *pretCount;
			PVOID* arrays = (PVOID*)Addressa;
			for (int i = 0; i < scount; i++)
			{
				if (arrays[i] == ghwnd)
				{
					//如果我们的句柄就是第一个
					if (i == 0)
					{
						DbgPrintEx(77, 0, "[db]:i arrays[i],%llx\r\n", arrays[i]);
						//只有一个情况
						if (scount == 1)
						{
							arrays[i] = 0;
							*pretCount = 0;
							break;
						}

						arrays[i] = arrays[i + 1];
						break;
					}
					else
					{
						DbgPrintEx(77, 0, "[db]:W arrays[i],%llx\r\n", arrays[i]);
						arrays[i] = arrays[i - 1];
						break;
					}

				}
			}
		}

	}

	return status;
}



void HookCallBack(
	_In_ unsigned int SystemCallIndex,
	_Inout_ void** SystemCallFunction)
{
	if (*SystemCallFunction == GetUserGetForegroundWindow())
	{
		*SystemCallFunction = MyNtUserGetForegroundWindow;
		DbgPrintEx(77, 0, "[db]:GetUserGetForegroundWindow\r\n");
	}
	else if (*SystemCallFunction == GetUserFindWindowEx())
	{
		DbgPrintEx(77, 0, "[db]:GetUserFindWindowEx\r\n");
		*SystemCallFunction = MyNtUserFindWindowEx;
	}
	else if (*SystemCallFunction == GetUserBuildHwndList())
	{
		*SystemCallFunction = MyNtUserBuildHwndList;
		DbgPrintEx(77, 0, "[db]:GetUserBuildHwndList\r\n");

	}
	else if (*SystemCallFunction == GetUserQueryWindow())
	{
		DbgPrintEx(77, 0, "[db]:GetUserQueryWindow\r\n");
		*SystemCallFunction = MyNtUserQueryWindow;
	}
	else if (*SystemCallFunction == GetUserWindowFromPoint())
	{

		*SystemCallFunction = MyNtUserWindowFromPoint;
		DbgPrintEx(77, 0, "[db]:GetUserWindowFromPoint\r\n");

	}
}

VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);

	KdPrintEx((0, 0, "[%s] \n", __FUNCTION__));

	IfhRelease2();
}


NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver,PUNICODE_STRING pReg_Path)
{

	UNREFERENCED_PARAMETER(pReg_Path);

	InitHook();
	//__debugbreak();
	IfhInitialize2(HookCallBack);
	//0x0004005A
	ghwnd = (PVOID)0x00091458;
	pDriver->DriverUnload = DriverUnload;

	return STATUS_SUCCESS;
}