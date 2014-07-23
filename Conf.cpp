
#include "Conf.hpp"
#include "ConfigFile.hpp"
#include <iostream>
#include <set>
#include <algorithm>
#include <sys/ipc.h>

#include "func.h"
using namespace std;

Conf* Conf::m_instance = 0;

Conf* 
Conf::Instance()
{
    if ( m_instance == NULL )
        m_instance = new Conf();
    return m_instance;
}

Conf::Conf()
{
}

Conf::~Conf()
{
}

int 
Conf::LoadConf(const string& conffile)
{
    ConfigFile conf = ConfigFile(conffile);
    if ( !conf.IsValid() )
    {
        m_lasterr = "invalid config file!" + conffile;
        return -2;
    }
    //System info
    m_sysinfo.programid = conf.ReadString("system", "programID", "search_server");
    m_sysinfo.port = conf.ReadInteger("system", "port", 61020);
    m_sysinfo.indextime = conf.ReadInteger("system", "indextime", 20) * 60;

    //Print();
    return 0;
}

const SystemInfo& 
Conf::GetSysInfo() const
{
    return m_sysinfo;
}

const string& 
Conf::GetLastError() const
{
    return m_lasterr;
}

void 
Conf::Print() const
{
	cout << "[SystemInfo]" << endl;
	cout << "programID:" << m_sysinfo.programid << endl;
	cout << "port:" << m_sysinfo.port << endl;
	cout << "indextime:" << m_sysinfo.indextime << endl;
}
