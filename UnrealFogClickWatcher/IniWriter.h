#ifndef INIWRITER_H
#define INIWRITER_H
#include <string>
class CIniWriter
{
public:
	CIniWriter(const char* szFileName);
	void WriteInt(const char* szSection, const  char* szKey, int iValue);
	void WriteFloat(const char* szSection, const  char* szKey, float fltValue);
	void WriteBool(const char* szSection, const  char* szKey, int bolValue);
	void WriteString(const char* szSection, const  char* szKey, const char* szValue);
private:
	std::string m_szFileName;
};
#endif //INIWRITER_H