#include "server.h"

//usernames and passwords
char *usernames[] = {"user1","user2","user3"};
char *passwords[] = {"pass1","pass2","pass3"};

CLIENT* client1;
CLIENT* client2;
CLIENT* current_client;
CONN_INFO *connection;
char* REQ_MSG = "REQ";
char* RES_MSG = "RES";

int DEBUG;
char *challenge;

int main(int argc, char *argv[]){
	
	int offset = 0;
	if(getopt(argc,argv,"d") != -1){
		DEBUG = 1;
		offset = 1;
	}
	else{
		DEBUG = 0;
	}

	//must have 3 arguments - the port number to bind to, the ip address and port of NetEmu
	if(argc < (2+offset)){
		print_use_and_exit();
	}
	
	//initialize the clients
	client1 = calloc(1,sizeof(CLIENT));
	client2 = calloc(1,sizeof(CLIENT));
	current_client = calloc(1,sizeof(CLIENT));
	challenge = calloc(64,sizeof(char));
	int port = atoi(argv[1+offset]);
	//only accept odd port numbers
	if(port % 2 != 1){
		printf("Error, must use odd port number\n");
		print_use_and_exit();
	}
	//create a socket to bind on

	//I got this from: https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
	int sock;
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("Cannot create socket\n");
		return 0;
	}
	connection = calloc(1, sizeof(CONN_INFO));
	printf("Binding server on port %d\n",port);
	struct sockaddr_in myaddr;
	struct sockaddr_in remaddr;
	socklen_t addrlen = sizeof(remaddr);
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(port);
	int bindResult = bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr));
	if(bindResult == 0){
		printf("Ready to receive on port %d\n",port);      
	}
	else{
		perror("Binding failed");
		return 0;
	}
	
	
	//now, infinitely wait and process requests as they come in
	while(1){
		if(DEBUG) printf("-----------------Waiting to receive-----------------\n");
		if(DEBUG) printf("Allocating Buffers\n");
		char* buffer = calloc(BUFFER_SIZE,sizeof(char));
		char* response = calloc(BUFFER_SIZE,sizeof(char));
		int sizeReceived = 0;
		//allow a maximum of 1024 bytes for this login process, don't need more than that
		sizeReceived = recvfrom(sock,buffer,BUFFER_SIZE,0, (struct sockaddr*)&remaddr,&addrlen);
		connection->socket = sock;
		connection->addrlen = addrlen;
		connection->addr = (struct sockaddr*)&remaddr;
		if(DEBUG) printf("Message Received:%s\nMessage size:%d\nPort Received From:%d\nIP Received from: %s\n",buffer,sizeReceived,remaddr.sin_port,inet_ntoa(remaddr.sin_addr));
		//figure out the client
		//figureOutClient(remaddr);
		//process the request
		if(process(buffer,sizeof(buffer),port,&response)){
			if(DEBUG) printf("Sending Response:\n%s\n",response);
			//send the response
			int numBytesSent = 0;
			if((numBytesSent = sendto(sock, response,BUFFER_SIZE,0,(struct sockaddr*)&remaddr,addrlen)) < 0){
				perror("Failed on sending response");
				return 0;
			}
		}
		//clear the current client, we're done
		current_client->port = -1;	
		bzero(connection,sizeof(CONN_INFO));		
	}
	return 0;
}

void print_use_and_exit(){
	fprintf(stderr,"Usage:  server-udp [-d] port NetEmuAddr NetEmuPort\n\n");
	exit (EXIT_FAILURE);
}

int process(char* message,int sizeOfBuffer, int port, char ** response){
	//make sure there is a newline character, that signifies the end of a request
	if(DEBUG) printf("Starting messsage processing\n");
	//find the newLine
	char* newLine = strchr(message,'\n');
	if(newLine != NULL){
		//valid response, can actually process it
		int posOfNewLine = newLine - message;
		char* buffer = calloc(posOfNewLine,sizeof(char));
		memcpy(buffer,message,posOfNewLine);
		if(DEBUG) printf("Valid message, processing:%s_blah\n",buffer);
		//first 3 chars are the type of request from a client(Either REQ or RES)
		if(strncmp(buffer,"REQ",3) == 0){
			return process_request(&buffer[5],posOfNewLine,response);
		}
		if(strncmp(buffer,"RES",3) == 0){
			return process_response(&buffer[5],posOfNewLine,response);
		}
		if(strncmp(buffer,"GET",3) == 0){
			return put_file(&buffer[5],posOfNewLine,response);
		}
		if(strncmp(buffer,"PUT",3) == 0){
			return get_file(&buffer[5],posOfNewLine,response);
		}
		return 1;
	}
	else{
		return 0;
	}
}

int get_file(char* buffer, int sizeOfBuffer, char** response){
	//convert filename to get/filename
	char* fullpath = convert_name(buffer,"get/");
	
	//send off the ACK
	sendto(connection->socket,"ACK",3,0,connection->addr,connection->addrlen);
	
	//open the file in binary mode for writing
	FILE* file = fopen(fullpath,"wb");
	char* recvBuffer = calloc(1000,sizeof(char));
	int numBytesWritten = 0;
	while(1){
		int numBytesRecv = timeout_recvfrom(connection->socket,recvBuffer,1000,0,
									connection->addr,&(connection->addrlen),2,"ACK");
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
			sendto(connection->socket,"ACK",3,0,connection->addr,connection->addrlen);
		}
		bzero(recvBuffer,1000);
		numBytesWritten = 0;
	}
	
	fclose(file);
	return 1;
}

