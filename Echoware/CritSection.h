#ifndef _CRITSECTION_H
#define _CRITSECTION_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "EchoToOSX.h"

//critical section class
class CCritSection
{
public:
	CCritSection(void);
	virtual ~CCritSection(void);

	void Lock();
	
	void Unlock();

protected:		
		pthread_mutex_t m_critSection;
	//CRITICAL_SECTION m_critSection;
};

#endif