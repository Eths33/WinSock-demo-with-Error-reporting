/*
	Working socket demo with error reporting as of VS2015 x64. Sharing this because when I was making samples there
	were limited working samples for my needs, and it was a bit hard to find all of the error reporting code.
	-Greg Gutmann

	The terms client and sever are used loosly: the Server is the code that starts first and waits for a connection.
	For this code the sever is the main receiver and the client is the main sender.
	The code connects exchanges parameters - starts the main data sending loop - after a given time the "ClientFile"
	sets a flag for the connection to be closed.
	There are many print lines commented out, they can be uncommented to watch more of what is going on in the CMD.

	This work is licensed under a Creative Commons Attribution 4.0 International License
	This license alows distribution, remix, tweak, and building upon the work, even commercially, as long as
	credit is given for the original creation. https://creativecommons.org/licenses/by/4.0
*/

/*
	Internet quote :
	The practical limit for the data length which is imposed by the underlying IPv4 protocol is 65, 507 bytes
	(65, 535 − 8 byte UDP header − 20 byte IP header).
	In IPv6 jumbograms it is possible to have UDP packets of size greater than 65, 535 bytes.

	However the SO_RCVBUF & SO_SNDBUF can be resized quite large > MB
*/

#ifndef UNICODE
#define UNICODE
#endif

//WinSock inclusdes
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <stdio.h>
#include <conio.h>
#include <iostream>
#include <math.h>

//Can fail to compile without the define below with some compilers
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "pthreadVC2.lib")

struct timer_ms {
	double PCFreq;
	__int64 CounterStart;
};

const UINT initVarCount = 4;
const UINT initDataSize = sizeof(UINT64)*initVarCount;
UINT64 initSettings[initVarCount];

const char * IP_Address = "127.0.0.1"; //IP is the computer the will be connected to
const unsigned short Port = 27015;
timer_ms timer;
timer_ms timerAll;

char * SendBuf_init = NULL;
float * locations = NULL;
char * SendBuf_data = NULL;
WSADATA SendWsaData;
sockaddr_in address;

SOCKET ConnectSocket = INVALID_SOCKET;

WSADATA RecvWsaData;
sockaddr_in RecvAddr;

char statusBuffer[sizeof(int)];  
int running;
int loopCnt; 
int dataAccSend;
UINT64 cordanateCount;
UINT64 floatCount;
UINT64 sizeSend;
UINT64 otherValue; // Not used

