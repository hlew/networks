/* used for network functions  */
/* Author: Dr. Hugh Smith
 * Used with permission and modified by
 * Hiram Riley Lew */

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
	DATA=1, RESEND=2, ACK=3, NAK=4, FNAME=6, FNAME_OK=7, FNAME_BAD=8,  END_OF_FILE=9, END_OF_FILE_ACK=10, CRC_ERROR=-1
};

enum SELECT
  {
    SET_NULL, NOT_NULL
  };

enum WINDOW {
	OPEN, CLOSED
};

enum BOOLEAN {
	FALSE, TRUE
};

typedef struct connection Connection;

struct connection
{
  int32_t sk_num;
  struct sockaddr_in remote;
  u_int32_t len;
  u_int32_t buf_size; 		  //buffer-size for data
  u_int32_t window_size;	  // window size
  int32_t base;				  // base of window
  int32_t win_status;		  // whether or not the window is OPEN or CLOSED
  uint32_t final_seq_num;	  // handles last window of ACKs
  uint32_t last_ack;		  // last ack to wait for
  BOOLEAN final_seq_sent;	  // EOF reached?
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
