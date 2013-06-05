/* CPE464 Networks - Program 3
 * Author: Hiram Riley Lew
 *
 * server.cpp
 *
 * File Transfer Server:
 * Implements a sliding window selective repeat ARQ error correction
 * technique. The program uses cumulative ACKs (or RRs) and SREJ
 * to signal client state.
 *
 * Handles multiple clients through the use of multiprocessing fork()
 *
 * 6/2/2013
 *
 * This code was adapted with permission from Dr. Hugh Smith's
 * STOP-AND_WAIT code handouts.
 */



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

#define DEFAULT_WINDOW_SIZE 1
#define FILENAME_MAX_LEN 1000

using namespace std;


enum State {
	START, DONE, FILENAME, SEND_DATA, WAIT_ON_ACK, WINDOW_CLOSED_WAIT, TIMEOUT_ON_ACK, EO_FILE
};

typedef enum State STATE;

enum status {
	SENT, NOT_SENT, SREJ, TIMEOUT
};


struct buffer {
	uint32_t seq_num;
	status send_status;
	uint8_t data[MAX_LEN];
	int32_t len_read;
	int32_t resend_count;
};

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection * client);
STATE filename(Connection *client, uint8_t *buf, int32_t recv_len, int32_t *data_file, int32_t *buf_size, int32_t *win_size);
STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num, buffer *window);
STATE end_of_file(Connection *client, int32_t *eof_count);
STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len, buffer *window);
STATE wait_on_ack(Connection *client, buffer *window);
WINDOW check_window_state(Connection *client, buffer *window);



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

    sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);


    server_sk_num = udp_server();
    cout << "Server socket descriptor: " << server_sk_num <<endl;

    while (1) {

    	if (select_call(server_sk_num, 1, 0, NOT_NULL) == 1) {
    		recv_len = recv_buf(buf, FILENAME_MAX_LEN, server_sk_num, &client, &flag, &seq_num);
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
    			//printf("processed wait\n");
    		}
    		// printf("after process wait...back to select\n");
    	}
    }

    return 0;
}

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client) {
	STATE state = START;
	int32_t data_file = 0;
	int32_t eof_count = 0;
	int32_t packet_len = 0;
	uint8_t packet[MAX_LEN];
	int32_t buf_size = 0;
	int32_t win_size = 0;
	int32_t seq_num = START_SEQ_NUM;
	buffer *window = new buffer [DEFAULT_WINDOW_SIZE];


	while (state != DONE) {
		switch(state) {
		case START:
			state = FILENAME;
			break;

		case FILENAME:
			seq_num = START_SEQ_NUM;
			delete [] window;
			state = filename(client, buf, recv_len, &data_file, &buf_size, &win_size);
			client->base = seq_num;
			client->window_size = win_size;
			client->final_seq_sent = FALSE;
			window = new buffer [win_size];
			break;

		case SEND_DATA:
			state = send_data(client, packet, &packet_len, data_file, buf_size, &seq_num, &window[0]);
			break;

		case WAIT_ON_ACK:
			state = wait_on_ack(client, &window[0]);
			break;

		case TIMEOUT_ON_ACK:
			state = timeout_on_ack(client,packet,packet_len, &window[0]);
			break;

		case EO_FILE:
			state = end_of_file(client, &eof_count);
			break;

		case DONE:
			delete [] window;
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

STATE send_data(Connection *client, uint8_t *packet, int32_t *packet_len,
				int32_t data_file, int buf_size, int32_t *seq_num, buffer *window) {
	uint8_t buf[MAX_LEN];
	int32_t len_read = 0;
	int32_t base = client->base;
	int32_t win_size = client->window_size;
	int i;

	// The window closes during final window of frames sent. Don't update the client->base
	if (client->final_seq_sent == TRUE) {
		base = client->final_seq_num;

		// If last packet has been RR'ed then send EOF flag
		if (client->final_seq_num == client->last_ack) {
			return EO_FILE;
		}
	}


	if (client->final_seq_sent == FALSE) {
		// Fill buffer
		while ( (*seq_num) < (base + win_size)) {
			window[(*seq_num) % win_size].len_read = read(data_file, window[(*seq_num) % win_size].data, buf_size);
			window[(*seq_num) % win_size].send_status = NOT_SENT;
			window[(*seq_num) % win_size].seq_num = (*seq_num);
			window[(*seq_num) % win_size].resend_count = 0;
			client->win_status = OPEN;
			(*seq_num)++;
		}
	}
	//else {printf("SEQ: %i\n", (*seq_num));}

	// Resend Timeout on Ack
	if (window[base % win_size].send_status == TIMEOUT) {
		memcpy(buf, window[base % win_size].data, window[base % win_size].len_read);
		(*packet_len) = send_buf(buf, window[base % win_size].len_read, client, RESEND, window[base % win_size].seq_num, packet);
		window[base % win_size].send_status = SENT;
		window[base % win_size].resend_count++;
		//printf("resend count for frame %i is: %i\n", window[base % win_size].seq_num, window[base % win_size].resend_count);
		if(window[base % win_size].resend_count > 9) {
			printf("Sent data 10 times no ACK client session terminated\n");
			return(DONE);
		}
		return WAIT_ON_ACK;
	}


	// Resend ALL packets in the window which have the SREJ flag (lost or errored packets)
	// Keep track of the number of times resent.
	for (i = base; i < (base + win_size); i++) {
		if (window[i % win_size].send_status == SREJ) {
			memcpy(buf, window[i % win_size].data, window[i % win_size].len_read);
			(*packet_len) = send_buf(buf, window[i % win_size].len_read, client, RESEND, window[i % win_size].seq_num, packet);
			window[i % win_size].send_status = SENT;
			window[i % win_size].resend_count++;
			//printf("resend count for frame %i is: %i\n", window[i % win_size].seq_num, window[i % win_size].resend_count);
			if(window[i % win_size].resend_count > 9) {
				printf("Sent data 10 times no ACK client session terminated\n");
				return(DONE);
			}
			//printf("Resent -- -- Sequence number: %i\n", window[i % win_size].seq_num);
		}
	}

	// Find the first NOT_SENT packet in the window
	// If there are no packets NOT_SENT then close the window.
	for (i = base; i <= (base + win_size); i++) {
		if (i == (base + win_size)) {
			client->win_status = CLOSED;
			//printf("window closed\n");
			return WAIT_ON_ACK;
		}

		if (window[i % win_size].send_status == NOT_SENT) {
			memcpy(buf, window[i % win_size].data, window[i % win_size].len_read);
			break;
		}
	}


	switch(window[i % win_size].len_read)
	{
	case -1:
		perror("send_data, read err");
		return DONE;
		break;

	case 0:
		client->win_status = CLOSED;
		//printf("Final Packet Sent\n");
		client->final_seq_sent = TRUE;
		client->final_seq_num = client->base;
		client->last_ack = window[i % win_size].seq_num;
		return WAIT_ON_ACK;
		break;

	default:
		(*packet_len) = send_buf(buf, window[i % win_size].len_read, client, DATA, window[i % win_size].seq_num, packet);
		printf("Sequence number sent: %i\n", window[i % win_size].seq_num);
		window[i % win_size].send_status = SENT;
		return WAIT_ON_ACK;
		break;
	}

}

STATE wait_on_ack(Connection *client, buffer *window) {
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = 1000;
	uint8_t flag = 0;
	int32_t seq_num = 0;

	// Non blocking select call if window is open.
	if (client->win_status == OPEN) {
		if (select_call(client->sk_num, 0, 0, NOT_NULL) != 1) {
			return (SEND_DATA);
		}
		else {
			// do nothing and process packet below
		}
	}
	// Blocking select call if window is open.
	else if (client->win_status == CLOSED) {
		if (select_call(client->sk_num, 1, 0, NOT_NULL) != 1) {
			return (TIMEOUT_ON_ACK);
		}
	}
	else {
		printf("In wait_on_ack but win_status is not OPEN or CLOSED (this should never happen).\n");
	}

	crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

	if (crc_check == CRC_ERROR)
		return WAIT_ON_ACK;

	if (flag == ACK) {
		// Handle the final window (different behavior because client->base does not update
		if (client->final_seq_sent == TRUE)
			client->final_seq_num = seq_num;
		else
			client->base = seq_num;
		/*ack is good so check for more acks */
		return WAIT_ON_ACK;
	}

	if (flag == NAK) {
		window[seq_num % client->window_size].send_status = SREJ;
		/*ack is good so reset count and then go send some more data */
		return SEND_DATA;
	}

//	if (flag == END_OF_FILE_ACK) {
//		printf("File Transfer Complete\n");
//		return DONE;
//	}

	if (flag != ACK || flag != NAK) {
		printf("In wait_on_ack but its not an ACK flag (this should never happen) is: %d\n", flag);
		exit(-1);
	}




	return SEND_DATA;
}

STATE end_of_file(Connection *client, int32_t *eof_count) {
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t recv_check = 0;

	if ((*eof_count) > 9) {
		printf("No response from client after 10 tries. Disconnecting...");
		return DONE;
	}

	send_buf(buf, 8, client, END_OF_FILE, 777777777, packet);
	(*eof_count)++;

	if(select_call(client->sk_num, 1, 0, NOT_NULL) == 1) {
		recv_check = recv_buf(packet, 1000, client->sk_num, client, &flag, &seq_num);

		/* check for bit flip ... if so send the file name again */
		if (recv_check == CRC_ERROR) {
			return EO_FILE;
		}

		if (flag == END_OF_FILE_ACK) {
			printf("File Transfer Complete\n");
			return DONE;
		}
	}
	return(EO_FILE);
}



WINDOW get_window_state(Connection *client, buffer *window) {
	int i;
	int32_t base =  client->base;
	int32_t win_size = client->window_size;
	for (i = base; i < (base + win_size); i++) {
		if (window[i % win_size].send_status == NOT_SENT)
			return OPEN;
	}
	return CLOSED;
}


STATE timeout_on_ack(Connection *client, uint8_t *packet, int32_t packet_len, buffer *window) {
	uint32_t base = client->base;

	// During the final window of data, the window closes and client->base is not updated
	// Because of this client->final_seq_num is updated instead to inform send_data when to send the final packet.
	if (client->final_seq_sent == TRUE) {
		base = client->final_seq_num;
	}

	window[base % client->window_size].send_status = TIMEOUT;

	return SEND_DATA;
}
