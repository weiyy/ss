#include "func.h"
#include "netdb.h"
#include <stdarg.h>
#include "MemPool.h"
/*
 */
int safe_strlen(char * str)
{
	if(!str)
		return 0;
	
	return strlen(str);
}

int safe_atoi(const char* str)
{
	if(!str)
		return 0;
	
	return atoi(str);
}

double safe_atof(const char* str)
{
	if(!str)
		return 0;
	
	return atof(str);
}

int64_t safe_atoi64(char* str)
{
	if(!str)
		return 0;
	
	return atoll(str);
}

int safe_strcpy(char * dest_buff, int dest_buff_size, char * src_str)
{
	if(!src_str)
		return 0;
	
	int copy_len = strlen(src_str);
	if(copy_len >= dest_buff_size)
	{
		copy_len = dest_buff_size - 1;
	}
	
	memcpy(dest_buff, src_str, copy_len);
	dest_buff[copy_len] = '\0';
	
	return copy_len;
}

int safe_memcpy(char * dest_buff, int dest_buff_size, char * src_buff, int src_buff_len)
{
	if(!src_buff)
		return 0;
	
	int copy_len = src_buff_len > dest_buff_size ? dest_buff_size : src_buff_len;

	memcpy(dest_buff, src_buff, copy_len);
	
	return copy_len;
}

int Utf8ToUtf16(unsigned char* utf8, int size8, unsigned short* utf16, int& size16)
{
    int count =0, i;
    unsigned short int integer;
    for(i=0;i<size16-1 && count<size8;i++)
    {    
        if( utf8[count] < 0x80)
        {
            // <0x80
            integer = utf8[count];            
            count++;
        }
        else if( (utf8[count] < 0xDF) && (utf8[count]>=0x80))
        {
            integer = utf8[count] & 0x1F;
            integer = integer << 6;
            integer += utf8[count+1] &0x3F;
            count+=2;
        }
        else if( (utf8[count] <= 0xEF) && (utf8[count]>=0xDF))
        {
            integer = utf8[count] & 0x0F;
            integer = integer << 6;
            integer += utf8[count+1] &0x3F;
            integer = integer << 6;
            integer += utf8[count+2] &0x3F;
            count+=3;
        }
        else
        {
            //printf("error!\n");
        }
        
        utf16[i] = integer;
    }

    size16 = i;

    return size16;
}

int UTF16ToUTF8(char* utf16, int size16, char* utf8, int& size8)
{
    int i=0, count=0;
    char tmp1, tmp2;
    
    unsigned short int integer;
    for(i=0;i<size16-1 && count<size8-2;i+=2)
    {    
        integer = *(unsigned short int*)&utf16[i];
        
        if( integer<0x80)
        {            
            utf8[count] = utf16[i] & 0x7f;
            count++;
        }
        else if( integer>=0x80 && integer<0x07ff)
        {
            tmp1 = integer>>6;
            utf8[count] = 0xC0 | (0x1F & integer>>6);
            utf8[count+1] = 0x80 | (0x3F & integer);
            count+=2;
        }
        else if( integer>=0x0800 )
        {            
            tmp1 = integer>>12;
            utf8[count] = 0xE0 | (0x0F & integer>>12);
            utf8[count+1] = 0x80 | ((0x0FC0 & integer)>>6);
            utf8[count+2] = 0x80 | (0x003F & integer);
            
            count += 3;
        }    
        else 
        {
            printf("error\n");
        }
    }

    //size16 = i;
    size8  = count;
    return count;
}

int UTF162UTF8(unsigned short* utf16, int size16, char* utf8, int& size8)
{
    int i=0, count=0;
    char tmp1, tmp2;
    
    unsigned short integer;
    for(i=0; i<size16;i++)
    {    
        integer = *(unsigned short*)&utf16[i];
        
        if( integer<0x80 && count<size8)
        {            
            utf8[count] = utf16[i] & 0x7f;
            count++;
        }
        else if( integer>=0x80 && integer<0x07ff && count<size8-1)
        {
            tmp1 = integer>>6;
            utf8[count] = 0xC0 | (0x1F & integer>>6);
            utf8[count+1] = 0x80 | (0x3F & integer);
            count+=2;
        }
        else if( integer>=0x0800 && count<size8-2 )
        {            
            tmp1 = integer>>12;
            utf8[count] = 0xE0 | (0x0F & integer>>12);
            utf8[count+1] = 0x80 | ((0x0FC0 & integer)>>6);
            utf8[count+2] = 0x80 | (0x003F & integer);
            
            count += 3;
        }    
        else 
        {
            printf("error\n");
        }
    }

    //size16 = i;
    size8  = count;
    return count;
}

