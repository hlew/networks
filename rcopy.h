/*
 * rcopy.hpp
 *
 *  Created on: May 4, 2013
 *      Author: Hiram Lew
 */

#include "networks.h"
#include <string>
#include <iostream>

class GPacket {
protected:
	uint32_t seq_num;
	unsigned short cksum;
	uint8_t flag;

public:
	GPacket () {seq_num=0;cksum=0;flag=0;};
	GPacket (int, int);
	virtual ~GPacket() {};
	GPacket &operator= (const void *ptr);
	virtual int get_packet_size() {return 0;};
};

GPacket::GPacket(int a, int b) {
	seq_num = a;
	cksum = 0;
	flag = b;
}

GPacket &GPacket::operator=(const void *pack) {
	char *ptr = (char *) pack;
	memcpy(&seq_num, ptr, sizeof(uint32_t));
	memcpy(&cksum, ptr + sizeof(uint32_t), sizeof(unsigned short));
	memcpy(&flag, ptr + sizeof(uint32_t) + sizeof(flag), sizeof(uint8_t));
	return *this;
}


class DPacket : public GPacket {
public:
	static uint8_t flag;

};

uint8_t DPacket::flag = 0x01;



enum State {
	DONE, FILENAME, RECV_DATA, FILE_OK
};
typedef enum State STATE;
