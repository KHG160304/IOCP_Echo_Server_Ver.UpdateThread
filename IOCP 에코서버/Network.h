#pragma once
#ifndef __NETWORK_H__
#define	__NETWORK_H__
#include "SerializationBuffer.h"
#define WINAPI	__stdcall

typedef unsigned short		WORD;
typedef unsigned long		DWORD;
typedef	void*				LPVOID;
typedef unsigned long long	SESSIONID;

void SetOnRecvEvent(void (*_OnRecv)(SESSIONID sessionID, SerializationBuffer& packet));
void SendPacket(SESSIONID sessionID, SerializationBuffer& sendPacket);

size_t GetAcceptTotalCnt();
size_t GetCurrentSessionCnt();

void RequestExitNetworkLibThread(void);
bool InitNetworkLib(WORD port, DWORD createIOCPWorkerThreadCnt = 0, DWORD runningIOCPWorkerThreadCnt = 0);

#endif // !__NETWORK_H__