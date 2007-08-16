#if !defined(_ECHOWARE_H)
#define _ECHOWARE_H

#if _MSC_VER > 1000
#pragma once
#endif 

extern "C" char* GetDllVersion();
extern "C" bool InitializeProxyDll();
extern "C" void SetLoggingOptions(bool bEnablelogging, char *szLogPath);
extern "C" void EnableLogging(bool bEnablelogging);
extern "C" bool SetPortForOffLoadingData(int DataOffLoadingPort);
extern "C" void* CreateProxyInfoClassObject();
extern "C" void DeleteProxyInfoClassObject (void* pProxyInfo);

extern "C" void AutoConnect();
extern "C" int ConnectProxy(void* pProxyInfo);
extern "C" bool DisconnectProxy(void* pProxyInfo);
extern "C" bool DisconnectAllProxies();
extern "C" void StopConnecting(void* pProxyInfo);

extern "C" void SetEncryptionLevel(int level, void* pProxyInfo);
extern "C" void SetLocalProxyInfo(char* ip, char* port, char* username, char* password);
extern "C" bool CheckServer(char* ip, unsigned short port);

#ifndef ECHOWARE_API
#ifdef ECHOWARE_EXPORTS
#define ECHOWARE_API __declspec(dllexport)
#else
#define ECHOWARE_API __declspec(dllimport)
#endif
#endif



#endif


