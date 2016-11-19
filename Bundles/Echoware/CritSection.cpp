#include "StdAfx.h"
#include "critsection.h"
#include "errno.h"

CCritSection::CCritSection(void)
{
	pthread_mutex_init(&m_critSection, NULL);
}

CCritSection::~CCritSection(void)
{
	if (pthread_mutex_destroy(&m_critSection) == EBUSY)
	{
		Unlock();
		pthread_mutex_destroy(&m_critSection);
	}
}

void CCritSection::Lock()
{
	pthread_testcancel();
	pthread_mutex_lock(&m_critSection);
}

void CCritSection::Unlock()
{
	pthread_mutex_unlock(&m_critSection);
	pthread_testcancel();
}
