#include <Windows.h>
#include "../SuperSharedMemory/globals.h"
#include <iostream>
#include <stdio.h>

int main(int argc, char* argv[])
{

	if (argc < 3)
	{
		printf("<PID> <REMOTE_ADDR>\n");
		return 0;

	}

	void* addr;
	unsigned int pid;
	int result = sscanf_s(argv[1], "%d", &pid);
	result = sscanf_s(argv[2], "%p", &addr);

	printf("pid %d, addr %p\n", pid, addr);

	SuperSharedMemoryShareMemoryAddress ssData;
	ssData.remoteAddress = addr;
	ssData.localAddress = 0x0;
	ssData.pid = (PVOID)pid;
	ssData.size = 0x10;

	PVOID x;

	const char* to_change = "HACKED!";

	HANDLE hDevice = CreateFile(L"\\\\.\\SuperSharedMemory", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
		return 1;

	DWORD returned;
	BOOL success = DeviceIoControl(hDevice, IOCTL_SUPERSHAREDMEMORY_SHARE_MEM, &ssData, sizeof(ssData), &x, sizeof(x), &returned, nullptr);
	if (success)
	{
		printf("%p is mapped to %p\n", ssData.remoteAddress, x);
		for (int i = 0; i < ssData.size && i < strlen(to_change); ++i)
		{
			printf("0x%02x \n", reinterpret_cast<char*>(x)[i]);

			reinterpret_cast<char*>(x)[i] = to_change[i];
		}
	}
	else
		printf("change failed!\n");

	Sleep(3000);

	if (argc == 3)
	{
		success = DeviceIoControl(hDevice, IOCTL_SUPERSHAREDMEMORY_UNSHARE_MEM, NULL, 0, NULL, 0, &returned, nullptr);
		if (success)
		{
			printf("Unshare\n");
		}

		CloseHandle(hDevice);
		printf("waiting!\n");
		Sleep(1000);
	}
	else if (argc == 4)
	{
		printf("wait for char!\n");

		char c = getc(stdin);
	}


	return 0;
}
