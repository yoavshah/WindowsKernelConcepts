This is a kernel code that hook another driver's IRP_MJ_DEVICE_CONTROL and setting it with its own.

Load MajorExample.sys, then execute MajorExampleClient.exe to contact it with IOCTL.
Then load the MajorHooker.sys and execute again MajorExampleClient.exe, you should see in the windbg view that the original MajorExample.sys IOCTL was hooked.
