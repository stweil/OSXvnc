#ifndef _PROXYINFO_H
#define _PROXYINFO_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include <string>
#include "InterfaceDllProxyInfo.h"

//#define ID_STRING_SIZE		255

//IDllProxyInfo implementation 

class CDllProxyInfo : public IDllProxyInfo
{
protected:    
	std::string m_strName;
	std::string m_strIP;
	std::string m_strPort;
	std::string  m_strPass;
	std::string m_strMyID;	
	DWORD m_dwStatus;
	
	int m_nConnectTimeOut, m_nReceiveTimeOut, m_nSendTimeOut;	

	std::string m_strIPPort;
	bool m_bReconnectProxy;

public:
	CDllProxyInfo();
	virtual ~CDllProxyInfo();

	virtual void SetName(char* name);
	virtual void SetIPPort(char* ipport);
	virtual void SetIP(const char* ip);
	virtual void SetPort(const char* port);
	virtual void SetPassword(const char* pass);
	virtual void SetStatus(int Status, bool bStatus);
	virtual bool SetMyID(const char* MyID);
	virtual bool SetSocketTimeout(int connectTimeout, int ReceiveTimeout, int SendTimeout);
	virtual void SetReconnectProxy(bool bReconnectProxy);

	virtual const char* GetName() const;
	virtual const char* GetIpPort() const;
	virtual const char* GetIP() const;
	virtual const char* GetPort() const;
	virtual const char* GetPassword() const;
	virtual int	  GetStatus() const;
	virtual	const char* GetMyID() const;
	virtual bool GetReconnectProxy()const;

	virtual int GetConnectTimeout() const;
	virtual int GetReceiveTimeout() const;
	virtual int GetSendTimeout() const;	
};
#endif 
