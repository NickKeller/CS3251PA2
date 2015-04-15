#include "client.h"

int DEBUG;
CONN_INFO* connection;
int connection_set;

char* clientPort;
int windowSize;

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
	int opts;
	int offset = 0;
	if((opts = getopt(argc,argv,"d")) != -1){
		DEBUG = 1;
		argc -= 1;
		offset = 1;
	}
	else{
		DEBUG = 0;
	}
	//grab the port number to bind to, and the IP and port
	if(argc < 3){
		quit("Example Usage: ./fxa-client [-d] Port NetEmuAddr NetEmuPort");
	}
	
	clientPort = argv[1+offset];
	char* netEmuAddr = argv[2+offset];
	char* netEmuPort = argv[3+offset];
	
	if(atoi(clientPort) % 2 != 0){
		quit("Error, the port number to bind to must be an even number");
	}	
	//create a socket
	connection = setup_socket(netEmuAddr,netEmuPort);
	if(DEBUG) printf("Attempting to bind on port %s\n",clientPort);
	
	//bind to the port number
	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(atoi(clientPort));
	int bindResult = bind(connection->socket, (struct sockaddr*)&myaddr, connection->addrlen);
	if(bindResult != 0){
		if(DEBUG) perror("Failed to bind on port number given\n");
		exit(0);
	}
	if(connection == NULL){
		return 0;
	}
	//default window size of 1
	windowSize = 1;
	
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
				if(DEBUG) printf("IP:%s\nPort:%s\n",netEmuAddr,netEmuPort);
				printf("Connecting to server...");
				if(connect_to_server()){
					printf("Done!\n");
				}
				else{
					printf("Could not connect to server. Please try again later\n");
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
				printf("Could not download file\n");
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
				printf("Could not upload file\n");
			}
		}
		
		else if(strcmp(cmd,"close") == 0){
			if(!connection_set){
				printf("Must set up connection first\n");
			}
			
			else{
				if(fxa_close()){
					printf("Connection successfully closed\n");
				}
			}
		}
		
		else if(strcmp(cmd,"window") == 0){
			if(strlen(arg1) == 0){
				printf("Example usage: window windowSize\n");
			}
			
			else if(atoi(arg1) == 0 || atoi(arg1) > 5){
				printf("Error: Window Size must be between 1 and 5\n");
			}
			
			else{
				int size = atoi(arg1);
				windowSize = size;
				printf("Window Size set to %d packets\n",windowSize);
			}
		}
		
		else if(strcmp(cmd,"quit") == 0){
			if(connection_set){
				fxa_close();
			}
			quit("Goodbye!");
		}
		
		else{
			printf("Available commands are: connect,get,put,window,close,quit\n");
		}
		
		bzero(buffer,sizeof(buffer));
		bzero(cmd,sizeof(cmd));
		bzero(arg1,sizeof(arg1));
		bzero(arg2,sizeof(arg2));
		
		printf("Please enter a command:");
	}
	
	
}


int connect_to_server(){
	
	char request[] = "REQ: Please Connect\n";
	
	int retval = -1;

	int numBytesSent = sendto(connection->socket,request,sizeof(request),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Message size: %d, num bytes sent: %d\n",sizeof(request),numBytesSent);
	//wait for a response
	char recvBuffer[100];
	int size = sizeof(recvBuffer);
	int numBytesRecv = timeout_recvfrom(connection->socket,recvBuffer,size,0,
								connection->remote_addr,&(connection->addrlen),2,request,5,1);
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
								connection->remote_addr,&(connection->addrlen),2,respBuffer,5,1);
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
	
	FILE* file = fopen(fullpath,"wb");
	int numBytesWritten = 0;
	
	char* message = calloc(12 + strlen(filename), sizeof(char));
	char* window = calloc(7,sizeof(char));
	char* windowsize = calloc(4,sizeof(char));
	int bytes = snprintf(windowsize,sizeof(windowSize),"%d",windowSize);
	printf("WindowSize: %s\nBytes:%d\n",windowsize,bytes);
	
	//string concatenation,yay!....
	memcpy(message,"GET: ",5);
	memcpy(&message[5],filename,strlen(filename));
	memcpy(window," WIN:",5);
	memcpy(&window[5],windowsize,1);
	memcpy(&window[6],"\n",1);
	memcpy(&message[5+strlen(filename)],window,strlen(window));
	
	if(DEBUG) printf("Message is: %s\n",message);
	
	//send the GET message
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	
	//wait for the ACK from the server before preparing to receive files
	char* recvBuffer = calloc(112,sizeof(char));
	if(DEBUG) printf("Waiting for ACK\n");
	int AckBytesReceived = timeout_recvfrom(connection->socket,recvBuffer,112,0,connection->remote_addr,&(connection->addrlen),2,message,5,1);
	if(DEBUG) printf("Received an ACK?\n");
	if(strncmp("ACK",recvBuffer,3) != 0){
		printf("Error, file does not exist on the server!\n");
		return 0;
	}
	if(DEBUG) printf("ACK received, starting download\n");
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
											connection->remote_addr,
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
			sendto(connection->socket,"ACK",3,0,connection->remote_addr,connection->addrlen);
			fflush(file);
		}
		else{
			if(DEBUG) printf("There was a bit error. Sending NAK\n");
			sendto(connection->socket,"NAK",3,0,connection->remote_addr,connection->addrlen);
			bitError = 0;
		}
		bzero(recvBuffer,112);
	}
	
	fclose(file);
	return 1;
}

