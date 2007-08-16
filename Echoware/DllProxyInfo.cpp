#include "stdafx.h"
#include "DllProxyInfo.h"
#include "globals.h"

CDllProxyInfo::CDllProxyInfo()
:	m_strName("")
	,m_strIP("")
	,m_strPort("")
	,m_strPass("")
	,m_strMyID("")	
	,m_dwStatus(0)	
	,m_strIPPort("")
	,m_bReconnectProxy(false)
{	
	m_nConnectTimeOut=m_nReceiveTimeOut=m_nSendTimeOut=30;
}

CDllProxyInfo::~CDllProxyInfo()
{
}

//Sets the Proxy Name for the proxy Info object
void CDllProxyInfo::SetName(char* name)
{
	m_strName=name;
}

//Sets the IP Address of the proxy along with a specified Port
void CDllProxyInfo::SetIPPort(char* ipport)
{
	char* p;
	if ((p=strchr(ipport, ':')))
	{
		m_strIP.assign(ipport, p-ipport);
		m_strPort.assign(p);
	}
	else
		m_strIP=ipport;

	m_strIPPort=ipport;
}

//Sets the IP Address of the proxy info Object
void CDllProxyInfo::SetIP(const char* ip)
{
	m_strIP=ip;

	m_strIPPort=m_strIP;

	if (!m_strPort.empty())
		m_strIPPort+=":"+m_strPort;
}

//Sets the port of the proxy info object
void CDllProxyInfo::SetPort(const char* port)
{
	m_strPort=port;

	m_strIPPort=m_strIP+":"+m_strPort;
}

//Sets the authentication channel password for the Proxy
void CDllProxyInfo::SetPassword(const char* pass)
{
	m_strPass=pass;
}

//Sets the status of the proxy whether connected or not
void CDllProxyInfo::SetStatus(int Status, bool bStatus)
{
	if (bStatus)
		m_dwStatus |=Status;
	else
		m_dwStatus &=~Status;
}

//Set ID for authentication and/or identification with a proxy server
//Each proxyInfo object may have a different MyID value.
bool CDllProxyInfo::SetMyID(const char* MyID)
{
	m_strMyID=MyID;
	return true;
}

//Set timeouts for Connect, Receive, Send
bool CDllProxyInfo::SetSocketTimeout(int connectTimeout, int ReceiveTimeout, int SendTimeout)
{
	m_nConnectTimeOut=connectTimeout;
	m_nReceiveTimeOut=ReceiveTimeout;
	m_nSendTimeOut=SendTimeout;

	g_globals.m_logger.WriteFormated("CDllProxyInfo: connectTimeout=%d ReceiveTimeout=%d SendTimeout=%d", m_nConnectTimeOut, m_nReceiveTimeOut, m_nSendTimeOut);
	return true;
}

//Retrieves the name of the Proxy
const char* CDllProxyInfo::GetName() const
{
	return m_strName.c_str();
}

//Retrieves the IP and Port of the Proxy info together
const char* CDllProxyInfo::GetIpPort() const
{
	if (m_strPort.empty())
		return m_strIP.c_str();

	return m_strIPPort.c_str();
}

//Retrieves the IP address for the Proxy Info
const char* CDllProxyInfo::GetIP() const
{
	return m_strIP.c_str();
}

//Retrieves the Port for the Proxy
const char* CDllProxyInfo::GetPort() const
{
	return m_strPort.c_str();
}

//Retrieves the authentication channel password of the Proxy
const char* CDllProxyInfo::GetPassword() const
{
	return m_strPass.c_str();
}

//Retrieves the status of the Proxy, connected or not, the status of any "in-process" 
//connection attempts, and all active data connections with those proxies.
int CDllProxyInfo::GetStatus() const
{
	return m_dwStatus;
}

//Obtain the MyID value associated with a proxyInfo object
const char* CDllProxyInfo::GetMyID() const
{
	return m_strMyID.c_str();
}

int CDllProxyInfo::GetConnectTimeout() const
{
	return m_nConnectTimeOut;
}

int CDllProxyInfo::GetReceiveTimeout() const
{
	return m_nReceiveTimeOut;
}

int CDllProxyInfo::GetSendTimeout() const
{
	return m_nSendTimeOut;
}

bool CDllProxyInfo::GetReconnectProxy() const
{
	return m_bReconnectProxy;
}

void CDllProxyInfo::SetReconnectProxy(bool bReconnectProxy)
{
	m_bReconnectProxy=bReconnectProxy;
}