#include <Windows.h>
#include <string>
#include "IniReader.h"

CIniReader::CIniReader(const char* szFileName)
{
	m_szFileName = szFileName;
}
int CIniReader::ReadInt(const char* szSection, const char* szKey, int iDefaultValue)
{
	int iResult = GetPrivateProfileIntA(szSection, szKey, iDefaultValue, m_szFileName.c_str());
	return iResult;
}
float CIniReader::ReadFloat(const char* szSection, const char* szKey, float fltDefaultValue)
{
	char szResult[255];
	char szDefault[255];
	float fltResult;
	sprintf_s(szDefault, 255, "%f", fltDefaultValue);
	GetPrivateProfileStringA(szSection, szKey, szDefault, szResult, 255, m_szFileName.c_str());
	fltResult = (float)atof(szResult);
	return fltResult;
}
int CIniReader::ReadBool(const char* szSection, const  char* szKey, int bolDefaultValue)
{
	char szResult[255];
	char szDefault[255];
	int bolResult;
	sprintf_s(szDefault, 255, "%s", bolDefaultValue ? "true" : "false");
	GetPrivateProfileStringA(szSection, szKey, szDefault, szResult, 255, m_szFileName.c_str());
	bolResult = _stricmp(szResult, "true") == 0 ||  _stricmp(szResult, "1") == 0 ? true : false;
	return bolResult;
}
char* CIniReader::ReadString(const char* szSection, const  char* szKey, const char* szDefaultValue)
{
	char* szResult = new char[255];
	memset(szResult, 0x00, 255);
	GetPrivateProfileStringA(szSection, szKey,
		szDefaultValue, szResult, 255, m_szFileName.c_str());
	return szResult;
}