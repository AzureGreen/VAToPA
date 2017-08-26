// app.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <intrin.h>


#define LINK_NAME    L"\\??\\VAToPALinkName"

#define IOCTL_VATOPA  (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, \
	0x801, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


int main()
{
	CHAR szBuffer[] = "HelloWorld";
	printf("virtual address: %p\r\n", szBuffer);

	UINT_PTR VA = (UINT_PTR)szBuffer;
	printf("VA : %I64x\r\n", VA);

	UINT64 PA = 0;

	HANDLE DeviceHandle = CreateFile(LINK_NAME, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (DeviceHandle == INVALID_HANDLE_VALUE)
	{
		printf("open device error\r\n");
		return 0;
	}

	DWORD dwReturn = 0;
	BOOL bOk = DeviceIoControl(DeviceHandle, IOCTL_VATOPA,
		&VA, sizeof(UINT_PTR),
		&PA, sizeof(UINT64),
		&dwReturn, NULL);
	if (bOk == FALSE || DeviceHandle == INVALID_HANDLE_VALUE)
	{
		printf("Device Io Control error\r\n");
		
		int a = GetLastError();
		printf("error no: %d\r\n", a);
		getchar();
		return 0;
	}

	printf("PA : %I64x\r\n", PA);

	CloseHandle(DeviceHandle);

	getchar();

    return 0;
}

