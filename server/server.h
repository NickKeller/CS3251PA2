#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#define BUFFER_SIZE 100

typedef struct _CONN_INFO{
	int socket;
	socklen_t addrlen;
	struct sockaddr *addr;
} CONN_INFO;

typedef struct _CLIENT{
	int port;
	char* ip;
	char challenge[64];
} CLIENT;


//function declarations
void print_use_and_exit(void);
int process(char* buffer, int sizeOfBuffer, int port,char ** response);
int process_response(char *buffer, int sizeOfBuffer, char ** response);
int process_request(char *buffer, int sizeOfBuffer, char ** response);
int put_file(char* buffer, int sizeOfBuffer, char** response);
int get_file(char* buffer, int sizeOfBuffer, char** response);
int close_connection(char* buffer,char** response);
int goodMessage(char* buffer);
char* convert_name(char* filename, char* prefix);
int timeout_recvfrom (int sock, char *buf, int bufSize, int flags, struct sockaddr *connection, socklen_t *addrlen,int timeoutinseconds,char* messageToSend, int numIter, int resend);
char* add_header_info(short packet_num,char num_bytes, char msg_type, char* packet);
char * generate_string(void);
char* doMD5(char* buffer);
unsigned *md5( const char *msg, int mlen);
void figureOutClient(struct sockaddr_in remaddr);

//CONN_INFO* setup_socket(char* port);


#endif//end udp-server.h
