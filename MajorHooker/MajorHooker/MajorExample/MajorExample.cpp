#include "defs.h"
#include "globals.h"
#include "memory.h"


UNICODE_STRING devName, symLink;
PDEVICE_OBJECT pDeviceObject;

NTSTATUS MajorBasic(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS MajorDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void DriverUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS HelloWorld(PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath) {

	NTSTATUS status = 0;

	UNREFERENCED_PARAMETER(pRegistryPath);

	DbgPrintEx(0, 0, "Driver Loaded\n");

	RtlInitUnicodeString(&devName, L"\\Device\\" DRIVER_NAME);
	RtlInitUnicodeString(&symLink, L"\\??\\" DRIVER_NAME);

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
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = MajorBasic;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MajorBasic;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MajorDeviceControl;

	return STATUS_SUCCESS;
}


NTSTATUS MajorBasic(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	printf("Created or Close :)\n");

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS MajorDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION pioStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	Irp->IoStatus.Information = 0;

	switch (pioStack->Parameters.DeviceIoControl.IoControlCode) {

	case(IOCTL_MAJOREXAMPLE_HELLOWORLD):
		printf("IOCTL_MAJORHOOKER_HELLOWORLD\n");
		status = HelloWorld(Irp);
		break;


	default:
		break;
	}

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

void DriverUnload(IN PDRIVER_OBJECT DriverObject) {

	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);
}



NTSTATUS HelloWorld(PIRP Irp)
{
	PIO_STACK_LOCATION pioStack = IoGetCurrentIrpStackLocation(Irp);

	UNREFERENCED_PARAMETER(pioStack);

	printf("Hello World!\n");

	return STATUS_SUCCESS;
}


