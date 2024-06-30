#include "defs.h"
#include "globals.h"

UNICODE_STRING devName, symLink;
PDEVICE_OBJECT pDeviceObject;

NTSTATUS SuperSharedMemoryMajorBasic(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS SuperSharedMemoryDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void SuperSharedMemoryUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS SetSharedMemory(PIRP Irp);
NTSTATUS UnsetSharedMemory();

void ProcessNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create);

typedef struct _SHARED_MEMORY
{
	bool used;
	PMDL mdl;
	HANDLE callerPid;
	HANDLE remotePid;
} SHARED_MEMORY, *PSHARED_MEMORY;


SHARED_MEMORY g_SharedMemory;

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



	pDriverObject->DriverUnload = SuperSharedMemoryUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = SuperSharedMemoryMajorBasic;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = SuperSharedMemoryMajorBasic;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SuperSharedMemoryDeviceControl;

	g_SharedMemory.callerPid = 0x00;
	g_SharedMemory.remotePid = 0x00;
	g_SharedMemory.mdl = NULL;
	g_SharedMemory.used = false;

	// Cleanup the physical memory pointer when the process exits
	status = PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, FALSE);
	if (!NT_SUCCESS(status)) {

		printf("Failed to create symLink %x\n", status);
		IoDeleteDevice(pDeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS SuperSharedMemoryMajorBasic(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	printf("Created or Close :)\n");

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS SuperSharedMemoryDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION pioStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	Irp->IoStatus.Information = 0;

	switch (pioStack->Parameters.DeviceIoControl.IoControlCode) {

	case(IOCTL_SUPERSHAREDMEMORY_SHARE_MEM):
		printf("IOCTL_SUPERSHAREDMEMORY_SHARE_MEM\n");
		status = SetSharedMemory(Irp);
		break;

	case(IOCTL_SUPERSHAREDMEMORY_UNSHARE_MEM):
		printf("IOCTL_SUPERSHAREDMEMORY_UNSHARE_MEM\n");
		status = UnsetSharedMemory();
		break;

	default:
		break;
	}

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

void SuperSharedMemoryUnload(IN PDRIVER_OBJECT DriverObject) {

	PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, TRUE);


	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);


}



NTSTATUS SetSharedMemory(PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION pioStack = IoGetCurrentIrpStackLocation(Irp);

	SuperSharedMemoryShareMemoryAddress* ssData = NULL;
	ULONG inBufLength = pioStack->Parameters.DeviceIoControl.InputBufferLength;

	PVOID lockedBuffer = NULL;
	PEPROCESS process;
	KAPC_STATE apc;

	PMDL usedMdl = NULL;
	printf("SetSharedMemory\n");


	do
	{
		if (g_SharedMemory.used)
		{
			status = STATUS_ALREADY_REGISTERED;
			break;
		}

		if (inBufLength < sizeof(SuperSharedMemoryShareMemoryAddress))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		ssData = (SuperSharedMemoryShareMemoryAddress*)Irp->AssociatedIrp.SystemBuffer;
		if (ssData == nullptr)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		printf("PsLookupProcessByProcessId pid - %d\n", ssData->pid);
		status = PsLookupProcessByProcessId(ssData->pid, &process);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		printf("KeStackAttachProcess\n");
		KeStackAttachProcess(process, &apc);

		do
		{
			printf("IoAllocateMdl %p %d\n", ssData->remoteAddress, ssData->size);
			usedMdl = IoAllocateMdl(ssData->remoteAddress, ssData->size, FALSE, FALSE, NULL);
			if (usedMdl == NULL)
			{
				status = STATUS_FAIL_CHECK;
				break;
			}

			printf("MmProbeAndLockPages\n");
			__try
			{
				MmProbeAndLockPages(usedMdl, UserMode, IoReadAccess);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				printf("Exception raised! %p\n", GetExceptionCode());
				status = STATUS_FAIL_CHECK;
				break;
			}

			status = STATUS_SUCCESS;

		} while (false);

		KeUnstackDetachProcess(&apc);
		ObDereferenceObject(process);
		
		if (!NT_SUCCESS(status))
		{
			break;
		}

		__try
		{
			lockedBuffer = MmMapLockedPagesSpecifyCache(
				usedMdl,
				UserMode,
				MmNonCached,
				NULL,
				FALSE,
				NormalPagePriority
			);

		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			printf("Exception raised! %p\n", GetExceptionCode());
			status = STATUS_FAIL_CHECK;
			break;
		}



	} while (false);

	if (!NT_SUCCESS(status))
	{
		printf("Failed, clearing this");
		if (usedMdl)
		{
			MmUnlockPages(usedMdl);
			IoFreeMdl(usedMdl);
		}
	}
	else
	{
		g_SharedMemory.callerPid = PsGetCurrentProcessId();
		g_SharedMemory.remotePid = ssData->pid;
		g_SharedMemory.mdl = usedMdl;
		g_SharedMemory.used = true;

		printf("Address %p is mapped into %p\n", ssData->remoteAddress, lockedBuffer);

		*((PVOID*)Irp->AssociatedIrp.SystemBuffer) = lockedBuffer;
		Irp->IoStatus.Information = sizeof(PVOID);
	}

	return status;
}

NTSTATUS UnsetSharedMemory()
{
	if (g_SharedMemory.used)
	{
		MmUnlockPages(g_SharedMemory.mdl);
		IoFreeMdl(g_SharedMemory.mdl);
		g_SharedMemory.used = false;
	}

	return STATUS_SUCCESS;
}

void ProcessNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create)
{
	UNREFERENCED_PARAMETER(ParentId);

	// Process is deleted
	if (!Create)
	{
		if (ProcessId == g_SharedMemory.callerPid || ProcessId == g_SharedMemory.remotePid)
		{
			printf("Unset the shared memory from the process %d\n", ProcessId);
			UnsetSharedMemory();
		}
	}
}