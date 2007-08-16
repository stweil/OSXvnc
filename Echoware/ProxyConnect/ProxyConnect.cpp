#include "stdafx.h"
#include <string>
#include "proxyconnect.h"

#include "ntlm.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "EchoToOSX.h"

int CProxyConnect::m_nConnectTimeout_secs = 5;
int CProxyConnect::m_nReceiveTimeout_secs = 5;
int CProxyConnect::m_nSendTimeout_secs =    5;


/*int TCPConnectWithTimeout(int sock, const struct sockaddr FAR *name, int namelen, int timeout_secs);
int SockReadyToRead(int socket, int timeout_secs);
int SockReadyToWrite(int socket, int timeout_secs);
int read_byte(int fd);
*/

extern void base64(unsigned char *out, const unsigned char *in, int len);

int read_byte(APISocket::CSocket sock)
{
	unsigned char c;
	int nLen=1;
	if(sock.Receive((char*)&c, nLen) == 0 && nLen==1)
	{		
		return c;
	}
	return -1;
}

#if 0
char * base64Encode(char *str)
{
    static char *encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			    "abcdefghijklmnopqrstuvwxyz"
			    "0123456789+/";
    char *s = NULL;
    char *outBuf = NULL;
    int len = -1;
    int i = 0;
    unsigned long bits;

    if (str == NULL)
    {
		return NULL;
    }

    len = strlen(str);

    /*
    ** Base64 encoding expands 6 bits to 1 char, padded with '=', if
    ** necessary.
    */

	int totalLen = 4 * ((len + 2) / 3) + 1;
    if ((outBuf = (char*)malloc(totalLen)) == NULL)
    {
		return NULL;
    }

	memset(outBuf, 0, totalLen);
    s = outBuf;
    for (i = 0; i <= len - 3; i += 3)
    {
		bits = ((unsigned long)(str[i])) << 24;
		bits |= ((unsigned long)(str[i + 1])) << 16;
		bits |= ((unsigned long)(str[i + 2])) << 8;

		*s++ = encoding[bits >> 26];
		bits <<= 6;
		*s++ = encoding[bits >> 26];
		bits <<= 6;
		*s++ = encoding[bits >> 26];
		bits <<= 6;
		*s++ = encoding[bits >> 26];
    }

    switch (len % 3)
    {
		case 0:
			bits = ((unsigned long)(str[i])) << 24;
			bits |= ((unsigned long)(str[i + 1])) << 16;
			bits |= ((unsigned long)(str[i + 2])) << 8;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			break;

		case 2:
			bits = ((unsigned long)(str[len - 2])) << 24;
			bits |= ((unsigned long)(str[len - 1])) << 16;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			*s++ = '=';
			break;

		case 1:
			bits = ((unsigned long)(str[len - 2])) << 24;
			*s++ = encoding[bits >> 26];
			bits <<= 6;
			*s++ = encoding[bits >> 26];
			*s++ = '=';
			*s++ = '=';
			break;
    }

    *s = '\0';
    return outBuf;
}

#endif

/*
int SockReadyToRead(int socket, int timeout_secs)
{
	struct timeval	timeout;
	fd_set			readfds;
	timeout.tv_sec  = timeout_secs;
    timeout.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(socket, &readfds);
	select(1, &readfds, NULL, NULL, &timeout);
	return FD_ISSET(socket, &readfds);
}


int SockReadyToWrite(int socket, int timeout_secs)
{
	struct timeval	timeout;
	fd_set			writefds;
	timeout.tv_sec  = timeout_secs;
    timeout.tv_usec = 0;

	FD_ZERO(&writefds);
	FD_SET(socket, &writefds);
	select(1, NULL, &writefds, NULL, &timeout);
	return FD_ISSET(socket, &writefds);
}

int TCPConnectWithTimeout(int sock, const struct sockaddr FAR *name, int namelen, int timeout_secs)
{
	int retval = SOCK_OK;
    unsigned long blockArg;
	
    blockArg = 1;
    if (ioctlsocket(sock, FIONBIO, &blockArg)==0)
	{
		connect(sock, name, namelen);
		if (!SockReadyToWrite(sock, timeout_secs)) 
		{		
			WSASetLastError(WSAETIMEDOUT);
			retval = SOCK_CONNECT_TIMEOUT;
		} 
		else 
		{
			blockArg = 0;
			ioctlsocket(sock, FIONBIO, &blockArg);
			retval = SOCK_OK;
		}
	}
	else 
	{
		retval = SOCK_ERROR;
	}
	return retval;
}

*/


