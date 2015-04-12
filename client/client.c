#include "client.h"

int DEBUG;
CONN_INFO* connection;
int connection_set;

int main(int argc, char *argv[]){
	int opts;
	if((opts = getopt(argc,argv,"d")) != -1){
		DEBUG = 1;
	}
	else{
		DEBUG = 0;
	}
	connection_set = 0;
	printf("Welcome to FxA file transfer application!\nPlease enter a command:");
	char buffer[300] = {0};
	char cmd[100] = {0};
	char arg1[100] = {0};
	char arg2[100] = {0};
	while(fgets(buffer,sizeof(buffer),stdin)){
		sscanf(buffer, "%s %s %s",cmd,arg1,arg2);
		//connect command
		if(strcmp(cmd,"connect") == 0){
			if(connection_set){
				printf("You already have an open connection. Please close that one before opening a new connection\n");
			}
			else{
				char* colon = strchr(buffer,':');
				if(colon != NULL || strlen(arg2) == 0 || strlen(arg1) == 0){
					printf("Example usage: 127.0.0.1 5000\n");
				}
				else{
					if(DEBUG) printf("IP:%s\nPort:%s\n",arg1,arg2);
					printf("Connecting to server...");
					if(connect_to_server(arg1,arg2)){
						printf("Done!\n");
					}
					else{
						printf("Could not connect to server. Please try again later");
					}
				}
			}
		}
		//get command, will only work if a connection is set up
		else if(strcmp(cmd,"get") == 0){
			if(!connection_set){
				printf("Must set up connection first\n");
			}
			
			else if(strlen(arg1) == 0){
				printf("Example usage: get file.c\n");
			}
			
			else if(fxa_get(arg1)){
				printf("File %s successfully downloaded!\n",arg1);
			}
			
			else{
				printf("Could not upload file\n");
			}
		}
		
		else if(strcmp(cmd,"put") == 0){
			if(!connection_set){
				printf("Must set up connection first\n");
			}
			
			else if(strlen(arg1) == 0){
				printf("Example usage: put file.c\n");
			}
			
			else if(fxa_put(arg1)){
				printf("File %s successfully uploaded!\n",arg1);
			}
			
			else{
				printf("Could not download file\n");
			}
		}
		
		else if(strcmp(cmd,"close") == 0){
			if(!connection_set){
				printf("Must set up connection first\n");
			}
			
			else{
				if(fxa_close()){
					printf("Connection successfully closed\n");
					connection_set = 0;
					bzero(connection,sizeof(CONN_INFO));
				}
			}
		}
		
		else if(strcmp(cmd,"quit") == 0){
			quit("Goodbye!");
		}
		
		else{
			printf("Available commands are: connect,get,put,close,quit\n");
		}
		
		bzero(buffer,sizeof(buffer));
		bzero(cmd,sizeof(cmd));
		bzero(arg1,sizeof(arg1));
		bzero(arg2,sizeof(arg2));
		
		printf("Please enter a command:");
	}
	
	
}


