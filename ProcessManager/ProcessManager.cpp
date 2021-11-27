#include "ProcessManager.h"
#include <iostream>
#include <queue>
#include "ThreadMutex.h"
using namespace std;

ProcessXMonitorStruct x_monitor;
HANDLE driver;
bool isAlive = true;
queue<HANDLE> y_proc_instances;
ThreadMutex mutex;

void CloseControl() {
    x_monitor.monitor_on = FALSE;
    DWORD bytesReturn;
    if (!DeviceIoControl(driver, IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE, &x_monitor, sizeof(ProcessXMonitorStruct), 0, 0, &bytesReturn, NULL))
        wcout << L"Error Sending IO Control message in Close Control" << endl;
}


DWORD WINAPI TerminYProcessThreadProcedure(LPVOID state) {
    HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, L"Global\\" SYNC_TERMIN_PROC_Y_EVENT);
    do
    {
        DWORD status = WaitForSingleObject(hEvent, 1000);

        if (isAlive && status == WAIT_OBJECT_0)
        {
            HANDLE process = INVALID_HANDLE_VALUE;

            mutex.Lock();
                if (!y_proc_instances.empty()) {
                    process = y_proc_instances.front();
                    y_proc_instances.pop();
                }
            mutex.Unlock();

            if (process != INVALID_HANDLE_VALUE) {
                if (!TerminateProcess(process, EXIT_SUCCESS)) {
                    wcout << "Error terminating Y process!" << endl;
                    wcout << GetLastError() << endl;
                }
                else {
                    wcout << "Process Y terminated!" << endl;
                }

                CloseHandle(process);
            }
            else {
                wcout << "Invalid handle terminated" << endl;
            }
        }
    } while (isAlive);

    CloseHandle(hEvent);
    return EXIT_SUCCESS;
}

DWORD WINAPI CreateYProcessThreadProcedure(LPVOID state) {
    HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, L"Global\\" SYNC_CREATE_PROC_Y_EVENT);
    do {
        DWORD status = WaitForSingleObject(hEvent, 1000);
        if (isAlive && status == WAIT_OBJECT_0)
        {
            STARTUPINFO startupInfo;
            ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
            startupInfo.cb = sizeof(STARTUPINFO);

            PROCESS_INFORMATION processInformation;
            if (!CreateProcess(y_process_file_name, NULL, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo, &processInformation))
                wcout << "Error creating Y process " << y_process_file_name << " at " << GetLastError() << endl;
            else
            {
                wcout << "Process Y created!" << endl;
                mutex.Lock();
                    y_proc_instances.push(processInformation.hProcess);
                mutex.Unlock();
            }
        }
    } while (isAlive);

    CloseHandle(hEvent);
    return EXIT_SUCCESS;
}

void WaitExit() {
    HANDLE createThread = CreateThread(0, 0, CreateYProcessThreadProcedure, NULL, 0, NULL);
    HANDLE terminThread = CreateThread(0, 0, TerminYProcessThreadProcedure, NULL, 0, NULL);
    do {
        WCHAR buf[1024];
        wcout << "Write exit to close application" << endl;
        wcin >> buf;
        if (!wcscmp(buf, L"exit"))
            break;
    } while (true);

    isAlive = false;

    if (WaitForSingleObject(createThread, 2000) != WAIT_OBJECT_0) {
        wcout << L"Timeout close createThread, terminated" << endl;
        TerminateThread(createThread, EXIT_FAILURE);
    }

    if (WaitForSingleObject(terminThread, 2000) != WAIT_OBJECT_0) {
        wcout << L"Timeout close terminThread, terminated" << endl;
        TerminateThread(terminThread, EXIT_FAILURE);
    }
}

void SyncWithDriver() {
    driver = CreateFile(L"\\\\.\\" DRIVER_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (driver != INVALID_HANDLE_VALUE) {
        DWORD bytesReturn;
        if (!DeviceIoControl(driver, IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE, &x_monitor, sizeof(ProcessXMonitorStruct), 0, 0, &bytesReturn, NULL))
            wcout << L"Error Sendind IO Control Message" << endl;
        else {
            WaitExit();
            CloseControl();
        }

        CloseHandle(driver);
    }
    else
        wcout << L"Error CreateFile " DRIVER_NAME << endl;
}

bool GetProcXAndY()
{
    wcout << L"Enter process X executable name:" << endl;
    cin >> x_monitor.x_process_name;
    wcout << L"OK" << endl;

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = y_process_file_name;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(y_process_file_name);
    ofn.lpstrFilter = NULL;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    return GetOpenFileName(&ofn) != 0;
}

string GetProcessNameByHandle(HANDLE hProcess) {
    if (NULL == hProcess)
        return "<unknown>";

    CHAR szProcessName[MAX_PATH] = "<unknown>";
    HMODULE hMod;
    DWORD cbNeeded;

    if (::EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        ::GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName));

    return string(szProcessName);
}

string GetProcessNameByID(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, processID);
    string result = GetProcessNameByHandle(hProcess);
    CloseHandle(hProcess);
    return result;
}

void InitializeXMonitorStruct() {
    x_monitor.monitor_on = TRUE;
    string processName = GetProcessNameByID(GetCurrentProcessId());
    strcpy(x_monitor.manager_name, processName.c_str());
}

int main() {
    InitializeXMonitorStruct();
    if (GetProcXAndY()) {
        wcout << L"Chosen executable for Y process " << y_process_file_name << endl;
        SyncWithDriver();
    }

    system("PAUSE");
    return 0;
}