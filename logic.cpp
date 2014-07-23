
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "func.h"
#include "MemPool.h"
#include "RB_tree.h"
#include "ObjectPool.h"
#include "DblQueue.h"
#include "MinHeap.h"
#include "pinyin.h"
#include "JsonBase.h"

#include "sphinxclient.h"
#include "Conf.hpp"


#define SWAPIN_BUFFER_SIZE	1024*1024+1024

#define MAXRESID 3

///////////////////////////////////////////////////////////////////////////////////////////
#define MAXSWAPOUTBUFF				4096
#define DATABASE_INTERVAL			1200
#define DOWNLOAD_URL_CHANGE_TIME_LIMIT	2592000


enum {
	IH_E_OK								= 0,
	IH_E_TEST							= 1,
	IH_E_INVALID_DEVICE_FAMILY			= 5,
	IH_E_NEGATIVE_VALUE					= 8,
	IH_E_UNKNOWN						= 17,
	IH_E_INVALID_REQUEST				= 20,
	IH_E_DBRELOAD_ISINRELOAD			= 30,
	IH_E_DBRELOAD_INVALID_REQUEST		= 31,
	IH_E_DBRELOAD_FAILED				= 32,
};


enum {
	IH_RES_UNKNOWN			= -1,
	IH_RES_HOME				= 0,
	IH_RES_SOFTWARE			= 1,
	IH_RES_GAME				= 2,
	IH_RES_CALL_RING		= 3,
	IH_RES_CAF_RING			= 4,
	IH_RES_WALLPAPER		= 5,
	IH_RES_ALBUM			= 6,
	IH_RES_SONG				= 7,
	IH_RES_TMFAPP			= 10,
	IH_RES_CRACKAPP 		= 11,
	IH_RES_APPLEAPP			= 12,
	IH_RES_SHAREPC			= 13,
	IH_RES_SHAREDC			= 14,
};

#pragma pack(push)
#pragma pack(1)
struct SWAPOUTBUFFER
{
	uint16_t len;
	int client_sock;
	int session_id;
	uint32_t flags;
	char buff[MAXSWAPOUTBUFF];
};

struct SWAPINBUFFER
{
    int client_sock;
    uint32_t ip;
    int session_id;
	uint32_t flags;
    char packet[];
};

struct CLIENTPACKETTCP
{
	uint32_t len;
	uint32_t command;
	char data[];
};

#pragma pack(pop)

#define KETWORDS_COUNT_LIMIT 	5
#define SWAPOUTBUFF_HEADER_SIZE	14		//SWAPOUTBUFFER::buff-SWAPOUTBUFFER::len
#define SWAPINBUFF_HEADER_SIZE	16		//SWAPINBUFFER::packet-SWAPINBUFFER::client_sock
#define TIME_NOW g_time_tick

#define 	isspace(x) 		((x)==' '||(x)=='\r'||(x)=='\n'||(x)=='\f'||(x)=='\b'||(x)=='\t')

char *strtrim(char *str){
	char *p = str;
	char *p1 = str;
	if(p)
	{
		p1 = p + strlen(str) - 1;
		while(*p && *p>0 && isspace(*p)) p++;
		while(p1 > p && *p1>0 && isspace(*p1)) *p1-- = '\0';
	}
	return p;
}

///////////////////////////////////////////////////////////////////////////////////////////
uint32_t g_time_tick = 0;
uint32_t g_app_domain_limit = 10000;
///////////////////////////////////////////////////////////////////////////////////////////

bool g_continue_work = true;
CCriticalSection g_continue_lock;

bool is_continue_work(){
	bool bresult = false;
	g_continue_lock.Lock();
	bresult = g_continue_work;
	g_continue_lock.Unlock();
	return bresult;
}

void set_continue_work(bool bcontinue){
	g_continue_lock.Lock();
	g_continue_work = bcontinue;
	g_continue_lock.Unlock();
}

uint32_t CRCTABLE[256] = {0};
void InitCrcTable()
{
	uint32_t crc;
	int i,j;

	DWORD poly=0xEDB88320L;
	for(i=0;i<256;i++)
	{
		crc=i;
		for(j=8;j>0;j--)
		{
			if(crc & 1)
				crc=(crc>>1)^poly;
			else
				crc>>=1;
		}
		CRCTABLE[i]=crc;
	}
}

uint32_t CRC32(unsigned char *buf, int len)
{
	uint32_t ret = 0xFFFFFFFF;
	int   i;
	for(i = 0; i < len;i++)
	{
		ret = CRCTABLE[((ret & 0xFF) ^ buf[i])] ^ (ret >> 8);
	}
	ret = ~ret;
	return ret;
}

unsigned char ParseAppType(char * str){
    if (!str)
        return 0;

    if (strcmp(str, "soft") == 0)
        return 1;
    else if (strcmp(str, "game") == 0)
        return 2;
    else
        return 0;
}

unsigned char ParsePacketType(char * str){
    if (!str)
        return 0;

    if (strcmp(str, "ipa") == 0)
        return 0;
    else if (strcmp(str, "pxl") == 0)
        return 1;
    else if (strcmp(str, "deb") == 0)
        return 2;
    else
        return 0;
}

void ParseVersionString(char * version, int version_len, uint32_t * out_ver_num){
    char version_substr[20];

    for (int i=0; i<8; i++)
        out_ver_num[i] = 0;

    if (version_len > 200)
        return;

    char version_copy[210];
    strcpy(version_copy, version);
    strcat(version_copy, ".");
    version_len ++;

    bool check_version_fonud_startpos = false;
    bool check_version_prev_is_point = true;
    int version_num_index = 0;
    version_substr[0] = '\0';
    int version_substr_pos = 0;
    for (int k=0; k<version_len; k++)
    {
        if (version_copy[k] == '.')
        {
            if (check_version_prev_is_point)
                continue;

            check_version_prev_is_point = true;

            out_ver_num[version_num_index] = atoi(version_substr);

            version_num_index ++;
            if (version_num_index > 7)
                break;
            version_substr[0] = '\0';
            version_substr_pos = 0;
        }
        else if (version_copy[k] >= '0'&& version_copy[k] <= '9')
        {
            check_version_prev_is_point = false;
            check_version_fonud_startpos = true;

            if (version_substr_pos < 18)
            {
                version_substr[version_substr_pos] = version_copy[k];
                version_substr[version_substr_pos+1] = '\0';
                version_substr_pos ++;
            }
        }
        else
        {
            if (check_version_fonud_startpos)
                break;
            else
                continue;
        }
    }
}

uint32_t ParseIOSVersion(char * ver_str){
    if ( (!ver_str) || (strlen(ver_str) > 40) )
        return 0;

    uint32_t v = 0;

    int n = 0;
    char * s;
    char c[50];
    strcpy(c, ver_str);
    strcat(c, ".0.0.0.");
    char * t = c;
    while (s = strstr(t, "."))
    {
        *s = '\0';
        v = v << 8;
        v = v | (atoi(t) % 0x100);
        //*s = '.';
        t = s + 1;

        n++;
        if (n == 4)
            break;
    }

    return v;
}

int tail_safe_atoi(char *s){
	if (s) {
		int ilength = strlen(s);
		return atoi(s+ilength-1);
	} else {
		return 0;
	}
}

char *mem_pool_dup_str(CMemPool *mem_pool, char *s){
	if (!s) {
		//return "";
		//Modified by zwj186: 2012 4-11
		char *p = (char *)mem_pool->Alloc(1);
		*p = '\0';
		return p;
	}

	int n = strlen(s);
	char *p = (char *)mem_pool->Alloc(n+1);
	memcpy(p, s, n+1);

	return p;
}

char *replace_strchar(char *pInput, int length, char pSrc, char pDst){
	if(pInput)
	{
		char *pstr = pInput;
		while(*pstr !='\0' && (pstr-pInput)<length)
		{
			if(*pstr == pSrc)
				*pstr = pDst;
			pstr++;
		}
	}
	return pInput;
}

static const char base64_str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_pad = '=';

static const signed char base64_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

unsigned int base64encode(const unsigned char *inbuf, unsigned int *inbuf_size, char *outbuf,  unsigned int outbuf_size){
	unsigned int outlen = (*inbuf_size / 3) * 4 + 1;
	if (!inbuf || !outbuf || !inbuf_size || !(*inbuf_size > 0) || outbuf_size<outlen) return 0;
	//char *outbuf = (char*)g_application_mem_pool->Alloc(outlen+5); // 4 spare bytes + 1 for '\0'
	size_t n = 0;
	size_t m = 0;
	unsigned char input[3];
	unsigned int output[4];
	while (n < *inbuf_size) {
		input[0] = inbuf[n];
		input[1] = (n+1 < *inbuf_size) ? inbuf[n+1] : 0;
		input[2] = (n+2 < *inbuf_size) ? inbuf[n+2] : 0;
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 3) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 15) << 2) + (input[2] >> 6);
		output[3] = input[2] & 63;
		outbuf[m++] = base64_str[(int)output[0]];
		outbuf[m++] = base64_str[(int)output[1]];
		outbuf[m++] = (n+1 < *inbuf_size) ? base64_str[(int)output[2]] : base64_pad;
		outbuf[m++] = (n+2 < *inbuf_size) ? base64_str[(int)output[3]] : base64_pad;
		n+=3;
	}
	outbuf[m] = 0; // 0-termination!
	//*outbuf_size = m;
	return m;
}

CMemPool *g_application_mem_pool = NULL;
CMemPool *g_db_mem_pool = NULL;

uint32_t g_search_all_count = 0;
uint32_t g_search_app_count = 0;
uint32_t g_search_share_app_count = 0;
uint32_t g_search_ring_count = 0;
uint32_t g_search_wallpaper_count = 0;

