// Echoware.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "Echoware.h"
#include "Globals.h"
#include "DllProxyInfo.h"

//BOOL APIENTRY DllMain( HANDLE hModule, 
BOOL DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved ) {
//#define DLL_PROCESS_ATTACH 0	
//#define DLL_THREAD_ATTACH  1
//#define DLL_THREAD_DETACH  2
//#define	DLL_PROCESS_DETACH 3
//	switch (ul_reason_for_call)
//	{
//	case DLL_PROCESS_ATTACH:		
//		break;
//	case DLL_THREAD_ATTACH:
//		break;
//	case DLL_THREAD_DETACH:
//		break;
//	case DLL_PROCESS_DETACH:
//		/*if (g_globals.GetDllInitialized())
//		{
//			g_globals.m_logger.Write("ReleaseSockets");
//			g_globals.ReleaseSockets();
//		}*/
//		break;
//	}
    return TRUE;
}

#define ECHOWARE_VERSION		"1.926"

extern "C" ECHOWARE_API char* GetDllVersion()
{
	return ECHOWARE_VERSION;
}

extern "C" ECHOWARE_API bool InitializeProxyDll()
{
	if (g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("InitializeProxyDll : already been intialized");
		return true;
	}

	if (!g_globals.InitSockets(1, 1))
	{
		g_globals.SetDllInitialized(false);
		return false;
	}

	g_globals.SetDllInitialized(true);

	return true;
}

extern "C" ECHOWARE_API void SetLoggingOptions(bool bEnablelogging, char *szLogPath)
{
	g_globals.m_logger.SetLogger(bEnablelogging);
	g_globals.m_logger.SetLoggerPath(szLogPath);
}

extern "C" ECHOWARE_API void EnableLogging(bool bEnablelogging)
{
	g_globals.m_logger.SetLogger(bEnablelogging);
}

extern "C" ECHOWARE_API bool SetPortForOffLoadingData(int DataOffLoadingPort)
{
	g_globals.SetPortForOffLoadingData(DataOffLoadingPort);

	g_globals.m_logger.WriteFormated("Data offloading port : %d", DataOffLoadingPort);

	return true;
}

extern "C" ECHOWARE_API void* CreateProxyInfoClassObject()
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("CreateProxyInfoClassObject : not been initialized");
		return 0;
	}

	CDllProxyInfo* pProxyInfo=new CDllProxyInfo;
	g_globals.m_proxiesManager.AddProxy(pProxyInfo);	

	return (IDllProxyInfo*)pProxyInfo;
}

extern "C" ECHOWARE_API void DeleteProxyInfoClassObject (void* pProxyInfo)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("DeleteProxyInfoClassObject : not been initialized");
		return;
	}

	g_globals.m_proxiesManager.RemoveProxy((CDllProxyInfo*)pProxyInfo);
}

extern "C" ECHOWARE_API void AutoConnect()
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("AutoConnect : not been initialized");
		return ;
	}

	g_globals.m_proxiesManager.AutoConnect();
}

extern "C" ECHOWARE_API int ConnectProxy(void* pProxyInfo)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("ConnectProxy : not been initialized");
		return ERROR_CONNECTING_TO_PROXY;
	}

	return g_globals.m_proxiesManager.ConnectProxy((CDllProxyInfo*)pProxyInfo);
}

extern "C" ECHOWARE_API bool DisconnectProxy(void* pProxyInfo)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("DisconnectProxy : not been initialized");
		return false;
	}

	return g_globals.m_proxiesManager.DisconnectProxy((CDllProxyInfo*)pProxyInfo);
}

extern "C" ECHOWARE_API bool DisconnectAllProxies()
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("DisconnectAllProxies : not been initialized");
		return false;
	}

	return g_globals.m_proxiesManager.DisconnectAllProxies();
}

extern "C" ECHOWARE_API void StopConnecting(void* pProxyInfo)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("StopConnecting : not been initialized");
		return;
	}

	g_globals.m_proxiesManager.StopConnecting((CDllProxyInfo*)pProxyInfo);
}

extern "C" ECHOWARE_API int EstablishNewDataChannel(void* pProxyInfo , char* IDOfPartner)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("EstablishNewDataChannel : not been initialized");
		return 0;
	}

	return g_globals.m_proxiesManager.EstablishNewDataChannel((CDllProxyInfo*)pProxyInfo, IDOfPartner);
}

extern "C" ECHOWARE_API void SetEncryptionLevel(int level, void* pProxyInfo) 
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("SetEncryptionLevel : not been initialized");
		return;
	}

	g_globals.m_logger.WriteFormated("Set Encryption Level to %d for %s", level, ((CDllProxyInfo*)pProxyInfo)->GetIpPort());

	g_globals.m_proxiesManager.SetEncryptionLevel(level, (CDllProxyInfo*)pProxyInfo);
}

extern "C" ECHOWARE_API void SetLocalProxyInfo(char* ip, char* port, char* username, char* password)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("SetLocalProxyInfo : not been initialized");
		return;
	}

	g_globals.m_proxiesManager.SetLocalProxyInfo(ip, port, username, password);
}

extern "C" ECHOWARE_API bool CheckServer(char* ip, unsigned short port)
{
	if (!g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("SetLocalProxyInfo : not been initialized");
		return false;
	}

	APISocket::CClientSocket sock;
	sock.Create();
	if (sock.Connect(ip, port)==0)
		return true;

	return g_globals.m_proxiesManager.ConnectViaProxy(&sock, ip, port);

	return false;
}