int CProxyConnect::ConnectViaSocks5Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password)
{	

	int ret=sock.Connect(proxyIp, proxyPort);

	if (ret!=0)
		return LOCAL_PROXY_CONNECT_FAIL;
	

	int i;
	char buf[512];
	unsigned char len;
	unsigned char atyp;

	buf[0] = 0x05;
	buf[1] = 0x01;
	if(username && password)
		buf[2] = 0x02;
	else
		buf[2] = 0x00;

	int nLen=3;
	sock.Send(buf, nLen);	

	if(read_byte(sock) != 0x05 || read_byte(sock) != buf[2]) 
	{		
		sock.Close();
		return LOCAL_PROXY_RESP_FAIL;
	}	

	if(username && password) 
	{
		unsigned char tmplen;

		memset(buf, 0, 512);

		buf[0] = 0x01;
		len = (unsigned char)((strlen(username) > 255) ? 255 : strlen(username));
		buf[1] = len;
		memcpy(buf + 2, username, len);

		tmplen = (unsigned char)((strlen(password) > 255) ? 255 : strlen(password));
		buf[2 + len] = tmplen;
		memcpy(buf + 3 + len, password, tmplen);

		int nLen=(3 + len + tmplen);
		sock.Send( buf, nLen);		

		if(read_byte(sock) != 0x01 || read_byte(sock) != 0x00) 
		{			
			sock.Close();
			return LOCAL_PROXY_AUTH_FAIL;
		}
	}

	buf[0] = SOCKS_V5;
	buf[1] = SOCKS_CONNECT;
	buf[2] = 0x00;
	buf[3] = 0x01; // ipv4 format

	struct sockaddr_in tempAddr;
	tempAddr.sin_addr.s_addr = inet_addr(destIp);

	char ipstr[4];
	memcpy(ipstr, &tempAddr.sin_addr.s_addr, 4);

	memcpy(buf+4, ipstr, 4);

	unsigned short port = htons(destPort);
	memcpy(buf+8, (char*)&port, 2);

	nLen=10;
	sock.Send(buf, nLen);	

	if(read_byte(sock) != 0x05 || read_byte(sock) != 0x00) 
	{		
		sock.Close();
		return LOCAL_PROXY_RESP_FAIL;
	}

	read_byte(sock);
	atyp = read_byte(sock);
	if(atyp == 0x01) 
	{
		for(i = 0; i < 4; i++)
			read_byte(sock);
	} 
	else if(atyp == 0x03) 
	{
		len = read_byte(sock);
		for(i = 0; i < len; i++)
			read_byte(sock);
	} 
	else 
	{		
		sock.Close();
		return LOCAL_PROXY_RESP_FAIL;
	}

	for(i = 0; i < 2; i++)
		read_byte(sock);
	
	return 1;
}



int CProxyConnect::ConnectViaSocks4Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password)
{
	if (sock.Connect(proxyIp, proxyPort)!=0)
		return LOCAL_PROXY_CONNECT_FAIL;

	int len=9;

	char buf[512];

	memset(buf, 0, 512);

	buf[0] = SOCKS_V4;
	buf[1] = SOCKS_CONNECT;
	
	unsigned short port = htons(destPort);
	memcpy(buf+2, (char*)&port, 2);

	struct sockaddr_in tempAddr;
	tempAddr.sin_addr.s_addr = inet_addr(destIp);
	char ipstr[4];
	memcpy(ipstr, &tempAddr.sin_addr.s_addr, 4);
	memcpy(buf+4, ipstr, 4);

	if (username)
	{
    	memcpy(buf+8,username,strlen(username));
		len = 9 + (int)strlen(username);
	}
	else
	{
		buf[8] = 0;	/* empty username */
		len = 9;
	}

	int n=len;
	sock.Send(buf, n);	

	unsigned char vn_byte = read_byte(sock);

	if (vn_byte != 0x00) 
	{		
		sock.Close();
		return LOCAL_PROXY_RESP_FAIL;
	}

	unsigned char cd_byte = read_byte(sock);
	if(cd_byte != 90) 
	{		
		sock.Close();
		return LOCAL_PROXY_AUTH_FAIL;
	}
	
	return 1;
}


#define MAXLEN 512

