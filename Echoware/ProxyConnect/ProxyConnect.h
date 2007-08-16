#if !defined(_PROXY_CONNECT_)
#define _PROXY_CONNECT_

#if _MSC_VER > 1000
#pragma once
#endif 

#define LOCAL_PROXY_CONNECT_FAIL		-10
#define LOCAL_PROXY_AUTH_FAIL			-11
#define LOCAL_PROXY_RESP_FAIL			-12
#define LOCAL_PROXY_RECV_FAIL			-13


#define SOCK_ERROR				-1
#define SOCK_OK					0

#define MAX_LINE_SIZE			1024
#define MAX_BUFFER_SIZE			65536

#define SOCK_CONNECT_TIMEOUT	100
#define SOCK_SEND_TIMEOUT		101
#define SOCK_RECEIVE_TIMEOUT	102

#define SOCKS_V5				5
#define SOCKS_V4				4
#define SOCKS_NOAUTH			0
#define SOCKS_NOMETHOD			0xff
#define SOCKS_CONNECT			1


#define HTTP_AUTH_NO			1
#define HTTP_AUTH_BASIC			2
#define HTTP_AUTH_NTLM			3
#define HTTP_AUTH_NTLM_BASIC	4
#define HTTP_AUTH_BASIC_NTLM	5



#define NONE_TYPE					0
//#define SOCKS4_TYPE				1
//#define SOCKS5_TYPE				2
//#define HTTP_TYPE					3


//#define PROXY_TYPE_AUTO_DECTECT			0
#define PROXY_TYPE_SOCKS4					1
#define PROXY_TYPE_SOCKS5					2
#define PROXY_TYPE_HTTP						3


#include "../APISocket/APISocket.h"

class CProxyConnect
{
public:

	CProxyConnect();
	~CProxyConnect();

	int TestSocks4Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password);

	int TestSocks5Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password);	

	int TestHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password);	

	int DetectProxyType(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password);
	
	int ConnectViaHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username, char* password);

	int ConnectViaSocks5Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password);

	int ConnectViaSocks4Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password);

	int ConnectViaProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password);	
	
	int AnalyzeHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password);	

	int m_httpProxyAuthType;
	int m_temphttpProxyAuthType;
	int m_proxyType;
	bool m_bIsAutoDetectDone;

protected:
	static int m_nConnectTimeout_secs;
	static int m_nReceiveTimeout_secs;
	static int m_nSendTimeout_secs;
};


#endif

