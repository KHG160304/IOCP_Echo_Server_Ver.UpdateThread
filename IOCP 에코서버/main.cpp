#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <synchapi.h>
#include <process.h>
#include "Network.h"
#include "RingBuffer.h"
#include "Log.h"

#define SERVERPORT	6000
#define IOCP_WORKER_THREAD_CREATE_CNT	0
#define IOCP_WORKER_THREAD_RUNNING_CNT	0

int shutdownServer = false;
RingBuffer updateThreadQueue(1048576);
HANDLE hEventExitUpdateThread;
HANDLE hEventWakeUpdateThread;
HANDLE hThreadUpdate;
HANDLE hThreadUpdate2;
SRWLOCK updateThreadQueueLock = SRWLOCK_INIT;

unsigned WINAPI UpdateThread(LPVOID args);

struct EchoMessage
{
	SESSIONID sessionID;
	SerializationBuffer* ptrPayload;
};

void OnRecv(SESSIONID sessionID, SerializationBuffer& packet)
{
	/*SerializationBuffer sendPacket(8);
	_int64 echoBody;
	packet >> echoBody;
	sendPacket << echoBody;
	
		//여기에서 SendPacket 호출하는 것은 
		//해당 sessionID에 대해서 싱글스레드로 동작하기 때문에 문제가 안된다.
		//Update 스레드에서 SendPacket을 호출하는 것은 해당 세션에 대해서 멀티스레드로 동작하기 때문에
		//Thread Safe 하지 않습니다 문제가 발생합니다..
	
	SendPacket(sessionID, sendPacket);*/
	EchoMessage message;
	message.sessionID = sessionID;
	message.ptrPayload = new SerializationBuffer(packet.GetUseSize());
	message.ptrPayload->Enqueue(packet.GetFrontBufferPtr(), packet.GetUseSize());
	AcquireSRWLockExclusive(&updateThreadQueueLock);
	updateThreadQueue.Enqueue((char*)&message, sizeof(EchoMessage));
	ReleaseSRWLockExclusive(&updateThreadQueueLock);
	SetEvent(hEventWakeUpdateThread);
}

int main(void)
{
	int key;

	hEventExitUpdateThread = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEventExitUpdateThread == nullptr)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "Create Event Error %d", GetLastError());
		return 0;
	}
	hEventWakeUpdateThread = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEventWakeUpdateThread == nullptr)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "Create Event Error %d", GetLastError());
		return 0;
	}

	hThreadUpdate = (HANDLE)_beginthreadex(nullptr, 0, UpdateThread, nullptr, false, nullptr);
	if (hThreadUpdate == nullptr)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "Update Thread Create Error code %d", GetLastError());
		CloseHandle(hEventExitUpdateThread);
		CloseHandle(hEventWakeUpdateThread);
		return 0;
	}

	/* Update 스레드 멀티로 해봄
	
	hThreadUpdate2 = (HANDLE)_beginthreadex(nullptr, 0, UpdateThread, nullptr, false, nullptr);
	if (hThreadUpdate == nullptr)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "Update Thread Create Error code %d", GetLastError());
		CloseHandle(hEventExitUpdateThread);
		CloseHandle(hEventWakeUpdateThread);
		return 0;
	}*/

	SetOnRecvEvent(OnRecv);
	if (!InitNetworkLib(SERVERPORT, IOCP_WORKER_THREAD_CREATE_CNT, IOCP_WORKER_THREAD_RUNNING_CNT))
	{
		SetEvent(hEventExitUpdateThread);
		CloseHandle(hEventExitUpdateThread);
		CloseHandle(hEventWakeUpdateThread);
		return 0;
	}

	while (!shutdownServer)
	{
		if (_kbhit())
		{
			key = _getch();
			if (key == 'Q' || key == 'q')
			{
				RequestExitNetworkLibThread();
				shutdownServer = true;
			}
		}
	}

	SetEvent(hEventExitUpdateThread);
	if (WaitForSingleObject(hThreadUpdate, INFINITE) == WAIT_FAILED)
	{
		_Log(dfLOG_LEVEL_SYSTEM, "hThreadUpdate - WaitForSingleObject error code: %d", GetLastError());
	}

	CloseHandle(hThreadUpdate);
	CloseHandle(hEventExitUpdateThread);
	CloseHandle(hEventWakeUpdateThread);

	printf("메인 스레드 종료 완료\n");
	return 0;
}


unsigned WINAPI UpdateThread(LPVOID args)
{
	int useSize;
	EchoMessage message;
	DWORD result;
	HANDLE arrEvent[2] = { hEventWakeUpdateThread, hEventExitUpdateThread };
	_Log(dfLOG_LEVEL_SYSTEM, "Start UpdateThread");
	for (;;)
	{
		/* Update 스레드를 멀티스레드로 돌려보기 위한 코드조각
		result = WaitForMultipleObjects(2, arrEvent, FALSE, INFINITE);
		if (result == WAIT_FAILED)
		{
			_Log(dfLOG_LEVEL_SYSTEM, "Event WAIT_FAILED error code: %d", GetLastError());
			_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
			return -1;
		}
		else if (result == (WAIT_OBJECT_0 + 1))
		{
			_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
			return 0;
		}

		while (updateThreadQueue.GetUseSize())
		{
			AcquireSRWLockExclusive(&updateThreadQueueLock);
			if (!updateThreadQueue.Dequeue((char*)&message, sizeof(message)))
			{
				ReleaseSRWLockExclusive(&updateThreadQueueLock);
				break;
			}
			SendPacket(message.sessionID, *(message.ptrPayload));
			ReleaseSRWLockExclusive(&updateThreadQueueLock);
			delete message.ptrPayload;
		}*/

		/*
			아래는 Update 스레드가 싱글스레드에서 작동하는 코드이다.
		*/
		if (!(useSize = updateThreadQueue.GetUseSize()))
		{
			result = WaitForMultipleObjects(2, arrEvent, FALSE, INFINITE);
			if (result == WAIT_FAILED)
			{
				_Log(dfLOG_LEVEL_SYSTEM, "Event WAIT_FAILED error code: %d", GetLastError());
				_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
				return -1;
			}
			else if (result == (WAIT_OBJECT_0 + 1))
			{
				_Log(dfLOG_LEVEL_SYSTEM, "Exit UpdateThread");
				return 0;
			}
			useSize = updateThreadQueue.GetUseSize();
		}

		while (useSize)
		{
			updateThreadQueue.Dequeue((char*)&message, sizeof(message));
			SendPacket(message.sessionID, *(message.ptrPayload));
			delete message.ptrPayload;
			useSize -= sizeof(EchoMessage);
		}
	}
}
