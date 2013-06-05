/* CPE464 Networks - Program 3
 * Author: Hiram Riley Lew
 *
 * rcopy.cpp
 *
 * File Transfer Client:
 * Implements a sliding window selective repeat ARQ error correction
 * technique. The program uses cumulative ACKs (or RRs) and SREJ
 * to signal client state.
 *
 * 6/2/2013
 *
 * This code was adapted  with permission from Dr. Hugh Smith's
 * STOP-AND_WAIT code handouts.
 */


#include "rcopy.h"

using namespace std;



#define DEFAULT_WINDOW_SIZE 1
#define FILENAME_MAX_LEN 1000

enum status {
	RECVD, NOT_RECVD, SREJ
};

struct buffer {
	uint32_t seq_num;
	status recv_status;
	uint8_t data[MAX_LEN];
	int32_t len_read;
};


STATE filename(char *fname, int32_t buf_size, int32_t win_size);
STATE recv_data(int32_t output_file, buffer *window);
void check_args(int argc, char **argv);

Connection server;

int main (int argc, char **argv) {
	int32_t output_file = 0;
	int32_t select_count = 0;
	STATE state = FILENAME;
    check_args(argc, argv);
    int32_t win_size = atoi(argv[5]);
	buffer *window = new buffer [win_size];
	int i;

	for (i=0; i < win_size; i++) {
		window[i].recv_status = NOT_RECVD;
	}

    sendtoErr_init(atof(argv[4]), DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);

    state = FILENAME;
    while (state != DONE) {
    	switch (state) {
    	case FILENAME:

    		/*Every time we try to start/restart a connection get a new socket */
    	    if (udp_client_setup(argv[6], atoi(argv[7]), &server) < 0) {
    	        exit(1);
    	    }

    	    // 				 filename, buffer-size, window size
    	    state = filename(argv[1], atoi(argv[3]), atoi(argv[5]));

    	    /* If no response from the server then repeat sending filename (close socket) so you can open another */
    	    if (state == FILENAME)
    	    	close(server.sk_num);

    	    select_count++;
    	    if (select_count > 9) {
    	    	printf("Server unreachable, client terminating");
    	    	state = DONE;
    	    }
    	    break;

    	case FILE_OK:
    		select_count = 0;

    		if ((output_file = open(argv[2], O_CREAT|O_TRUNC|O_WRONLY, 0600)) <0) {

    			perror("File open");
    			state = DONE;
    		}
    		else {
    			server.base = START_SEQ_NUM;
    			server.window_size = win_size;
    			state = RECV_DATA;
    		}
    		break;

    	case RECV_DATA:
    		state = recv_data(output_file, &window[0]);
    		break;

    	case DONE:
    		break;

    	default:
    		printf("Error - in default state\n");
    		break;
    	}
    }

    return 0;
}

STATE filename(char *fname, int32_t buf_size, int32_t win_size) {
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t fname_len = strlen(fname) +1;
	int32_t recv_check = 0;

	memcpy(buf, &buf_size, 4);
	memcpy(&buf[4], &win_size, 4);
	memcpy(&buf[8], fname, fname_len);

	send_buf(buf, fname_len + 8, &server, FNAME, 0, packet);

	if(select_call(server.sk_num, 1, 0, NOT_NULL) == 1) {
		recv_check = recv_buf(packet, FILENAME_MAX_LEN, server.sk_num, &server, &flag, &seq_num);

		/* check for bit flip ... if so send the file name again */
		if (recv_check == CRC_ERROR) {
			return FILENAME;
		}

		if (flag == FNAME_BAD) {
			printf("File %s not found\n", fname);
			return(DONE);
		}

		return (FILE_OK);
	}
	return(FILENAME);
}