/*
 * U-00000000 - U-0000007F: 0xxxxxxx
 * U-00000080 - U-000007FF: 110xxxxx 10xxxxxx
 * U-00000800 - U-0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
 * U-00010000 - U-001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-00200000 - U-03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 */

int UTF32ToUTF8(unsigned long code, char *s)
{
	int n = 0;
	unsigned char mask, mark;

	if (code < 0) {
		// invalid?
		return n;
	} else if (code < 0x00000080) {
		*s = code;
		return 1;	
	} else if (code < 0x00000800) {
		n = 2;
		mask = 0x1F;
		mark = 0xC0;
	} else if (code < 0x00010000) {
		n = 3;
		mask = 0x0F;
		mark = 0xE0;
	} else if (code < 0x00200000) {
		n = 4;
		mask = 0x07;
		mark = 0xF0;
	} else if (code < 0x04000000) {
		n = 5;
		mask = 0x03;
		mark = 0xF8;
	} else {
		n = 6;	
		mask = 0x01;
		mark = 0xFC;
	}

	int i;
	for (i = n-1; i > 0; --i) {
		*(s+i) = (code&0x3F) | 0x80;
		code >>= 6;
	}
	*(s+i) = (code&mask) | mark;

	return n;
}

int UTF8ToUTF32(const char *s, unsigned long *code)
{
	long tc = 0x00000000;
	int n;
	unsigned char c, mask;
	*code = tc;

	c = *s;
	if ((c&0x80) == 0x00) {
		*code = c;
		return 1;
	} else if ((c&0xE0) == 0xC0) {
		n = 2;
		mask = 0x1F;
	} else if ((c&0xF0) == 0xE0) {
		n = 3;
		mask = 0x0F;
	} else if ((c&0xF8) == 0xF0) {
		n = 4;
		mask = 0x07;
	} else if ((c&0xFC) == 0xF8) {
		n = 5;
		mask = 0x03;
	} else if ((c&0xFE) == 0xFC) {
		n = 6;
		mask = 0x01;
	} else {
		return 1;
	}

	tc |= (c & mask);
	for (int i = 1; i < n; ++i) {
		tc <<= 6;
		c = *(s+i);
		if (c == '\0' || (c&0xC0) != 0x80) {
			return i;
		}
		tc |= (c & 0x3F);
	}

	*code = tc;
	return n;
}

int Utf8ToUtf32(char* utf8, int size8, unsigned long* utf32, int& size32)
{
    int count = 0, i;
    long integer;
    for(i=0; i<size32-1 && count<size8; i++)
    {
		count += UTF8ToUTF32(utf8+count, utf32+i);
    }

    size32 = i;

    return size32;
}

int convertUTF32UTF8(unsigned long* utf32, int size32, char* utf8, int& size8)
{
    int i=0, count=0;
    char tmp1, tmp2;
    
    for(i=0; i<size32; i++)
    {
		count += UTF32ToUTF8(utf32[i], utf8+count);
    }
    
    size8  = count;
    return size8;
}

