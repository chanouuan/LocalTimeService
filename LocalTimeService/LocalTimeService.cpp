// 主函数

#include "stdafx.h"

char iniFile[200];
char logFile[200];
char *paramServiceName;
char *paramNtpServer;
int  paramNtpPort;
char *paramNtpInterval;

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

#define SLEEP_TIME 1000
#define SAFE_CALL(FuncCall, ErrorCode) \
if (FuncCall == ErrorCode) { \
	char error[100]; \
	sprintf(error, "ErrorCode:%ld,Line:%ld", GetLastError(), __LINE__); \
	WriteToLog(error); \
	printf(error); \
	exit(-1); \
} \

struct _tagGlobalTickCount_t
{
	//API ULONGLONG WINAPI GetTickCount64(void);
	typedef ULONGLONG(WINAPI *GETTICKCOUNT64)(void);
	GETTICKCOUNT64 pGetTickCount64;

	//HIGH-RESOLUTION PERFORMANCE COUNTER
	BOOL bMMTimeValid;
	LARGE_INTEGER m_Start, m_Freq;

	_tagGlobalTickCount_t()
	{
		pGetTickCount64 = NULL;
		bMMTimeValid = FALSE;
		memset(&m_Start, 0, sizeof(m_Start));
		memset(&m_Freq, 0, sizeof(m_Freq));

		if (pGetTickCount64 = (GETTICKCOUNT64)GetProcAddress(GetModuleHandle("Kernel32.dll"), "GetTickCount64")) //API valid
		{
			OutputDebugStringA("GetTickCount64 API Valid\r\n");
		}
		else if (QueryPerformanceCounter(&m_Start) && QueryPerformanceFrequency(&m_Freq)) //high-resolution count valid
		{
			bMMTimeValid = TRUE;
			OutputDebugStringA("high-resolution count valid\r\n");
		}
		else //use default time
		{
			OutputDebugStringA("just GetTickCount() support only\r\n");
		}
	}

	ULONGLONG GetTickCount64(void)
	{
		if (pGetTickCount64) //api
		{
			return pGetTickCount64();
		}
		else if (bMMTimeValid) //high-resolution count
		{
			LARGE_INTEGER m_End = { 0 };
			QueryPerformanceCounter(&m_End);
			return (ULONGLONG)((m_End.QuadPart - m_Start.QuadPart) / (m_Freq.QuadPart / 1000));
		}
		else //normal
		{
			return GetTickCount();
		}
	}
}GlobalTickCount;
#define _GetTickCount64()(GlobalTickCount.GetTickCount64())

struct LocalTimeStruct
{
	long  LastTickCount;
	long  RealLocalTime;
	long  CurrLocalTime;
};
struct LocalTimeStruct localTimeStruct;

// 获取开机时长
long GetTickLongCount()
{
	return (long)(_GetTickCount64() / 1000);
}

int WriteLocalTime()
{
	FILE *fp;
	if ((fp = fopen(iniFile, "r+")) == NULL) {
		if (!InitFile()) {
			return 0;
		}
		if ((fp = fopen(iniFile, "r+")) == NULL) {
			return 0;
		}
	}

	// 读取配置
	if (!GetInIStruct(fp)) {
		return 0;
	}

	// 获取开机时长
	long tickCount = GetTickLongCount();
	long localTime = (long)time(NULL);

	if (localTimeStruct.RealLocalTime <= 0) {
		// 初始值
		localTimeStruct.RealLocalTime = localTime;
		localTimeStruct.LastTickCount = tickCount;
		localTimeStruct.CurrLocalTime = localTime;
	}
	else {
		if (localTimeStruct.LastTickCount > tickCount) {
			// 时间被重置
			localTime += 1;
			if (localTime > localTimeStruct.CurrLocalTime) {
				localTimeStruct.RealLocalTime = localTimeStruct.RealLocalTime + (localTime - localTimeStruct.CurrLocalTime);
			}
			// 记录日志
			char str[200];
			sprintf(str, "上次真实时间:%ld,上次本地时间:%ld,上次运行时间:%ld,本地时间:%ld,运行时间:%ld",
				localTimeStruct.RealLocalTime,
				localTimeStruct.CurrLocalTime,
				localTimeStruct.LastTickCount,
				localTime,
				tickCount);
			WriteToLog(str);
		}
		else {
			// 真实时间
			localTimeStruct.RealLocalTime = localTimeStruct.RealLocalTime + (tickCount - localTimeStruct.LastTickCount);
		}
		localTimeStruct.LastTickCount = tickCount;
		localTimeStruct.CurrLocalTime = localTime;
	}

	rewind(fp);
	fprintf(fp, "LastTickCount=%ld\nRealLocalTime=%ld\nCurrLocalTime=%ld\n",
		localTimeStruct.LastTickCount,
		localTimeStruct.RealLocalTime,
		localTimeStruct.CurrLocalTime);
	fclose(fp);
	return 1;
}

