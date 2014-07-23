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
#include <stdlib.h>
#include "func.h"
#include "MemPool.h"
#include "RB_tree.h"
#include "plist.h"
#include "logic.h"
#include "Conf.hpp"
///////////////////////////////////////////////////////////////////////////////////////////
#define SWAPIN_BUFFER_SIZE	1024*1024+1024
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
#define MAXSWAPOUTBUFF	4096
#pragma pack(push)
#pragma pack(1)
struct SWAPOUTBUFFER
{
	uint16_t len;
	int client_sock;
	int session_id;
	uint32_t flags;
	char buff[MAXSWAPOUTBUFF];
	
	SWAPOUTBUFFER * next;
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


#define SWAPOUTBUFF_HEADER_SIZE	14		//SWAPOUTBUFFER::buff-SWAPOUTBUFFER::len
#define SWAPINBUFF_HEADER_SIZE	16		//SWAPINBUFFER::packet-SWAPINBUFFER::client_sock

///////////////////////////////////////////////////////////////////////////////////////////
struct GATESERVERCONNECTION
{
	SWAPOUTBUFFER * first;
	SWAPOUTBUFFER * last;
	
	CEvent event;
	int state;	//1-online   0-offline
};

CMemPool * g_mempool_for_gateservermap;

class rbtree_node_alloctor_for_gateservermap {
public:
    void *alloc(size_t n)
    {
        return g_mempool_for_gateservermap->Alloc(n);
    }

