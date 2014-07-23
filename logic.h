#pragma once
//#include "../func/func.h"

bool is_continue_work();

void set_continue_work(bool bcontinue);

int OnInit(char * bind_ip, int &bind_port);

int HandlePacket(uint32_t client_ip, char * packet, int packet_len, char * out_buff);