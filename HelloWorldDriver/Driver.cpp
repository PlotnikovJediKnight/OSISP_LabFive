#include "Driver.h"

PDEVICE_OBJECT g_pDeviceObject;

PDriverData GetDeviceData() {
    return (PDriverData)g_pDeviceObject->DeviceExtension;
}

PCSTR GetProcessFileNameById(IN HANDLE handle) {
    PEPROCESS Process;
    PsLookupProcessByProcessId(handle, &Process);
    return gPsGetProcessImageFileName(Process);
}

VOID FireProcessYEvent() {
    PDriverData driverInformation = GetDeviceData();

    DbgPrint("Process X: %s has been %s. . About to fire Process Y event...\n",
             driverInformation->x_monitor.x_process_name,
             driverInformation->process_ctn.isCreate ? "created" : "terminated");

    if (driverInformation->process_ctn.isCreate)
        driverInformation->process_y_events.SetCreateEvent();
    else
        driverInformation->process_y_events.SetCloseEvent();
}

BOOLEAN IsProcessXManagerChild() {
    PDriverData driverInformation = GetDeviceData();
    PCSTR szProcessName = GetProcessFileNameById(driverInformation->process_ctn.hParentID);
    return RtlEqualMemory(szProcessName, driverInformation->x_monitor.manager_name, strlen(szProcessName));
}

BOOLEAN IsProcessX() {
    PDriverData driverInformation = GetDeviceData();
    PCSTR szProcessName = GetProcessFileNameById(driverInformation->process_ctn.hProcessID);
    DbgPrint("WorkItem %s.\n", szProcessName);
    return RtlEqualMemory(szProcessName, driverInformation->x_monitor.x_process_name, strlen(szProcessName));
}

VOID WorkItem(IN PDEVICE_OBJECT  DeviceObject, IN OPTIONAL PVOID  Context) {
    if (IsProcessX()) {
        if (!IsProcessXManagerChild())
            FireProcessYEvent();
        else
            DbgPrint("Recursive creation has been evaded!\n");
    }
    IoFreeWorkItem((PIO_WORKITEM)Context);
}

VOID ProcessCallback(IN HANDLE  hParentId, IN HANDLE  hProcessId, IN BOOLEAN isCreate) {
    DbgPrint("Process CallBack %s has been called.\n", (isCreate ? "Create" : "Close"));

    PDriverData driverInformation = GetDeviceData();
    driverInformation->process_ctn.InitProcessCTN(hParentId, hProcessId, isCreate);
    PIO_WORKITEM allocWorkItem = IoAllocateWorkItem(g_pDeviceObject);
    IoQueueWorkItem(allocWorkItem, WorkItem, DelayedWorkQueue, allocWorkItem);
}

VOID InitializeDriverData() {
    PDriverData driverInformation = GetDeviceData();
    driverInformation->Initialize();
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath) {

    UNICODE_STRING sPsGetProcessImageFileName = RTL_CONSTANT_STRING(L"PsGetProcessImageFileName");
    gPsGetProcessImageFileName = (GET_PROCESS_IMAGE_NAME)MmGetSystemRoutineAddress(&sPsGetProcessImageFileName);
    if (!gPsGetProcessImageFileName) {
        DbgPrint("PSGetProcessImageFileName not found\n");
        return STATUS_UNSUCCESSFUL;
    }

    UNICODE_STRING DeviceName, Win32Device;
    NTSTATUS status;
    PDEVICE_OBJECT DeviceObject = NULL;
    DbgPrint("Entered Driver Entry\n");

    RtlInitUnicodeString(&DeviceName, L"\\Device\\" DRIVER_NAME);
    RtlInitUnicodeString(&Win32Device, L"\\DosDevices\\" DRIVER_NAME);

    unsigned i;
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DriverUnsupportedMajorFunction;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateCloseFunction;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateCloseFunction;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = DriverShutdownFunction;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchIoctlFunction;
    DriverObject->DriverUnload = DriverUnloadFunction;

    status = IoCreateDevice(DriverObject,
        sizeof(DriverData),
        &DeviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &DeviceObject);


    if (!NT_SUCCESS(status))
        return status;
    if (!DeviceObject)
        return STATUS_UNEXPECTED_IO_ERROR;

    DeviceObject->Flags |= DO_DIRECT_IO;
    DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
    status = IoCreateSymbolicLink(&Win32Device, &DeviceName);
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    g_pDeviceObject = DeviceObject;

    InitializeDriverData();

    return STATUS_SUCCESS;
}

VOID UnloadXProcessMonitor() {
    PDriverData driverInformation = GetDeviceData();

    if (driverInformation->x_monitor.monitor_on)
        SetCTNRoutine(TRUE);

    driverInformation->Finalize();
}

NTSTATUS DriverUnsupportedMajorFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    DbgPrint("Unsupported Major Function\n");
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS DriverCreateCloseFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    DbgPrint("Create Close Function\n");
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DriverShutdownFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    DbgPrint("Shutdown Function\n");
    UnloadXProcessMonitor();
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DriverDispatchIoctlFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    DbgPrint("Dispatch IO Control Function\n");

    NTSTATUS               ntStatus = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE:
        {
            ntStatus = SwitchXMonitorStruct(Irp);
            break;
        }
    }

    Irp->IoStatus.Status = ntStatus;

    Irp->IoStatus.Information = (ntStatus == STATUS_SUCCESS)
        ? irpStack->Parameters.DeviceIoControl.OutputBufferLength
        : 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return ntStatus;
}

void DriverUnloadFunction(IN PDRIVER_OBJECT DriverObject) {
    DbgPrint("Unload driver Function\n");
    UnloadXProcessMonitor();
    UNICODE_STRING Win32Device;
    RtlInitUnicodeString(&Win32Device, L"\\DosDevices\\" DRIVER_NAME);
    IoDeleteSymbolicLink(&Win32Device);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS SetCTNRoutine(IN BOOLEAN isDisabled) {
    DbgPrint("Process Creation & Termination Monitoring has been %s by ProcessCallback\n", isDisabled ? "disabled" : "enabled");
    return PsSetCreateProcessNotifyRoutine(ProcessCallback, isDisabled);
}

VOID CopyXMonitorStruct(IN PProcessXMonitorStruct pActivateInfo) {
    PDriverData driverInformation = GetDeviceData();
    RtlCopyMemory(&driverInformation->x_monitor, pActivateInfo, sizeof(driverInformation->x_monitor));
    DbgPrint("Copying XMonitorStruct %s\n",
        (driverInformation->x_monitor.x_process_name == NULL ? "Null" : driverInformation->x_monitor.x_process_name));
}

NTSTATUS SwitchXMonitorStruct(IN PIRP Irp) {
    DbgPrint("Enter SwitchXMonitorStruct\n");
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);

    if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof(ProcessXMonitorStruct)) {
        PProcessXMonitorStruct pActivateInfo = (PProcessXMonitorStruct)(Irp->AssociatedIrp.SystemBuffer);
        PDriverData driverInformation = GetDeviceData();

        if (driverInformation->x_monitor.monitor_on != pActivateInfo->monitor_on)
        {
            CopyXMonitorStruct(pActivateInfo);
            ntStatus = SetCTNRoutine(!pActivateInfo->monitor_on);
        }
        else
            DbgPrint("XMonitor state has been repeated.\n");
    }
    else
        DbgPrint("XMonitor. Memory sizes aren't equal.\n");

    return ntStatus;
}