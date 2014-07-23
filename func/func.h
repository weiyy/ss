#ifndef _FUNC_
#define _FUNC_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define INFO __FILE__,__LINE__

#define gettid() syscall(__NR_gettid)
///////////////////////////////////////////////////////
#define OFFSET(type, member) (unsigned int)(&(((type *)0)->member))
///////////////////////////////////////////////////////

#define isequal16bytes(x, y) ( (*(uint64_t*)(x)==*(uint64_t*)(y)) && (*(uint64_t*)&(x)[8]==*(uint64_t*)&(y)[8]) )

#define copy16bytes(d, s) *(uint64_t*)(d)=*(uint64_t*)(s);*(uint64_t*)&(d)[8]=*(uint64_t*)&(s)[8];

#define isequal32bytes(x, y) ( (*(uint64_t*)(x)==*(uint64_t*)(y)) && (*(uint64_t*)&(x)[8]==*(uint64_t*)&(y)[8]) && (*(uint64_t*)&(x)[16]==*(uint64_t*)&(y)[16]) && (*(uint64_t*)&(x)[24]==*(uint64_t*)&(y)[24]) )

#define copy32bytes(d, s) *(uint64_t*)(d)=*(uint64_t*)(s);*(uint64_t*)&(d)[8]=*(uint64_t*)&(s)[8];*(uint64_t*)&(d)[16]=*(uint64_t*)&(s)[16];*(uint64_t*)&(d)[24]=*(uint64_t*)&(s)[24];





class map_compare_char16 {
public:
	inline int64_t operator()(const char* s1, const char* s2){
		return memcmp(s1, s2, 16);
	}
};


class map_compare_char32 {
public:
	inline int64_t operator()(char * s1, char * s2) const {
		return memcmp(s1, s2, 32);
	}
};


class map_compare_int32
{
public:
	inline int operator()(uint32_t s1, uint32_t s2) const
	{
		return s1 - s2;
	}
};


class map_compare_int64
{
public:
	inline int64_t operator()(uint64_t s1, uint64_t s2) const
	{
		return s1 - s2;
	}
};


class map_compare_int16
{
public:
	inline int operator()(uint16_t s1, uint16_t s2) const
	{
		return s1 - s2;
	}
};


class map_compare_string
{
public:
	inline int operator()(const char* s1, const char* s2) const
	{
		return strcmp(s1, s2);
	}
};

class map_compare_string_icase
{
public:
	inline int operator()(const char* s1, const char* s2) const
	{
		return strcasecmp(s1, s2);
	}
};

class map_compare_mem16
{
public:
	inline int operator()(const char* s1, const char* s2) const
	{
		return memcmp(s1, s2, 16);
	}
};

class map_compare_mem32
{
public:
	inline int operator()(const char* s1, const char* s2) const
	{
		return memcmp(s1, s2, 32);
	}
};

class map_compare_mem128
{
public:
	inline int operator()(const char* s1, const char* s2) const
	{
		return memcmp(s1, s2, 128);
	}
};



/*
 
 */
int safe_strlen(const char * str);

int safe_atoi(const char* str);

//int safe_atoi(char const* str);

double safe_atof(const char* str);

int64_t safe_atoi64(const char* str);

int safe_strcpy(char * dest_buff, int dest_buff_size, char * src_str);

int safe_memcpy(char * dest_buff, int dest_buff_size, char * src_buff, int src_buff_len);

void itoa(int val, char* buff, int radix);

int Utf8ToUtf16(unsigned char* utf8, int size8, unsigned short* utf16, int& size16);
int UTF16ToUTF8(char* utf16, int size16, char* utf8, int& size8);
int UTF162UTF8(unsigned short* utf16, int size16, char* utf8, int& size8);
int UTF16ToUTF8(char* utf16, int size16, char* utf8, int& size8);

int Utf8ToUtf32(char* utf8, int size8, unsigned long* utf32, int& size32);

int convertUTF32UTF8(unsigned long* utf32, int size32, char* utf8, int& size8);

/*
 */

void hex_to_str(unsigned char * hex, int hex_len, char * str);

inline unsigned char hexchar2num(char hexchar);

void str_to_hex(char * str, int str_len, unsigned char * hex);

/*
 */
typedef void*(*THREAD_FUNC_POINTER)(void*);
int create_thread(pthread_t * ntid, THREAD_FUNC_POINTER func, void * arg);

#define	MAXFD	1024
void daemon_init();
/*
 */
uint32_t QueryIP(char * szIP);

int tcpsend(int s, char * buff, int len);

int tcprecv(int s, char * buff, int len);

/*
 */
bool is_file_exists(const char *path);
/*
 */
void get_default_time_string(char * time_str);
/*
 */
int write_log(const char * file_path, const char * format, ...);
void save_log(char* filename, int line, const char *msgfmt, ...);

#endif
