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

#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

typedef struct _CONN_INFO{
	int socket;
	socklen_t addrlen;
	struct sockaddr *remote_addr;
} CONN_INFO;

//Message Type Definitions
#define REQ '0'
#define CHA '1'
#define RES '2'
#define ACK '3'
#define NAK '4'
#define DTA '5'
#define LST '6'
#define EFI '7'
#define FIN '8'

//function declarations
void print_use_and_exit(void);
CONN_INFO* setup_socket(char* host, char* port);
char* doMD5(char* buffer);
int connect_to_server();
int fxa_get(char* filename);
int fxa_put(char* filename);
int fxa_close(void);
char* convert_name(char* filename, char* prefix);
unsigned *md5( const char *msg, int mlen);
int timeout_recvfrom (int sock, char *buf, int bufSize, int flags, struct sockaddr *connection, socklen_t *addrlen,int timeoutinseconds,char* messageToSend, int numIter, int resend);
void quit(char* message);
#endif//end udp-client.h