int CProxyConnect::ConnectViaHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username, char* password)
{	
	//char logstr[512];
	char buf[MAX_LINE_SIZE];
	char proxyAuth[MAX_LINE_SIZE];
	char*bufPtr=NULL;
	int numRead, numWrite;//, retval;
	std::string ans;
	char authstring[MAX_LINE_SIZE];
	char* pAuthstring=NULL;

	memset(buf, 0 , sizeof(buf));	

	if (sock.Connect(proxyIp, proxyPort)!=0)
	{
		return LOCAL_PROXY_CONNECT_FAIL;
	}

	if (m_temphttpProxyAuthType==HTTP_AUTH_NTLM)
	{
		unsigned char host[MAX_NAME_LEN];
		unsigned char domain[MAX_NAME_LEN];
		unsigned char user[MAX_NAME_LEN];
		char tempusername[MAXLEN];

		// 
		// Username = "domain\user" in this case
		// 
		strcpy((char*)tempusername, (char*)username);

		if (strstr(tempusername, "\\") !=NULL)
		{
			char * pch;
			pch = strtok (tempusername,"\\");
			strcpy((char*)domain, pch);

			pch  = strtok(0, "\\");
			strcpy((char*)user, pch);
		}
		else
		{
			strcpy((char*)domain, "");
			strcpy((char*)user, tempusername);
		}

//		char pcName[HOST_NAME_MAX+1];
//		DWORD	nSize = 256;
//		GetComputerName(pcName, &nSize);
//		strcpy((char*)host, pcName);
		gethostname((char *)host, MAX_NAME_LEN);

		int retnum= DoNTLMv2
					(
						sock.getSocket(), 
						destIp, 
						destPort,
						(unsigned char*)host,
						(unsigned char*)domain,
						(unsigned char*)user,
						(unsigned char*)password
					);
		if (retnum <0)	
		{			
			sock.Close();
			return retnum;
		}
		else
		{
			return retnum;
		}
	}

	if (m_temphttpProxyAuthType==HTTP_AUTH_BASIC)
	{
		//
		// Construct a CONNECT string
		//
		if (username && password)
		{
			sprintf(proxyAuth, "%s:%s", username, password);

			memset(authstring, 0, MAX_LINE_SIZE);
			
			base64((unsigned char*)authstring, (unsigned char*)proxyAuth, strlen(proxyAuth));		
			sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nProxy-Authorization: Basic %s\r\nUser-Agent: echoWare\r\n\r\n", destIp, destPort, authstring);
		}
		else
		{
			sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nUser-Agent: echoWare\r\n\r\n", destIp, destPort);
		}

		//
		// Send to the Proxy
		//
		int len=strlen(buf);

		numWrite = len;
		
		if (sock.Send(buf, numWrite)!=0)
		{			
			goto CloseError;
		}
		
		//
		// Receive response from Proxy
		//
		memset(buf, 0 , sizeof(buf));
		bufPtr = buf;
		
		numRead=MAX_LINE_SIZE;
		if (sock.Receive(bufPtr, numRead)!=0 || numRead==0)
		{			
			sock.Close();
			return LOCAL_PROXY_RESP_FAIL;
		}
		
		//
		// Check for an OK response
		//
		if (strncmp(buf, "HTTP/1.0 200", 12)==0 || strncmp(buf, "HTTP/1.1 200", 12)==0)
		{
			return 1;
		}
		else
		{			
			sock.Close();
			return LOCAL_PROXY_AUTH_FAIL;
		}
	}

	CloseError:
		sock.Close();
	return -1;
}



//
// Return -1=error, 0=non-proxy, 1=sock4, 2=sock5, 3=http/https
//
int CProxyConnect::TestSocks4Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password)
{		
	sock.SetTimeouts(m_nConnectTimeout_secs*1000, m_nSendTimeout_secs*1000, m_nReceiveTimeout_secs*1000, sock.GetAcceptTimeout());
	if (sock.Connect(proxyIp, proxyPort)!=0)
		return LOCAL_PROXY_CONNECT_FAIL;

	

	char buf[512];
	buf[0] = SOCKS_V4;
	buf[1] = SOCKS_CONNECT;
	
	unsigned short port = htons(destPort);
	memcpy(buf+2, (char*)&port, 2);

	struct sockaddr_in tempAddr;
	tempAddr.sin_addr.s_addr = inet_addr(destIp);
	char ipstr[4];
	memcpy(ipstr, &tempAddr.sin_addr.s_addr, 4);
	memcpy(buf+4, ipstr, 4);

	buf[8] = 0;	/* empty username */
	int len=9;

	if (sock.Send(buf, len) !=0)
	{		
		sock.Close();
		return NONE_TYPE;
	}	

	unsigned char vn_byte = read_byte(sock);
	
	if (vn_byte != 0x00) 
	{		
		sock.Close();
	}
	else
	{
		unsigned char cd_byte = read_byte(sock);
		
		if(cd_byte == 90 || cd_byte == 91 || cd_byte == 92 || cd_byte == 93) 
		{			
			sock.Close();
			return PROXY_TYPE_SOCKS4;
		}
	}

	return NONE_TYPE;
}