CCriticalSection g_data_locker;//lock for g_db_mem_pool !

struct SEARCH_LIST{
    uint32_t index;
    SEARCH_LIST * next;
};

struct SEARCH_CACHE{
	//app: device_family(4),ios_version(4),search_key(string)
	char key[128];

	int count;
	SEARCH_LIST * sl;
	
	uint32_t last_visit_time;
	
	int search_type;
	
	SEARCH_CACHE *prev;
	SEARCH_CACHE *next;
};

class rbtree_searchcache_alloctor {
public:
	void *alloc(size_t n)
	{
	  return g_db_mem_pool->Alloc(n);
	}
	
	void dealloc(void *p)
	{
		g_db_mem_pool->Free(p);
	}
};

_RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor> * g_ring_search_cache_tree = NULL;
_RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor> * g_wallpaper_search_cache_tree = NULL;
_RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor> * g_apple_app_search_cache_tree = NULL;

CDblQueue<SEARCH_CACHE> g_search_cache_lru;

int get_device_family(char *devices){
	int result = 0;
	if(!devices)
		return result;
	if(strncasecmp(devices, "iphone", 6) == 0 && strlen(devices)>6)
	{
		result = 1;//pow(2, 0);
		int mac = atoi(devices+6);
		if(mac <3)
			mac = 1;
		else if(mac < 5)
			mac = 2;
		result = result | (mac<<4);
	}
	else if(strncasecmp(devices, "ipod", 4) == 0 && strlen(devices)>4)
	{
		result = 1;//pow(2, 0);
		int mac = atoi(devices+4);
		if(mac < 4)
			mac = 1;
		else if(mac < 5)
			mac = 2;
		result = result | (mac<<4);
	}
	else if(strncasecmp(devices, "ipad", 4) == 0 && strlen(devices)>4)
	{
		result = 2;//pow(2, 1);
		int mac = atoi(devices+4);
		if(mac < 3)
			mac = 1;
		else if(mac < 4)
			mac = 2;
		result = result | (mac<<4);
	}
	return result;
}

unsigned char CalcDownloadCountLevel(uint32_t dc){
    if (dc >= 10000000)
        return 7;
    else if (dc >= 1000000)
        return 6;
    else if (dc >= 100000)
        return 5;
    else if (dc >= 10000)
        return 4;
    else if (dc >= 1000)
        return 3;
    else if (dc >= 100)
        return 2;
    else if (dc >= 10)
        return 1;
    else
        return 0;
}

int unpack_strlen(char *s, int n){
	int length = 0;
	char *p = s;
	while (*p && length < n) {
		length += 1;
		p += 1;
	}

	if (*p) {
		return -1;
	} else {
		return length;
	}
}


struct UnpackStream {
public:
	UnpackStream(char *buffer, int n)
		: m_buffer(buffer), m_n(n), m_unpack_index(0), m_error_flag(0)
	{
	
	}

	inline char *unpack_string()
	{
		char *p = m_buffer + m_unpack_index;
		int n = unpack_strlen(p, m_n-m_unpack_index);
		if (n < 0) {
			m_error_flag = 1;
			return NULL;
		} else {
			m_unpack_index += n+1;
			return p;
		}
	}

	inline char unpack_int8()
	{
		if (m_unpack_index+1 > m_n) {
			m_error_flag = 1;
			return 0;
		} else {
			char i = m_buffer[m_unpack_index];
			m_unpack_index += 1;
			return i;
		}
	}

	inline short unpack_int16()
	{
		if (m_unpack_index+sizeof(short) > m_n) {
			m_error_flag = 1;
			return 0;
		} else {
			short i = *(short *)(m_buffer+m_unpack_index);
			m_unpack_index += sizeof(short);
			return i;
		}
	}

	inline int unpack_int32()
	{
		if (m_unpack_index+sizeof(int) > m_n) {
			m_error_flag = 1;
			return 0;
		} else {
			int i = *(int *)(m_buffer+m_unpack_index);
			m_unpack_index += sizeof(int);
			return i;
		}
	}

	char *m_buffer;
	int m_n;
	int m_unpack_index;
	int m_error_flag;
};

static bool isControlCharacter(char ch)
{
	return ch > 0 && ch <= 0x1F;
}

static bool containsControlCharacter( const char* str ){
	while ( *str ) 
	{
		if ( isControlCharacter( *(str++) ) )
			return true;
	}
	return false;
}

struct PackStream {
public:
	PackStream(char *buffer, int n)
		: m_buffer(buffer), m_n(n), m_pack_index(0)
	{
	}

	inline int get_pack_index()
	{
		return m_pack_index;
	}

	inline void set_pack_index(int i)
	{
		m_pack_index = i;
	}

	inline void pack_int8(char i)
	{
		//Modified by zwj186: 2012 4-12
		if(m_pack_index + 1 < SWAPIN_BUFFER_SIZE * 10)
		{
			m_buffer[m_pack_index] = i;
			m_pack_index += 1;
		}
		else
			printf("PackStream out of Buffer at pack_int8!\n");
	}

	inline void pack_int16(short i)
	{
		//Modified by zwj186: 2012 4-12
		int isize = sizeof(short);
		if(m_pack_index + isize < SWAPIN_BUFFER_SIZE * 10)
		{
			(*(short *)(m_buffer+m_pack_index)) = i;
			m_pack_index += isize;
		}
		else
			printf("PackStream out of Buffer at pack_int16!\n");
	}

	inline void pack_float(float f)
	{
		//Modified by zwj186: 2012 4-12
		int isize = sizeof(float);
		if(m_pack_index + isize < SWAPIN_BUFFER_SIZE * 10)
		{
			(*(float *)(m_buffer+m_pack_index)) = f;
			m_pack_index += isize;
		}
		else
			printf("PackStream out of Buffer at pack_float!\n");
	}

	void pack_int32(int i)
	{
		//Modified by zwj186: 2012 4-12
		int isize = sizeof(int);
		if(m_pack_index + isize < SWAPIN_BUFFER_SIZE * 10)
		{
			(*(int *)(m_buffer+m_pack_index)) = i;
			m_pack_index += isize;
		}
		else
			printf("PackStream out of Buffer at pack_int32!\n");
	}
	
	void pack_string(const char *s, int n)
	{
		//Modified by zwj186: 2012 4-12
		if(m_pack_index + n+1 < SWAPIN_BUFFER_SIZE * 10)
		{
			memcpy(m_buffer+m_pack_index, s, n+1);
			m_pack_index += n+1;
		}
		else
			printf("PackStream out of Buffer at pack_string!\n");
	}
	
	void pack_string(const char *s)
	{
		int n = strlen(s);
		//Modified by zwj186: 2012 4-12
		pack_string(s, n);
	}
	
	void pack_jsonstr(const char *s, int n)
	{
		//Modified by zwj186: 2013 3-16 Cancel end of '\0' for Javascript
		if(m_pack_index + n < SWAPIN_BUFFER_SIZE * 10)
		{
			memcpy(m_buffer+m_pack_index, s, n);
			m_pack_index += n;
		}
		else
			printf("PackStream out of Buffer at pack_jsonstr!\n");
	}
	
	void pack_jsonstr(const char *s)
	{
		int n = strlen(s);
		//Modified by zwj186: 2012 4-12
		pack_jsonstr(s, n);
	}
	
	void pack_quotedjsonstr(const char *s, int n)
	{
		//Modified by zwj186: 2013 3-16 Cancel end of '\0' for Javascript
		if (strpbrk(s, "\"\\\b\f\n\r\t") == NULL && !containsControlCharacter( s ))
		{
			if(m_pack_index + n < SWAPIN_BUFFER_SIZE * 10)
			{
				memcpy(m_buffer+m_pack_index, s, n);
				m_pack_index += n;
			}
			else
				printf("PackStream out of Buffer at pack_string!\n");
			return;
		}

		int len = 0;
		for (const char* c=s; *c != 0 && m_pack_index + 6 < SWAPIN_BUFFER_SIZE * 10; ++c)
		{
			switch(*c)
			{
			case '\"':	
				len=strlen("\\\"");
				memcpy(m_buffer+m_pack_index,"\\\"",len);
				m_pack_index+=len;
				break;
			case '\\':
				len=strlen("\\\\");
				memcpy(m_buffer+m_pack_index,"\\\\",len);
				m_pack_index+=len;
				break;
			case '\b':
				len=strlen("\\b");
				memcpy(m_buffer+m_pack_index,"\\b",len);
				m_pack_index+=len;
				break;
			case '\f':
				len=strlen("\\f");
				memcpy(m_buffer+m_pack_index,"\\f",len);
				m_pack_index+=len;
				break;
			case '\n':
				len=strlen("\\n");
				memcpy(m_buffer+m_pack_index,"\\n",len);
				m_pack_index+=len;
				break;
			case '\r':
				len=strlen("\\r");
				memcpy(m_buffer+m_pack_index,"\\r",len);
				m_pack_index+=len;
				break;
			case '\t':
				len=strlen("\\t");
				memcpy(m_buffer+m_pack_index,"\\t",len);
				m_pack_index+=len;
				break;
			default:
				if (isControlCharacter( *c ))
				{
					char dest[9]= {0};
					sprintf(dest,"\\u%04x",static_cast<int>(*c));
					len=strlen(dest);
					memcpy(m_buffer+m_pack_index,dest,len);
					m_pack_index+=len;
				}
				else
				{
					*(m_buffer+m_pack_index)=*c;
					++m_pack_index;
				}
				break;
			}
		}
	}
	
