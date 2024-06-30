#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>
#include "globals.h"

#define DRIVER_NAME_DEBUG "MajorHooker: "
#define printf(...) DbgPrintEx(0, 0, DRIVER_NAME_DEBUG __VA_ARGS__)

#define print_variable(var) printf("%s = %p\n", #var, var)


