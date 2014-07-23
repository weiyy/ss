#ifndef CONFPARSE_HPP_HEADER_INCLUDED
#define CONFPARSE_HPP_HEADER_INCLUDED

#include <string>
#include <list>
#include <vector>

using namespace std;

class ConfigFile
{
public:
    ConfigFile(const string& filename, bool ifWrite = false);

    virtual ~ConfigFile();

public://read
	void ReadSection(const string& sSection, list< string > &List) const;

	string ReadString(const string& sSection, const string& sIdent, const string& sDefault = "") const;

    bool ReadStringArray(const string& sSection, const string& sIdent, vector< string >& vet) const;

    int ReadInteger(const string& sSection, const string& sIdent, int nDefault) const;

    long ReadLongInt(const string& sSection, const string& sIdent, long nDefault) const;

	bool ReadBool(const string& sSection, const string& sIdent, bool bDefault) const;

	float ReadFloat(const string& sSection, const string& sIdent, float fDefault) const;

public://write
    bool WriteString(const string& sIdent, const string& sValue, bool ifTrunc = false) const;

    bool WriteInt(const string& sIdent, int sValue, bool ifTrunc = false) const;

    bool WriteBool(const string& sIdent, bool sValue, bool ifTrunc = false) const;

    bool WriteStringArray(const string& sIdent, const vector<string>& vetValue, bool ifTrunc = false) const;

    void WriteBlankLine() const;

    /*
     *  params:
     *      ifTrunc false从头开始写入 true直接写入
     */
    void WriteSection(const string& sSection, bool ifTrunc = false) const;

public:
    bool IsValid() const;

private:
	bool IsShieldLine(const string& str) const;

	void Trim(string& str) const;

	string GetIdent(const string& str) const;

	string GetValue(const string& str) const;

private:
    string m_sFileName;

    bool m_valid;
};

#endif /*CONFPARSE_HPP_HEADER_INCLUDED*/