    void dealloc(void *p)
    {
        g_mempool_for_gateservermap->Free(p);
    }
};
_RB_tree<uint32_t, GATESERVERCONNECTION*, map_compare_int32, rbtree_node_alloctor_for_gateservermap> * g_gateserver_map =
    new _RB_tree<uint32_t, GATESERVERCONNECTION*, map_compare_int32, rbtree_node_alloctor_for_gateservermap>;
	
CCriticalSection g_gateservermap_locker;

int g_gateserver_connection_id = 0;
///////////////////////////////////////////////////////////////////////////////////////////
void handle_pipe(int sig)
{
    //不做任何处理即可
}
///////////////////////////////////////////////////////////////////////////////////////////
int TCPSend(int s, char * buff, int len){
    int sent_size = 0;

    while (sent_size < len)
    {
        int ret = send(s, &buff[sent_size], len - sent_size, 0);

        if (ret < 1)
            return ret;

        sent_size += ret;
    }

    return sent_size;
}

int TCPSendHugePacket(GATESERVERCONNECTION * gsc, int client_sock, int session_id, uint32_t flags, char * huge_buff, int huge_buff_len){
	int send_pos = 0;
	
	gsc->event.Lock();
	
	if(gsc->state != 1){
		gsc->event.Unlock();
		return -20;
	}
	
	while (huge_buff_len > 0){
		int need_buff_len = huge_buff_len > MAXSWAPOUTBUFF ? MAXSWAPOUTBUFF : huge_buff_len;
		
		SWAPOUTBUFFER * sb = (SWAPOUTBUFFER*)g_mempool_for_gateservermap->Alloc(sizeof(SWAPOUTBUFFER));
		
				//save_log(INFO, "gateservermapAllocsb\n");
		sb->next = NULL;
		
		//len
		sb->len = need_buff_len;
		//fd
		sb->client_sock = client_sock;
		//session_id
		sb->session_id = session_id;
		//flags
		sb->flags = (send_pos == 0 ? flags : 0);
		//copy data
		
		memcpy(sb->buff, &huge_buff[send_pos], need_buff_len);
		
		
		if (gsc->first){
			gsc->last->next = sb;
			gsc->last = sb;
		}
		else{
			gsc->first = sb;
			gsc->last = sb;
		}
		
		send_pos += need_buff_len;
		huge_buff_len -= need_buff_len;
	}//while
	
	gsc->event.Post();
	gsc->event.Unlock();
	
	return 0;
}


void* data_swapout_thread(void* nparam){
	int sockfd = ((uint64_t)nparam) & 0xFFFFFFFF;
	int gateserver_connection_id = ((uint64_t)nparam) >> 32;
	
	g_gateservermap_locker.Lock();
	_RB_tree_node<uint32_t, GATESERVERCONNECTION*> * node = g_gateserver_map->find(gateserver_connection_id);
	g_gateservermap_locker.Unlock();

	if (node == g_gateserver_map->nil())
		return NULL;
	
	GATESERVERCONNECTION * gsc = node->value();
	
	SWAPOUTBUFFER * sb;
	
	while (true){
		gsc->event.Lock();
		while (true){
			if(gsc->state != 1)
			{
				break;
			}
	
			sb = gsc->first;
			if (sb)
			{
				gsc->first = sb->next;

				if (sb == gsc->last)
		  		gsc->last = NULL;
		
				break;
			}
			gsc->event.Wait();
		}//while
		gsc->event.Unlock();
	
		if(gsc->state != 1){
			break;
		}
		
		int send_len = sb->len + SWAPOUTBUFF_HEADER_SIZE;
		int sent_bytes = TCPSend(sockfd, (char*)sb, send_len);
		
		g_mempool_for_gateservermap->Free(sb);
		//save_log(INFO, "gateservermapFreesb\n");
		if (sent_bytes != send_len){
		  break;
		}
	}//while
	
	
	gsc->event.Lock();
	gsc->state = 0;		//set this gateserver offline
	while (gsc->first){
		sb = gsc->first;
		gsc->first = sb->next;
		
		g_mempool_for_gateservermap->Free(sb);
		//save_log(INFO, "gateservermapFreesb\n");
	}
	gsc->event.Unlock();
	
	//printf("data_swapout_thread close fd %d\n", sockfd);
	close(sockfd);
	
		g_gateservermap_locker.Lock();
		g_mempool_for_gateservermap->Free(gsc);
		g_gateserver_map->remove(gateserver_connection_id);
		//save_log(INFO, "gateservermapFree gsc \n");
		g_gateservermap_locker.Unlock();	
		
	return NULL;
}

void* data_swapin_thread(void* nparam){
/*
packet format
{
	//SWAPINBUFFER * sb
	int client_sock;
	uint32_t ip;
	int session_id;
	
	//CLIENTPACKETTCP* tcpdata
	uint32		len
	uint32		commmand
	char[]		data
};
*/
	
	int ret, recvlen;
	char *buff = new char[SWAPIN_BUFFER_SIZE * 2];
	char *outbuff = new char[SWAPIN_BUFFER_SIZE * 10];
	memset(buff, 0, SWAPIN_BUFFER_SIZE * 2);
	memset(outbuff, 0, SWAPIN_BUFFER_SIZE * 10);

	int sockfd = ((uint64_t)nparam) & 0xFFFFFFFF;
	int gateserver_connection_id = ((uint64_t)nparam) >> 32;
	
	g_gateservermap_locker.Lock();
	_RB_tree_node<uint32_t, GATESERVERCONNECTION*> * gateserver_connection_node = g_gateserver_map->find(gateserver_connection_id);
	g_gateservermap_locker.Unlock();
	
	
	if (gateserver_connection_node == g_gateserver_map->nil())
		return NULL;
	
	GATESERVERCONNECTION * gsc = gateserver_connection_node->value();
	
	recvlen = 0;
	uint32_t ppmNum = 0, totalNum = 0;
	time_t timeStart = time(NULL);
	char t1_buffer[64], buffer[1024];

	while (is_continue_work()){
		ret = recv(sockfd, &buff[recvlen], SWAPIN_BUFFER_SIZE-recvlen, 0);
		
		//printf("ret:%d, errno:%d, %s\n", ret, errno, strerror(errno));
		fflush(stdout);
		if (ret < 1){
			break;
		}
		
		recvlen += ret;	
		
		while(true){
			if (recvlen >= SWAPINBUFF_HEADER_SIZE + sizeof(CLIENTPACKETTCP)){
				SWAPINBUFFER * sb = (SWAPINBUFFER*)buff;
				CLIENTPACKETTCP* tcpdata = (CLIENTPACKETTCP*)sb->packet;
				
				uint32_t packet_size = tcpdata->len + SWAPINBUFF_HEADER_SIZE;
				
				if (recvlen >= packet_size){
					char temp_save;
					if(recvlen > packet_size)
					{
						temp_save = buff[packet_size];
					}
					//process data
					int hp_ret = HandlePacket(sb->ip, sb->packet, tcpdata->len, outbuff);
					
					if(hp_ret > 0){
						TCPSendHugePacket(gsc, sb->client_sock, sb->session_id, sb->flags, outbuff, hp_ret);
					}
					
					//copy surplus data to the front
					if (recvlen > packet_size){
						memcpy(buff, &buff[packet_size], recvlen - packet_size );
						buff[0] = temp_save;
						recvlen -= packet_size;
					}
					else{
					  recvlen = 0;
					}
				}//if
				else
				{
					break;
				}
			}
			else
			{
			    break;
			}
		}//while
	}//while
	
	gsc->event.Lock();
	gsc->state = 0;		//set gateserver offline
	gsc->event.Post();
	gsc->event.Unlock();
	
	delete buff;
	delete outbuff;
	printf("data_swapin_thread with socket(%u) exit.\n", sockfd);
	return NULL;
}

char dataserver_name[32];

int TCPRecv(int s, char * buff, int len)
{
	int nret = 0, socklen = 0;

	while((nret+=socklen) < len)
	{
		//printf("len-nret: %d\n", len-nret);
		socklen = recv(s, (char*)&buff[nret], len-nret, MSG_DONTWAIT);
		printf("recv socklen: %d\n", socklen);
		if(0 >= socklen)
		{
			return socklen;
		}
	}

	return nret;
}


int main(int argc, char **argv)
{	
	save_log(INFO,"///////////////////////////////////\n");
	save_log(INFO,"/////search_dataserver start!//////\n");
	save_log(INFO,"///////////////////////////////////\n");


/*	return 0;
	PLIST plist;
	if(0 != plist.Load("./dataserver_config.plist"))
	{
		printf("load config error.\n");
		return -1;
	}
	
	if(!plist.Root()->first_child_node)
	{
		printf("load config error.\n");
		return -1;
	}
	
	PLIST_NODE * node = plist.Root()->first_child_node->FindChild("server");
	if(!node)
	{
		printf("load config error.\n");
		return -1;
	}
	
	strncpy(dataserver_name, node->value, 31);
	printf("dataserver is: %s\n", dataserver_name);
	
*/	
   
   Conf::Instance()->LoadConf("search.conf");
   
	//send或者write socket遭遇SIGPIPE信号
	struct sigaction action;
	action.sa_handler = handle_pipe;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGPIPE, &action, NULL);
	
