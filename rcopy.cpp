#include "rcopy.h"

using namespace std;

Connection server;

int main (int argc, char **argv) {
	int32_t output_file = 0;
	int32_t select_count = 0;
	STATE state = FILENAME;
    check_args(argc, argv);
    
    sendtoErr_init(atof(argv[4]), DROP_OFF, FLIP_OFF, DEBUG_OFF, RSEED_ON);

    state = FILENAME;
    while (state != DONE) {
    	switch (state) {
    	case FILENAME:

    		/*Every time we try to start/restart a connection get a new socket */
    	    if (udp_client_setup(argv[6], atoi(argv[7]), &server) < 0) {
    	        exit(1);
    	    }

    	    state = filename(argv[1], atoi(argv[3]));

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
    			state = RECV_DATA;
    		}
    		break;

    	case RECV_DATA:
    		state = recv_data(output_file);
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

STATE filename(char *fname, int32_t buf_size) {
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	int32_t seq_num = 0;
	int32_t fname_len = strlen(fname) +1;
	int32_t recv_check = 0;

	memcpy(buf, &buf_size, 4);
	memcpy(&buf[4], fname, fname_len);

	send_buf(buf, fname_len + 4, &server, FNAME, 0, packet);

	if(select_call(server.sk_num, 1, 0, NOT_NULL) == 1) {
		recv_check = recv_buf(packet, 1000, server.sk_num, &server, &flag, &seq_num);

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


STATE recv_data(int32_t output_file) {
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

	data_len = recv_buf(data_buf, 1400, server.sk_num, &server, &flag, &seq_num);

	/* do state RECV_DATA again if there is a CRC error (don't send ack, don't write data) */
	if (data_len == CRC_ERROR) {
		return RECV_DATA;
	}

	/*send ACK*/
	send_buf(packet, 1, &server, ACK, 0, packet);

	if (flag == END_OF_FILE) {
		printf("File done\n");
		return DONE;
	}

	if (seq_num == expected_seq_num)
	{
		expected_seq_num++;
		write(output_file, & data_buf, data_len);
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
    	printf("error rate needs to be between 0 and less than 1 and is: %\n", atoi(argv[4]));
    }
}
