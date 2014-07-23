#ifndef CONF_HPP_HEADER_INCLUDED
#define CONF_HPP_HEADER_INCLUDED

#include <time.h>
#include <string.h>
#include <string>
#include <vector>


using namespace std;

/*
 *  ϵͳ����
 */
struct SystemInfo
{
    string programid;       //�����־
    unsigned short port;     //���������ö˿�
    time_t indextime;     //�ؽ�����ʱ����
};

/*
 *  �����ļ���,�������ļ�·����Makefileָ��֮��
 *  �������е����þ����������ļ��ж�ȡ
 */
class Conf
{
public:
    static Conf* Instance();

    virtual ~Conf();

public:
    /*
     *  -2  �����ļ�·��������־���ô������ô���
     *  -1  ��ȡ�����ļ�����
     *  0   �ɹ�
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

