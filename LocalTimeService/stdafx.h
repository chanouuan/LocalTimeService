// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

// TODO:  在此处引用程序需要的其他头文件
#include <winsock2.h>
#include <crtdbg.h>
#include <sys/timeb.h>

#include <windows.h>
#include <direct.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

void ServiceMain(int argc, char** argv);
void ControlHandler(DWORD request);
int  InitService();
int  InitFile();
long GetTickLongCount();
int  GetInIStruct(FILE *fp);
int  WriteLocalTime();
int  WriteToLog(char* str);
void runService();
void installService();
void startService();
void uninstallService();
int  adjustTime(char *ntpServer, int ntpPort);