STATE recv_data(int32_t output_file, buffer *window) {
	int32_t seq_num = 0;
	uint8_t flag = 0;
	int32_t data_len = 0;
	uint8_t data_buf[MAX_LEN];
	uint8_t packet[MAX_LEN];
	static int32_t expected_seq_num = START_SEQ_NUM;

	if (select_call(server.sk_num, 10, 0, NOT_NULL)== 0) {
		printf("Timeout after 10 seconds, client done.\n");
		return DONE;
	}

	data_len = recv_buf(data_buf, MAX_LEN, server.sk_num, &server, &flag, &seq_num);

	/* do state RECV_DATA again if there is a CRC error (don't send ack, don't write data) */
	if (data_len == CRC_ERROR) {
		return RECV_DATA;
	}


	if (flag == END_OF_FILE) {
		//printf("Flag is %i\n", flag);
		printf("File done.\n");
		send_buf(packet, 1, &server, END_OF_FILE_ACK, seq_num + 1, packet);
		return DONE;
	}

	 /* discard if not in window */
	if (seq_num >= (server.base + server.window_size)) {
		printf("seq_num above window: should never be here\n");
		return RECV_DATA;
	}

	/* There are three possibilities for the received packet:
	 *
	 * 1. The frame is equal to the window base
	 * 		- The base should be moved forward to the next lowest non-received frame
	 * 		- All data between the current base and new base should be forwarded to
	 * 		  upper layer (written to file).
	 * 2. Frame is greater than the window base
	 * 		- The frame should be buffered
	 * 		- A SREJ ACK should be sent for every frame between window base and received
	 * 3. Frame is less than the window base
	 * 		- These frames should be ACK'ed with the same sequence number
	 */

	/* 1. */
	if (seq_num == expected_seq_num) {
		if (seq_num == server.base) {
			// send the frame to the upper layer
			server.base++;
			expected_seq_num = server.base; // or expected_seq_num++;
			if (write(output_file, & data_buf, data_len) < 0)
				perror("in recv data");
			else
				window[seq_num % server.window_size].recv_status = NOT_RECVD;

		}
		else {
			// buffer the frame to the window
			expected_seq_num = expected_seq_num + 1;
			window[seq_num % server.window_size].recv_status = RECVD;
			window[seq_num % server.window_size].len_read = data_len;
			window[seq_num % server.window_size].seq_num = seq_num;
			memcpy(window[seq_num % server.window_size].data, data_buf, data_len);
		}
		/*send ACK - RR base with cumulative ACK*/
		send_buf(packet, 1, &server, ACK, server.base, packet);

		return RECV_DATA;
	}

	/* 2. */
	else if (seq_num > expected_seq_num) {
		// Send a SREJ for every packet missing
		for (; expected_seq_num < seq_num; expected_seq_num++) {
			send_buf(packet, 1, &server, NAK, expected_seq_num, packet);
			window[seq_num % server.window_size].recv_status = SREJ;
		}
		expected_seq_num = seq_num + 1;
		window[seq_num % server.window_size].recv_status = RECVD;
		window[seq_num % server.window_size].len_read = data_len;
		window[seq_num % server.window_size].seq_num = seq_num;
		memcpy(window[seq_num % server.window_size].data, data_buf, data_len);
		return RECV_DATA;
	}

	/* 3. */
	else if (seq_num < expected_seq_num) {
		if (seq_num == server.base) {
			window[seq_num % server.window_size].recv_status = RECVD;
			window[seq_num % server.window_size].len_read = data_len;
			window[seq_num % server.window_size].seq_num = seq_num;
			memcpy(window[seq_num % server.window_size].data, data_buf, data_len);
			/* Find the next base where  new base equals lowest non received in window */
			/* Deliver each packet between old base and new base to file */
			for (; server.base < (server.base + server.window_size); server.base++) {
				if (window[server.base % server.window_size].recv_status == RECVD) {
					if (write(output_file, window[server.base % server.window_size].data,
						window[server.base % server.window_size].len_read) < 0)
					{
						perror("in recv data");
					}
					window[server.base % server.window_size].recv_status = NOT_RECVD;

				}
				else { break; }
			}

			/*send ACK - RR base with cumulative ACK*/
			send_buf(packet, 1, &server, ACK, server.base, packet);

		}

		/* ACK all packets received below window base */
		else if (seq_num < server.base) {
			send_buf(packet, 1, &server, ACK, seq_num, packet);
		}

		/* Buffer packet only -- received sequence is not equal to base */
		else if (seq_num > server.base) {
			window[seq_num % server.window_size].recv_status = RECVD;
			window[seq_num % server.window_size].len_read = data_len;
			window[seq_num % server.window_size].seq_num = seq_num;
			memcpy(window[seq_num % server.window_size].data, data_buf, data_len);
		}
		return RECV_DATA;
	}

	return RECV_DATA;

}





void check_args(int argc, char **argv)
{
    if (argc != 8) {
        cout << "usage: " << argv[0]
             << " remote-file local-file buffer-size error-percent"
             << " window-size remote-machine remote-port" << endl;
        exit(-1);
    }

    if (strlen(argv[1]) > 1000) {
    	printf("REMOTE filename too long. needs to be less than 1000 and is %d\n", strlen(argv[1]));
        exit(-1);

    }
    if (strlen(argv[2]) > 1000) {
    	printf("LOCAL filename too long. needs to be less than 1000 and is %d\n", strlen(argv[2]));
        exit(-1);

    }

    if (atoi(argv[3]) < 400 || atoi(argv[3]) > 1400) {
    	printf("Buffer size needs to be between 400 and 1400 and is %d\n", atoi(argv[3]));
        exit(-1);
    }

    if (atoi(argv[4]) < 0 || atoi(argv[4]) >= 1) {
    	printf("error rate needs to be between 0 and less than 1 and is: %d\n", atoi(argv[4]));
    }
}
