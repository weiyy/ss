
#include "ConfigFile.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define ISREGF S_ISREG
#include <fstream>
#include <algorithm>
#include <ctype.h>
#include <iostream>


string& toLower(string& s) {
  // Modify each char element
   transform(s.begin(), s.end(), s.begin(), (int (*)(int))tolower);
  return s;
}

ConfigFile::ConfigFile(const string& filename, bool ifWrite /* = false */)
           :m_sFileName(filename),
            m_valid(false)
{
    struct stat buf;
    if ( ifWrite == false )
    {
        if ( stat( m_sFileName.c_str(),  &buf) == 0 && ISREGF(buf.st_mode) )
        {
            m_valid = true;
        }
    }
    else
        m_valid = true;
}

ConfigFile::~ConfigFile()
{
   
}

void 
ConfigFile::ReadSection(const string& sSection, list< string > &strList) const
{
	fstream fs(m_sFileName.c_str(), ios_base::in|ios_base::binary);
	string line, sSub;
	bool bFound = false;

	if(!fs)
		return;

	//toLower(sSection);
	while(!fs.eof())
	{
	    //fs >> line;
		getline(fs, line);	
		Trim(line);
    if(IsShieldLine(line))
    	continue;
		if(line.length() > 2)
		{
			//如果是section
			if(line[0] == '[' && line[line.length()-1] == ']')
			{
				sSub = line.substr(1, line.length()-2);
				bFound = (toLower(sSub) == sSection);
				continue;   //读取section以下的内容
			}
			//如果找到相应的section，将section内的所有有效内容行读入列表
			if(bFound)
			{
				if(!IsShieldLine(line))
				{
					strList.push_back(line);
			  }
			}
		}
	}
	fs.close();
}

string 
ConfigFile::ReadString(const string& sSection, const string& sIdent, const string &sDefault) const
{	
	
	//std::cout << "sSection: " << sSection << "sIdent: " << sIdent << std::endl;
	list< string > strList;
	int nPos;
	ReadSection(sSection, strList);
	list< string >::iterator iter = strList.begin();
	for ( ; iter != strList.end(); iter++ )
	{
	  string str = *iter;
		nPos = str.find(sIdent.c_str());
		if(nPos != -1)
		{
			return GetValue(str);
		}
	}
	return sDefault;
}

bool 
ConfigFile::ReadStringArray(const string& sSection, const string& sIdent, vector< string >& vet) const
{
    fstream fs(m_sFileName.c_str(), ios_base::in|ios_base::binary);
    if ( !fs )
        return false;
    string line;
    bool bFound = false;
    while ( !fs.eof() )
    {
		getline(fs, line);
        Trim(line);
        if ( IsShieldLine(line) )
            continue;
		if(line.length() > 2)
		{
			if(line.at(0) == '[' && line.at(line.length()-1) == ']')
			{
				string sSub = line.substr(1, line.length()-2);
				bFound = (sSub == sSection);
				continue;
			}
			if ( bFound )
			{
			    string sSub = GetIdent(line);
			    Trim(sSub);
			    if ( sSub == sIdent )
			    {
			        string sValue = GetValue(line);
			        Trim(sValue);
			        while ( 1 )
			        {
    			        size_t n = sValue.find_first_of(",");
    			        string tmp = sValue.substr(0, n);
    			        Trim(tmp);
    			        if ( !tmp.empty() )
    			        {
        			        vector< string >::iterator iter;
        			        iter = find(vet.begin(), vet.end(), tmp);
        			        if ( iter == vet.end() )
        			            vet.push_back(tmp);
    			        }
    			        if ( n == string::npos )
    			        {
    			            break;
    			        }
                        sValue.erase(0,n+1);
			        }
			    }
			}
		}
    }
    fs.close();
    return true;
}

int 
ConfigFile::ReadInteger(const string& sSection, const string& sIdent, int nDefault) const
{
	string s;

	s = ReadString(sSection, sIdent, "");
	if(s != "")
		return atoi(s.c_str());
	else
		return nDefault;
}

long 
ConfigFile::ReadLongInt(const string& sSection, const string& sIdent, long nDefault) const
{
	string s;

	s = ReadString(sSection, sIdent, "");
	if(s != "")
		return atol(s.c_str());
	else
		return nDefault;
}

