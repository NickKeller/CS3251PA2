Name: Nick Keller
GTID: 902906955
Email: nkeller3@gatech.edu
Class: CS3251
Section: B
Date: April 15, 2015
Title: Project 2

Files Submitted
Client Files: client.c client.h md5.c Makefile
Server Files: server.c server.h md5.c Makefile
Sample.txt

Descriptions:
client.c/server.c - self explanatory, main code for the application
client.h/server.h - header files for respective c files
md5.c - c file that performs the md5 hash
Sample.txt - Sample debugging output from running the client

Compiling
Both the client and server directories have their own respective Makefiles. You just need the command "make" to compile the client and the server

Running

The command for running the client is:
./fxa-client [-d] port NetEmuAddr NetEmuPort
Example:
./fxa-client -d 8080 localhost 5000

Options:
-d: The debugging flag. Having this will enable debugging output
port: The port that the client will bind to. NOTE: this must be an even number. I used 8080 in my testing
NetEmuAddr: The address of NetEmu. You can also use DNS, like the example
NetEmuPort: The port number of NetEmu

The client takes 6 commands: connect,get,put,window,close,quit
The client tells you if you get the command wrong

Special note:
The get and put commands look like this:
get filename
put filename

You only need to specify the filename, like md5.c. Both the client and server will go to their get/ and put/ for downloading and uploading files, respectively.

The command for running the server is:
./fxa-server [-d] port
Example:
./fxa-server [-d] 8081

Options:
-d: The debugging flag. Having this will enable debugging output
port: The port that the server will bind to. NOTE: this must be an odd number. I used 8081 in my testing

NOTE: The server doesn't need the Addr and Port of NetEmu because of the way it is designed

Limitations
-only one client at a time
-cannot deal with lost packets and random bit errors

Known Bugs
-md5 hash function problems again. When I was concatenating the header, the md5 string would sometimes corrupt/overwrite the other bits, and the downloaded/uploaded file would end up getting corrupted, even if the checksum passes. To counter this, I put 1's in the space where the checksum is supposed to go. When that happened, the files would transfer correctly.


Updated Protocol

The only messages that have the header information are the messages included in the file transfer. The messages exchanged in order to connect, to close, and to start a transfer do not have a header. 

NOTE: All communication is initiated by the client. All messages must be terminated with a newline in order to be considered a valid message

Messages exchanged in order to connect:
Client: REQ: Please Connect
Server: CHA: challenge string
Client: RES: challenge response
Server: ACK: Welcome to our service OR NAK: Wrong Credentials

Messages Exchanged in order to get a file:
Client: GET: filename WIN: windowSize
Server: ACK: windowsize
Client: ACK

-the extra ACK is needed in order to prevent the file packets from arriving before the ACK: windowsize packet

Messages Exchanged in order to put a file:
Client: PUT: filename WIN: windowSize
Server: ACK: windowsize

During file transfer:
The message type is encoded in the 4th byte of the header.

Messages Exchanged in order to close a connection:
Client: FIN
Server: ACK
Client: GBE
