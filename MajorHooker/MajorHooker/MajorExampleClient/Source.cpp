#include <Windows.h>
#include "../MajorExample/globals.h"
#include <iostream>
#include <stdio.h>

int main(int argc, char* argv[])
{
	HANDLE hDevice = CreateFile(L"\\\\.\\" DRIVER_NAME, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed %d\n", GetLastError());
		return 1;

	}

	DWORD returned;
	BOOL success = DeviceIoControl(hDevice, IOCTL_MAJOREXAMPLE_HELLOWORLD, NULL, 0, NULL, 0, &returned, nullptr);
	if (success)
	{
		printf("OKAY\n");
	}
	else
	{
		printf("change failed!\n");
	}


	CloseHandle(hDevice);


	return 0;
}

