This is a kernel code that allows a usermode program to request virtual memory that points to the same physical memory as another process virtual memory.

To execute this first load SuperSharedMemory.sys
Then execute TestExample.exe which loop and prints its pid + text + pointer to the text
Then execute SuperSharedMemoryClient.exe [pid] [pointer] which provides itself a virtual memory that points to the printed text of TestExample.exe and then overwrites it (from SuperSharedMemoryClient.exe) with the string HACKED!.