void hex_to_str(unsigned char * hex, int hex_len, char * str)
{
	char HEX[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	
	int p = 0;
	for(int i=0; i<hex_len; i++)
	{
		//int n1 = hex[i] >> 4;
		//int n2 = hex[i] & 0xF;
		str[p] = HEX[hex[i] >> 4];
		++p;
		str[p] = HEX[hex[i] & 0xF];
		++p;
		str[p] = ' ';
		++p;
	}
	
	str[p] = '\0';
}

inline unsigned char hexchar2num(char hexchar)
{
	char HEX1[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	char HEX2[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	
	for(char i=0; i<16; i++)
	{
		if(hexchar == HEX1[i] || hexchar == HEX2[i])
			return i;
	}
	
	return 0;
}

void str_to_hex(char * str, int str_len, unsigned char * hex)
{
	int p = 0;
	for(int i=0; i<str_len; i++)
	{
		hex[p] = (hexchar2num(str[i])<<4) | (hexchar2num(str[i+1]));
		p++;
		i++;
	}
}

void itoa(int val, char* buff, int radix)
{
	switch(radix)
	{
		case 8:
		{
			sprintf(buff, "%o", val); //converts to octal base.
			break;
		}
		case 10:
		{
			sprintf(buff, "%d", val); //converts to decimal base.
			break;
		}
		case 16:
		{
			sprintf(buff, "%x", val); //converts to hexadecimal base.
			break;
		}
	}
}

typedef void*(*THREAD_FUNC_POINTER)(void*);
int create_thread(pthread_t * ntid, THREAD_FUNC_POINTER func, void * arg)
{
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(ntid, &thread_attr, func, arg);
    pthread_attr_destroy(&thread_attr);
}

#define	MAXFD	1024
void daemon_init(){
	int		i;
	pid_t	pid;

	if ( (pid = fork()) != 0)
		exit(0);			/* parent terminates */

	setsid();				/* become session leader */

	signal(SIGHUP, SIG_IGN);
	if ( (pid = fork()) != 0)
		exit(0);			/* 1st child terminates */

	//chdir("/");				/* change working directory */

	umask(0);				/* clear our file mode creation mask */

	for (i = 0; i < MAXFD; i++)
		close(i);
}

uint32_t QueryIP(char * szIP)
{
	uint32_t ip;
	ip = inet_addr(szIP);
	if(ip == INADDR_NONE)
	{
		struct hostent *host=NULL;
		host = gethostbyname(szIP);
		if(host != NULL)
			memcpy(&ip, host->h_addr_list[0], host->h_length);
		else
		{
			return INADDR_NONE;
		}
	}

	return ip;
}

int tcpsend(int s, char * buff, int len)
{
	int nret = 0, socklen = 0;

	while((nret+=socklen) < len)
	{
		socklen = send(s, &buff[nret], len-nret, 0);
		if(0 >= socklen)
		{
			return socklen;
		}
	}

	return nret;
}

int tcprecv(int s, char * buff, int len)
{
	int nret = 0, socklen = 0;

	while((nret+=socklen) < len)
	{
		socklen = recv(s, &buff[nret], len-nret, 0);
		if(0 >= socklen)
		{
			return socklen;
		}
	}

	return nret;
}

bool is_file_exists(const char *path)
{
    struct stat buf;
    int err = stat(path,&buf);
    if (err == 0)
	{
        return true;
	}
    if (errno == 2) //判断errno 是否为“ ENOENT         参数file_name指定的文件不存在”
    {
        return false;
    }
    //return true;
    return false;
}

void get_default_log_path(char * file_path)
{
    struct   tm     *ptm;
    long       ts;
    int         y,m,d;
    ts   =   time(NULL);
    ptm   =   localtime(&ts);

    y   =   ptm->tm_year + 1900;     //年
    m   =   ptm->tm_mon + 1;             //月
    d   =   ptm->tm_mday;               //日

    sprintf(file_path, "./log/%04u-%02u-%02u.log", y, m, d);
}

void get_default_time_string(char * time_str)
{
    struct   tm     *ptm;
    long       ts;
    int         y,m,d;
    ts   =   time(NULL);
    ptm   =   localtime(&ts);
	
	sprintf(time_str, "[%02u:%02u:%02u]", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
}

int write_log(const char * file_path, const char * format, ...)
{
	char default_path[100];
	if(!file_path)
	{
		get_default_log_path(default_path);
		file_path = default_path;
	}
	
	FILE * f = fopen(file_path, "ab");
    if (f)
    {
        va_list arg_ptr;
        va_start(arg_ptr, format);
        int bytes_written = vfprintf(f, format, arg_ptr);
        va_end(arg_ptr);
		fwrite("\n", 1, 1, f);
		fclose(f);
        return bytes_written;
    }
    
    return -1;
}

CCriticalSection log_lock;
void save_log(char* filename, int line, const char *msgfmt, ...)
{
	if (!filename)
		return;
	va_list ap;
    va_start(ap,msgfmt);
  
	FILE *pf = NULL;

  struct   tm     *ptm;
  long       ts;
  int         y,m,d,hour,min,sec;
  ts   =   time(NULL);
  ptm   =   localtime(&ts);

  y   =   ptm->tm_year + 1900;     //年
  m   =   ptm->tm_mon + 1;             //月
  d   =   ptm->tm_mday;               //日
  hour = ptm->tm_hour;
  min = ptm->tm_min;
  sec = ptm->tm_sec;
  
	char log_name[64] = {0};
	sprintf(log_name, "%s/%s_%04d%02d%02d.log", LOG_PATH, LOG_NAME, y, m, d);
	
	log_lock.Lock();

	pf = fopen(log_name, "a+");
	if (pf == NULL){
		log_lock.Unlock(); 
		return;
	}

	char* p = NULL;
	if (NULL == (p = strrchr((char*)filename, '/'))){
		log_lock.Unlock();
		return;
	}

	fprintf(pf, ("[%s(%d) %d]%02d:%02d:%02d "), p+1, line, gettid(), hour, min, sec);
	vfprintf(pf, msgfmt, ap);
	fclose(pf);

	va_end(ap);

	log_lock.Unlock(); 
}