	void pack_quotedjsonstr(const char *s)
	{
		int n = strlen(s);
		pack_quotedjsonstr(s, n);
	}

	void pack_buffer(char *s, int n)
	{
		//Modified by zwj186: 2012 4-12
		if(m_pack_index + n < SWAPIN_BUFFER_SIZE * 10)
		{
			memcpy(m_buffer+m_pack_index, s, n);
			m_pack_index += n;
		}
		else
			printf("PackStream out of Buffer at pack_buffer!\n");
	}

	char *m_buffer;
	int m_n;
	int m_pack_index;
};

enum {
	RESPONES_DATA_INT8			= 1,
	RESPONES_DATA_INT16			= 2,
	RESPONES_DATA_INT32			= 3,
	RESPONES_DATA_INT64			= 4,
	RESPONES_DATA_FLOAT			= 5,
	RESPONES_DATA_STRING		= 6,
};

enum {
	IH_REQ_SEARCH_APPS			 = 0xFE102060,
	IH_REQ_SEARCH_SHARE_APPS = 0xFE102061,
	IH_REQ_SEARCH_RINGS			 = 0xFE102022,
	IH_REQ_SEARCH_WALLPAPERS = 0xFE102063,
};

enum {
	IH_ACK_SEARCH_APPS			 = 0xFE102060,
	IH_ACK_SEARCH_SHARE_APPS = 0xFE102061,
	IH_ACK_SEARCH_RINGS			 = 0xFE102022,
	IH_ACK_SEARCH_WALLPAPERS = 0xFE102063,
};

int response_error(char *out_buffer, int out_n, int command, short error_code)
{
	PackStream ps(out_buffer, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(command);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	char cuint32[13] = {0};
	sprintf(cuint32, "%u", error_code);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr("}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	return n;
}

int response_null(char *out_buffer, int out_n, int command, short error_code){
	PackStream ps(out_buffer, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(command);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	char cuint32[13] = {0};
	sprintf(cuint32, "%u", error_code);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"pageCount\":0, \"searchCount\":0,\"content\":[]}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	return n;	
}

#define RESPONSE_NULL(command, error_code) \
	response_null(outbuff, out_n, command, error_code);

#define RESPONSE_ERROR(command, error_code) \
	response_error(outbuff, out_n, command, error_code);

#define UNPACK_INT8() \
	us.unpack_int8(); if (us.m_error_flag) { return -1; }
#define UNPACK_INT16() \
	us.unpack_int16(); if (us.m_error_flag) { return -1; }
#define UNPACK_INT32() \
	us.unpack_int32(); if (us.m_error_flag) { return -1; }
#define UNPACK_STRING() \
	us.unpack_string(); if (us.m_error_flag) { return -1; }

int AppOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe);
int IosOutput(sphinx_result* res, int result_count, char* outbuff, int out_n);
int RingOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe);
int WallpaperOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe);

struct _search_input{
	
	sphinx_int64_t phoneid;
	const char* tagids;
		
	uint32_t page;
	uint32_t page_count;
	sphinx_int64_t catid;
	sphinx_int64_t posup;
	
	sphinx_int64_t status;
	sphinx_int64_t display;
	float stars;

	int num_bundleid;
	int tagids_num;
	
	const char * search_key;
	const char * type;
	const char * sort;
	const char * order;
	const char * indexer;
	
	int order_num;
	
	const char * bundleid;

//ring:
	const char * song_dowm;
	
	_search_input(){
		phoneid = 0;//iphone & ipad
		page = 0;
		page_count = 10;
		catid = 0;
		posup = -1;
		tagids = NULL;
		status = 99;
		display = 1;
		order_num = 0;
		tagids_num = 0;
		
		stars = 0.0;
		num_bundleid = 0;
		search_key = NULL;
		type = NULL;
		sort = NULL;
		order = NULL;
		bundleid = NULL;
		song_dowm = NULL;
		indexer = NULL;
		//resType = IH_RES_SHAREPC; 	
 	}
	
};
int GetIutput(char* input, int len, int command, _search_input& ssp){
	char *requeststr = NULL;
	JsonValue requestroot;
	JsonBase reader;
	//UnpackStream us(data, tcpdata->len-8);
	UnpackStream us(input, len);
	requeststr = UNPACK_STRING();
	
	save_log(INFO, "%d: %s\n", command, requeststr);
	
	if(!reader.parse(requeststr, requestroot)){
		save_log(INFO, "invalid request:%s\n", requeststr);
		return -1;
	}
	
	const JsonValue *value;
	
	value = requestroot["phone_typeid"];
	if(!value->isNull() && value->isInt())
		ssp.phoneid = value->asInt();
	
	value = requestroot["page"];
	if(!value->isNull() && value->isInt())
		ssp.page = value->asInt();
		
	value = requestroot["limit"];
	if(!value->isNull() && value->isInt())
		ssp.page_count = value->asInt();
	
	if(ssp.page_count > 30 || ssp.page_count < 1)
		ssp.page_count = 10;
		
	value = requestroot["posup"];
	if(!value->isNull() && value->isInt())
		ssp.posup = value->asInt();
	
	value = requestroot["tagids"];
	if(!value->isNull() && value->isString())
		ssp.tagids = value->asString();
	value = requestroot["tagids_num"];
	if(!value->isNull() && value->isInt())
		ssp.tagids_num = value->asInt();
		
	value = requestroot["catid"];
	if(!value->isNull() && value->isInt())
		ssp.catid = value->asInt();

	value = requestroot["status"];
	if(!value->isNull() && value->isInt())
		ssp.status = value->asInt();
	
	value = requestroot["display"];
	if(!value->isNull() && value->isInt())
		ssp.display = value->asInt();
		
	value = requestroot["order_num"];
	if(!value->isNull() && value->isInt())
		ssp.order_num = value->asInt();
		
	value = requestroot["stars"];
	if(!value->isNull() && value->isInt())
		//char tmpstars[8] = {0};
		ssp.stars = (float)(value->asInt());
	if (ssp.stars < 0 || ssp.stars > 5){
		save_log(INFO, "invalid stars : %lf\n", ssp.stars);
		return -1;
	}
										
	value = requestroot["keyword"];
	if(!value->isNull() && value->isString())
		ssp.search_key = value->asString();
	value = requestroot["type"];
	if(!value->isNull() && value->isString())
		ssp.type = value->asString();
	value = requestroot["sort"];
	if(!value->isNull() && value->isString())
		ssp.sort = value->asString();
	value = requestroot["order"];
	if(!value->isNull() && value->isString())
		ssp.order = value->asString();
		
	value = requestroot["indexer"];
	if(!value->isNull() && value->isString())
		ssp.indexer = value->asString();
								
	value = requestroot["BundleId"];
	if(!value->isNull() && value->isString())
		ssp.bundleid = value->asString();
	value = requestroot["BundleIdNum"];
	if(!value->isNull() && value->isInt())
		ssp.num_bundleid = value->asInt();
		
		
		return 0;
}
/**********************************************************************/
/*
# packet
	uint32		len
	uint32		commmand
	char[]		data
	
# Put your return packet in out_buff, and return len. return 0 for send nothing
 */
