#include "StdAfx.h"
#include <time.h>
#include "logger.h"

//if defined _CONSOLE_ then the logging data are writted to a console, not to a file
//#define _CONSOLE_

CLogger::CLogger(bool bLogging, const char* szFilePath)
{
	m_strFilePath=(szFilePath)?(szFilePath):("");
	m_bLogging=bLogging;	

#ifdef _CONSOLE_
	AllocConsole();
	freopen("CONIN$","rb",stdin);
    freopen("CONOUT$","wb",stdout);
    freopen("CONOUT$","wb",stderr);
    setbuf(stderr, 0);
#endif
}

CLogger::~CLogger(void)
{	
#ifdef _CONSOLE_
	FreeConsole();
#endif
}

void CLogger::SetLogger(bool bLogging)
{
	m_crtSection.Lock();

	m_bLogging=bLogging;

	m_crtSection.Unlock();
}

bool CLogger::GetLogger()
{
	m_crtSection.Lock();

	return m_bLogging;

	m_crtSection.Unlock();
}

void CLogger::SetLoggerPath(const char* szFilePath)
{
	m_crtSection.Lock();

	if (szFilePath)
	{
		m_strFilePath=szFilePath;
	}

	m_crtSection.Unlock();
}

const char* CLogger::GetLoggerPath()
{
	m_crtSection.Lock();

	return m_strFilePath.c_str();

	m_crtSection.Unlock();
}

void CLogger::Write(const char* szText)
{
	m_crtSection.Lock();

	if (m_bLogging && !m_strFilePath.empty())
	{
#ifndef _CONSOLE_
		FILE* f=fopen(m_strFilePath.c_str(), "a");
			
		if (f)
#endif
		{
			time_t theTime;
			time(&theTime);
			
			char* tmp_time = ctime(&theTime);
			int len_time = strlen(tmp_time);
			if (tmp_time[len_time - 1] == '\n')
				tmp_time[len_time - 1] = '\0';

#ifdef _CONSOLE_
			fprintf(stdout, "[%s] echoServer : %s \n", tmp_time, szText);
#else
			fprintf(f, "[%s] echoServer : %s\n", tmp_time, szText);
			
			fclose(f);
#endif
		}
	}

	m_crtSection.Unlock();
}

void CLogger::WriteFormated(const char* szFormat, ...)
{
	va_list valist;

	va_start(valist, szFormat);

	char szMsg[1024];
	vsprintf(szMsg, szFormat, valist);
	
	va_end(valist);

	Write(szMsg);
}