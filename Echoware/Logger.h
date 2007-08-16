#ifndef _LOGGER_H
#define _LOGGER_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include <string>
#include "CritSection.h"

//a generic logger class
class CLogger
{
public:
	CLogger(bool bLogging=false, const char* szFilePath=0);
	~CLogger(void);

	//set enable/disable loggings
	void SetLogger(bool bLogging);
	//get enable/disable loggings
	bool GetLogger();
	//set the logger path
	void SetLoggerPath(const char* szFilePath);
	//get the logger path
	const char* GetLoggerPath();

	//write the string to logger file
	void Write(const char* szText);
	//write formated string(like printf) to logger file
	void WriteFormated(const char* szFormat, ...);

protected:
	std::string m_strFilePath;
	bool m_bLogging;

	CCritSection m_crtSection;
};

#endif