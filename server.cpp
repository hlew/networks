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
#include <sys/wait.h>

#include <cstdlib>
#include <iostream>
#include "networks.h"

#define WINDOW_SIZE 1

using namespace std;


enum State {
	START, DONE, FILENAME, SEND_DATA, WAIT_ON_ACK, TIMEOUT_ON_ACK
};

typedef enum State STATE;

enum status {
	SENT, NOT_SENT
};

struct buffer {
	status send_status;
	uint8_t data[MAX_LEN];
};

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection * client);
STATE filename(Connection *client, uint8_t *buf, int32_t recv_len, int32_t *data_file, int32_t *buf_size, int32_t *win_size);
STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num, buffer *window);
STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len);
STATE wait_on_ack(Connection *client);




int main (int argc, char **argv) {
	int32_t server_sk_num = 0;
	pid_t pid = 0;
	int status =0;
	uint8_t buf[MAX_LEN];
	Connection client;
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t recv_len = 0;

    struct sockaddr_in local;  // socket address for us
    uint32_t len = sizeof(local); // length of local address

    if (argc != 2) {
        cout << "usage: " << argv[0] << " error-percent"
             << endl;
        exit(-1);
    }

    sendtoErr_init(atof(argv[1]), DROP_OFF, FLIP_OFF, DEBUG_OFF, RSEED_ON);


    server_sk_num = udp_server();
    cout << "Server socket descriptor: " << server_sk_num <<endl;

    while (1) {

    	if (select_call(server_sk_num, 1, 0, NOT_NULL) == 1) {
    		recv_len = recv_buf(buf, 1000, server_sk_num, &client, &flag, &seq_num);
    		if (recv_len != CRC_ERROR) {

    			/* fork will go here */
    			if((pid=fork()) < 0) {
    				perror("fork");
    				exit(-1);
    			}

    			//process child
    			if(pid == 0) {
    				process_client(server_sk_num, buf, recv_len,&client);
    				exit(0);
    			}
    		}

    		//check to see if any children quit
    		while (waitpid(-1, &status, WNOHANG) > 0)
    		{
    			printf("processed wait\n");
    		}
    		// printf("after process wait...back to select\n");
    	}
    }

    return 0;
}

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client) {
	STATE state = START;
	int32_t data_file = 0;
	int32_t packet_len = 0;
	uint8_t packet[MAX_LEN];
	int32_t buf_size = 0;
	int32_t win_size = 0;
	int32_t seq_num = START_SEQ_NUM;
	buffer *window;


	while (state != DONE) {
		switch(state) {
		case START:
			state = FILENAME;
			break;

		case FILENAME:
			seq_num = 1;
			state = filename(client, buf, recv_len, &data_file, &buf_size, &win_size);
			break;

		case SEND_DATA:
			window = new buffer [win_size];
			state = send_data(client, packet, &packet_len, data_file, buf_size, &seq_num, &window[0]);
			break;

		case WAIT_ON_ACK:
			state = wait_on_ack(client);
			break;

		case TIMEOUT_ON_ACK:
			state = timeout_on_ack(client,packet,packet_len);
			break;

		case DONE:
			break;

		default:
			printf("In default. should not be here. \n");
			state = DONE;
			break;

		}
	}
	return;
}

STATE filename(Connection *client, uint8_t *buf, int32_t recv_len, int32_t * data_file, int32_t *buf_size, int32_t *win_size) {
	uint8_t response[1];
	char fname[MAX_LEN];

	memcpy(buf_size, buf, 4);
	memcpy(win_size, &buf[4], 4);
	memcpy(fname, &buf[8], recv_len-8);

	/*Create client socket to allow for processing this particular client */

	if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("filename, open client socket");
		exit(-1);
	}

	if (((*data_file) = open(fname, O_RDONLY)) < 0) {
		send_buf(response, 0 , client, FNAME_BAD, 0, buf);
		return DONE;
	}
	else
	{
		send_buf(response, 0, client, FNAME_OK, 0, buf);
		return SEND_DATA;
	}
}

STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int buf_size, int32_t *seq_num, buffer *window) {
	uint8_t buf[MAX_LEN];
	int32_t len_read = 0;

	len_read = read(data_file, buf, buf_size);

	switch(len_read)
	{
	case -1:
		perror("send_data, read err");
		return DONE;
		break;

	case 0:
		(*packet_len) = send_buf(buf, 1, client, END_OF_FILE, *seq_num, packet);
		printf("File Transfer Complete\n");
		return DONE;
		break;

	default:
		(*packet_len) = send_buf(buf, len_read, client, DATA, *seq_num, packet);
		printf("Sequence number sent: %i\n",(*seq_num));
		(*seq_num)++;
		return WAIT_ON_ACK;
		break;
	}
}

STATE wait_on_ack(Connection *client) {
	static int32_t send_count = 0;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = 1000;
	uint8_t flag = 0;
	int32_t seq_num = 0;

	send_count++;
	if(send_count > 5) {
		printf("Sent data 5 times no ACK client session terminated\n");
		return(DONE);
	}

	if (select_call(client->sk_num, 1, 0, NOT_NULL) != 1) {
		return (TIMEOUT_ON_ACK);
	}

	crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

	if (crc_check == CRC_ERROR)
		return WAIT_ON_ACK;

	if (flag != ACK) {
		printf("In wait_on_ack but its not an ACK flag (this should never happen) is: %d\n", flag);
		exit(-1);
	}

	/*ack is good so reset count and then go send some more data */
	send_count = 0;

	return SEND_DATA;
}

STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len) {
	if(sendtoErr(client->sk_num, packet, packet_len, 0 , (struct sockaddr *) &(client->remote), client->len) < 0) {
		perror("timeout_on_ack_sendto");
		exit(-1);
	}

	return WAIT_ON_ACK;
}
