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
	credit is given for the original creation. https://creativecommons.org/licenses/by/4.0/
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

const char * IP_Address = "127.0.0.1"; //IP is the computer running this code
unsigned short Port_ComputeToRender = 27015;
timer_ms timer;

char * SendBuf_init = NULL;
float * locations = NULL;
char * RecvBuf_data = NULL;
WSADATA RecvWsaData;

WSADATA SendWsaData;
SOCKET connectSocket = INVALID_SOCKET;
SOCKET listenSocket = INVALID_SOCKET;

sockaddr_in address;
char statusBuffer[sizeof(int)]; 
int running;
int loopCnt; 
int dataAccRecv;
UINT64 cordanateCount;
UINT64 floatCount;
UINT64 sizeSend;
UINT64 otherValue; // Not used

bool PAUSE_ON;

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
	//ExitProcess(dw); 
}
void cleanUpSockets(){
	wprintf(L"\nExiting.\n");
	if(closesocket(connectSocket) != 0) { /* Error or already closed */ }
	if(closesocket(listenSocket) != 0) { /* Error or already closed */ }
	if(locations != NULL)
		free(locations);
	if(SendBuf_init != NULL)
		free(SendBuf_init);
	if(RecvBuf_data != NULL)
		free(RecvBuf_data);
	WSACleanup();

	float dataS =(float)(loopCnt*sizeSend)/1000000.0f;
	printf("Bytes Recv: %5.2f  Out of %5.2f\n", (dataAccRecv/1000000.0f), dataS);

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
void checkInitSettings(){
	if ((initSettings[0]*3 != initSettings[1]) || (initSettings[1]*sizeof(float) != initSettings[2])) {
		wprintf(L"Corrupted Parameters \n");
		cleanUpSockets();
		pthread_exit((void*)EXIT_FAILURE);
	}
}
void initServer(){
	int iResult;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &RecvWsaData);
	checkError_MaybeExit(iResult, L"WSAStartup");
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++

	// Create a receiver socket to receive datagrams
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sock_checkErrorMaybeExit(&listenSocket);

	// Bind the socket to local address to listen
	address.sin_family = AF_INET;
	address.sin_port = htons(Port_ComputeToRender);
	address.sin_addr.s_addr = inet_addr(IP_Address);

	iResult = bind(listenSocket, (SOCKADDR *) &address, sizeof (address));
	checkError_MaybeExit(iResult, L"bind");

	iResult = listen(listenSocket, SOMAXCONN);
	checkError_MaybeExit(iResult, L"listen");
	//OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO

	// Create a socket for sending data
	connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sock_checkErrorMaybeExit(&connectSocket);
}
void changeBuffer(int siz, SOCKET * sock){
	int iResult = 0;
	int optVal;
	int optLen = sizeof(int);
	int optValNew = siz;
	int optLenNew = sizeof(int);

	//Receive Buffer: Checking -> changing -> confirming change by recheacking
	if (getsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld", optVal);

	iResult = setsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char *) &optValNew, optLenNew);
	if (iResult == SOCKET_ERROR) {
		wprintf(L"\tsetsockopt for failed with error: %u\n", WSAGetLastError());
	} else
		wprintf(L"Checking SO_RCVBUF to %d\n", optValNew);

	if (getsockopt(*sock, SOL_SOCKET, SO_RCVBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Changed SockOpt Value: %ld\n", optVal);

	//Send Buffer: Checking -> changing -> confirming change by recheacking
	if (getsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld", optVal);
	
	iResult = setsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char *) &optValNew, optLenNew);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"\tsetsockopt for failed with error: %u\n", WSAGetLastError());
    } else
        wprintf(L"\tChanged SO_SNDBUF to %d\n", optValNew);

	if (getsockopt(*sock, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optLen) != SOCKET_ERROR)
		printf("Checking SockOpt Value: %ld\n", optVal);
}
void indicateReady(int msgSize){
	//wprintf(L"Indicating ready\t");
	memset(statusBuffer, 0, sizeof(statusBuffer));
	memcpy(statusBuffer, &running, msgSize);

	int iResult = send(connectSocket, statusBuffer, msgSize, 0);
	checkError_MaybeExit(iResult, L"indicateReady");
	//wprintf(L"Status sent\n");

	if(!running){
		Sleep(5); //Give time for the client to recv
		cleanUpSockets();
		pthread_exit((void*)0);
	}
}
void waitOnOther(int msgSize){
	//wprintf(L"Wating on other\t");
	memset(statusBuffer, 0, sizeof(statusBuffer));
	int iResult = recv(connectSocket, statusBuffer, msgSize, 0);
	checkError_MaybeExit(iResult, L"waitOnOther");

	//This prevents the exit code 0 from being over written, but accepts a 0 from client still if sever is 1
	if(running){
		memcpy(&running, statusBuffer, msgSize);
		//printf("Other Connection is ready Continuing\n");
		if(!running){
			cleanUpSockets();
			pthread_exit((void*)0);
		}
	}
}
void * serverThread(void * arg)
{
	//############# Start Step 1 init ######################
	double t21 = 0.0;
	int iResult = 0;
	running = 1;
	loopCnt = 0;
	dataAccRecv = 0;
	int satusBuff_size = sizeof(int);
	int addressSize = sizeof (address);
	SendBuf_init = (char*)malloc(initDataSize);

	initServer();

	std::cout << "Wating On connection\n";
	if(connectSocket = accept(listenSocket, (SOCKADDR*)&address, &addressSize)){

		// Call the recvfrom function to receive datagrams
		// on the bound socket.
		wprintf(L"Receiving init message...\n");
		iResult = recv(connectSocket, SendBuf_init, initDataSize, 0);
		checkError_MaybeExit(iResult, L"recv init");

		memcpy(initSettings, SendBuf_init, initDataSize);
		std::cout << "cordanateCount: " << initSettings[0] << "  floatCount: " << initSettings[1] << "\nsizeSend: " << initSettings[2] << "  messageCount: " << initSettings[3] << "\n"; 

		checkInitSettings();

		//The numbeer of data points to receive at a time - designed to send sets of float(x,y,z) 
		cordanateCount = initSettings[0]; 
		floatCount = initSettings[1];
		sizeSend = initSettings[2];
		otherValue = initSettings[3];

		//====================  Allocate  =================================
		StartCounter_ms(timer);
		RecvBuf_data = (char*)malloc(sizeSend);
		locations = (float*)malloc(sizeSend);
		t21 = GetCounter_ms(timer);
		wprintf(L"Allocate: %f\n", float(t21));

		//############# End Step 1 init ########################

		//############# Start Step 2 receive and translate ############

		changeBuffer((int)sizeSend+28, &connectSocket);

		for(;;){

			//>>>>>>>>>>>>>>>>>>>>Send>>>>>>>>>>>>>>>>>>
			while(PAUSE_ON) { Sleep(2); }
			indicateReady(satusBuff_size);//Real wait

			loopCnt++;

			wprintf(L"<<< Frame %d >>>\n", loopCnt);
			//wprintf(L"Receiving locations datagram");
			StartCounter_ms(timer);

			iResult = recv(connectSocket, RecvBuf_data, (int)sizeSend, 0);
			checkError_MaybeExit(iResult, L"recvfrom locations");
			dataAccRecv += iResult;

			t21 = GetCounter_ms(timer);
			//wprintf(L"\nPackets Received in: %f\n", (float)t21);

			//======================  Translate Data  ====================
			memcpy(locations, RecvBuf_data, sizeSend);

			t21 = GetCounter_ms(timer);
			//wprintf(L"Copy to location array: %f\n", float(t21));

			//<<<<<<<<<<<<<<<<<<<<Recv<<<<<<<<<<<<<<<<<<
			waitOnOther(satusBuff_size); // Status recive
		}
		
		//############# End Step 2 receive and translate ############

		// Close the socket when finished receiving datagrams
		wprintf(L"Finished receiving. Closing socket out of loop, a issue.\n");
		iResult = closesocket(connectSocket);
		checkError_MaybeExit(iResult, L"closesocket");

	}
	
	// Clean up and exit.
	cleanUpSockets();

	return 0;
}

int main()
{
	PAUSE_ON = true;

	pthread_t tid1;
	void *returnVal;
	int err = pthread_create(&tid1, NULL, serverThread, NULL);
	if (err != 0)
		wprintf(L"can't create thread error code %d\n", err);
	else
		wprintf(L"Created thread\n");

	PAUSE_ON = false;

	err = pthread_join(tid1, &returnVal);
	if (err != 0)
		wprintf(L"Thread exit code: %d, thread returned: %d\n", err, (int)returnVal);
	else
		wprintf(L"Thread exited and returned: %d\n", (int)returnVal);

	wprintf(L"\nPress a key to exit\n");
	_getch();
	exit(0);
}