#include "defs.h"
#include "globals.h"

UNICODE_STRING devName, symLink;
UNICODE_STRING otherDevName, otherSymLink;
PDEVICE_OBJECT pDeviceObject;

PFILE_OBJECT otherFileObject;
PDEVICE_OBJECT otherDevice;
PDRIVER_OBJECT otherDriver;

NTSTATUS(*pMajorDeviceControl)(PDEVICE_OBJECT DeviceObject, PIRP Irp);

void DriverUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS UnHookDriverMajorFunction();
NTSTATUS HookDriverMajorFunction();


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath) {

	NTSTATUS status = 0;

	UNREFERENCED_PARAMETER(pRegistryPath);

	DbgPrintEx(0, 0, "Driver Loaded\n");

	RtlInitUnicodeString(&devName, L"\\Device\\" DRIVER_NAME);
	RtlInitUnicodeString(&symLink, L"\\??\\" DRIVER_NAME);

	RtlInitUnicodeString(&otherDevName, L"\\Device\\" OTHER_DRIVER_NAME);
	RtlInitUnicodeString(&otherSymLink, L"\\??\\" OTHER_DRIVER_NAME);

	status = IoGetDeviceObjectPointer(&otherDevName, FILE_READ_DATA, &otherFileObject, &otherDevice);
	if (!NT_SUCCESS(status)) {
		printf("Failed to get device object %x\n", status);
		return status;
	}
	otherDriver = otherDevice->DriverObject;

	status = IoCreateDevice(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
	if (!NT_SUCCESS(status)) {

		printf("IoCreateDevice Failed with code: %x\n", status);
		return status;
	}

	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {

		printf("Failed to create symLink %x\n", status);
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	pDriverObject->DriverUnload = DriverUnload;


	HookDriverMajorFunction();


	return STATUS_SUCCESS;
}

void DriverUnload(IN PDRIVER_OBJECT DriverObject) {

	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);

	UnHookDriverMajorFunction();

}


NTSTATUS HookedMajorDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	printf("HOOKED MAJOR DEVICE CONTROL!!\n");

	return pMajorDeviceControl(DeviceObject, Irp);
}

NTSTATUS UnHookDriverMajorFunction()
{
	printf("UnHookDriverMajorFunction\n");
	InterlockedExchange64((volatile LONG64*)(&(otherDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL])), (LONG64)pMajorDeviceControl);


	return STATUS_SUCCESS;
}

NTSTATUS HookDriverMajorFunction()
{
	printf("HookDriverMajorFunction\n");
	
	
	print_variable(otherDriver);
	print_variable(otherDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL]);
	print_variable(&(otherDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL]));
	
	pMajorDeviceControl = (decltype(pMajorDeviceControl))(InterlockedExchange64((volatile LONG64*)(&(otherDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL])), (LONG64)HookedMajorDeviceControl));

	return STATUS_SUCCESS;
}


