#include <Windows.h>
#include <stdio.h>

int main()
{

	const char* my_string = "Welcome Yoav!";

	while (true)
	{
		printf("%d - %s (%p)\n", GetCurrentProcessId(), my_string, my_string);
		Sleep(1000);
	}

	return 0;
}