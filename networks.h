/* used for network functions  */
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "cpe464.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _NETWORKS_H_
#define _NETWORKS_H_

#define MAX_LEN 1500
#define START_SEQ_NUM 1

enum FLAG
{
	FNAME, DATA, FNAME_OK, FNAME_BAD, ACK, END_OF_FILE, CRC_ERROR
};

enum SELECT
  {
    SET_NULL, NOT_NULL
  };

typedef struct connection Connection;


struct connection
{
  int32_t sk_num;
  struct sockaddr_in remote;
  u_int32_t len;
};

int32_t udp_server();
int32_t udp_client_setup(char * hostname, uint16_t port_num, Connection * connection);
int32_t select_call(int32_t socket_num, int32_t seconds, int32_t microseconds, int32_t set_null);
int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int32_t recv_buf(uint8_t *buf, int32_t len, int32_t recv_sk_num, Connection * from_connection, uint8_t *flag, int32_t * seq_num);

#endif


#ifdef __cplusplus
}
#endif
