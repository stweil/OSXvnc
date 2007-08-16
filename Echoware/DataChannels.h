#ifndef _DATACHANNELS_H
#define _DATACHANNELS_H

#if _MSC_VER > 1000
#pragma once
#endif 

class CDataChannel;
class CDllProxyInfo;
#include "CritSection.h"
#include <list>
#include "pthread.h"

//data channels manager
//manages all data channels for a proxy connection
class CDataChannels
{
public:
	CDataChannels(CDllProxyInfo* pProxyInfo);
	virtual ~CDataChannels(void);

	//adds a data channel to the list
	void AddDataChannel(CDataChannel* pDataChannel);

	//removes a data channel from the list
	void RemoveDataChannel(CDataChannel* pDataChannel);
	//removes all the data channels from the list
	void RemoveAllDataChannels();
	//make a local data channel connection
	void LocalConnectDataChannel(CDataChannel* pDataChannel);

	//sets the encryption level for this proxy connection
	void SetEncriptionLevel(int nLevel);
	//gets the encryption level for this proxy connection
	int GetEncriptionLevel();	

	const CDllProxyInfo* GetProxyInfo() const {return m_pProxyInfo;}

	// Thread communication flags
	bool shouldQuit;
	bool hasQuit;
	
	bool removeAllChannels;
	CDataChannel *removeChannel; 
	CDataChannel *localConnectDC;
	pthread_cond_t m_ManageThreadCond;
	pthread_mutex_t m_ManageThreadMutex;

protected:
	void InternalRemoveDataChannel(CDataChannel* pDataChannel);
	void InternalRemoveAllDataChannels();
	void InternalLocalConnectDataChannel(CDataChannel* pDataChannel);

protected:
	CCritSection m_critSection;
	std::list<CDataChannel*> m_lstDataChannels;

	int m_nEncriptionLevel;

	CDllProxyInfo* m_pProxyInfo;
	
protected:
	void* m_hManageThread;	
	unsigned long m_dwManageThread;

	//thread proc for manage data channel notifications
	static unsigned long __stdcall ManageThreadProc(void* lpParameter);
};

#endif