int HandlePacket(uint32_t client_ip, char * packet, int packet_len, char * outbuff){
	int res_len = 0, out_n = SWAPIN_BUFFER_SIZE * 10;

	packet[packet_len] = '\0';
	CLIENTPACKETTCP* tcpdata = (CLIENTPACKETTCP*)packet;	
	char * data = tcpdata->data;
	
	if(tcpdata->len-8 > 300){
		save_log(INFO, "data too long : %d\n", tcpdata->len);
		return RESPONSE_ERROR(tcpdata->command, IH_E_INVALID_REQUEST);
	}
	
	//get result-set
	sphinx_client * client = NULL;
	client = sphinx_create ( SPH_TRUE );
	if ( !client ){
		save_log(INFO, "sphinx_create fail!\n");
		return RESPONSE_NULL(tcpdata->command, IH_E_OK);
	}	
	sphinx_result * res = NULL;
	
	
	/////////////////////////
	sphinx_set_server(client, "127.0.0.1", 9312);
	/////////////////////////
	
	
	
	switch (tcpdata->command){
		//越狱app
		case 261020:{
			++g_search_app_count;
			
			_search_input ssp;
			if (GetIutput(data, tcpdata->len-8, tcpdata->command, ssp) < 0)
				return RESPONSE_ERROR(IH_ACK_SEARCH_APPS, IH_E_INVALID_REQUEST);	
				
			if(ssp.search_key == NULL ){
				save_log(INFO, "no keyword, search what app? !\n");				
				return RESPONSE_NULL(IH_ACK_SEARCH_SHARE_APPS, IH_E_OK);
			}

			int key_len = strlen(ssp.search_key);
		
			//convert to utf16

			unsigned short wkey[100];
			int wkey_len = 10;
			Utf8ToUtf16((unsigned char*)ssp.search_key, key_len, wkey, wkey_len);
			wkey[wkey_len] = 0;		

			if (wkey_len > 20){
				save_log(INFO, "keyword too long:%d\n", wkey_len);
				return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);
			}
		
			//char *search_word = strtrim(search_key);
		/*		save_log(INFO, "search_word: %s\n", search_word);
			_RB_tree_node<char *, char *> *word_node = g_badword_map->find(search_word);
			if(word_node != g_badword_map->nil())
			{				
				save_log(INFO, "find badword return %d\n", n);
				return n;
			}
				
			//added for SearchKeyword
			if(ssp.page == 0){
				SearchKeyword *searchkeywords = (SearchKeyword *)g_application_mem_pool->Alloc(sizeof(SearchKeyword));
				memset(searchkeywords, 0, sizeof(SearchKeyword));
				
				//searchkeywords->keyword = mem_pool_dup_str(g_application_mem_pool, search_word);
				//searchkeywords->uuid = mem_pool_dup_str(g_application_mem_pool, uuid);
				searchkeywords->from_type = 2;
				//searchkeywords->phone_typeid = device_family_type;
				searchkeywords->searchtime = time(NULL);
				searchkeywords->res_type = 1;
				searchkeywords->ip = client_ip;
				g_dblqueue_search_keywords.AddTail(searchkeywords);
			}
*/			
			//result
			int result_count = 0;
			SEARCH_LIST * result_list = NULL;
			
			//check cache
			char cache_key[128] = {0};
			
			*(uint32_t*)cache_key = ssp.phoneid;
			//*(uint32_t*)&cache_key[4] = ssp.tagids;
			*(uint32_t*)&cache_key[8] = ssp.catid;
			*(uint32_t*)&cache_key[12] = (uint32_t)ssp.stars;
			*(uint32_t*)&cache_key[16] = ssp.posup;
			
			int len1 = 0;
			int len2 = 0;
			if (NULL != ssp.type){
				memcpy(&cache_key[20], ssp.type, 4);
			}
			if (NULL != ssp.order){
				memcpy(&cache_key[24], ssp.order, strlen(ssp.order));
				len1 = strlen(ssp.order);
			}
			if (NULL != ssp.tagids){
				memcpy(&cache_key[24 + len1], ssp.tagids, strlen(ssp.tagids));
				len2 = strlen(ssp.tagids);
			}
			memcpy(&cache_key[24 + len1 + len2], wkey, wkey_len*sizeof(unsigned short));
		
			g_data_locker.Lock();
				
			bool isCahe = false;
			_RB_tree_node<char*, SEARCH_CACHE*> * cache_node = g_apple_app_search_cache_tree->find(cache_key);
			//if (cache_node == g_apple_app_search_cache_tree->nil())
			{
				sphinx_set_ranking_mode(client, SPH_RANK_EXPR, "downloads");
				//sphinx_set_ranking_mode(client, SPH_RANK_EXPR, "sum(lcs*user_weight)*pow(2,48) + sortlevel*pow(2,40) + downloads");
				//sphinx_set_ranking_mode(client, SPH_RANK_SPH04, NULL);
				if (ssp.tagids_num > 0 && ssp.tagids != NULL){
					sphinx_int64_t t[10] = {0};
					char *pt = (char*) ssp.tagids;
					char* pt1 = pt;
					for (int i = 0; i < ssp.tagids_num; ++i){
						if (ssp.tagids_num - 1 == i){
							t[i] = atol(pt);
							break;
						}
						char tmp[16] = {0};
						pt1 = strchr(pt1, ',');
						if (NULL == pt1)
							break;
						
						memcpy(tmp, pt, pt1 - pt);
						pt = ++pt1;
						t[i] = atol(tmp);
						
					}
					//save_log(INFO, "ssp.tag:%d %d %d %d\n", ssp.tagids_num, t[0], t[1], t[2]);
					sphinx_add_filter(client, "tagids", ssp.tagids_num, t, SPH_FALSE);
				}
				if (ssp.phoneid > 0)
					sphinx_add_filter(client, "phone_typeid", 1, &(ssp.phoneid), SPH_FALSE);
				if (ssp.catid > 0)
					sphinx_add_filter(client, "catid", 1, &(ssp.catid), SPH_FALSE);
				
				if (ssp.stars > 0.0){
					sphinx_add_filter_float_range(client, "stars", (float)((ssp.stars - 1.0)*2.0), 10.0, SPH_FALSE);
				}
				if (NULL != ssp.type && strlen(ssp.type) > 0){
					sphinx_int64_t type = CRC32((unsigned char*)ssp.type, strlen(ssp.type));
					sphinx_add_filter(client, "crc_type", 1, &type, SPH_FALSE);
				}
				if (ssp.posup > 0){
					sphinx_add_filter(client, "posup", 1, &(ssp.posup), SPH_FALSE);
				}
				if (NULL != ssp.order){
					save_log(INFO, "app: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				else{
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, "@weight DESC updatetime DESC");	
				}
				//sphinx_add_filter(client, "status", 1, &(ssp.status), SPH_FALSE);
				//sphinx_add_filter(client, "display", 1, &(ssp.display), SPH_FALSE);
				//sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
			const char * field_names[2];
			int field_weights[2];
		
			field_names[0] = "title";
			field_names[1] = "keywords";
			field_weights[0] = 2;
			field_weights[1] = 0;
			sphinx_set_field_weights ( client, 2, field_names, field_weights );
			
				save_log(INFO, "app keyword:%s\n", ssp.search_key);
				res = sphinx_query (client, ssp.search_key, "iapp", NULL);
				if (NULL == res){
					g_data_locker.Unlock();
					save_log(INFO, "app NULL == res. error:%s!\n", sphinx_error(client));
					g_data_locker.Unlock();
					return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "app num_matches:%d  error:%s!\n", res->num_matches, sphinx_error(client));
					return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
				}

				//add to cache
				SEARCH_LIST *temp_sl = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
				result_list = temp_sl;
				for (int i=0; i<res->num_matches - 1; ++i){
					temp_sl->index = sphinx_get_id (res, i);
					temp_sl->next = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
					temp_sl = temp_sl->next;
				}//for
				temp_sl->index = sphinx_get_id (res, res->num_matches - 1);
				temp_sl->next = NULL;
				
				SEARCH_CACHE * sc = (SEARCH_CACHE*)g_db_mem_pool->Alloc(sizeof(SEARCH_CACHE));
				memcpy(sc->key, cache_key, sizeof(cache_key));
				sc->count = res->num_matches;
				sc->sl = result_list;
				sc->last_visit_time = TIME_NOW;
				
				g_apple_app_search_cache_tree->insert(sc->key, sc);
				
				save_log(INFO, "app:search:%s, result: %d\n", ssp.search_key, res->num_matches);
				
				sc->search_type = 1;
				g_search_cache_lru.AddTail(sc);
				
				result_count = res->num_matches;
			}
/*			
			else{
				cache_node->value()->last_visit_time = TIME_NOW;
				result_count = cache_node->value()->count;
				result_list = cache_node->value()->sl;
				g_search_cache_lru.MoveToTail(cache_node->value());
				save_log(INFO, "result from cache ok, result_count:%d\n", result_count);
				
				//SEARCH_LIST *head = result_list;
				for (int i = 0; i < ssp.page*ssp.page_count && i < result_count; ++i){
					result_list = result_list->next;
				}
				
				if (NULL == result_list || result_count - ssp.page*ssp.page_count <=0){
					save_log(INFO, "app, get null list from cache!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
				}
				
				sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
				
				sphinx_int64_t filte_id[30];
				int num = 0;
				for (int i = 0; i < ssp.page_count && i < (result_count - ssp.page*ssp.page_count); ++i){
					filte_id[i] = result_list->index;
					++num;
					//sphinx_set_id_range(client, result_list->index, result_list->index);
					//sphinx_add_query(client, "", "iapp", NULL);
					result_list = result_list->next;
				}
				sphinx_add_filter(client, "filte_id", num, filte_id, SPH_FALSE);

				if (NULL != ssp.order){
					save_log(INFO, "app: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				
				res = sphinx_query(client, "", "iapp", NULL);
				//res = sphinx_run_queries(client);
				if (NULL == res){
					save_log(INFO, "app no cache!\n");
					g_data_locker.Unlock();
					return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "app num_matches:%d\n", res->num_matches);
					return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
				}
				
				isCahe = true;

			}//else
*/		
			//if (!isCahe)
				res_len = AppOutput(res, res->num_matches, result_count, ssp.page, ssp.page_count, outbuff, out_n, isCahe);
			//else
			//	res_len = AssembleOutput(res, sphinx_get_num_results(client), 0, 10, outbuff, out_n, isCahe);

			g_data_locker.Unlock();
			
			break;
		}
		//正版app
		case 261021:{
			++g_search_share_app_count;
			
			_search_input ssp;
			if (GetIutput(data, tcpdata->len-8, tcpdata->command, ssp) < 0)
				return RESPONSE_ERROR(IH_ACK_SEARCH_SHARE_APPS, IH_E_INVALID_REQUEST);////////////////////////////////////

			if (ssp.num_bundleid < 1 || strlen(ssp.bundleid) < 1)
				return RESPONSE_NULL(IH_ACK_SEARCH_SHARE_APPS, IH_E_INVALID_REQUEST);////////////////////////////////////
			
			sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);

			sphinx_int64_t cbundleid[30];
			char* h = (char*)ssp.bundleid;
			for (int i = 0; i < ssp.num_bundleid; ++i){
				char *p = strchr(h, ',');
				if (NULL != p && ssp.num_bundleid - 1!= i){
					char tmp[64] = {0};
					memcpy(tmp, h, p - h);
					cbundleid[i] = CRC32((unsigned char*)tmp, strlen((const char*)tmp));
					h = p + 1;
				}else if (strlen(h) > 0 && ssp.num_bundleid - 1 == i){
					cbundleid[i] = CRC32((unsigned char*)h, strlen((const char*)h));
					break;					
				}
			}//for
			sphinx_add_filter(client, "crc_bundleid", ssp.num_bundleid, cbundleid, SPH_FALSE);
			
			if (ssp.order_num > 0 && NULL != ssp.order){
				save_log(INFO, "app: order:%s\n", ssp.order);
				sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
			}

			res = sphinx_query (client, "", "iosapp", NULL);
			if (NULL == res){
				save_log(INFO, "no iosapp..!\n");
				return RESPONSE_NULL(IH_ACK_SEARCH_SHARE_APPS, IH_E_OK);///////////////////////
			}

			res_len = IosOutput(res, res->num_matches, outbuff, out_n);		
			break;
		}
		//铃声
		case 261022:{
			++g_search_ring_count;
			
			_search_input ssp;
			if (GetIutput(data, tcpdata->len-8, tcpdata->command, ssp) < 0)
				return RESPONSE_ERROR(IH_ACK_SEARCH_RINGS, IH_E_INVALID_REQUEST);//////////////
				
			if(ssp.search_key == NULL){
				save_log(INFO, "no keyword, search what ring? !\n");				
				return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);
			}

			int key_len = strlen(ssp.search_key);	
			//convert to utf16
			unsigned short wkey[100];
			int wkey_len = 10;
			Utf8ToUtf16((unsigned char*)ssp.search_key, key_len, wkey, wkey_len);
			wkey[wkey_len] = 0;		

			if (wkey_len > 20){
				save_log(INFO, "ring: keyword too long:%d\n", wkey_len);
				return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);
			}
			
			//result
			int result_count = 0;
			SEARCH_LIST * result_list = NULL;
			
			//check cache
			char cache_key[128] = {0};
			
			int len1 = 0;
			int len2 = 0;
			*(uint32_t*)cache_key = ssp.catid;
			*(uint32_t*)&cache_key[4] = ssp.posup;		
			if (NULL != ssp.type){
				memcpy(&cache_key[8], ssp.type, strlen(ssp.type));
				len1 = strlen(ssp.type);
			}
			if (NULL != ssp.order){
				memcpy(&cache_key[8 + len1], ssp.order, strlen(ssp.order));
				len2 = strlen(ssp.order);
			}
			memcpy(&cache_key[8 + len1 + len2], wkey, wkey_len*sizeof(unsigned short));
		
			g_data_locker.Lock();
				
			bool isCahe = false;
			_RB_tree_node<char*, SEARCH_CACHE*> * cache_node = g_ring_search_cache_tree->find(cache_key);
			if (cache_node == g_ring_search_cache_tree->nil())
			{
				//sphinx_set_ranking_mode(client, SPH_RANK_EXPR, "(sum(lcs*user_weight)*1000 + bm25)");

				if (ssp.catid > 0){
					//save_log(INFO, "RING:catid:%d\n", ssp.catid);
					sphinx_add_filter(client, "catid", 1, &(ssp.catid), SPH_FALSE);
				}
				if (NULL != ssp.type && strlen(ssp.type) > 0){
					sphinx_int64_t type = CRC32((unsigned char*)ssp.type, strlen(ssp.type));
					//save_log(INFO, "RING:type:%d\n", type);
					sphinx_add_filter(client, "crc_type", 1, &type, SPH_FALSE);
				}
				if (ssp.posup > 0){
					sphinx_add_filter(client, "posup", 1, &(ssp.posup), SPH_FALSE);
				}
				if (NULL != ssp.order){
					save_log(INFO, "ring: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				
				res = sphinx_query (client, ssp.search_key, "iring", NULL);
				
				if (NULL == res){
					g_data_locker.Unlock();
					save_log(INFO, "ring no res!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);///////////////////fix
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "ring: num_matches:%d\n", res->num_matches);
					return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);	
				}
/////////////////
//////////////////

				//add to cache
				SEARCH_LIST *temp_sl = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
				result_list = temp_sl;
				for (int i=0; i<res->num_matches - 1; ++i){
					temp_sl->index = sphinx_get_id (res, i);
					temp_sl->next = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
					temp_sl = temp_sl->next;
				}//for
				temp_sl->index = sphinx_get_id (res, res->num_matches - 1);
				temp_sl->next = NULL;
				
				SEARCH_CACHE * sc = (SEARCH_CACHE*)g_db_mem_pool->Alloc(sizeof(SEARCH_CACHE));
				memcpy(sc->key, cache_key, sizeof(cache_key));
				sc->count = res->num_matches;
				sc->sl = result_list;
				sc->last_visit_time = TIME_NOW;
				
				g_ring_search_cache_tree->insert(sc->key, sc);
				
				save_log(INFO, "ring:search:%s, result: %d\n", ssp.search_key, res->num_matches);

				g_search_cache_lru.AddTail(sc);
				
				result_count = res->num_matches;
			}
			else{
				cache_node->value()->last_visit_time = TIME_NOW;
				result_count = cache_node->value()->count;
				result_list = cache_node->value()->sl;
				g_search_cache_lru.MoveToTail(cache_node->value());
				save_log(INFO, "ring:result from cache ok, result_count:%d\n", result_count);
				
				for (int i = 0; i < ssp.page*ssp.page_count && i < result_count; ++i){
					result_list = result_list->next;
				}
				result_count -= ssp.page*ssp.page_count;
				
				if (NULL == result_list || result_count - ssp.page*ssp.page_count <= 0){
					g_data_locker.Unlock();
					save_log(INFO, "ring:get null list from cache!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);	
				}
				
				sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
				
				sphinx_int64_t filte_id[30];
				int num = 0;
				for (int i = 0; i < ssp.page_count && i < (result_count - ssp.page*ssp.page_count); ++i){
					filte_id[i] = result_list->index;
					++num;
					result_list = result_list->next;
				}
				sphinx_add_filter(client, "filte_id", num, filte_id, SPH_FALSE);
				
				if (NULL != ssp.order){
					save_log(INFO, "ring: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				
				res = sphinx_query(client, "", "iring", NULL);
				
				if (NULL == res){
					g_data_locker.Unlock();
					save_log(INFO, "ring:get nothing from cache!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);	
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "ring num_matches:%d\n", res->num_matches);
					return RESPONSE_NULL(IH_ACK_SEARCH_RINGS, IH_E_OK);	
				}
				
				isCahe = true;

			}//else
		
			res_len = RingOutput(res, res->num_matches, result_count, ssp.page, ssp.page_count, outbuff, out_n, isCahe);

			g_data_locker.Unlock();
			
			break;
		}
		//壁纸
		case 261023:{
			++g_search_wallpaper_count;
			
			_search_input ssp;
			if (GetIutput(data, tcpdata->len-8, tcpdata->command, ssp) < 0)
				return RESPONSE_ERROR(IH_ACK_SEARCH_WALLPAPERS, IH_E_INVALID_REQUEST);//////////////
				
			if(ssp.search_key == NULL){
				save_log(INFO, "no keyword, search what wallpaper? !\n");				
				return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);
			}

			int key_len = strlen(ssp.search_key);	
			//convert to utf16
			unsigned short wkey[100];
			int wkey_len = 10;
			Utf8ToUtf16((unsigned char*)ssp.search_key, key_len, wkey, wkey_len);
			wkey[wkey_len] = 0;		

			if (wkey_len > 20){
				save_log(INFO, "wallpaper: keyword too long:%d\n", wkey_len);
				return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);
			}
			
			//result
			int result_count = 0;
			SEARCH_LIST * result_list = NULL;
			
			//check cache
			char cache_key[128] = {0};
			
			int orlen = 0;
			*(uint32_t*)cache_key = ssp.catid;
			*(uint32_t*)&cache_key[4] = ssp.posup;
			*(uint32_t*)&cache_key[8] = ssp.phoneid;
			if (NULL != ssp.order){
				orlen = strlen(ssp.order);
				memcpy(&cache_key[12], ssp.order, strlen(ssp.order));
			}
			memcpy(&cache_key[12 + orlen], wkey, wkey_len*sizeof(unsigned short));
		
			g_data_locker.Lock();
				
			bool isCahe = false;
			_RB_tree_node<char*, SEARCH_CACHE*> * cache_node = g_wallpaper_search_cache_tree->find(cache_key);
			if (cache_node == g_wallpaper_search_cache_tree->nil())
			{
				//sphinx_set_ranking_mode(client, SPH_RANK_EXPR, "(sum(lcs*user_weight)*1000 + bm25)");

				if (ssp.catid > 0){
					sphinx_add_filter(client, "catid", 1, &(ssp.catid), SPH_FALSE);
				}
				if ( ssp.phoneid > 0){
					sphinx_add_filter(client, "phone_typeid", 1, &ssp.phoneid, SPH_FALSE);
				}
				if (ssp.posup > 0){
					sphinx_add_filter(client, "posup", 1, &(ssp.posup), SPH_FALSE);
				}
				if (NULL != ssp.order){
					save_log(INFO, "wallpaper: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				
				//sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
				
				save_log(INFO, "wallkeyword:%s\n", ssp.search_key);
				res = sphinx_query (client, ssp.search_key, "iwallpaper", NULL);
																										 
				if (NULL == res){
					g_data_locker.Unlock();
					save_log(INFO, "wallpaper no res!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);///////////////////fix
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "wallpaper num_matches:%d\n", res->num_matches);
					return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);	
				}

				//add to cache
				SEARCH_LIST *temp_sl = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
				result_list = temp_sl;
				for (int i=0; i<res->num_matches - 1; ++i){
					temp_sl->index = sphinx_get_id (res, i);
					temp_sl->next = (SEARCH_LIST*)g_db_mem_pool->Alloc(sizeof(SEARCH_LIST));
					temp_sl = temp_sl->next;
				}//for
				temp_sl->index = sphinx_get_id (res, res->num_matches - 1);
				temp_sl->next = NULL;
				
				SEARCH_CACHE * sc = (SEARCH_CACHE*)g_db_mem_pool->Alloc(sizeof(SEARCH_CACHE));
				memcpy(sc->key, cache_key, sizeof(cache_key));
				sc->count = res->num_matches;
				sc->sl = result_list;
				sc->last_visit_time = TIME_NOW;
				
				g_wallpaper_search_cache_tree->insert(sc->key, sc);
				
				save_log(INFO, "wallpaper:search:%s, result: %d\n", ssp.search_key, res->num_matches);

				g_search_cache_lru.AddTail(sc);

				result_count = res->num_matches;
			}
			else{
				cache_node->value()->last_visit_time = TIME_NOW;
				result_count = cache_node->value()->count;
				result_list = cache_node->value()->sl;
				g_search_cache_lru.MoveToTail(cache_node->value());
				save_log(INFO, "wallpaper:result from cache ok, result_count:%d\n", result_count);
				
				for (int i = 0; i < ssp.page*ssp.page_count && i < result_count; ++i){
					result_list = result_list->next;
				}
				result_count -= ssp.page*ssp.page_count;
				
				if (NULL == result_list || result_count - ssp.page*ssp.page_count <= 0){
					g_data_locker.Unlock();
					save_log(INFO, "wallpaper:get null list from cache!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);	
				}
				
				sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
				
				sphinx_int64_t filte_id[30];
				int num = 0;
				for (int i = 0; i < ssp.page_count && i < (result_count - ssp.page*ssp.page_count); ++i){
					filte_id[i] = result_list->index;
					++num;
					result_list = result_list->next;
				}
				sphinx_add_filter(client, "filte_id", num, filte_id, SPH_FALSE);
				
				if (NULL != ssp.order){
					save_log(INFO, "wallpaper: order:%s\n", ssp.order);
					sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
				}
				
				res = sphinx_query(client, "", "iwallpaper", NULL);
				if (NULL == res){
					g_data_locker.Unlock();
					save_log(INFO, "wallpaper:get nothing from cache!\n");
					return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);	
				}else if (0 == res->num_matches){
					g_data_locker.Unlock();
					save_log(INFO, "wallpaper num_matches:%d\n", res->num_matches);
					return RESPONSE_NULL(IH_ACK_SEARCH_WALLPAPERS, IH_E_OK);	
				}
				
				isCahe = true;

			}//else
		
			res_len = WallpaperOutput(res, res->num_matches, result_count, ssp.page, ssp.page_count, outbuff, out_n, isCahe);

			g_data_locker.Unlock();
			
			break;
		}
		//纯排序，无关键字
		case 261024:{
			//++g_search_app_count;
			
			_search_input ssp;
			if (GetIutput(data, tcpdata->len-8, tcpdata->command, ssp) < 0)
				return RESPONSE_ERROR(IH_ACK_SEARCH_APPS, IH_E_INVALID_REQUEST);	
				
			if(ssp.indexer == NULL ){
				save_log(INFO, "no indexer, search what? !\n");				
				return RESPONSE_NULL(IH_ACK_SEARCH_SHARE_APPS, IH_E_OK);
			}	
		
			if (ssp.tagids_num > 0 && ssp.tagids != NULL){
				sphinx_int64_t t[10] = {0};
				char *pt = (char*) ssp.tagids;
				char* pt1 = pt;
				for (int i = 0; i < ssp.tagids_num; ++i){
					if (ssp.tagids_num - 1 == i){
						t[i] = atol(pt);
						break;
					}
					char tmp[16] = {0};
					pt1 = strchr(pt1, ',');
					if (NULL == pt1)
						break;
					
					memcpy(tmp, pt, pt1 - pt);
					pt = ++pt1;
					t[i] = atol(tmp);
					
				}
				//save_log(INFO, "ssp.tag:%d %d %d %d\n", ssp.tagids_num, t[0], t[1], t[2]);
				sphinx_add_filter(client, "tagids", ssp.tagids_num, t, SPH_FALSE);
			}
			if (ssp.phoneid > 0)
				sphinx_add_filter(client, "phone_typeid", 1, &(ssp.phoneid), SPH_FALSE);
			if (ssp.catid > 0)
				sphinx_add_filter(client, "catid", 1, &(ssp.catid), SPH_FALSE);
			
			if (ssp.stars > 0.0){
				sphinx_add_filter_float_range(client, "stars", (float)((ssp.stars - 1.0)*2.0), 10.0, SPH_FALSE);
			}
			if (NULL != ssp.type && strlen(ssp.type) > 0){
				sphinx_int64_t type = CRC32((unsigned char*)ssp.type, strlen(ssp.type));
				sphinx_add_filter(client, "crc_type", 1, &type, SPH_FALSE);
			}
			if (ssp.posup > 0){
				sphinx_add_filter(client, "posup", 1, &(ssp.posup), SPH_FALSE);
			}
			if (NULL != ssp.order){
				//save_log(INFO, "app: order:%s\n", ssp.order);
				sphinx_set_sort_mode(client, SPH_SORT_EXTENDED, ssp.order);
			}

			sphinx_set_match_mode(client, SPH_MATCH_FULLSCAN);
			
			save_log(INFO, "indexer:%s\n", ssp.indexer);
			res = sphinx_query (client, NULL, ssp.indexer, NULL);
			if (NULL == res){
				g_data_locker.Unlock();
				save_log(INFO, "indexer NULL == res. error:%s!\n", sphinx_error(client));
				g_data_locker.Unlock();
				return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
			}else if (0 == res->num_matches){
				g_data_locker.Unlock();
				save_log(INFO, "indexer num_matches:%d\n", res->num_matches);
				return RESPONSE_NULL(IH_ACK_SEARCH_APPS, IH_E_OK);	
			}
			
			if (0 == strcmp(ssp.indexer, "iapp"))
				res_len = AppOutput(res, res->num_matches, res->num_matches, ssp.page, ssp.page_count, outbuff, out_n, false);
			else if (0 == strcmp(ssp.indexer, "iring"))
				res_len = RingOutput(res, res->num_matches, res->num_matches, ssp.page, ssp.page_count, outbuff, out_n, false);
			else if (0 == strcmp(ssp.indexer, "iwallpaper"))
				res_len = WallpaperOutput(res, res->num_matches, res->num_matches, ssp.page, ssp.page_count, outbuff, out_n, false);
			else if (0 == strcmp(ssp.indexer, "iosapp"))
				res_len = IosOutput(res, res->num_matches, outbuff, out_n);
				
			break;
		}
		//清缓存
		case 261025:{
			g_data_locker.Lock();
			
			save_log(INFO, "begin to clear the cache!\n");
			if (g_db_mem_pool){
				delete g_db_mem_pool;
				g_db_mem_pool = NULL;
			}
			
			g_db_mem_pool = new CMemPool();
			g_db_mem_pool->AddUnit(32, 1024);
			g_db_mem_pool->AddUnit(64, 1024);
			g_db_mem_pool->AddUnit(128, 1024);
			g_db_mem_pool->AddUnit(256, 1024);
			g_db_mem_pool->AddUnit(512, 1024);
			g_db_mem_pool->AddUnit(sizeof(SEARCH_LIST), 1024);
			g_db_mem_pool->AddUnit(sizeof(SEARCH_CACHE), 1024);
			if (g_apple_app_search_cache_tree){
				delete g_apple_app_search_cache_tree;
				g_apple_app_search_cache_tree = NULL;
			}
			if (g_wallpaper_search_cache_tree){
				delete g_wallpaper_search_cache_tree;
				g_wallpaper_search_cache_tree = NULL;
			}
			if (g_ring_search_cache_tree){
				delete g_ring_search_cache_tree;
				g_ring_search_cache_tree = NULL;
			}
			
			g_apple_app_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
			g_wallpaper_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
			g_ring_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
						
			g_search_cache_lru.RemoveAll();

			g_data_locker.Unlock();	
		}
		default:
			break;
	}
	
	sphinx_destroy(client);
	return res_len;
}

int WallpaperOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe){

	int result_pages = (result_count / page_count) + (result_count % page_count == 0 ? 0 : 1);
	
	char cuint32[13] = {0};
	
	PackStream ps(outbuff, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(IH_ACK_SEARCH_WALLPAPERS);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	sprintf(cuint32, "%u", IH_E_OK);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"pageCount\":");
	sprintf(cuint32, "%u", result_pages);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"searchCount\":");
	sprintf(cuint32, "%u", total);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"content\":[");
	
	//item repeat
	if(page >= result_pages)
		page = 0;
	
	int item_count = 0;
	
	int init = 0;
	if (!isCahe){
		init = page*page_count;
	}
	for (int r=init; r<result_count; ++r){
		if(r == init)
			ps.pack_jsonstr("{");
		else
			ps.pack_jsonstr(",{");

//v9_bizhi:
		ps.pack_jsonstr("\"Id\":");
		sprintf(cuint32, "%u", sphinx_get_id (res, r));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"catId\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 0));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"downloads\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 3));
		ps.pack_jsonstr(cuint32);
						
		ps.pack_jsonstr(",\"title\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 4));
		
		ps.pack_jsonstr("\",\"download_url\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 5));

		ps.pack_jsonstr("\",\"pc_preview_url\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 6));
		
		ps.pack_jsonstr("\",\"pc_thumbnail_url\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 7));

		ps.pack_jsonstr("\",\"bizhi_wh\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 8));

		ps.pack_jsonstr("\",\"updatetime\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 9));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr("}");
		++item_count;
		if(item_count >= page_count)
			break;

	}//for
	
	ps.pack_jsonstr("]}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	
	//save_log(INFO, "buflen: %d content: %s\n", n, outbuff+11);
	return n;
}

int RingOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe){

	int result_pages = (result_count / page_count) + (result_count % page_count == 0 ? 0 : 1);
	
	char cuint32[13] = {0};
	
	PackStream ps(outbuff, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(IH_ACK_SEARCH_RINGS);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	sprintf(cuint32, "%u", IH_E_OK);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"pageCount\":");
	sprintf(cuint32, "%u", result_pages);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"searchCount\":");
	sprintf(cuint32, "%u", total);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"content\":[");
	
	//item repeat
	if(page >= result_pages)
		page = 0;
	
	int item_count = 0;
	
	int init = 0;
	if (!isCahe){
		init = page*page_count;
	}
	for (int r=init; r<result_count; ++r){
		if(r == init)
			ps.pack_jsonstr("{");
		else
			ps.pack_jsonstr(",{");

//v9_lingsheng:
		ps.pack_jsonstr("\"Id\":");
		sprintf(cuint32, "%u", sphinx_get_id (res, r));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"catId\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 0));
		ps.pack_jsonstr(cuint32);
				
		ps.pack_jsonstr(",\"title\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 3));
		
		ps.pack_jsonstr("\",\"type\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 1));

		ps.pack_jsonstr("\",\"song_down\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 4));

		ps.pack_jsonstr("\",\"updatetime\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 5));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr("}");
		++item_count;
		if(item_count >= page_count)
			break;

	}//for
	
	ps.pack_jsonstr("]}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	
	//save_log(INFO, "buflen: %d content: %s\n", n, outbuff+11);
	return n;
}

int IosOutput(sphinx_result* res, int result_count, char* outbuff, int out_n){

	char cuint32[13] = {0};
	
	PackStream ps(outbuff, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(IH_ACK_SEARCH_SHARE_APPS);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	sprintf(cuint32, "%u", IH_E_OK);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"searchCount\":");
	sprintf(cuint32, "%u", result_count);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"content\":[");
	
	for (int r = 0; r < result_count; ++r){
		if(r == 0)
			ps.pack_jsonstr("{");
		else
			ps.pack_jsonstr(",{");
			
//appstore_buydata:
		ps.pack_jsonstr("\"itemid\":");
		sprintf(cuint32, "%u", sphinx_get_id (res, r));
		ps.pack_jsonstr(cuint32);
				
		ps.pack_jsonstr(",\"price\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 4));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr(",\"BundleId\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 0));
//		sprintf(cuint32, "%u", sphinx_get_int(res, r, 0));
//		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr("\",\"version\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 1));
		
		ps.pack_jsonstr("\",\"filepath\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 2));
	
		ps.pack_jsonstr("\",\"last_audit\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 3));
		ps.pack_jsonstr(cuint32);

//appstore_share_hits:				
		ps.pack_jsonstr(",\"downloads\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 5));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr("}");					
	}//for

	ps.pack_jsonstr("]}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	
	//save_log(INFO, "buflen: %d content: %s\n", n, outbuff+11);
	return n;
}

int AppOutput(sphinx_result* res, int result_count, int total, int page, int page_count, char* outbuff, int out_n, bool isCahe){
	/*
		uint32		len;
		uint32		command;
		char*		uuid;
		-repeat{
			char * bundleid
			char * d_version
			char * d_url
			char * z_version
			uint32	z_itemid
		}
		*/
	int result_pages = (result_count / page_count) + (result_count % page_count == 0 ? 0 : 1);
	
	char cuint32[13] = {0};
	
	PackStream ps(outbuff, out_n);
	ps.set_pack_index(4);
	ps.pack_int32(IH_ACK_SEARCH_APPS);
	char utf8bom[3] = {0xEF, 0xBB, 0xBF};
	ps.pack_buffer(utf8bom, 3);
	ps.pack_jsonstr("{\"status\":");
	sprintf(cuint32, "%u", IH_E_OK);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"pageCount\":");
	sprintf(cuint32, "%u", result_pages);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"searchCount\":");
	sprintf(cuint32, "%u", total);
	ps.pack_jsonstr(cuint32);
	ps.pack_jsonstr(",\"content\":[");
	
	//item repeat
	if(page >= result_pages)
		page = 0;
	
	int timenow = time(NULL);
	int item_count = 0;
	
	int init = 0;
	if (!isCahe){
		init = page*page_count;
	}
	for (int r=init; r<result_count; ++r){
		if(r == init)
			ps.pack_jsonstr("{");
		else
			ps.pack_jsonstr(",{");

	save_log(INFO, "id:%d, weight:%ld, title:%s downloads:%u\n", sphinx_get_id (res, r), sphinx_get_weight(res, r), sphinx_get_string(res, r, 9), sphinx_get_int(res, r, 34));
//v9_download:
		ps.pack_jsonstr("\"Id\":");
		sprintf(cuint32, "%u", sphinx_get_id (res, r));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"itemid\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 11));
		ps.pack_jsonstr(cuint32);
				
		ps.pack_jsonstr(",\"catId\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 1));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr(",\"phone_typeid\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 3));
		ps.pack_jsonstr(cuint32);
					
		ps.pack_jsonstr(",\"title\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 9));

		//char icon_url[512] = {0};
		//int icon_url_len = 0;

		ps.pack_jsonstr("\",\"thumb\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 10));
		
		ps.pack_jsonstr("\",\"type\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 4));

		ps.pack_jsonstr("\",\"version\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 14));
		
		ps.pack_jsonstr("\",\"version_mini\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 15));
		
		ps.pack_jsonstr("\",\"soft_down\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 16));

		ps.pack_jsonstr("\",\"soft_truename\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 17));

		ps.pack_jsonstr("\",\"soft_phone\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 19));
		
		ps.pack_jsonstr("\",\"filesize\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 18));
		
		ps.pack_jsonstr("\",\"from_company\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 20));

		ps.pack_jsonstr("\",\"tag\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 0));

		ps.pack_jsonstr("\",\"description\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 21));

		ps.pack_jsonstr("\",\"language\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 24));
		
		////////////////////////
		ps.pack_jsonstr("\",\"keywords\":\"");
		//ps.pack_quotedjsonstr(sphinx_get_string(res, r, 0));
		////////////////////////
					
		ps.pack_jsonstr("\",\"stars\":");
		sprintf(cuint32, "%lf", sphinx_get_float(res, r, 2));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"total_stars\":");
		sprintf(cuint32, "%lf", sphinx_get_float(res, r, 22));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"total_stars_num\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 23));
		ps.pack_jsonstr(cuint32);
						
		ps.pack_jsonstr(",\"down\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 26));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr(",\"up\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 25));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr(",\"updatetime\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 12));
		ps.pack_jsonstr(cuint32);

		ps.pack_jsonstr(",\"inputtime\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 13));
		ps.pack_jsonstr(cuint32);

//v9_download_data:
		ps.pack_jsonstr(",\"content\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 28));
		
		ps.pack_jsonstr("\",\"soft_imgs\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 29));
		
		ps.pack_jsonstr("\",\"ipad_imgs\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 30));
		
		ps.pack_jsonstr("\",\"relatedsoftware\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 31));
		
		ps.pack_jsonstr("\",\"relation\":\"");
		ps.pack_quotedjsonstr(sphinx_get_string(res, r, 32));	

//v9_hits:
		ps.pack_jsonstr("\",\"views\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 33));
		ps.pack_jsonstr(cuint32);
						
		ps.pack_jsonstr(",\"downloads\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 34));
		ps.pack_jsonstr(cuint32);
		
		ps.pack_jsonstr(",\"commenttotal\":");
		sprintf(cuint32, "%u", sphinx_get_int(res, r, 35));
		ps.pack_jsonstr(cuint32);
		
		
		ps.pack_jsonstr("}");
		++item_count;
		if(item_count >= page_count)
			break;
	}//for
	
	
/*	
	else{
		for (int r = 0; r < result_count; ++r){
			if(r == 0)
				ps.pack_jsonstr("{");
			else
				ps.pack_jsonstr(",{");
			
			if ((res + r)->num_matches){
	//v9_download:
				ps.pack_jsonstr("\"Id\":");
				sprintf(cuint32, "%u", sphinx_get_id (res + r, 0));
				ps.pack_jsonstr(cuint32);
	
				ps.pack_jsonstr(",\"itemid\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 11));
				ps.pack_jsonstr(cuint32);
						
				ps.pack_jsonstr(",\"catId\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 1));
				ps.pack_jsonstr(cuint32);
				
				ps.pack_jsonstr(",\"phone_typeid\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 8));
				ps.pack_jsonstr(cuint32);
							
				ps.pack_jsonstr(",\"title\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 9));
	
				//char icon_url[512] = {0};
				//int icon_url_len = 0;
	
				ps.pack_jsonstr("\",\"thumb\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 10));
				
				ps.pack_jsonstr("\",\"type\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 4));
	
				ps.pack_jsonstr("\",\"version\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 14));
				
				ps.pack_jsonstr("\",\"version_mini\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 15));
				
				ps.pack_jsonstr("\",\"soft_down\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 16));
	
				ps.pack_jsonstr("\",\"soft_truename\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 17));
	
				ps.pack_jsonstr("\",\"soft_phone\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 19));
				
				ps.pack_jsonstr("\",\"filesize\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 18));
				
				ps.pack_jsonstr("\",\"from_company\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 20));
	
				ps.pack_jsonstr("\",\"tag\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 0));
	
				ps.pack_jsonstr("\",\"description\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 21));
	
				ps.pack_jsonstr("\",\"language\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 24));
				
				////////////////////////
				ps.pack_jsonstr("\",\"keywords\":\"");
				//ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 0));
				////////////////////////
							
				ps.pack_jsonstr("\",\"stars\":");
				sprintf(cuint32, "%lf", sphinx_get_float(res + r, 0, 2));
				ps.pack_jsonstr(cuint32);
	
				ps.pack_jsonstr(",\"total_stars\":");
				sprintf(cuint32, "%lf", sphinx_get_float(res + r, 0, 22));
				ps.pack_jsonstr(cuint32);
	
				ps.pack_jsonstr(",\"total_stars_num\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 23));
				ps.pack_jsonstr(cuint32);
								
				ps.pack_jsonstr(",\"down\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 26));
				ps.pack_jsonstr(cuint32);
				
				ps.pack_jsonstr(",\"up\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 25));
				ps.pack_jsonstr(cuint32);
				
				ps.pack_jsonstr(",\"updatetime\":");
				sprintf(cuint32, "%ul", sphinx_get_int(res + r, 0, 12));
				ps.pack_jsonstr(cuint32);
	
				ps.pack_jsonstr(",\"inputtime\":");
				sprintf(cuint32, "%ul", sphinx_get_int(res + r, 0, 13));
				ps.pack_jsonstr(cuint32);
	
	//v9_download_data:
				ps.pack_jsonstr(",\"content\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 28));
				
				ps.pack_jsonstr(",\"soft_imgs\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 29));
				
				ps.pack_jsonstr(",\"ipad_imgs\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 30));
				
				ps.pack_jsonstr(",\"relatedsoftware\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 31));
				
				ps.pack_jsonstr(",\"relation\":\"");
				ps.pack_quotedjsonstr(sphinx_get_string(res + r, 0, 32));	
	
	//v9_hits:
				ps.pack_jsonstr("\",\"views\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 33));
				ps.pack_jsonstr(cuint32);
								
				ps.pack_jsonstr(",\"downloads\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 34));
				ps.pack_jsonstr(cuint32);
				
				ps.pack_jsonstr(",\"commenttotal\":");
				sprintf(cuint32, "%u", sphinx_get_int(res + r, 0, 35));
				ps.pack_jsonstr(cuint32);
					
				ps.pack_jsonstr("}");
			}//if
		}//for
		
	}
*/
	ps.pack_jsonstr("]}");
	int n = ps.get_pack_index();
	ps.set_pack_index(0);
	ps.pack_int32(n);
	
	//save_log(INFO, "buflen: %d content: %s\n", n, outbuff+11);
	return n;
}

int InitIndex(){
	int ret = -1;
	FILE* fp = popen("ps -ef | grep searchd", "r");
	if (NULL == fp){
		save_log(INFO, "exe ps -ef | grep searchd err! errno:%d, %s\n", errno, strerror(errno));
		return -1;
	}
	char line[256] = {0};
	bool isfind = false;
	while (fgets(line, 256, fp)){
		if (strlen(line) > 0 && strstr(line, "searchd") != NULL && strstr(line, "grep") == NULL){	
			isfind = true;
			save_log(INFO, "searchd is running:%s, killall searchd!\n", line);
			break;
		}
		memset(line, 0, sizeof(line));
	}
	pclose(fp);
	
	if (isfind){
		ret = system("killall searchd");
		if (0 != ret){
			save_log(INFO, "killall searchd err! errno:%d, %s\n", errno, strerror(errno));
			return -1;
		}
		sleep(1);		
	}
	
	char cmd[] = "/usr/local/sphinx-for-chinese/bin/indexer --all";
	ret = system(cmd);
	if (0 != ret){
		save_log(INFO, "exe indexer err! errno:%d, %s\n", errno, strerror(errno));
		return -1;
	}
	
	memset(cmd, 0, sizeof(cmd));
	sprintf(cmd, "%s", "/usr/local/sphinx-for-chinese/bin/searchd");
	ret = system(cmd);
	if (0 != ret){
		save_log(INFO, "exe searchd err! errno:%d, %s\n", errno, strerror(errno));
		return -1;
	}
	
	if(access(LOG_PATH, F_OK) != 0){
		memset(cmd, 0 , sizeof(cmd));
		sprintf(cmd, "mkdir -p %s", LOG_PATH);
		ret = system(cmd);
		if (0 != ret){
			save_log(INFO, "mkdir log err! errno:%d, %s\n", errno, strerror(errno));
			return -1;
		}
	}
	
	return 0;
}

void* timer_thread(void*){
	bool bcontinue = true;
	int delay_second = 0;
	
	while(true){
		sleep(1);	
		++g_time_tick;

		//printf("in logic:%ld\n",  Conf::Instance()->GetSystemInfo().indextime);
		if (g_time_tick % Conf::Instance()->GetSysInfo().indextime == 0){//20分钟
			char cmd[] = "/usr/local/sphinx-for-chinese/bin/indexer --all --rotate";
			int ret = system(cmd);
			if (0 != ret){
				save_log(INFO, "rebuild index err! errno:%d, %s\n", errno, strerror(errno));
			}
			
			g_data_locker.Lock();
			
			if (g_db_mem_pool){
				delete g_db_mem_pool;
				g_db_mem_pool = NULL;
			}
			
			g_db_mem_pool = new CMemPool();
			g_db_mem_pool->AddUnit(32, 1024);
			g_db_mem_pool->AddUnit(64, 1024);
			g_db_mem_pool->AddUnit(128, 1024);
			g_db_mem_pool->AddUnit(256, 1024);
			g_db_mem_pool->AddUnit(512, 1024);
			g_db_mem_pool->AddUnit(sizeof(SEARCH_LIST), 1024);
			g_db_mem_pool->AddUnit(sizeof(SEARCH_CACHE), 1024);
			if (g_apple_app_search_cache_tree){
				delete g_apple_app_search_cache_tree;
				g_apple_app_search_cache_tree = NULL;
			}
			if (g_wallpaper_search_cache_tree){
				delete g_wallpaper_search_cache_tree;
				g_wallpaper_search_cache_tree = NULL;
			}
			if (g_ring_search_cache_tree){
				delete g_ring_search_cache_tree;
				g_ring_search_cache_tree = NULL;
			}
			
			g_apple_app_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
			g_wallpaper_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
			g_ring_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
						
			g_search_cache_lru.RemoveAll();

			g_data_locker.Unlock();
		
		}//if 20
				
		if(g_time_tick % 300 == 0){
			g_search_all_count += g_search_app_count;
			g_search_all_count += g_search_wallpaper_count;
			g_search_all_count += g_search_ring_count;
			g_search_all_count += g_search_share_app_count;
			save_log(INFO, "g_search_all_count:%u, g_search_app_count:%u, g_search_share_app_count:%u, g_search_ring_count:%u, g_search_wallpaper_count:%u\n", \
			g_search_all_count, g_search_app_count, g_search_share_app_count, g_search_ring_count, g_search_wallpaper_count);
			
			g_search_app_count = 0;
			g_search_wallpaper_count = 0;
			g_search_ring_count = 0;
			g_search_share_app_count = 0;
			g_data_locker.Lock();
			
			while(g_search_cache_lru.GetCount() > 100000){
				SEARCH_CACHE * sc = g_search_cache_lru.GetHeadAndRemove();
				
				switch(sc->search_type){
					case 0:
						g_apple_app_search_cache_tree->remove(sc->key);
						break;
					case 1:
						g_ring_search_cache_tree->remove(sc->key);
						break;
					case 2:
						g_wallpaper_search_cache_tree->remove(sc->key);
						break;
				}
				
				for(int i=0; i<sc->count; i++){
					SEARCH_LIST * sl_del = sc->sl;
					sc->sl = sc->sl->next;			
					g_db_mem_pool->Free(sl_del);
				}
				
				g_db_mem_pool->Free(sc);
			}//while

			g_data_locker.Unlock();
		}
	
		if(access("./stop_store_search", F_OK) == 0){
			remove("./stop_store_search");
			bcontinue = false;
			set_continue_work(bcontinue);
		}
		
		if(!bcontinue)
			exit(0);
			
	}//while
	
	return NULL;
}

#define SEARCH_SERVER_PORT 61020
int OnInit(char * bind_ip, int &bind_port){
	//if (InitIndex() < 0)
	//	return -1;
		
	InitCrcTable();
	
	strcpy(bind_ip, "0");
	bind_port = SEARCH_SERVER_PORT;

	if(g_application_mem_pool == NULL){
		g_application_mem_pool = new CMemPool();
		g_application_mem_pool->AddUnit(4096, 8);
		g_application_mem_pool->AddUnit(2048, 8);
		g_application_mem_pool->AddUnit(1024, 16);
		g_application_mem_pool->AddUnit(512, 32);
		g_application_mem_pool->AddUnit(256, 64);
		g_application_mem_pool->AddUnit(64, 64);
		g_application_mem_pool->AddUnit(32, 64);
		g_application_mem_pool->AddUnit(16, 32);
		g_application_mem_pool->AddUnit(8, 16);
	}
	
	if(g_db_mem_pool == NULL){
		g_db_mem_pool = new CMemPool();
		g_db_mem_pool->AddUnit(4096, 8);
		g_db_mem_pool->AddUnit(2048, 8);
		g_db_mem_pool->AddUnit(1024, 16);
		g_db_mem_pool->AddUnit(512, 32);
		g_db_mem_pool->AddUnit(256, 64);
		g_db_mem_pool->AddUnit(sizeof(SEARCH_LIST), 1024);
		g_db_mem_pool->AddUnit(sizeof(SEARCH_CACHE), 1024);
	}
	g_apple_app_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
	g_ring_search_cache_tree      = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;
	g_wallpaper_search_cache_tree = new _RB_tree<char*, SEARCH_CACHE*, map_compare_mem128, rbtree_searchcache_alloctor>;

	pthread_t ntid;
	create_thread(&ntid, timer_thread, NULL);

	return 0;
}
/**********************************************************************/