bool 
ConfigFile::ReadBool(const string& sSection, const string& sIdent, bool bDefault) const
{
	string s;

	s = ReadString(sSection, sIdent, "");
    if ( s.empty() )
        return bDefault;
    transform(s.begin(),s.end(),s.begin(),ptr_fun<int,int>(tolower));   //全部字母变成小写,再比较
    if ( s == "true" || atoi(s.c_str()) != 0 )
        return true;
    return false;
}

float 
ConfigFile::ReadFloat(const string& sSection, const string& sIdent, float fDefault) const
{
	string s;

	s = ReadString(sSection, sIdent, "");
	if(s != "")
		return atof(s.c_str());
	else
		return fDefault;
}

bool 
ConfigFile::WriteString(const string& sIdent, const string& sValue, bool ifTrunc /* = false*/) const
{
    if ( sIdent.empty() || sValue.empty() )
        return false;
    fstream fs;
    if ( ifTrunc == true )
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::trunc);
	else
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);
	if( !fs.is_open() )
		return false;

    string str = sIdent + "=" + sValue;
    fs << str << endl;
    fs.close();
    return true;
}

bool 
ConfigFile::WriteInt(const string& sIdent, int sValue, bool ifTrunc /* = false*/) const
{
    if ( sIdent.empty() )
        return false;
    fstream fs;
    if ( ifTrunc == true )
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::trunc);
	else
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);
	if( !fs.is_open() )
		return false;

    fs << sIdent << "=" << sValue << endl;
    fs.close();
    return true;
}

bool 
ConfigFile::WriteBool(const string& sIdent, bool sValue, bool ifTrunc /* = false*/) const
{
    if ( sIdent.empty() )
        return false;
    fstream fs;
    if ( ifTrunc == true )
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::trunc);
	else
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);
	if( !fs.is_open() )
		return false;

    fs << sIdent << "=" << (sValue==true?"true":"false") << endl;
    fs.close();
    return true;
}

bool 
ConfigFile::WriteStringArray(const string& sIdent, const vector<string>& vetValue, bool ifTrunc /* = false*/) const
{
    if ( sIdent.empty() || vetValue.empty() )
        return false;
    fstream fs;
    if ( ifTrunc == true )
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::trunc);
	else
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);
	if( !fs.is_open() )
		return false;

    vector<string>::const_iterator iter;
    fs << sIdent << "=";
    for ( iter = vetValue.begin(); iter != vetValue.end(); iter++ )
    {
        if ( iter->empty() )
            continue;
        fs << *iter;
        if ( (iter+1) != vetValue.end() )
            fs << ",";
    }
    fs << endl;
    fs.close();
    return true;
}

void 
ConfigFile::WriteBlankLine() const
{
    fstream fs;
    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);

	if( !fs.is_open() )
		return;
    fs << endl;
	fs.close();
}

void 
ConfigFile::WriteSection(const string& sSection, bool ifTrunc /* = false*/) const
{
    fstream fs;
    if ( ifTrunc == true )
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::trunc);
	else
	    fs.open(m_sFileName.c_str(), ios_base::out|ios_base::app);

	if( !fs.is_open() )
		return;
    fs << "[" << sSection << "]" << endl;
	fs.close();
}

bool 
ConfigFile::IsValid() const
{
    return m_valid;
}

bool 
ConfigFile::IsShieldLine(const string& str) const
{
	if( str.length() == 0 || str.length() >= 1 && str[0] == '#')
		return true;
	else
		return false;
}

void 
ConfigFile::Trim(string& str) const
{
	if ( str.length() > 0 && (str[str.length() - 1] == 0X0D || str[str.length() - 1] == 0X0A) )
	{
	    str.erase(str.length() - 1);
  }
	string::iterator Iter;
	Iter = remove(str.begin(), str.end(), '	');//tab

	if(Iter != str.end())
	{
		str.erase(Iter, str.end());
	}

	Iter = remove(str.begin(), str.end(), ' ');//space

	if(Iter != str.end())
	{
		str.erase(Iter, str.end());
	}
}

string 
ConfigFile::GetIdent(const string& str) const
{
	string Ident;
	int nPos;

	nPos = str.find("=");
	if(nPos != -1)
	{
		Ident = str.substr(0, nPos);
		Trim(Ident);
	}

	return Ident;
}

string 
ConfigFile::GetValue(const string& str) const
{
	string Value;
	string::size_type nIndex;

	nIndex = str.find("=");
	if(nIndex != string::npos)
	{
		Value = str.substr(nIndex+1, str.length()-nIndex);
		Trim(Value);
	}

	return Value;
}
