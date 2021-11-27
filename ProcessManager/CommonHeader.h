#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <Psapi.h>
#define _X64_

#define DRIVER_NAME L"OSISP_LabFive_Driver"
#define SYNC_CREATE_PROC_Y_EVENT L"CreateProcessYEvent"
#define SYNC_TERMIN_PROC_Y_EVENT L"TerminateProcessYEvent"

#define IOCTL_UNKNOWN_BASE FILE_DEVICE_UNKNOWN

#define IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE \
	CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0800, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define FILE_NAME_SIZE 260
typedef struct _ProcessXMonitorStruct
{
	BOOLEAN monitor_on;
	CHAR manager_name[FILE_NAME_SIZE];
	CHAR x_process_name[FILE_NAME_SIZE];
}ProcessXMonitorStruct, *PProcessXMonitorStruct;