int connect_to_server(char *ip, char* port){
	//create a socket
	connection = setup_socket(ip,port);
	//created socket, now to make data and sendto my server
	char request[] = "REQ: Please Connect\n";
	
	int retval = -1;

	int numBytesSent = sendto(connection->socket,request,sizeof(request),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Message size: %d, num bytes sent: %d\n",sizeof(request),numBytesSent);
	//wait for a response
	char recvBuffer[100];
	int size = sizeof(recvBuffer);
	int numBytesRecv = timeout_recvfrom(connection->socket,recvBuffer,size,0,
								connection->remote_addr,&(connection->addrlen),2,request);
	if(numBytesRecv == -1){
		return 0;
	}

	if(DEBUG) printf("Received message: %s\n",recvBuffer);
	//compute md5 hash
	//find the first space
	char* space = strchr(recvBuffer,' ');
	int pos = space - recvBuffer;

	char* newLine = strchr(recvBuffer,'\n');
	int newLineSpace = newLine - recvBuffer;

	if(DEBUG) printf("Challenge string is:%s_blah\n",&recvBuffer[pos]);

	char* challenge = calloc(110,sizeof(char));
	memcpy(challenge,&recvBuffer[pos+1],newLineSpace-pos-1);

	if(DEBUG) printf("Filtered challenge string is:%s_blah\n",challenge);
	//now have the challenge string, now to compute the md5 hash
	char* md5Res = calloc(110,sizeof(char));
	md5Res = doMD5(challenge);

	//build the response buffer
	char* respHead = "RES: ";
	char* respBuffer = calloc(100,sizeof(char));

	strcat(respBuffer,respHead);
	strcat(respBuffer,md5Res);
	strcat(respBuffer,"\n");
	if(DEBUG) printf("Sending Response:\n%s_blah\n",respBuffer);
	//send off the response
	numBytesSent = sendto(connection->socket,respBuffer,strlen(respBuffer),0,
							connection->remote_addr,connection->addrlen);
	//wait for a response
	memset(recvBuffer,0,100);
	numBytesRecv = timeout_recvfrom(connection->socket,recvBuffer,sizeof(recvBuffer),0,
								connection->remote_addr,&(connection->addrlen),2,respBuffer);
	if(numBytesRecv == -1){
		return 0;
	}

	if(DEBUG) printf("%s\n",&recvBuffer[5]);
	if(strcmp(&recvBuffer[5],"Welcome to our service\n") == 0){
		connection_set = 1;
		return 1;
	}

	return 0;
}

int fxa_get(char* filename){

	char* fullpath = convert_name(filename,"get/");	
	
	FILE* file = fopen(filename,"wb");
	int numBytesWritten = 0;
	char* newFileName = calloc(1 + strlen(filename), sizeof(char));
	memcpy(newFileName,filename,strlen(filename));
	memcpy(&newFileName[strlen(filename)],"\n",1);
	char* message = calloc(5 + strlen(newFileName), sizeof(char));
	memcpy(message,"GET: ",5);
	memcpy(&message[5],newFileName,strlen(newFileName));
	if(DEBUG) printf("Message is: %s\n",message);
	//send the GET message
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	char* recvBuffer = calloc(1000,sizeof(char));
	while(1){
		int numBytesRecv = timeout_recvfrom(connection->socket,recvBuffer,1000,0,
									connection->remote_addr,&(connection->addrlen),2,message);
		if(strncmp(recvBuffer,"EOF",3) == 0){
			if(DEBUG) printf("Reached EOF\n");
			break;
		}
	
		if(DEBUG) printf("Num Bytes Recv: %d\nStrlen:%d\n",numBytesRecv,strlen(recvBuffer));
	//	if(DEBUG) printf("Message Received: %s\n",recvBuffer);
		//write the buffer into the file
		numBytesWritten = fwrite(recvBuffer,sizeof(char),numBytesRecv,file);
		if(numBytesWritten == numBytesRecv){
			//send an ACK
			if(DEBUG) printf("Successfully wrote %d bytes to file\n",numBytesWritten);
			sendto(connection->socket,"ACK",3,0,connection->remote_addr,connection->addrlen);
		}
		bzero(recvBuffer,1000);
		numBytesWritten = 0;
	}
	
	fclose(file);
	return 1;
}

int fxa_put(char* filename){
	char* newFileName = calloc(1 + strlen(filename), sizeof(char));
	memcpy(newFileName,filename,strlen(filename));
	memcpy(&newFileName[strlen(filename)],"\n",1);
	char* message = calloc(5 + strlen(newFileName), sizeof(char));
	memcpy(message,"PUT: ",5);
	memcpy(&message[5],newFileName,strlen(newFileName));
	if(DEBUG) printf("Message is: %s\n",message);
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	
	//wait for an ACK before proceeding
	char ACK[10] = {0};
	recvfrom(connection->socket,ACK,10,0, connection->remote_addr,&(connection->addrlen));
	
	int result = 1;
	if(DEBUG) printf("File to put is: %s_blah\n",filename);
	
	//time to send the file
	char* fileFolder = "put/";
	char* fullpath = calloc(4 + strlen(filename),sizeof(char));
	memcpy(fullpath,fileFolder,4);
	memcpy(&fullpath[4],filename,strlen(filename));
	if(DEBUG) printf("Opening file:%s_blah\n",fullpath);
	
	//this code came from a lc3 emulator that I wrote in my CS2110 class. It was used to read an object file
	FILE *file = fopen(fullpath, "rb");
	if(file == NULL){
		printf("Error, %s doesn't exist!\n",filename);
		return 0;
	}
	//allocate space for the shorts to be read in
	char packet[1000];
	int numElements = 0;
	char bstop = 0;
	//run the loop until end of file
	while(!bstop){
		//read in the starting address and number of elements
		numElements = fread(packet, sizeof(char), 1000, file);
		//check for EOF
		if(numElements < 1000){
			//EOF reached, break
			bstop = 1;
		}
		//send off the file, wait for an ACK
		int numBytesSent = sendto(connection->socket,packet,strlen(packet),0,
							  connection->remote_addr,connection->addrlen);
		
		if(DEBUG) printf("Num bytes sent: %d\n",numBytesSent);
		if(DEBUG) printf("Message Sent: %s\n",packet);
		
		char recvBuffer[100];
		int numBytesRecv = recvfrom(connection->socket,recvBuffer,100,0, connection->remote_addr,&(connection->addrlen));
		if(strncmp(recvBuffer,"ACK",3) == 0){
			if(DEBUG) printf("Received ACK\n");
		}
		numElements = 0;
		bzero(packet,1000);
		bzero(recvBuffer,100);
	}
	
	fclose(file);
	//send the EOF message
	sendto(connection->socket,"EOF",3,0, connection->remote_addr,connection->addrlen);
	return result;
}

int fxa_close(){
	char* message = "FIN\n";
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	return 1;
}

void print_use_and_exit(){
	fprintf(stderr,"Usage:  client-udp [-d] ip port username password\n\n"); 
	exit (EXIT_FAILURE);
}

CONN_INFO* setup_socket(char* host, char* port){
	//this code was from my CS2200 class, for the networking project we did
	struct addrinfo *connections, *conn = NULL;
	struct addrinfo info;
	memset(&info, 0, sizeof(struct addrinfo));
	int sock = 0;

	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_DGRAM;
	info.ai_protocol = IPPROTO_UDP;
	getaddrinfo(host, port, &info, &connections);

	/*for loop to determine correct addr info*/ 
	for(conn = connections; conn != NULL; conn = conn->ai_next){
		sock = socket(conn->ai_family, conn->ai_socktype, conn->ai_protocol);
		if(sock <0){
			continue;
		}
		break;
	}
	if(conn == NULL){
		perror("Failed to find and bind a socket\n");
		return NULL;
	}
	CONN_INFO* conn_info = malloc(sizeof(CONN_INFO));
	conn_info->socket = sock;
	conn_info->remote_addr = conn->ai_addr;
	conn_info->addrlen = conn->ai_addrlen;
	return conn_info;
}

char* doMD5(char* buffer){
	//concat the 3 strings
	unsigned char *temp = calloc(strlen(buffer),sizeof(char));
	strcat(temp,buffer);
	char* output = md5(temp,strlen(temp));
	char* value = calloc(16,sizeof(char));
	memcpy(value,output,16);
	if(DEBUG) printf("MD5 hash is:%s_blah\nLength is %d\nStrlen of temp:%d\n",value,strlen(value),strlen(temp));
	return value;
}


int timeout_recvfrom (int sock, char *buf, int bufSize, int flags, struct sockaddr *connection, socklen_t *addrlen,int timeoutinseconds,char* messageToSend)
{
    fd_set socks;
    struct timeval t;
    t.tv_usec=0;
    FD_ZERO(&socks);
    FD_SET(sock, &socks);
    t.tv_sec = timeoutinseconds;
    if(DEBUG) printf("Starting Select\n");
    int tries = 0;
    int numBytesRecv = 0;
    while(select(sock + 1, &socks, NULL, NULL, &t) <= 0){
    	//timeout, send again
    	tries++;
    	//don't try more than 5 times
    	if(tries == 5){
	    	return -1;
    	}
    	if(DEBUG) printf("Connection Timeout - Trying again\n");
    	t.tv_sec = timeoutinseconds;
    	int res = 0;
    	while(res <= 0){
    		res = sendto(sock,messageToSend,strlen(messageToSend),0,connection,*addrlen);
			if(DEBUG) printf("Sent %d bytes\n",res);
			if(res == -1){
				printf("Error\n");
			}    		
    	}
    	if(DEBUG) printf("Done Sending\n");
    	FD_ZERO(&socks);
	    FD_SET(sock, &socks);
    }
    if(DEBUG) printf("Done with select\n");
    numBytesRecv = recvfrom(sock, buf, bufSize, 0, connection, addrlen);
    if (numBytesRecv !=-1)
        {
        return numBytesRecv;
        }
    else
        return 0;
}


char* convert_name(char* filename, char* prefix){
	char* fullpath = calloc(strlen(prefix) + strlen(filename),sizeof(char));
	memcpy(fullpath,prefix,4);
	memcpy(&fullpath[4],filename,strlen(filename));
	return fullpath;
}

void quit(char* message){
	printf("%s\n",message);
	exit(0);
}