int put_file(char* buffer, int sizeOfBuffer, char** response){
	int result = 1;
	int windowSize = 0;
	//find the space to capture the window size
	char* posOfSpace = strchr(buffer,' ');
	int sizeOfFileName = posOfSpace - buffer;
	//grab the window size
	char* windowParam = calloc(5,sizeof(char));
	memcpy(windowParam, &buffer[sizeOfFileName+1], 5);
	if(strncmp(&buffer[sizeOfFileName+1],"WIN:",4) == 0){
		windowSize = atoi(&windowParam[4]);
		if(DEBUG) printf("Window Size: %d\n",windowSize);
	}
	char* filename = calloc(sizeOfFileName,sizeof(char));
	memcpy(filename,buffer,sizeOfFileName);
	if(DEBUG) printf("File to put is: %s_blah\n",filename);
	char* message = calloc(4 + strlen(filename),sizeof(char));
	memcpy(message,"EOF ",4);
	memcpy(&message[4],filename,strlen(filename));
	*response = message;
	
	//time to send the file
	char* fullpath = convert_name(filename,"put/");
	if(DEBUG) printf("Opening file:%s_blah\n",fullpath);
	
	//this code came from a lc3 emulator that I wrote in my CS2110 class. It was used to read an object file
	FILE *file = fopen(fullpath, "rb");
	if(file == NULL){
		*response = "Error, file doesn't exist!\n";
		return 1;
	}
	//allocate space for the shorts to be read in
	char packet[100];
	int numElements = 0;
	char bstop = 0;
	//run the loop until end of file
	while(!bstop){
		for(int i = 0; i < windowSize; i++){
			//read in the starting address and number of elements
			numElements = fread(packet, sizeof(char), 100, file);
			//check for EOF
			if(numElements < 100){
				//EOF reached, break
				bstop = 1;
			}
			//send off the file, wait for an ACK
			int numBytesSent = sendto(connection->socket,packet,strlen(packet),0,
								  connection->addr,connection->addrlen);
		
			if(DEBUG) printf("Num bytes sent: %d\n",numBytesSent);
			if(DEBUG) printf("Message Sent: %s\n",packet);
			bzero(packet,100);
		}
	
		
		
		char recvBuffer[100];
		int numBytesRecv = recvfrom(connection->socket,recvBuffer,100,0, connection->addr,&(connection->addrlen));
		if(strncmp(recvBuffer,"ACK",3) == 0){
			if(DEBUG) printf("Received ACK\n");
		}
		numElements = 0;
		bzero(packet,100);
		bzero(recvBuffer,100);
	}
		
	fclose(file);
	return result;
}

int process_response(char *buffer, int sizeOfBuffer, char ** response){
	if(DEBUG) printf("Response to process is:%s\n",buffer);
	int result = 0;
	//compare the user's md5 hash with my md5 hash
	//int size = strlen(challenge) + strlen(username) + strlen(password);
	char* md5 = calloc(110,sizeof(char));
	md5 = doMD5(challenge);

	if(DEBUG) printf("Doing comparison:\nCalculated: %s_blah\nReceived:   %s_blah\n",md5,buffer);
	int cmpRes = strncmp(md5,buffer,strlen(md5));
	if(DEBUG) printf("Done with comparison, result is %d\n",cmpRes);
	if(cmpRes == 0){
		//have a match!
		result = 1;
		*response = "ACK: Welcome to our service\n";
	}
	else{
		result = 1;
		*response = "NAK: Wrong Credentials\n";
		if(DEBUG) printf("Set response to %s\n",*response);
	}
	return 1;
}
int process_request(char *buffer, int sizeOfBuffer, char ** response){
	if(DEBUG) printf("Request to process is:%s\n",buffer);
	int result = 0;
	//has to also have the text "Please Connect" to be a valid request
	if(strncmp(buffer,"Please Connect",sizeOfBuffer) == 0){
		if(DEBUG) printf("Valid Request\n");
		char * genString = generate_string();
		if(DEBUG) printf("String returned was %s\n",genString);
		//memcpy(current_client->challenge,genString,64);
		challenge = genString;
		//test MD5
		//char* test = calloc(110,sizeof(char));
		//test = doMD5(genString,"user1","pass1");
		char *head = "CHA: ";
		strcat(*response,head);
		strcat(*response,genString);
		strcat(*response,"\n");
		
		result = 1;
	}
	return result;
}

char * generate_string(){
	//random generation code modified from: http://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string
	char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int length = 64;
    char * result = calloc(length,sizeof(char));
    srand(time(NULL));//+current_client->port);
    char * dest = result;
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
	return result;
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

void figureOutClient(struct sockaddr_in remaddr){
	int port = remaddr.sin_port;
	char* ip = inet_ntoa(remaddr.sin_addr);
	//first, check for an available client
	if(client1->port == -1){
		client1->port = port;
		client1->ip = ip;
		current_client = client1;
	}
	else if(client2->port == -1){
		client2->port = port;
		client2->ip = ip;
		current_client = client2;
	}
	else if((client1->port == port) && (strcmp(client1->ip,ip) == 0)){
		current_client = client1;
	}
	else if((client2->port == port) && (strcmp(client2->ip,ip) == 0)){
		current_client = client2;
	}
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
