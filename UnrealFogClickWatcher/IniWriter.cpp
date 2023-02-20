#include <Windows.h>
#include <string>
#include "IniWriter.h"

CIniWriter::CIniWriter(const char* szFileName)
{
	m_szFileName = szFileName;
}
void CIniWriter::WriteInt(const char* szSection, const  char* szKey, int iValue)
{
	char szValue[255];
	sprintf_s(szValue, 255, "%d", iValue);
	WritePrivateProfileStringA(szSection, szKey, szValue, m_szFileName.c_str());
}
void CIniWriter::WriteFloat(const char* szSection, const  char* szKey, float fltValue)
{
	std::string FixedValue = std::to_string(fltValue);

	if (FixedValue.find('.') != std::string::npos)
	{
		while (FixedValue.ends_with('0'))
		{
			FixedValue.pop_back();
		}

		if (FixedValue.ends_with('.'))
		{
			FixedValue.pop_back();
		}
	}

	WritePrivateProfileStringA(szSection, szKey, FixedValue.c_str(), m_szFileName.c_str());
}
void CIniWriter::WriteBool(const char* szSection, const  char* szKey, int bolValue)
{
	char szValue[255];
	sprintf_s(szValue, 255, "%s", bolValue ? "true" : "false");
	WritePrivateProfileStringA(szSection, szKey, szValue, m_szFileName.c_str());
}
void CIniWriter::WriteString(const char* szSection, const  char* szKey, const char* szValue)
{
	WritePrivateProfileStringA(szSection, szKey, szValue, m_szFileName.c_str());
}