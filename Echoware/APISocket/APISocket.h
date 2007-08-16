#ifndef __APISOCKET__
#define __APISOCKET__

#if _MSC_VER > 1000
#pragma once
#endif

#undef FD_SETSIZE
#define FD_SETSIZE 1024

#define __stdcall 
#define MAX_BUFFER	(64*1024)

#import <CoreFoundation/CoreFoundation.h>

#include "CritSection.h"

namespace APISocket
{

//generic socket class
class CSocket
{
public:

	CSocket(int sock=0);	

	CSocket(const CSocket& other);

	virtual ~CSocket();

	//[in]Timeouts in milliseconds
	void SetTimeouts(int nConnectTimeout=-1, int nSendTimeout=-1, int nRecvTimeout=-1, int nAcceptTimeOut=-1);

	//[in]bBlock : true for bloking socket, false for non-blocking socket
	void SetBlockMode(bool bBlock);

	int GetConnectTimeout();
	int GetSendTimeout();
	int GetRecvTimeout();
	int GetAcceptTimeout();
	bool GetBlockMode();

	//[in] nPort : port to bind
	//return : true or false
	virtual bool Create(unsigned int nPort=0);

	//return : 0 fail, port bindit for ok
	unsigned int Bind(unsigned int nPort=0, bool bLocal=true);

	//[in] backlog
	//return : true or false
	bool Listen(int backlog=10);

	//return : -1 for error, 0 for success, 1 for timeout
	int Accept(int& sock);

	//[in] szServerIP
	//[in] nPort
	//connect socket to szServer (ip xxx.xxx.xxx.xxx or host name) on port nPort
	//return -1 for error, 0 for OK, 1 for timeout
	int Connect(const char* szServer, unsigned int nPort);

	//shutdown socket
	//[in] how : SD_RECEIVE, SD_SEND, SD_BOTH
	//return : true or false
	bool Shutdown(int how);

	//send
	//[in] buff : buff to send
	//[in] nLen : buff length
	//return -1 for error, 0 for OK, 1 for timeout
	int Send(const char* buff, int& nLen, int nFlag=0);

	//receive
	//[in] buff
	//[in, out] nLength : size of buff
	//[in] flag : 0, MSG_PEEK  or MSG_OOB
	//return -1 for error, 0 for OK, 1 for timeout
	int Receive(char* buff, int& nLength, int nFlag=0);

	bool Close();

	virtual void OnClose();
	virtual void OnError(int error);
	virtual void OnReceive(char* buff, int len);
	virtual void OnSend(char* buff, int &len);

	//gets the TCP_NODELAY socket option
	//return true for success and false for error to get the TCP_NODELAY socket option
	bool GetSockOption(bool& bTcpNodelay);

	//sets the TCP_NODELAY socket option
	//return true for success and false for error to set the TCP_NODELAY socket option
	bool SetSockOption(bool bTcpNodelay);

	//gets the socket last error
	int GetLastError();

	bool GetLocalPort(unsigned int& nPort);
	bool GetRemotePort(unsigned int& nPort);
	bool GetRemoteIP(char* szRemoteIP);

	//unsigned int GetSock(){return m_sock;};

	//atach a sock to this class
	int Attach(int sock);

	//detach the sock from this class
	int Detach();

	void OnWrite();
	void OnRead();

	int Send();
	int ReadableByteCount();

	int getSocket();
	
	bool StartAsync();
	bool StopAsync();

protected:
	//Set the socket to either blocking or non-blocking mode
	//[in] bNonBlock : true for non-blocking mode, false for blocking mode
	//return true or false
	bool NonBlock(bool bNonBlock);

	//[in] nTimeout in milliseconds
	//[in] nType 0 for read, 1 for write
	//return -1 for error, 0 for OK, 1 for timeout
	int WaitNonBlockCompletion(int nTimeout, int nType);

protected:
	int m_sock;

	int m_nConnectTimeout, m_nSendTimeout, m_nRecvTimeout, m_nAcceptTimeout;

	bool m_bBlockMode;

	void* m_hAsyncThread;
	unsigned long m_dwAsyncThread;
	bool shouldAsyncQuit;
	bool hasAsyncQuit;

	bool inAsyncNotificationLoop;
	bool errored;

	CCritSection m_csAsync;
	static unsigned long AsyncThreadProc(void* lpParameter);
	void AsyncNotifications();
	void AsyncNotificationLoop();
};

//a base class for client socket
class CClientSocket : public CSocket
{
public:
	CClientSocket(int sock=0);
	virtual ~CClientSocket(void);

	//start send data
	virtual bool StartSend();

	//stop send data
	//[in] dwTimeout : milliseconds for waitting to stop
	bool StopSend(unsigned long dwWaitTimeout);
	
	virtual void OnTimer();

protected:
	
	bool StartRoutine();
	
	bool StopRoutine(unsigned long dwWaitTimeout);

	//routine thread handle
	void* m_hRoutineThread;
	
	//routine thread id
	unsigned long m_dwRoutineThread;
	
	//some routine thread proc
	static unsigned long __stdcall RoutineThreadProc(void* lpParameter);

	bool routineShouldQuit;
	bool routineHasQuit;
};

//a base class for server socket
class CServerSocket : public CSocket
{
public:
	CServerSocket(unsigned int nPort=0);
	virtual ~CServerSocket(void);
	
	//return : true or false
	virtual bool Create();

	//[in] backlog
	//return : true or false
	bool Listen(int backlog=10);

	//start waitting for clients
	bool StartAccept();

	//stop waitting for clients
	//[in] dwTimeout : milliseconds for waitting to stop
	bool StopAccept(unsigned long dwWaitTimeout);

	//notification : new client connected
	virtual void OnNewClient(int sock);

	virtual void OnLeaveClient(CSocket* pClient);

	unsigned int GetListenPort();
protected:	
	//listen port
	unsigned int m_nPort;

	//accept thread handle
	void* m_hAcceptThread;

	//accept thread id
	unsigned long m_dwAcceptThread;

	//accept thread proc
	static unsigned long __stdcall AcceptThreadProc(void* lpParameter);
	
	bool shouldQuit;
	bool hasQuit;
};
}
#endif