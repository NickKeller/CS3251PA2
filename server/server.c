#include "server.h"

//usernames and passwords
char *usernames[] = {"user1","user2","user3"};
char *passwords[] = {"pass1","pass2","pass3"};

CLIENT* client1;
CLIENT* client2;
CLIENT* current_client;

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

	//must have one argument - the port number
	if(argc < (2+offset)){
		print_use_and_exit();
	}
	
	//initialize the clients
	client1 = calloc(1,sizeof(CLIENT));
	client2 = calloc(1,sizeof(CLIENT));
	current_client = calloc(1,sizeof(CLIENT));
	challenge = calloc(64,sizeof(char));
	int port = atoi(argv[1+offset]);
	//create a socket to bind on

	//I got this from: https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
	int sock;
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("Cannot create socket\n");
		return 0;
	}
	//CONN_INFO* connection = setup_socket(port);
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
	}
	return 0;
}

void print_use_and_exit(){
	fprintf(stderr,"Usage:  server-udp [-d] port\n\n");
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
		return 1;
	}
	else{
		return 0;
	}
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
