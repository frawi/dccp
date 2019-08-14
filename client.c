#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>	//basename

#include "dccp.h"
#include "common.h"

#define BUFFER_SIZE 4096

struct tfrc_tx_info {
	uint64_t tfrctx_x;
	uint64_t tfrctx_x_recv;
	uint32_t tfrctx_x_calc;
	uint32_t tfrctx_rtt;
	uint32_t tfrctx_p;
	uint32_t tfrctx_rto;
	uint32_t tfrctx_ipi;
};

int error_exit(const char *str)
{
	perror(str);
	exit(errno);
}

void sendfile(FILE *fp, int socket_fd);

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("Usage: ./client <server address> <file name> \n");
		exit(-1);
	}
	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(PORT),
	};

	if (!inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr)) {
		printf("Invalid address %s\n", argv[1]);
		exit(-1);
	}

	int socket_fd = socket(AF_INET, SOCK_DCCP, IPPROTO_DCCP);
	if (socket_fd < 0)
		error_exit("socket");

	if (setsockopt(socket_fd, SOL_DCCP, DCCP_SOCKOPT_SERVICE, &(int){htonl(SERVICE_CODE)}, sizeof(int)))
		error_exit("setsockopt(DCCP_SOCKOPT_SERVICE)");

	if (setsockopt(socket_fd, SOL_DCCP, DCCP_SOCKOPT_CCID, &(uint8_t){3}, sizeof(uint8_t)))
		error_exit("setsockopt(DCCP_SOCKOPT_CCID)");

	if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
		error_exit("connect");
	/*
	// Get the maximum packet size
	uint32_t mps;
	socklen_t res_len = sizeof(mps);
	if (getsockopt(socket_fd, SOL_DCCP, DCCP_SOCKOPT_GET_CUR_MPS, &mps, &res_len))
		error_exit("getsockopt(DCCP_SOCKOPT_GET_CUR_MPS)");
	printf("Maximum Packet Size: %d\n", mps);
	*/

	// Get the file name
	char *filename = basename(argv[2]);
	if(filename == NULL) {
		error_exit("File is not exist");
	}
	printf("The File Name is: %s\n", filename);

	// Send the file name (size of name should be same as buffer size)
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	strncpy(buffer, filename, strlen(filename));
	if(send(socket_fd, buffer, BUFFER_SIZE, 0) < 0) {
		error_exit("Failed to send file name");
	}
	
	// Open the file to be sen
	FILE *fp = fopen(argv[2], "rb");
	if(fp == NULL) {
		error_exit("Failed to open file");
	}

	// read and send the file
	sendfile(fp, socket_fd);
	puts("Send successfully");
		
	// Close the file
	fclose(fp);

	close(socket_fd);
	return 0;
}

void sendfile(FILE *fp, int socket_fd) {
	struct tfrc_tx_info tx_info;
	unsigned int tx_info_size;

	int n = 0;
	int total = 0;
	char sendline[BUFFER_SIZE];
	bzero(sendline, BUFFER_SIZE);

	while(n = fread(sendline, sizeof(char), BUFFER_SIZE, fp) > 0) {
		if (send(socket_fd, sendline, n, 0) < 0) {
			perror("Failed to send file");
			exit(0);
		}
		bzero(sendline, BUFFER_SIZE); // clear the send buffer

		total += n;
		fprintf(stderr, "total: %d\n", total);

		tx_info_size = sizeof(tx_info);
		if (getsockopt(socket_fd, SOL_DCCP, DCCP_SOCKOPT_CCID_TX_INFO, &tx_info, &tx_info_size)) {
			perror("getsockopt(DCCP_SOCKOPT_CCID_TX_INFO)");
		} else {
			fprintf(stderr, "rate: %lu rtt: %u loss: %u\n", tx_info.tfrctx_x, tx_info.tfrctx_rtt, tx_info.tfrctx_p);
		}
	}
}