void StartCounter_ms(timer_ms &t)
{
	LARGE_INTEGER li;
	if(!QueryPerformanceFrequency(&li))
		std::wcout << "QueryPerformanceFrequency failed!\n";

	t.PCFreq = double(li.QuadPart)/1000.0;

	QueryPerformanceCounter(&li);
	t.CounterStart = li.QuadPart;
}
double GetCounter_ms(timer_ms &t)
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart-t.CounterStart)/t.PCFreq;
}
void ErrorReport(ULONG dd) 
{ 
    // Retrieve the system error message for the last-error code and translate to English

    LPTSTR lpMsgBuf = NULL;
    DWORD dw = dd; 

    int length = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL );

	
	if(dw == 0)
		wprintf(L"%s\n", lpMsgBuf);
	else
		wprintf(L"Error: %s\n", lpMsgBuf);

    LocalFree(lpMsgBuf);
}
void cleanUpSockets(){
	wprintf(L"\nExiting.\n");
	if(closesocket(ConnectSocket) != 0) { /* Error or already closed */ }
	if(locations != NULL)
		free(locations);
	if(SendBuf_init != NULL)
		free(SendBuf_init);
	if(SendBuf_data != NULL)
		free(SendBuf_data);
	WSACleanup();

	float dataS =(float)(loopCnt*sizeSend)/1000000.0f;
	printf("Bytes Sent: %5.2f  Out of %5.2f\n", (dataAccSend/1000000.0f), dataS);

	//_getch();
}
void checkError_MaybeExit(int iResult, wchar_t * stringMsg){
	if (iResult == SOCKET_ERROR) {
		int error = WSAGetLastError();
        wprintf(L"%s failed with error: %d\n", stringMsg, error);
		ErrorReport(error);
        cleanUpSockets();
		pthread_exit((void*)EXIT_FAILURE);
    }
}
void sock_checkErrorMaybeExit(SOCKET * sock){
	if (*sock == INVALID_SOCKET) {
		int error = WSAGetLastError();
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		ErrorReport(error);
		cleanUpSockets();
		pthread_exit((void*)EXIT_FAILURE);
	}
}
void initSenderSocket(int addSize){
	int iResult;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &SendWsaData);
    checkError_MaybeExit(iResult, L"WSAStartup");
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // Create a socket for sending data
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sock_checkErrorMaybeExit(&ConnectSocket);
	
    address.sin_family = AF_INET;
    address.sin_port = htons(Port);
    address.sin_addr.s_addr = inet_addr(IP_Address);

	iResult = connect( ConnectSocket, (SOCKADDR*) &address, addSize );
	checkError_MaybeExit(iResult, L"connect");
}
void changeBuffer(int siz, SOCKET * sock) {
	int iResult = 0;
	int optVal;
	int optLen = sizeof(int);
	int optValNew = siz;
	int optLenNew = sizeof(int);

	//Receive Buffer: Checking -> changing -> confirming change by recheacking
	if (getsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld", optVal);

	iResult = setsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char *)&optValNew, optLenNew);
	if (iResult == SOCKET_ERROR) {
		wprintf(L"\tsetsockopt for failed with error: %u\n", WSAGetLastError());
	}
	else
		wprintf(L"Checking SO_RCVBUF to %d\n", optValNew);

	if (getsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Changed SockOpt Value: %ld\n", optVal);

	//Send Buffer: Checking -> changing -> confirming change by recheacking
	if (getsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld", optVal);

	iResult = setsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char *)&optValNew, optLenNew);
	if (iResult == SOCKET_ERROR) {
		wprintf(L"\tsetsockopt for failed with error: %u\n", WSAGetLastError());
	}
	else
		wprintf(L"\tChanged SO_SNDBUF to %d\n", optValNew);

	if (getsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld\n", optVal);
}
void indicateReady(int msgSize){
	wprintf(L"Indicating ready\t");
	memset(statusBuffer, 0, sizeof(statusBuffer));
	memcpy(statusBuffer, &running, msgSize);

    int iResult = send(ConnectSocket, statusBuffer, msgSize, 0);
    checkError_MaybeExit(iResult, L"indicateReady");
	wprintf(L"Status sent\n");

	if(!running){
		Sleep(5); //Give time for the server to recv
		cleanUpSockets();
		pthread_exit((void*)0);
	}
}
void waitOnOther(int msgSize){
	wprintf(L"Wating on other\t");
	memset(statusBuffer, 0, sizeof(statusBuffer));
    int iResult = recv(ConnectSocket, statusBuffer, msgSize, 0);
    checkError_MaybeExit(iResult, L"waitOnOther");
	memcpy(&running, statusBuffer, msgSize);

	printf("Other Connection is ready Continuing\n");

	if(!running){
		cleanUpSockets();
		pthread_exit((void*)0);
	}
}
void * clientThread(void * arg)
{
	//############# Start Step 1 init #####################
    int iResult;
	timer.CounterStart = 0;
	timer.PCFreq = 0;
	timerAll.CounterStart = 0;
	timerAll.PCFreq = 0;
	double runTimer = 0.0;
	double t21 = 0.0;
	loopCnt = 0;
	dataAccSend = 0;
	int addressSize = sizeof (address);
	int satusBuff_size = sizeof(int);
	
	//The numbeer of data points to send at a time - designed to send sets of float(x,y,z) 
	cordanateCount = 250000;
	floatCount = cordanateCount*3;
	sizeSend = sizeof(float)*floatCount;
	otherValue = 0;

	locations = (float*)malloc(sizeSend);
	SendBuf_init = (char*)malloc(initDataSize);

	for(UINT l = 0; l < floatCount; l++)
		locations[l] = (rand() % 10000)*0.001f;

	

	//===================  Allocate  ===============================================
	StartCounter_ms(timer);
	SendBuf_data = (char*)malloc(sizeSend);
	t21 = GetCounter_ms(timer);
	wprintf(L"Allocate: %f\n", float(t21));
	//--------------------------------------------------------------------------------------

	initSettings[0] = cordanateCount; initSettings[1] = floatCount; 
	initSettings[2] = sizeSend; initSettings[3] = otherValue; 
    memcpy(SendBuf_init, initSettings, initDataSize);

	std::cout << "cordanateCount: " << initSettings[0] << "  floatCount: " << initSettings[1] << "\nsizeSend: " << initSettings[2] << "  messageCount: " << initSettings[3] << "\n"; 

    initSenderSocket(addressSize);
	changeBuffer((int)sizeSend+28, &ConnectSocket);

    wprintf(L"Sending the init message to the receiver...\n");
    iResult = send(ConnectSocket, SendBuf_init, initDataSize, 0);
    checkError_MaybeExit(iResult, L"sendto");

	waitOnOther(satusBuff_size); //This insures both have been initialized, If initially paused the code will sit here

	//############# End Step 1 init #######################

	//############# Start Step 2 translate and send ############
	float dataS =(float)(sizeSend)/1000000.0f;
	StartCounter_ms(timerAll);
	for(;;){
		loopCnt++;
		wprintf(L"<<< Frame %d >>>\n", loopCnt);

		StartCounter_ms(timer);

		//======================  Translate Data  ====================
		memcpy(SendBuf_data, locations, sizeSend);

		t21 = GetCounter_ms(timer);
		wprintf(L"Copy to messages: %f\n", float(t21));

		wprintf(L"Sending locations data");
		StartCounter_ms(timer);

		iResult = send(ConnectSocket, SendBuf_data, (int)sizeSend, 0);
		checkError_MaybeExit(iResult, L"sendto locations");
		dataAccSend += iResult;
		
		t21 = GetCounter_ms(timer);
		wprintf(L"\nPackets Sent in: %f", (float)t21);
		wprintf(L"\nData Sent: %5.2f MB", dataS);
		wprintf(L"\nBandwidth: %f MB/s", dataS/(t21*0.001));
		wprintf(L"\nUpdate Rate: %f /s\n", 1000.f/(t21));

		if(runTimer + GetCounter_ms(timerAll) >= 1000.0) { 
			running = 0; 
			runTimer += GetCounter_ms(timerAll);
			wprintf(L"\nAll %d Packets Sent in: %d\n", loopCnt, (int)runTimer);
			//wprintf(L"Bytes Sent: %5.2f  Out of %5.2f\n", (dataAccSend/1000000.0f), loopCnt*dataS);
		}

		//>>>>>>>>>>>>>>>>>>>>Send>>>>>>>>>>>>>>>>>>
		indicateReady(satusBuff_size); //Status send

		//<<<<<<<<<<<<<<<<<<<<Recv<<<<<<<<<<<<<<<<<<
		runTimer += GetCounter_ms(timerAll);
		waitOnOther(satusBuff_size); //Real wait
		StartCounter_ms(timerAll);
	}

	//############# End Step 2 translate and send ############

    // When the application is finished sending, close the socket.
    wprintf(L"Finished sending. Closing socket out of loop, a issue.\n");
    iResult = closesocket(ConnectSocket);
	checkError_MaybeExit(iResult, L"closesocket");

    // Clean up and quit.
    cleanUpSockets();

    return 0;
}

int main()
{
	running = 1;
	if(running){
		pthread_t tid1;
		void *returnVal;
		int err = pthread_create(&tid1, NULL, clientThread, NULL);
		if (err != 0)
			wprintf(L"can't create thread error code %d\n", err);
		else
			wprintf(L"Created thread\n");

		err = pthread_join(tid1, &returnVal);
		if (err != 0)
			wprintf(L"Thread exit code: %d, thread returned: %d\n", err, (int)returnVal);
		else
			wprintf(L"Thread exited and returned: %d\n", (int)returnVal);
	}

	wprintf(L"\nPress a key to exit\n");
	_getch();
	exit(0);
}