#ifndef INIREADER_H
#define INIREADER_H
#include <string>
class CIniReader
{
public:
	CIniReader(const char* szFileName);
	int ReadInt(const char* szSection, const char* szKey, int iDefaultValue);
	float ReadFloat(const char* szSection, const char* szKey, float fltDefaultValue);
	int ReadBool(const char* szSection, const char* szKey, int bolDefaultValue);
	char* ReadString(const char* szSection, const char* szKey, const char* szDefaultValue);
private:
	std::string m_szFileName;
};
#endif//INIREADER_H