int CProxyConnect::TestSocks5Proxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password)
{
	char buf[512];	

	sock.SetTimeouts(m_nConnectTimeout_secs*1000, m_nSendTimeout_secs*1000, m_nReceiveTimeout_secs*1000, sock.GetAcceptTimeout());
	if (sock.Connect(proxyIp, proxyPort)!=0)
	{
		return LOCAL_PROXY_CONNECT_FAIL;
	}

	buf[0] = 0x05;
	buf[1] = 0x01;
	if(username && password)
		buf[2] = 0x02;
	else
		buf[2] = 0x00;	

	int len=3;
	if (sock.Send(buf, len) !=0)
	{		
		sock.Close();
		return NONE_TYPE;
	}	

	int ver = read_byte(sock);
	int method = read_byte(sock);

	if ((ver == 0x05 && method == buf[2]) ||
	    (ver == 0x05 && method == 0x00))
	    
	{
		sock.Close();
		return PROXY_TYPE_SOCKS5;
	}
	else
	{
		
		sock.Close();
	}

	return NONE_TYPE;
}


#include "../globals.h"

int CProxyConnect::TestHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password)
{	
	char buf[512];
	char authstring[MAX_LINE_SIZE];	
	
	sock.SetTimeouts(m_nConnectTimeout_secs*1000, m_nSendTimeout_secs*1000, m_nReceiveTimeout_secs*1000, sock.GetAcceptTimeout());
	if (sock.Connect(proxyIp, proxyPort)!=0)
	{
		return LOCAL_PROXY_CONNECT_FAIL;
	}

	char logstr[512];
	char proxyAuth[MAX_LINE_SIZE];
	char*bufPtr=NULL;
	int numRead, numWrite;
	std::string ans;
	memset(buf, 0 , sizeof(buf));

	//
	// Construct a CONNECT string
	//
    if (username && password)
    {
		sprintf(proxyAuth, "%s:%s", username, password);

		memset(authstring, 0, MAX_LINE_SIZE);
	
		base64((unsigned char*)authstring, (unsigned char*)proxyAuth, strlen(proxyAuth));		
		sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nProxy-Authorization: Basic %s\r\nUser-Agent: echoWare\r\n\r\n", destIp, destPort, authstring);
    }
    else
    {
		sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nUser-Agent: echoWare\r\n\r\n", destIp, destPort);
    }

	numWrite=strlen(buf);	
	if (sock.Send(buf, numWrite)!=0)
    {		
		sock.Close();
		return NONE_TYPE;
    }

	sprintf(logstr, "Sent to the server buf=%s", buf);
	

	//
	// Receive response from Proxy
	//
	memset(buf, 0 , sizeof(buf));
	bufPtr = buf;

	numRead=MAX_LINE_SIZE;
	
	if (sock.Receive(bufPtr, numRead)!=0)
	{
		
		sock.Close();
	}
	else
	{
		
		if (strncmp(buf, "HTTP/1.0 200", 12)==0 || strncmp(buf, "HTTP/1.1 200", 12)==0)
		{
			
			sock.Close();
			return PROXY_TYPE_HTTP;
		}
		else
		{
			
			sock.Close();
		}
	}
	
	return NONE_TYPE;
}



