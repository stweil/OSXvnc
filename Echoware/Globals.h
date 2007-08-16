#ifndef _GLOBALS_H
#define _GLOBALS_H

#if _MSC_VER > 1000
#pragma once
#endif 

#define CHANNEL_CODE_SIZE	11
#define ID_STRING_SIZE		255

#define CLIENT_ID_OFFSET		 0
#define APP_ID_OFFSET			64
#define APP_INDEX_OFFSET		96
#define ECHO_VERSION_OFFSET		128
#define RESERVED_OFFSET			136

#include "Logger.h"
#include "CritSection.h"
#include "ProxiesManager.h"

// in miliseconds
#define THREAD_STOP_TIMEOUT 2000
#define CONNECTION_TO_OFFLOAD_TIMER_VALUE	3000
#define RECONNECTION_TO_OFFLOAD_TIMER_VALUE	500
#define RECONNECTION_COUNT					3

class CGlobals
{
public:
	CGlobals();
	virtual ~CGlobals();

	void SetDllInitialized(bool bIsDllInitialized=true);
	bool GetDllInitialized();

	void SetPortForOffLoadingData(int nPortForOffLoadingData);
	int GetPortForOffLoadingData();

	bool InitSockets(unsigned char nHighVersion, unsigned char nLowVersion);
	bool ReleaseSockets();
	
	//format the MyID to the CONNECT_TO_PEER message format
	//[out szBuffer]	result of formating
	//[in len]			input length of the szBuffer
	//[in szPartner]	Peer Id in the form "ClientID:App1,App2"
	//[return] 0 if there is no appid and 1 if ithere is some, -1 if error
	int GetFormattedID(char *szBuffer, int len, const char* szPartner);

	int ParseFormattedID(char *szBuffer, int len, char* szPartner);

public:
	CLogger m_logger;

	//proxies list manager
	CProxiesManager m_proxiesManager;

protected:		
	bool m_bIsDllInitialized;
	int m_nPortForOffLoadingData;	

	CCritSection m_critSection;	
};

extern CGlobals g_globals;

//a generic buffer class
class CBuffer
{
public:
	CBuffer(unsigned int size=1024)
	{
		m_buff=new char[size];

		m_nSize=size;
		m_nWritePosition=0;
	}

	virtual ~CBuffer()
	{
		delete []m_buff;
	}

	void Write(void* buff, unsigned int write_size)
	{
		//g_globals.m_logger.WriteFormated("=>CBuffer: Write len=%d", write_size);

		m_critSection.Lock();

		if (write_size>m_nSize-m_nWritePosition)
		{
			char* tmp=new char[m_nWritePosition+write_size];
			memcpy(tmp, m_buff, m_nWritePosition);

			delete []m_buff;

			m_buff=tmp;

			m_nSize=m_nWritePosition+write_size;
		}

		memcpy(m_buff+m_nWritePosition, buff, write_size);

		m_nWritePosition+=write_size;

		m_critSection.Unlock();

		//g_globals.m_logger.WriteFormated("<=CBuffer: Write len=%d", write_size);
	}

	unsigned int Read(void* buff, unsigned int read_size)
	{	
		//g_globals.m_logger.WriteFormated("=>CBuffer: Read max=%d", read_size);
		m_critSection.Lock();

		//g_globals.m_logger.WriteFormated("read_size=%d m_nWritePos=%d", read_size, m_nWritePosition);

		if (read_size>m_nWritePosition)		
			read_size=m_nWritePosition;		

		memcpy(buff, m_buff, read_size);
		memmove(m_buff, m_buff+read_size, m_nWritePosition-read_size);

		m_nWritePosition-=read_size;

		m_critSection.Unlock();

		//g_globals.m_logger.WriteFormated("<=CBuffer: Read len=%d", read_size);

		return read_size;
	}

	unsigned int Peak(void* buff, unsigned int read_size)
	{
		m_critSection.Lock();		

		if (read_size>m_nWritePosition)		
			read_size=m_nWritePosition;		

		memcpy(buff, m_buff, read_size);				

		m_critSection.Unlock();

		return read_size;
	}

	unsigned int Size()
	{
		return m_nWritePosition;
	}

	void Drop(unsigned int drop_size)
	{
		m_critSection.Lock();

		if (drop_size>=m_nWritePosition)
		{
			//drop all
			m_nWritePosition=0;
		}
		else
		{
			memmove(m_buff, m_buff+drop_size, m_nWritePosition-drop_size);

			m_nWritePosition-=drop_size;
		}

		m_critSection.Unlock();
	}

	void Empty()
	{
		m_nWritePosition=0;
	}

protected:
	char* m_buff;
	unsigned int m_nSize;
	unsigned int m_nWritePosition;

	CCritSection m_critSection;
};

#endif