	g_mempool_for_gateservermap = new CMemPool;
	g_mempool_for_gateservermap->AddUnit(sizeof(_RB_tree_node<uint32_t, SWAPINBUFFER*>), 1024);
	g_mempool_for_gateservermap->AddUnit(sizeof(SWAPOUTBUFFER), 1024);
	g_mempool_for_gateservermap->AddUnit(sizeof(GATESERVERCONNECTION), 1);
	
	int ret;
	int i;
	int bind_port;
	socklen_t len;
	static struct sockaddr_in my_addr, their_addr;
	int new_fd;
	int listen_fd;
	char bind_ip[32] = {0};
	pthread_t ntid;

	if (OnInit(bind_ip, bind_port) < 0){
		save_log(INFO, "init err!\n");
		return -1;
	}

	len = sizeof(struct sockaddr_in);
	
	for (;;){
		listen_fd=socket(PF_INET,SOCK_STREAM,0);
		if (listen_fd<0){
			printf("socket error.\n");
			sleep(5);
			continue;
		}
		
		bzero(&my_addr, sizeof(my_addr));
		my_addr.sin_family = PF_INET;
		my_addr.sin_port = htons(bind_port);
		//my_addr.sin_addr.s_addr = INADDR_ANY;
		my_addr.sin_addr.s_addr = inet_addr(bind_ip);
		
		//add by shasure 2012.7.27 ->
		int tmp = 1;
		setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int));
		// <-
		
		if (bind(listen_fd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1){
			close(listen_fd);
			printf("bind error.\n");
			sleep(2);
			continue;
		}
		else{
			printf("IP & Port bind ok!\n");
		}
		
		if (listen(listen_fd, 5) == -1){
			close(listen_fd);
			printf("listen error.\n");
			sleep(2);
			continue;
		}
		
		save_log(INFO, "DataServer start ok!\n");
		
		
		for (;;){
			new_fd = accept(listen_fd, (struct sockaddr *) &their_addr,&len);
			if (new_fd < 1){
				sleep(1);
				continue;
			}

			//save_log(INFO, "accept at socket(%u).\n", new_fd);
			
			GATESERVERCONNECTION * gc = (GATESERVERCONNECTION*)g_mempool_for_gateservermap->Alloc(sizeof(GATESERVERCONNECTION));
			gc->first = NULL;
			gc->last = NULL;
			gc->state = 1;
			gc->event.Init();
			
			g_gateservermap_locker.Lock();
			int gci = ++g_gateserver_connection_id;
			g_gateserver_map->insert(gci, gc);
			
			save_log(INFO, "gateservermap gc : %d\n", gci);
			g_gateservermap_locker.Unlock();
			
			uint64_t param = ((uint64_t)gci << 32) | new_fd;
			create_thread(&ntid, data_swapin_thread, (void*)param);
			create_thread(&ntid, data_swapout_thread, (void*)param);
		}//for
		
		close(listen_fd);
		usleep(100000);
	}
	
	return 0;
}