int WriteToLog(char* str)
{
	FILE *fp;
	if ((fp = fopen(logFile, "a+")) == NULL) {
		return 0;
	}

	// 当前时间
	time_t rawtime = time(NULL);
	struct tm *info = localtime(&rawtime);
	char s[20];
	strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", info);

	fprintf(fp, "[%s] %s\n", s, str);
	fclose(fp);
	return 1;
}

// 读取配置
int GetInIStruct(FILE *fp)
{
	if (NULL == fp) {
		return 0;
	}

	char *chr;
	char sLine[25];

	localTimeStruct.LastTickCount = 0;
	localTimeStruct.RealLocalTime = 0;
	localTimeStruct.CurrLocalTime = 0;

	while (NULL != fgets(sLine, 25, fp)) {
		if (NULL == (chr = strchr(sLine, '='))) {
			continue;
		}
		if (0 == strncmp(sLine, "LastTickCount", 13)) {
			localTimeStruct.LastTickCount = atol(chr + 1);
		}
		if (0 == strncmp(sLine, "RealLocalTime", 13)) {
			localTimeStruct.RealLocalTime = atol(chr + 1);
		}
		if (0 == strncmp(sLine, "CurrLocalTime", 13)) {
			localTimeStruct.CurrLocalTime = atol(chr + 1);
		}
	}

	return 1;
}

// 创建文件
int InitFile()
{
	FILE *fp;
	// 判断文件是否打开失败
	if ((fp = fopen(iniFile, "r")) == NULL) {
		if ((fp = fopen(iniFile, "a")) == NULL) {
			return 0;
		}
	}
	fclose(fp);
	return 1;
}

// Control Handler
void ControlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		WriteToLog("Monitoring stopped.");

		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);
		return;

	case SERVICE_CONTROL_SHUTDOWN:
		WriteToLog("Monitoring stopped.");

		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &ServiceStatus);
		return;

	default:
		break;
	}

	// Report current status
	SetServiceStatus(hStatus, &ServiceStatus);

	return;
}

void ServiceMain(int argc, char** argv)
{
	int error;

	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;

	hStatus = RegisterServiceCtrlHandler(paramServiceName, (LPHANDLER_FUNCTION)ControlHandler);
	if (hStatus == (SERVICE_STATUS_HANDLE)0) {
		// Registering Control Handler failed
		return;
	}

	// Initialize Service 
	error = InitService();
	if (!error) {
		// Initialization failed
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		ServiceStatus.dwWin32ExitCode = -1;
		SetServiceStatus(hStatus, &ServiceStatus);
		return;
	}
	// We report the running status to SCM. 
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hStatus, &ServiceStatus);

	// The worker loop of a service
	while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		// todo
		int result = WriteLocalTime();

		if (!result)
		{
			WriteToLog("Monitoring error.");
		}
		Sleep(SLEEP_TIME);
	}
	return;
}

void installService()
{
	SC_HANDLE scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	SAFE_CALL(scmHandle, NULL);

	SC_HANDLE serviceHandle = OpenService(scmHandle,
		paramServiceName,
		SERVICE_ALL_ACCESS);

	if (NULL != serviceHandle) {
		printf("Error: LTS服务已安装！\n");
		exit(-1);
	}

	char basePath[200];
	char servicePath[200];
	GetModuleFileNameA(NULL, basePath, 200);

	sprintf(servicePath, "\"%s\" -run -servicename=%s -ntpserver=%s -ntpport=%d -ntpinterval=%s", 
		basePath, 
		paramServiceName, 
		NULL == paramNtpServer ? "" : paramNtpServer,
		NULL == paramNtpPort ? 0 : paramNtpPort,
		NULL == paramNtpInterval ? "" : paramNtpInterval);

	serviceHandle = CreateService(scmHandle,
		paramServiceName,
		paramServiceName,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		servicePath,
		NULL, NULL, NULL, NULL, NULL);
	SAFE_CALL(serviceHandle, NULL);

	CloseServiceHandle(scmHandle);
	CloseServiceHandle(serviceHandle);
}

void uninstallService()
{
	SC_HANDLE scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SAFE_CALL(scmHandle, NULL);

	SC_HANDLE serviceHandle = OpenService(scmHandle,
		paramServiceName,
		SERVICE_ALL_ACCESS);

	if (NULL == serviceHandle) {
		printf("Error: LTS服务未安装！\n");
		exit(-1);
	}

	SERVICE_STATUS serviceStatus;
	SAFE_CALL(QueryServiceStatus(serviceHandle, &serviceStatus), 0);
	if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
		SAFE_CALL(ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus), 0);
		SAFE_CALL(serviceStatus.dwCurrentState, NO_ERROR);

		do {
			SAFE_CALL(QueryServiceStatus(serviceHandle, &serviceStatus), 0);
			Sleep(1000);
		} while (serviceStatus.dwCurrentState != SERVICE_STOPPED);
	}

	SAFE_CALL(DeleteService(serviceHandle), FALSE);

	CloseServiceHandle(scmHandle);
	CloseServiceHandle(serviceHandle);
}

