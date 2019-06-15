// stdafx.cpp : ֻ������׼�����ļ���Դ�ļ�
// LocalTimeService.pch ����ΪԤ����ͷ
// stdafx.obj ������Ԥ����������Ϣ

#include "stdafx.h"

// TODO: �� STDAFX.H �������κ�����ĸ���ͷ�ļ���
//�������ڴ��ļ�������

const unsigned int JAN_1970 = 0X83AA7E80; //=2208988800(1970/1/1 - 1900/1/1 in seconds)*/

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
			printf("GetTickCount64 API Valid\r\n");
		}
		else if (QueryPerformanceCounter(&m_Start) && QueryPerformanceFrequency(&m_Freq)) //high-resolution count valid
		{
			bMMTimeValid = TRUE;
			printf("high-resolution count valid\r\n");
		}
		else //use default time
		{
			printf("just GetTickCount() support only\r\n");
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

// ��ȡ����ʱ��
long GetTickLongCount()
{
	return (long)(_GetTickCount64() / 1000);
}

// Service initialization
int InitService()
{
	if (!InitFile()) {
		return 0;
	}
	int result;
	result = WriteToLog("Monitoring started.");
	return result;
}

int SetSysCurrentTime(time_t rawtime)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	// Get a token for this process. 
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		return 0;
	}

	// Get the LUID for the shutdown privilege. 
	LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;  // one privilege to set    
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	// Get the shutdown privilege for this process. 
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	if (GetLastError() != ERROR_SUCCESS) {
		return 0;
	}

	// ת��ʱ���
	struct tm *info = localtime(&rawtime);

	SYSTEMTIME systm;
	systm.wYear = 1900 + info->tm_year;
	systm.wMonth = info->tm_mon + 1;
	systm.wDay = info->tm_mday;
	systm.wHour = info->tm_hour;
	systm.wMinute = info->tm_min;
	systm.wSecond = info->tm_sec;
	systm.wMilliseconds = 0;

	// NTPʱ��
	char ntpStr[20] = "";
	strftime(ntpStr, sizeof(ntpStr), "%Y-%m-%d %H:%M:%S", info);

	// ����ʱ��
	time_t curTime = time(NULL);
	struct tm *curInfo = localtime(&curTime);
	char curStr[20] = "";
	strftime(curStr, sizeof(curStr), "%Y-%m-%d %H:%M:%S", curInfo);

	// ���±���ʱ��
	int result = (int)SetLocalTime(&systm);

	char str[100] = "";
	sprintf(str, "CurTime=%s,NtpTime=%s,Result=%d", curStr, ntpStr, result);
	WriteToLog(str);

	return result;
}

// ==== Ntp ====

struct ntp_timestamp
{
	unsigned int secondsFrom1900;//1900��1��1��0ʱ0������������
	unsigned int fraction;//΢���4294.967296(=2^32/10^6)��
};

/* How to multiply by 4294.967296 quickly (and not quite exactly)
* without using floating point or greater than 32-bit integers.
* If you want to fix the last 12 microseconds of error, add in
* (2911*(x))>>28)
*/
inline unsigned int microseconds2ntp_fraction(unsigned int x)
{
	return (4294 * (x)+((1981 * (x)) >> 11));
}

