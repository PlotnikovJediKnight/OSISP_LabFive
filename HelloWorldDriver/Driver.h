#pragma once
#include "CommonHeader.h"

#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif


typedef PCHAR(*GET_PROCESS_IMAGE_NAME) (PEPROCESS Process); 
GET_PROCESS_IMAGE_NAME gPsGetProcessImageFileName;


typedef struct _DriverData {

    inline void Initialize() {
        RtlZeroMemory(x_monitor.x_process_name, sizeof(x_monitor.x_process_name));
        x_monitor.monitor_on = FALSE;
        process_y_events.Initialize();
    }

    inline void Finalize() {
        process_y_events.Close();
    }

    typedef struct _ProcessCreationTermination_Notification {

        inline void InitProcessCTN(HANDLE hParentID, HANDLE hProcessID, BOOLEAN isCreate) {
            this->hParentID = hParentID;
            this->hProcessID = hProcessID;
            this->isCreate = isCreate;
        }

        HANDLE hParentID;
        HANDLE hProcessID;
        BOOLEAN isCreate;

    }ProcessCTN;

    typedef struct _ProcessYEventsStruct {

        inline void Initialize() {
            UNICODE_STRING uszProcessEventString;
            RtlInitUnicodeString(&uszProcessEventString, L"\\BaseNamedObjects\\" SYNC_CREATE_PROC_Y_EVENT);
            create_process_y_event = IoCreateNotificationEvent(&uszProcessEventString, &create_process_y_handle);
            KeClearEvent(create_process_y_event);

            RtlInitUnicodeString(&uszProcessEventString, L"\\BaseNamedObjects\\" SYNC_TERMIN_PROC_Y_EVENT);
            termin_process_y_event = IoCreateNotificationEvent(&uszProcessEventString, &termin_process_y_handle);
            KeClearEvent(termin_process_y_event);
        };

        inline void Close() {
            ZwClose(create_process_y_handle);
            ZwClose(termin_process_y_handle);
        }

        inline void SetCreateEvent() {
            PulseEvent(create_process_y_event);
        }

        inline void SetCloseEvent() {
            PulseEvent(termin_process_y_event);
        }

    private:
        inline void PulseEvent(PKEVENT pEvent) {
            KeSetEvent(pEvent, 0, FALSE);
            KeClearEvent(pEvent);
        }

        PKEVENT create_process_y_event;
        HANDLE create_process_y_handle;
        PKEVENT termin_process_y_event;
        HANDLE termin_process_y_handle;
    }ProcessYEventsStruct;

    ProcessYEventsStruct process_y_events;
    ProcessCTN process_ctn;
    ProcessXMonitorStruct x_monitor;

}DriverData, *PDriverData;


PDriverData GetDeviceData();
PCSTR GetProcessFileNameById(IN HANDLE handle);
VOID FireProcessYEvent();
BOOLEAN IsProcessXManagerChild();
BOOLEAN IsProcessX();
VOID WorkItem(IN PDEVICE_OBJECT  DeviceObject, IN OPTIONAL PVOID  Context);
VOID InitializeDriverData();

//======================================================================

NTSTATUS SetCTNRoutine(IN BOOLEAN isDisabled);
VOID CopyXMonitorStruct(IN PProcessXMonitorStruct pActivateInfo);
NTSTATUS SwitchXMonitorStruct(IN PIRP Irp);

//======================================================================

/*Driver Callback Functions*/
NTSTATUS DriverUnsupportedMajorFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverCreateCloseFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverShutdownFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverDispatchIoctlFunction(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void DriverUnloadFunction(IN PDRIVER_OBJECT DriverObject);

VOID UnloadXProcessMonitor();