int CProxyConnect::DetectProxyType(APISocket::CSocket sock, const char* destIp, unsigned int destPort,  char* proxyIp, 
			unsigned int proxyPort, char* username,  char* password)
{
	int ret, ret1, ret2, ret3;

	

	ret = NONE_TYPE;
	m_proxyType = NONE_TYPE;	

	if (strcmp(proxyIp, "")==0)
	{		
		goto Done;
	}

	if (strcmp(username, "")!=0)
		ret1 = TestSocks5Proxy(sock, destIp, destPort, proxyIp, proxyPort, username, password); 
	else
		ret1 = TestSocks5Proxy(sock, destIp, destPort, proxyIp, proxyPort, NULL, NULL); 

	if (ret1 == PROXY_TYPE_SOCKS5)
	{
		
		m_proxyType = PROXY_TYPE_SOCKS5;
		ret = ret1;
		goto Done;
	}
	else if (ret1 == LOCAL_PROXY_CONNECT_FAIL)
	{		
		ret =  LOCAL_PROXY_CONNECT_FAIL;
		goto Done;
	}	

	if (strcmp(username, "")!=0)
		ret2 = TestSocks4Proxy(sock, destIp, destPort, proxyIp, proxyPort, username, password); 
	else
		ret2 = TestSocks4Proxy(sock, destIp, destPort, proxyIp, proxyPort, NULL, NULL); 

	if (ret2 == PROXY_TYPE_SOCKS4)
	{
		
		m_proxyType = PROXY_TYPE_SOCKS4;
		ret = ret2;
		goto Done;
	}
	else if (ret2 == LOCAL_PROXY_CONNECT_FAIL)
	{
		
		ret =  LOCAL_PROXY_CONNECT_FAIL;
		goto Done;
	}

	ret3 = AnalyzeHttpProxy(sock, destIp, destPort, proxyIp, proxyPort, username, password);

	if (ret3 == LOCAL_PROXY_CONNECT_FAIL)
	{
		
		ret =  LOCAL_PROXY_CONNECT_FAIL;
		goto Done;
	}
	else if (ret3 >0)
	{
		
		m_httpProxyAuthType = ret3;
		m_proxyType = PROXY_TYPE_HTTP;
		ret = PROXY_TYPE_HTTP;
	}
	else
	{
		
	}

Done:
	
	return ret;
}


int CProxyConnect::ConnectViaProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password)
{
	try
	{
		if (m_bIsAutoDetectDone==false)
		{
			DetectProxyType
			(
				sock,
				destIp,
				destPort, 
				proxyIp,
				proxyPort,
				username, 
				password
			);

			if (m_proxyType != NONE_TYPE)
				m_bIsAutoDetectDone = true;
		}

		if (m_proxyType==PROXY_TYPE_SOCKS4)
		{
			if (strcmp(username, "")!=0)
				return ConnectViaSocks4Proxy
					(
						sock,
						destIp,
						destPort, 
						proxyIp, 
						proxyPort,
						username, 
						password
					);
			else
				return ConnectViaSocks4Proxy
					(
						sock,
						destIp,
						destPort, 
						proxyIp, 
						proxyPort,
						NULL, 
						NULL
					);

		}
		else if (m_proxyType==PROXY_TYPE_SOCKS5)
		{
			if (strcmp(username, "")!=0)
				return ConnectViaSocks5Proxy
					(
						sock,
						destIp,
						destPort, 
						proxyIp, 
						proxyPort,
						username, 
						password
					);
			else
				return ConnectViaSocks5Proxy
					(
						sock,
						destIp,
						destPort, 
						proxyIp, 
						proxyPort,
						NULL, 
						NULL
					);
		}
		else if (m_proxyType==PROXY_TYPE_HTTP)
		{
			if (m_httpProxyAuthType==HTTP_AUTH_BASIC)
				m_temphttpProxyAuthType = HTTP_AUTH_BASIC;
			else if (m_httpProxyAuthType==HTTP_AUTH_NTLM)
				m_temphttpProxyAuthType = HTTP_AUTH_NTLM;
			else if (m_httpProxyAuthType==HTTP_AUTH_BASIC_NTLM)
				m_temphttpProxyAuthType = HTTP_AUTH_BASIC;
			else if (m_httpProxyAuthType==HTTP_AUTH_NTLM_BASIC)
				m_temphttpProxyAuthType = HTTP_AUTH_NTLM;
			else
				m_temphttpProxyAuthType = HTTP_AUTH_BASIC;

TryAgain:
			int retnum=-1;
			if (strcmp(username, "")!=0)
				retnum = ConnectViaHttpProxy
							(
								sock,
								destIp,
								destPort, 
								proxyIp, 
								proxyPort,
								username, 
								password
							);
			else
				retnum = ConnectViaHttpProxy
							(
								sock,
								destIp,
								destPort, 
								proxyIp, 
								proxyPort,
								NULL, 
								NULL
							);

			if (retnum <=0)
			{
				if (m_httpProxyAuthType==HTTP_AUTH_BASIC_NTLM && 
					m_temphttpProxyAuthType==HTTP_AUTH_BASIC)
				{
					m_temphttpProxyAuthType=HTTP_AUTH_NTLM;
					
					goto TryAgain;
				}
				else if (m_httpProxyAuthType==HTTP_AUTH_NTLM_BASIC && 
					m_temphttpProxyAuthType==HTTP_AUTH_NTLM)
				{
					m_temphttpProxyAuthType=HTTP_AUTH_BASIC;
					
					goto TryAgain;
				}

			}
			
			return retnum;
		}
	}
	catch(...)
	{}

	return -1;
}