/* The reverse of the above, needed if we want to set our microsecond
* clock based on the incoming time in NTP format.
* Basically exact.
*/
inline unsigned int ntp_fraction2microseconds(unsigned int x)
{
	return (((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16));
}

struct ntp_header
{
	union
	{
		struct
		{
			char local_precision;//��ʾ����ʱ�Ӿ���Ϊ2^local_precision�롣local_precisionͨ��Ϊ������
			char poll_intervals;//��ʾ���Լ��Ϊ2^poll_intervals�롣
			unsigned char stratum;//NTP�������׼���0��ʾ��ָ����1��ʾУ׼��ԭ���ӡ�Ӧ��Ϊ0��
			unsigned char mode : 3;//ͨ��ģʽ��Ӧ��Ϊ3����ʾ��client��
			unsigned char version_number : 3;//NTPЭ��汾�š�Ӧ��Ϊ3��
			unsigned char leap_indicator : 2;//����ָʾ��һ����0��
		};
		int noname;
	};

	int root_delay;//�����ɸ���2^16��ʾһ�롣���庬��μ�RFC1305��
	int root_dispersion;//ֻ��Ϊ����2^16��ʾһ�롣���庬��μ�RFC1305��
	int reference_clock_identifier;//���庬��μ�RFC1305��һ����0��
};//û�д���Ļ���ntp_header�Ĵ�СӦ��Ϊ16�ֽڡ�

struct ntp_packet
{
	ntp_header header;
	//�����ĸ�ʱ���Ϊ����ʱ�䡣��������ʱ��λ�õġ�
	ntp_timestamp reference;//���庬��μ�RFC1305��һ����0��
	ntp_timestamp originate;//�ϴη���ʱ��
	ntp_timestamp receive;//����ʱ��
	ntp_timestamp transmit;//����ʱ��
};//û�д���Ļ���ntp_header�Ĵ�СӦ��Ϊ48�ֽڡ�

bool send_ntp_packet(SOCKET sock, const sockaddr* to)
{
	ntp_packet packet;
	memset(&packet, 0, sizeof(ntp_packet));
	packet.header.local_precision = -6;
	packet.header.poll_intervals = 4;
	packet.header.stratum = 0;
	packet.header.mode = 3;
	packet.header.version_number = 3;
	packet.header.leap_indicator = 0;

	packet.header.root_delay = 1 << 16;
	packet.header.root_dispersion = 1 << 16;
	__timeb32 now;
	_ftime32(&now);
	packet.transmit.secondsFrom1900 = now.time + JAN_1970;
	packet.transmit.fraction = microseconds2ntp_fraction(now.millitm);

	packet.header.noname = htonl(packet.header.noname);
	packet.header.root_delay = htonl(packet.header.root_delay);
	packet.header.root_dispersion = htonl(packet.header.root_dispersion);
	packet.transmit.secondsFrom1900 = htonl(packet.transmit.secondsFrom1900);
	packet.transmit.fraction = htonl(packet.transmit.fraction);
	int bytesSent = sendto(sock, (const char*)&packet, sizeof(packet), 0, to, sizeof(struct sockaddr_in));
	return bytesSent != SOCKET_ERROR;
}

bool recv_ntp_packet(SOCKET sock, ntp_packet& packet)
{
	int bytesRead = recvfrom(sock, (char*)&packet, sizeof(ntp_packet), 0, NULL, NULL);
	return SOCKET_ERROR != bytesRead;
}

int set_local_time(const ntp_packet& packet)
{
	ntp_timestamp server_transmit_time;
	server_transmit_time.secondsFrom1900 = ntohl(packet.transmit.secondsFrom1900);
	server_transmit_time.fraction = ntohl(packet.transmit.fraction);
	timeval newtime;
	newtime.tv_sec = server_transmit_time.secondsFrom1900 - JAN_1970;
	newtime.tv_usec = ntp_fraction2microseconds(server_transmit_time.fraction);
	if (NULL == newtime.tv_sec || newtime.tv_sec <= 0) {
		return 0;
	}
	return SetSysCurrentTime((time_t)newtime.tv_sec);
}

int CleanSocket(SOCKET socket, int errorNo, char *msg)
{
	if (NULL != socket) {
		closesocket(socket);
	}
	WSACleanup();
	if (NULL != msg) {
		WriteToLog(msg);
		printf(msg);
	}
	return errorNo;
}

int adjustTime(char *ntpServer, int ntpPort)
{
	int errorNo = 0;
	char errorMsg[100] = "";
	//У�����ݽṹ����
	_ASSERTE(sizeof(ntp_header) == 16);
	_ASSERTE(sizeof(ntp_packet) == 48);
	if (sizeof(ntp_packet) != 48) {
		return CleanSocket(NULL, -1, "Socket Init Fail.");
	}

	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		return CleanSocket(NULL, -2, "Error at WSAStartup().");
	}

	// Create a socket.
	SOCKET m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET) {
		sprintf(errorMsg, "Error at socket(): %ld.", WSAGetLastError());
		return CleanSocket(m_socket, -3, errorMsg);
	}

	/* bind local address. */
	sockaddr_in addr_src;
	int addr_len = sizeof(struct sockaddr_in);
	memset(&addr_src, 0, addr_len);
	addr_src.sin_family = AF_INET;
	addr_src.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_src.sin_port = htons(0);
	if (SOCKET_ERROR == bind(m_socket, (struct sockaddr*)&addr_src, addr_len)) {
		sprintf(errorMsg, "Error at bind(): %ld.", WSAGetLastError());
		return CleanSocket(m_socket, -4, errorMsg);
	}

	/*
	timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv))) {
	printf("Error at bind(): %ld.", WSAGetLastError());
	errorNo = -4;
	goto Cleanup;
	}
	if (SOCKET_ERROR == setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv))) {
	printf("Error at bind(): %ld.", WSAGetLastError());
	errorNo = -4;
	goto Cleanup;
	}
	*/

	//fill addr_dst
	sockaddr_in addr_dst;
	memset(&addr_dst, 0, addr_len);
	addr_dst.sin_family = AF_INET;
	{
		struct hostent* host = gethostbyname(ntpServer);
		if (NULL == host || 4 != host->h_length) {
			sprintf(errorMsg, "Error at gethostbyname(%s).", ntpServer);
			return CleanSocket(m_socket, -5, errorMsg);
		}
		memcpy(&(addr_dst.sin_addr.s_addr), host->h_addr_list[0], 4);
	}
	addr_dst.sin_port = htons(ntpPort);

	/*
	u_long ul = 1;
	//����Ϊ����������
	if (SOCKET_ERROR == ioctlsocket(m_socket, FIONBIO, &ul)) {
	printf("Error at ioctlsocket(): %ld.", WSAGetLastError());
	errorNo = -6;
	goto Cleanup;
	}
	*/

	//���ö�ȡ��ʱ
	fd_set fds_read;
	timeval timeout;
	fds_read.fd_count = 1;
	fds_read.fd_array[0] = m_socket;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	if (SOCKET_ERROR == select(0, &fds_read, NULL, NULL, &timeout)) {
		sprintf(errorMsg, "Error at select(): %ld.", WSAGetLastError());
		return CleanSocket(m_socket, -6, errorMsg);
	}

	/*
	ul = 0;
	//����Ϊ��������
	if (SOCKET_ERROR == ioctlsocket(m_socket, FIONBIO, &ul)) {
	printf("Error at ioctlsocket(): %ld.", WSAGetLastError());
	errorNo = -6;
	goto Cleanup;
	}
	*/

	if (send_ntp_packet(m_socket, (sockaddr*)&addr_dst) == false) {
		sprintf(errorMsg, "Error at send(): %ld.", WSAGetLastError());
		return CleanSocket(m_socket, -7, errorMsg);
	}

	ntp_packet packet;
	if (recv_ntp_packet(m_socket, packet) == false) {
		sprintf(errorMsg, "Error at recvfrom(): %ld.", WSAGetLastError());
		return CleanSocket(m_socket, -8, errorMsg);
	}

	if (set_local_time(packet) == 0) {
		return CleanSocket(m_socket, -9, "Error at set_local_time.");
	}

	return CleanSocket(m_socket, 1, NULL);
}

int strInArray(char *str, char *delims, char *needle)
{
	char *result = NULL;
	char tmp[30] = "";
	strcpy(tmp, str);
	printf("%s \n", tmp);
	result = strtok(tmp, delims);
	while (result != NULL) {
		if (0 == strcmp(result, needle)) {
			return 1;
		}
		result = strtok(NULL, delims);
	}
	return 0;
}

