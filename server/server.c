#include "server.h"

CLIENT* client1;
CLIENT* client2;
CLIENT* current_client;
CONN_INFO *connection;

int DEBUG;
char *challenge;

//Message Type Definitions
char type_REQ = (char)0;
char type_CHA = (char)1;
char type_RES = (char)2;
char type_ACK = (char)3;
char type_NAK = (char)4;
char type_DTA = (char)5;
char type_LST = (char)6;
char type_EFI = (char)7;
char type_FIN = (char)8;


int main(int argc, char *argv[]){
	
	int offset = 0;
	if(getopt(argc,argv,"d") != -1){
		DEBUG = 1;
		offset = 1;
	}
	else{
		DEBUG = 0;
	}

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
	fprintf(stderr,"Usage:  fxa-server [-d] port\n\n");
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
		if(strncmp(buffer,"FIN",3) == 0){
			return close_connection(buffer,response);
		}
		return 1;
	}
	else{
		return 0;
	}
}

int get_file(char* buffer, int sizeOfBuffer, char** response){
	//convert filename to get/filename
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
	
	char* fullpath = convert_name(filename,"get/");
	
	char* message = calloc(12 + strlen(filename), sizeof(char));
	char* window = calloc(7,sizeof(char));
	char* windowsize = calloc(4,sizeof(char));
	int bytes = snprintf(windowsize,sizeof(windowSize),"%d",windowSize);
	printf("WindowSize: %s\nBytes:%d\n",windowsize,bytes);
	//send off the ACK
	memcpy(message,"ACK: ",5);
	memcpy(&message[5],filename,strlen(filename));
	memcpy(window," WIN:",5);
	memcpy(&window[5],windowsize,1);
	memcpy(&window[6],"\n",1);
	memcpy(&message[5+strlen(filename)],window,strlen(window));
	
	if(DEBUG) printf("Message is: %s\n",message);
	
	//send the GET message
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	
	//open the file in binary mode for writing
	FILE* file = fopen(fullpath,"wb");
	char* recvBuffer = calloc(112,sizeof(char));
	int numBytesWritten = 0;
	int eof = 0;
	int bitError = 0;
	while(!eof){
		int numBytesRecvTotal = 0;
		int numBytesWrittenTotal = 0;
		int numBytesRecv = 0;
		int numPacketsRecv = 0;
		char* packetArray[windowSize];
		for(int i = 0; i < windowSize; i++){
			packetArray[i] = NULL;
		}
		for(int i = 0; i < windowSize; i++){
			numBytesRecv = recvfrom(connection->socket,
											recvBuffer,112,0,
											connection->addr,
											&(connection->addrlen));
										
			//check if any bits were corrupted
			if(goodMessage(recvBuffer)){
				numPacketsRecv++;
				if(DEBUG) printf("Good message\n");
				//split the message
				char* firstPart = calloc(4,sizeof(char));
				memcpy(firstPart,recvBuffer,4);
				printf("FirstPart:%s\n",firstPart);
				char* charUpperPacketNum = calloc(1,sizeof(char));
				char* charLowerPacketNum = calloc(1,sizeof(char));
				char* messageType = calloc(1,sizeof(char));
				
				memcpy(charUpperPacketNum,&firstPart[0],1);
				memcpy(charLowerPacketNum,&firstPart[1],1);
				memcpy(messageType,&firstPart[3],1);
				
				short upperPacketNum = (short)(atoi(charUpperPacketNum)) << 8;
				short lowerPacketNum = (short)(atoi(charLowerPacketNum));
				printf("UpperNum:%d\nLowerNum:%d\n",upperPacketNum,lowerPacketNum);
				
				char msg_type = (char)(atoi(messageType));
				short packetNum = upperPacketNum | lowerPacketNum;
				
				//make sure I got everything
				printf("Packet Number:%d\nMsg_type:%d\n",packetNum,msg_type);
				
				if(msg_type == type_EFI){
					if(DEBUG) printf("Reached EOF\n");
					eof = 1;
					break;
				}
				if(msg_type == type_LST){
					if(DEBUG) printf("Last Packet before EOF\n");
					i = windowSize;
					numPacketsRecv = i;
				}
				
				char* message = calloc(100,sizeof(char));
				memcpy(message,&recvBuffer[12],100);
				
				//allocate space in the array, and place the message in there
				packetArray[packetNum] = message;
				printf("Message is:%s\n",packetArray[packetNum]);
				
				if(DEBUG) printf("Received %d Bytes\n",numBytesRecv);
				numBytesRecvTotal += (numBytesRecv-12);
				
				
	
				if(DEBUG) printf("Num Bytes Recv Total: %d\n",numBytesRecvTotal);
			//	if(DEBUG) printf("Message Received: %s\n",recvBuffer);
			}
			else{
				printf("Bit error\n");
				bitError = 1;
			}
			bzero(recvBuffer,112);
		}
		//write the buffer into the file
		if(bitError == 0){
			for(int i = 0; i < numPacketsRecv; i++){
				if(packetArray[i] != NULL){
					int numBytesWritten = fwrite(packetArray[i],sizeof(char),
												strlen(packetArray[i]),file);
					numBytesWrittenTotal += numBytesWritten;
				}		
			}
		
			if(DEBUG) printf("Num Bytes Written Total: %d\n",numBytesWrittenTotal);		
			//send an ACK
			if(DEBUG) printf("Successfully wrote %d bytes to file\n",numBytesWrittenTotal);
			sendto(connection->socket,"ACK",3,0,connection->addr,connection->addrlen);
			fflush(file);
		}
		else{
			if(DEBUG) printf("There was a bit error. Sending NAK\n");
			sendto(connection->socket,"NAK",3,0,connection->addr,connection->addrlen);
			bitError = 0;
		}
		bzero(recvBuffer,112);
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
	char* temp = calloc(4 + strlen(filename),sizeof(char));
	memcpy(temp,"EOF ",4);
	memcpy(&temp[4],filename,strlen(filename));
	
	char* message = add_header_info((short)0,(char)112,type_EFI, temp);
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
	
	//send an ACK confirming the window size
	char* windowSizeAck = calloc(7,sizeof(char));
	snprintf(windowSizeAck,7,"ACK: %d\n",windowSize);
	sendto(connection->socket,windowSizeAck,strlen(windowSizeAck),0,
								  connection->addr,connection->addrlen);
	
	//allocate space for the shorts to be read in
	char packet[100];
	int numElements = 0;
	char bstop = 0;
	//run the loop until end of file
	while(!bstop){
		//buffer up the packets
		char* packetArray[windowSize];
		for(int i = 0; i < windowSize; i++){
			packetArray[i] = "0";
		}
		int numPacketsCreated = 0;
		for(int i = 0; i < windowSize; i++){
			//read in the starting address and number of elements
			numElements = fread(packet, sizeof(char), 100, file);
		
			//check for EOF
			if(numElements < 100){
				//EOF reached, break
				if(DEBUG) printf("Found EOF, setting bstop to 1\nSending last part of file\n");
				bstop = 1;
				packetArray[i] = add_header_info((short)i,(char)112,type_LST, packet);
			}
		
			else{
				packetArray[i] = add_header_info((short)i,(char)112,type_DTA, packet);
			}
			numPacketsCreated++;
			bzero(packet,100);
			if(bstop) break;
		}
		
		//send off the packets
		int recvAck = 0;
		while(recvAck == 0){
			for(int i = 0; i < numPacketsCreated; i++){
				char* message = packetArray[i];
				int numBytesSent = sendto(connection->socket,message,
											strlen(message),0,
											connection->addr,connection->addrlen);
		
				if(DEBUG) printf("Num bytes sent: %d\n",numBytesSent);
				//if(DEBUG) printf("Message Sent: %s\n",packet);
				
			}
			if(DEBUG) printf("Waiting on ACK\n");
			char recvBuffer[100] = {0};
			int numBytesRecv = recvfrom(connection->socket,recvBuffer,100,0, connection->addr,&(connection->addrlen));
			if(strncmp(recvBuffer,"ACK",3) == 0){
				if(DEBUG) printf("Received ACK\n");
				recvAck = 1;
			}
			else{
				if(DEBUG) printf("Received NAK\n");
			}
			numElements = 0;
			bzero(recvBuffer,100);
		}
	}
		
	fclose(file);
	return result;
}

int close_connection(char* buffer, char** response){
	//send an ACK
	*response = "ACK\n";
	return 1;
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

int timeout_recvfrom (int sock, char *buf, int bufSize, int flags, struct sockaddr *connection, socklen_t *addrlen,int timeoutinseconds,char* messageToSend, int numIter, int resend)
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
    	//don't try more than numIter times
    	if(tries == numIter){
	    	return -1;
    	}
    	if(resend){
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
		}
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

char* add_header_info(short packet_num,char num_bytes, char msg_type, char* packet){
	printf("Packet_num:%d\nNum_Bytes:%d\nMsg_Type:%d\n",packet_num,num_bytes,msg_type);
	char* firstPart = calloc(4,sizeof(char));
	if(DEBUG) printf("Num Bytes to Hash:%d\n",4+strlen(packet));
	
	char* totalToHash = calloc(4+strlen(packet),sizeof(char));
	
	if(DEBUG) printf("Splitting packet num\n");
	//split up packet_num into chars
	char upperPacketNum = (char)(packet_num>>8);
	char lowerPacketNum = (char)(packet_num);
	if(DEBUG) printf("Upper Packet Num:%d\nLower Packet Num:%d\n",upperPacketNum,lowerPacketNum);
	
	if(DEBUG) printf("Loading first part of header\n");
	//copy packet_num,num_bytes,and msg_type into firstPart
	sprintf(&firstPart[0],"%d",upperPacketNum);
	sprintf(&firstPart[1],"%d",lowerPacketNum);
	sprintf(&firstPart[2],"%d",num_bytes);
	sprintf(&firstPart[3],"%d",msg_type);
	if(DEBUG) printf("Loading second part of header\nFirstPart:%s_blah\nPacket:%s_blah\n",firstPart,packet);
	//join firstPart and packet and hand it off to the md5 function
	memcpy(totalToHash,firstPart,4);
	memcpy(&totalToHash[4],packet,strlen(packet));
	
	if(DEBUG) printf("Doing MD5\nHashing: %s\n",totalToHash);
	char* md5_res = md5(totalToHash,4+strlen(packet));
	printf("MD5 returned:%s\n",md5_res);
	char* checksum = calloc(8,sizeof(char));
	memcpy(checksum,md5_res,8);
	
	if(DEBUG) printf("Putting Message together\n");
	char* message = calloc(112,sizeof(char));
	//double checking
	printf("Adjusted MD5:%s\n",checksum);
	
	memcpy(message,firstPart,4);
	if(DEBUG) printf("Message is:%s\n",message);
	memcpy(&message[4],checksum,8);
	if(DEBUG) printf("Message is:%s\n",message);
	memcpy(&message[12],packet,strlen(packet));
	if(DEBUG) printf("Message is:%s\n",message);
	return message;
}

int goodMessage(char* buffer){
	char* firstPart = calloc(4,sizeof(char));
	char* hash = calloc(8,sizeof(char));
	char* secondPart = calloc(100,sizeof(char));
	char* toCheck = calloc(104,sizeof(char));
	char* toCheckHash = calloc(8,sizeof(char));
	
	//split the buffer
	memcpy(firstPart,buffer,4);
	memcpy(hash,&buffer[4],8);
	memcpy(secondPart,&buffer[12],100);
	memcpy(toCheck,firstPart,4);
	memcpy(&toCheck[4],secondPart,100);
	
	char* md5_res = md5(toCheck,104);
	
	memcpy(toCheckHash,md5_res,8);
	
	//check the values
	if(strcmp(hash,toCheckHash) == 0){
		return 1;
	}
	
	return 1;
}