int fxa_put(char* filename){

	char* fullpath = convert_name(filename,"put/");	
	int numBytesWritten = 0;
	
	char* message = calloc(12 + strlen(filename), sizeof(char));
	char* window = calloc(7,sizeof(char));
	char* windowsize = calloc(4,sizeof(char));
	int bytes = snprintf(windowsize,sizeof(windowSize),"%d",windowSize);
	printf("WindowSize: %s\nBytes:%d\n",windowsize,bytes);
	memcpy(message,"PUT: ",5);
	memcpy(&message[5],filename,strlen(filename));
	memcpy(window," WIN:",5);
	memcpy(&window[5],windowsize,1);
	memcpy(&window[6],"\n",1);
	memcpy(&message[5+strlen(filename)],window,strlen(window));
	if(DEBUG) printf("Message is: %s\n",message);
	
	//send the PUT message
	sendto(connection->socket,message,strlen(message),0,connection->remote_addr,connection->addrlen);
	
	//wait for an ACK before proceeding
	char ack[10] = {0};
	recvfrom(connection->socket,ack,10,0, connection->remote_addr,&(connection->addrlen));
	
	int result = 1;
	if(DEBUG) printf("File to put is: %s_blah\n",filename);
	
	//time to send the file
	if(DEBUG) printf("Opening file:%s_blah\n",fullpath);
	
	//this code came from a lc3 emulator that I wrote in my CS2110 class. It was used to read an object file
	FILE *file = fopen(fullpath, "rb");
	if(file == NULL){
		printf("Error, %s doesn't exist!\n",filename);
		return 0;
	}
		
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
											connection->remote_addr,connection->addrlen);
		
				if(DEBUG) printf("Num bytes sent: %d\n",numBytesSent);
				//if(DEBUG) printf("Message Sent: %s\n",packet);
				
			}
			if(DEBUG) printf("Waiting on ACK\n");
			char recvBuffer[100] = {0};
			int numBytesRecv = recvfrom(connection->socket,recvBuffer,100,0, connection->remote_addr,&(connection->addrlen));
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
	//done, send the EOF
	char* temp = calloc(4 + strlen(filename),sizeof(char));
	memcpy(temp,"EOF ",4);
	memcpy(&temp[4],filename,strlen(filename));
	
	char* EOFmessage = add_header_info((short)0,(char)112,type_EFI, temp);
	sendto(connection->socket,EOFmessage,strlen(EOFmessage),0,
								  connection->remote_addr,connection->addrlen);
	fclose(file);
	return result;
}

int fxa_close(){
	char* message = "FIN\n";
	int numBytesSent = sendto(connection->socket,message,strlen(message),0,
							  connection->remote_addr,connection->addrlen);
	if(DEBUG) printf("Num Bytes Sent: %d\n",numBytesSent);
	//wait for an ack
	char* ACKbuffer = calloc(10,sizeof(char));
	recvfrom(connection->socket,ACKbuffer,10,0,connection->remote_addr,&(connection->addrlen));
	//send off a "GBE"
	sendto(connection->socket,"GBE",3,0,connection->remote_addr,connection->addrlen);
	connection_set = 0;
	return 1;
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
      // if(DEBUG) printf("Port Received From:%d\nIP Received From:%d\n",(struct sockaddr_in *)(connection)->sin_port,inet_ntoa((struct sockaddr_in *)(connection)->sin_addr));
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
