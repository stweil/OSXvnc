#include "StdAfx.h"
#include "DataChannels.h"
#include "DataChannel.h"

#include "pthread.h"

#define WM_REMOVE_CHANNEL		(WM_USER+1)
#define WM_REMOVE_ALLCHANNEL	(WM_USER+2)
#define WM_LOCAL_CONNECT_DC		(WM_USER+3)

CDataChannels::CDataChannels(CDllProxyInfo* pProxyInfo)
{
	pthread_mutex_init(&m_ManageThreadMutex, NULL);
	pthread_cond_init(&m_ManageThreadCond, NULL);

	m_nEncriptionLevel=1;
	m_pProxyInfo=pProxyInfo;
	removeChannel = 0;
	localConnectDC = 0;
	removeAllChannels = false; 

	shouldQuit = 0;
	hasQuit = 0;
	m_hManageThread=CreateThread(0, 0, ManageThreadProc, this, 0, &m_dwManageThread);
}

CDataChannels::~CDataChannels(void)
{
	shouldQuit=1;
	pthread_cond_signal(&m_ManageThreadCond);
	ShutdownThread(m_hManageThread, THREAD_STOP_TIMEOUT, &shouldQuit, &hasQuit);
	m_hManageThread=0;
	m_dwManageThread=0;	
}

//adds a data channel to the list
void CDataChannels::AddDataChannel(CDataChannel* pDataChannel)
{
	g_globals.m_logger.WriteFormated("CDataChannels: Enter Add data channel %p", pDataChannel);

	m_critSection.Lock();	

	m_lstDataChannels.push_back(pDataChannel);
	pDataChannel->SetEncriptionLevel(m_nEncriptionLevel);	

	m_critSection.Unlock();

	g_globals.m_logger.WriteFormated("CDataChannels: Exit Add data channel %p, count=%d", pDataChannel, m_lstDataChannels.size());
}

void CDataChannels::InternalRemoveDataChannel(CDataChannel* pDataChannel)
{
	g_globals.m_logger.WriteFormated("CDataChannels: Enter Remove data channel %p", pDataChannel); 

	m_critSection.Lock();	

	size_t size=m_lstDataChannels.size();

	m_lstDataChannels.remove(pDataChannel);		

	if (m_lstDataChannels.size()<size)
	{
		delete pDataChannel;
		pDataChannel=0;	
	}
	
	m_critSection.Unlock();

	g_globals.m_logger.WriteFormated("CDataChannels: Exit Remove data channel %p, count=%d", pDataChannel, m_lstDataChannels.size()); 
}

void CDataChannels::InternalRemoveAllDataChannels()
{
	m_critSection.Lock();

	int i = 0;
	for(std::list<CDataChannel*>::iterator it=m_lstDataChannels.begin(); it!=m_lstDataChannels.end(); it++)
	{
		i++;
		CDataChannel* pDataChannel=(CDataChannel*)(*it);
		g_globals.m_logger.WriteFormated("Remove channel %d %d", i, pDataChannel);

		delete pDataChannel;

		*it=0;
	}

	m_lstDataChannels.clear();

	g_globals.m_logger.Write("CDataChannels: Remove all data channel"); 

	m_critSection.Unlock();
}

//removes a data channel from the list
void CDataChannels::RemoveDataChannel(CDataChannel* pDataChannel)
{
	if (m_hManageThread)
	{
		pthread_mutex_lock(&m_ManageThreadMutex);
		removeChannel = pDataChannel;
		pthread_mutex_unlock(&m_ManageThreadMutex);
		pthread_cond_signal(&m_ManageThreadCond);
	}
}

//removes all the data channels from the list
void CDataChannels::RemoveAllDataChannels()
{
	if (m_hManageThread)
	{
		pthread_mutex_lock(&m_ManageThreadMutex);
		removeAllChannels = true;
		pthread_mutex_unlock(&m_ManageThreadMutex);
		pthread_cond_signal(&m_ManageThreadCond);
	}
}

//sets the encryption level for this proxy connection
void CDataChannels::SetEncriptionLevel(int nLevel)
{
	m_critSection.Lock();

	m_nEncriptionLevel=nLevel;

	for(std::list<CDataChannel*>::iterator it=m_lstDataChannels.begin(); it!=m_lstDataChannels.end(); it++)	
		((CDataChannel*)*it)->SetEncriptionLevel(nLevel);	

	m_critSection.Unlock();
}

//gets the encryption level for this proxy connection
int CDataChannels::GetEncriptionLevel()
{
	return m_nEncriptionLevel;
}

void CDataChannels::LocalConnectDataChannel(CDataChannel* pDataChannel)
{
	if (m_hManageThread)
	{
		pthread_mutex_lock(&m_ManageThreadMutex);
		localConnectDC = pDataChannel;
		pthread_mutex_unlock(&m_ManageThreadMutex);
		pthread_cond_signal(&m_ManageThreadCond);
	}
}

void CDataChannels::InternalLocalConnectDataChannel(CDataChannel* pDataChannel)
{
	//g_globals.m_logger.WriteFormated("CDataChannels: Enter InternalLocalConnectDataChannel data channel %p", pDataChannel); 

	m_critSection.Lock();	

	for (std::list<CDataChannel*>::iterator it=m_lstDataChannels.begin(); it!=m_lstDataChannels.end(); it++)
	{
		if (*it==pDataChannel)
		{			
			if (!pDataChannel->ConnectLocalServer(g_globals.GetPortForOffLoadingData()))
			{
				RemoveDataChannel(pDataChannel);
			}
			break;
		}
	}
	
	m_critSection.Unlock();

	//g_globals.m_logger.WriteFormated("CDataChannels: Exit InternalLocalConnectDataChannel data channel %p", pDataChannel); 
}

#include "InterfaceDllProxyInfo.h"
#include "DllProxyInfo.h"
unsigned long __stdcall CDataChannels::ManageThreadProc(void* lpParameter)
{
	CDataChannels* pDataChannels=(CDataChannels*)lpParameter;

	while (true)
	{
		pthread_cond_wait(&pDataChannels->m_ManageThreadCond, &pDataChannels->m_ManageThreadMutex);
		
		if (pDataChannels->shouldQuit)
		{
			pthread_mutex_unlock(&pDataChannels->m_ManageThreadMutex);
			break;
		}
			
		if (pDataChannels->removeChannel)
		{
			pDataChannels->InternalRemoveDataChannel(pDataChannels->removeChannel);
			pDataChannels->removeChannel=0;
		}
		if (pDataChannels->removeAllChannels)
		{
			pDataChannels->removeAllChannels=FALSE;
			pDataChannels->InternalRemoveAllDataChannels();
		}
		if (pDataChannels->localConnectDC)
		{
			pDataChannels->InternalLocalConnectDataChannel(pDataChannels->localConnectDC);
			pDataChannels->localConnectDC=0;
		}
		pthread_mutex_unlock(&pDataChannels->m_ManageThreadMutex);
	}
	
	pDataChannels->hasQuit=true;
	pthread_cond_destroy(&pDataChannels->m_ManageThreadCond);
	pthread_mutex_destroy(&pDataChannels->m_ManageThreadMutex);

	return 0;
}