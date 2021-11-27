#pragma once

#include "CommonHeader.h"


extern HANDLE driver;
extern WCHAR y_process_file_name[FILE_NAME_SIZE] = { 0 };
extern ProcessXMonitorStruct x_monitor;


void InitializeXMonitorStruct();
bool GetProcXAndY();
DWORD WINAPI CreateYProcessThreadProcedure(LPVOID state);
DWORD WINAPI TerminYProcessThreadProcedure(LPVOID state);
void WaitExit();
void CloseControl();
void SyncWithDriver();