// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
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

