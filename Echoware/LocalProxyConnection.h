#ifndef _LOCALPROXICONNECT_H
#define _LOCALPROXICONNECT_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "proxyconnection.h"

class CLocalProxyConnection : public CProxyConnection
{
public:
	CLocalProxyConnection(void);
	virtual ~CLocalProxyConnection(void);
};

#endif