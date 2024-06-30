#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <ntddk.h>
#include "globals.h"

#define DRIVER_NAME_DEBUG "SuperSharedMemory: "
#define printf(...) DbgPrintEx(0, 0, DRIVER_NAME_DEBUG __VA_ARGS__)



