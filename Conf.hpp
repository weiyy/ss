#ifndef CONF_HPP_HEADER_INCLUDED
#define CONF_HPP_HEADER_INCLUDED

#include <time.h>
#include <string.h>
#include <string>
#include <vector>


using namespace std;

/*
 *  系统配置
 */
struct SystemInfo
{
    string programid;       //程序标志
    unsigned short port;     //本程序所用端口
    time_t indextime;     //重建索引时间间隔
};

/*
 *  配置文件类,除配置文件路径由Makefile指定之外
 *  其它所有的配置均放在配置文件中读取
 */
class Conf
{
public:
    static Conf* Instance();

    virtual ~Conf();

public:
    /*
     *  -2  配置文件路径错误，日志配置错误，配置错误
     *  -1  读取配置文件错误
     *  0   成功
     */
    int LoadConf(const string& conffile);

    const SystemInfo& GetSysInfo() const;

    const string& GetLastError() const;

    void Print() const;

protected:
    Conf();

private:
    SystemInfo  m_sysinfo;
    string      m_lasterr;
    static Conf* m_instance;
};

#endif /*CONF_HPP_HEADER_INCLUDED*/

