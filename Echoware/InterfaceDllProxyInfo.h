#ifndef _INTERFACE_DLL_PROXY_INFO_H
#define _INTERFACE_DLL_PROXY_INFO_H

#if _MSC_VER > 1000
#pragma once
#endif 

//enum
//{
//	STATUS_MSG_BASE = WM_USER + 3000,

#define STATUS_CONNECTING					0x00000001
#define STATUS_CONNECTED					0x00000002
#define STATUS_AUTHENTICATING				0x00000004
#define STATUS_AUTHENTICATION_FAILED		0x00000008
#define STATUS_ESTABLISHING_DATA_CHANNEL	0x00000010
#define STATUS_SEARCHING_FOR_PARTNER		0x00000020
#define STATUS_DISCONNECTED_FROM_PROXY		0x00000040

//	STATUS_MSG_LAST	
//};

enum
{
	ERROR_CONNECTING_TO_PROXY = -1,
	CONNECTION_SUCCESSFUL,
	NO_PROXY_SERVER_FOUND_TO_CONNECT,
	AUTHENTICATION_FAILED,
	PROXY_ALREADY_CONNECTED,
	CONNECTION_TIMED_OUT,
	ID_FOUND_EMPTY
};

class IDllProxyInfo
{
public:	
	virtual void SetName(char* name) = 0;
	virtual void SetIPPort(char* ipport) = 0;
	virtual void SetIP(const char* ip) = 0;
	virtual void SetPort(const char* port) = 0;
	virtual void SetPassword(const char* pass) = 0;
	virtual void SetStatus(int Status, bool bStatus) = 0;
	virtual bool SetMyID(const char* MyID) = 0;
	virtual bool SetSocketTimeout(int connectTimeout, int ReceiveTimeout, int SendTimeout) =0;
	virtual void SetReconnectProxy(bool bReconnectProxy) = 0;

	virtual const char* GetName() const= 0;
	virtual const char* GetIpPort() const= 0;
	virtual const char* GetIP() const= 0;
	virtual const char* GetPort() const= 0;
	virtual const char* GetPassword() const= 0;
	virtual int	  GetStatus() const= 0;
	virtual	const char* GetMyID() const= 0;
	virtual bool GetReconnectProxy()const = 0;
};

#endif