void startService()
{
	SC_HANDLE scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SAFE_CALL(scmHandle, NULL);

	SC_HANDLE serviceHandle = OpenService(scmHandle,
		paramServiceName,
		SERVICE_ALL_ACCESS);
	SAFE_CALL(serviceHandle, NULL);

	SERVICE_STATUS serviceStatus;
	SAFE_CALL(QueryServiceStatus(serviceHandle, &serviceStatus), 0);
	if (serviceStatus.dwCurrentState == SERVICE_START &&
		serviceStatus.dwCurrentState != SERVICE_START_PENDING) {
		return;
	}

	SAFE_CALL(StartService(serviceHandle, 0, NULL), FALSE);

	CloseServiceHandle(scmHandle);
	CloseServiceHandle(serviceHandle);
}

void runService()
{
	SC_HANDLE scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SAFE_CALL(scmHandle, NULL);

	SC_HANDLE serviceHandle = OpenService(scmHandle,
		paramServiceName,
		SERVICE_ALL_ACCESS);

	if (NULL == serviceHandle) {
		printf("Error: 请先安装LTS服务！\n");
		exit(-1);
	}

	serviceHandle = NULL;

	SERVICE_TABLE_ENTRY ServiceTable[2];
	ServiceTable[0].lpServiceName = paramServiceName;
	ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

	ServiceTable[1].lpServiceName = NULL;
	ServiceTable[1].lpServiceProc = NULL;
	// Start the control dispatcher thread for our service
	SAFE_CALL(StartServiceCtrlDispatcher(ServiceTable), 0);
}

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	int t = 0;
	while (t != 1) {
		t = adjustTime(paramNtpServer, paramNtpPort);
	}
	ExitThread(-1);
	return t;
}

int main(int argc, char* argv[])
{
	// 变量初始值
	paramServiceName = "LTS";
	paramNtpServer   = "ntp1.aliyun.com";
	paramNtpPort     = 123;
	paramNtpInterval = ":0";

	// Usage
	char *usage = "Usage: LocalTimeService [-install/-uninstall] [-servicename] [-ntpserver] [-ntpport] [-ntpinterval]\n";

	// 获取本地路径
	char basePath[200];
	GetModuleFileNameA(NULL, basePath, 200);
	(strrchr(basePath, '\\'))[0] = 0;
	sprintf(iniFile, "%s\\%s", basePath, "localtime.ini");
	sprintf(logFile, "%s\\%s", basePath, "localtime.log");

	HANDLE hThread1 = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
	Sleep(5000);
	CloseHandle(hThread1);
	return 1;
	
	int i;
	char *chr;
	for (i = 1; i < argc; i++) {
		// 服务名称
		if (0 == strncmp(argv[i], "-servicename", 12)) {
			if (NULL != (chr = strchr(argv[i], '='))) {
				paramServiceName = chr + 1;
			}
		}
		// NTP服务器地址
		if (0 == strncmp(argv[i], "-ntpserver", 10)) {
			if (NULL != (chr = strchr(argv[i], '='))) {
				paramNtpServer = chr + 1;
			}
		}
		// NTP服务器端口
		if (0 == strncmp(argv[i], "-ntpport", 8)) {
			if (NULL != (chr = strchr(argv[i], '='))) {
				paramNtpPort = atol(chr + 1);
			}
		}
		// NTP同步时间 :0:1
		if (0 == strncmp(argv[i], "-ntpinterval", 12)) {
			if (NULL != (chr = strchr(argv[i], '='))) {
				paramNtpInterval = chr + 1;
			}
		}
	}
	chr = NULL;

	if (NULL == paramServiceName) {
		printf(usage);
		exit(-1);
	}

	if (argc >= 2) {
		if (0 == strncmp(argv[1], "-run", 4)) {
			runService();
			printf("LTS run success!\n");
		}
		else if (0 == strncmp(argv[1], "-install", 8)) {
			installService();
			printf("LTS install success!\n");
			startService();
			printf("LTS start success!\n");
		}
		else if (0 == strncmp(argv[1], "-uninstall", 10)) {
			uninstallService();
			printf("LTS uninstall success!\n");
		}
		else {
			printf(usage);
		}
	}
	else {
		printf(usage);
	}

	return 0;
}