CProxyConnect::CProxyConnect()
{
	m_bIsAutoDetectDone = false;
	m_httpProxyAuthType = -1;
	m_temphttpProxyAuthType = -1;
}


CProxyConnect::~CProxyConnect()
{

}




int CProxyConnect::AnalyzeHttpProxy(APISocket::CSocket sock, const char* destIp, unsigned int destPort, char* proxyIp, 
			unsigned int proxyPort, char* username, char* password)
{
	char buf[MAX_BUFFER_SIZE];
	/*int fd;
	struct sockaddr_in destAddr;
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1)
		return -1;

	memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;   
    destAddr.sin_addr.s_addr = inet_addr(proxyIp);
	destAddr.sin_port = htons(proxyPort); 

	if (TCPConnectWithTimeout(fd, (struct sockaddr*)&destAddr,sizeof(destAddr), connectTimeout_secs)!=SOCK_OK) 
	{
		
		closesocket(fd);
		return LOCAL_PROXY_CONNECT_FAIL;;
	}	*/

	sock.SetTimeouts(m_nConnectTimeout_secs*1000, m_nSendTimeout_secs*1000, m_nReceiveTimeout_secs*1000, sock.GetAcceptTimeout());
	if (sock.Connect(proxyIp, proxyPort)!=0)
		return LOCAL_PROXY_CONNECT_FAIL;

	char logstr[4096];
	char*bufPtr=NULL;
	int numRead, numWrite;	
	memset(buf, 0 , sizeof(buf));

	sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nUser-Agent: echoWare\r\n\r\n", destIp, destPort);
    
	numWrite=strlen(buf);
	 
	if (sock.Send(buf, numWrite)!=0)
    {		
		sock.Close();
		return -1;
    }

	sprintf(logstr, "Dll : AnalyzeHttpProxy: Sent buf(Actual:%d/Sent:%d)=%s", strlen(buf), numWrite, buf);	
	

	//
	// Receive response from Proxy
	//
	memset(buf, 0 , sizeof(buf));
	bufPtr = buf;

	numRead=MAX_BUFFER_SIZE;
	if (sock.Receive(buf, numRead)!=0)
	{
		sock.Close();
		return -1;
	}
	else
	{
		
		if ((strncmp(buf, "HTTP/1.1 407", strlen("HTTP/1.1 407"))==0) ||
			(strncmp(buf, "HTTP/1.0 407", strlen("HTTP/1.0 407"))==0))
		{
			char* s1 = strstr(buf, "Proxy-Authenticate: Basic");
			char* s2 = strstr(buf, "Proxy-Authenticate: NTLM");

			if (s1!=NULL && s2==NULL)
			{
				// Basic Auth
				sock.Close();
				return HTTP_AUTH_BASIC;
			}
			else if (s1==NULL && s2!=NULL)
			{
				// NTLM Auth
				sock.Close();
				return HTTP_AUTH_NTLM;
			}
			else if (s1!=NULL && s2!=NULL)
			{
				if (s1 < s2)
				{
					// Basic Auth
					sock.Close();
					//return HTTP_AUTH_BASIC_NTLM;
					return HTTP_AUTH_NTLM_BASIC; //always try NTLM first
				}
				else
				{
					// NTLM Auth
					sock.Close();

					//return HTTP_AUTH_BASIC_NTLM;
					return HTTP_AUTH_NTLM_BASIC;
				}
			}
			else
			{
				//something different
			}
		}	
		else if (strncmp(buf, "HTTP/1.0 200", 12)==0 || strncmp(buf, "HTTP/1.1 200", 12)==0)
		{
			sock.Close();
			return HTTP_AUTH_BASIC;
		}
	}

	sock.Close();
	return -1;
}