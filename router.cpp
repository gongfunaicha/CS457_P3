#include <time.h>
#include <fstream>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <fcntl.h>

using namespace std;

struct connection {
    int id;
    int cost;
    uint16_t port;
    bool recvACK = false;

    struct sockaddr_in myaddr;      		/* my address */
    struct sockaddr_in remaddr;     		/* remote address */
};


//message format that gets sent to the other nodes
struct nodePath {
    int from;
    int to;
    int distance;
};
vector<nodePath*> allPaths;
bool* pathsFrom;
bool gotID = false;

ofstream outputFile;
string tempbuffer = "";
void outputToFile(string message) {
    time_t result = time(nullptr);
    if (!gotID)
    {
        tempbuffer += (message + "   at " + asctime(localtime(&result)) + "\n");
    }
    else
    {
        outputFile << tempbuffer << message << "   at " << asctime(localtime(&result)) << endl;
        tempbuffer = "";
    }
}











void send_link_up_message(int sock)
{
    int32_t length;
    string message = "Link up";
    length = message.length();
    send(sock, &length, sizeof(int32_t), 0);
    // Then send the actual buffer
    outputToFile("sending link is up message");
    if(send(sock, message.c_str(), length, 0) < 0) {
        cout << "Error sending link is up message" << endl;
    }
}

void recv_network_up_message(int sock)
{
    int32_t length;
    recv(sock, &length, sizeof(length), MSG_WAITALL);
    char* networkupmsg = new char[length+1];
    bzero(networkupmsg, length);
    recv(sock, networkupmsg, length, 0);
    networkupmsg[length] = 0;
    if (strcmp(networkupmsg, "Network up") != 0)
    {
        cerr << "Error when receving network up message from manager. " << networkupmsg << endl;
        delete(networkupmsg);
        exit(-1);
    }
    delete(networkupmsg);
    outputToFile("Network is up, beginning algorithm");
}

void send_forwarding_table_up_message(int sock)
{
    int32_t  length;
    string message = "Forwarding table up";
    length = message.length();
    send(sock, &length, sizeof(int32_t), 0);
    // Then send the actual buffer
    send(sock, message.c_str(), length, 0);
}















int openUDPSocket();
void clearACKs();
void processInfo(nodePath* path);
void openConnection(connection* conn);
bool receivedACKs();
void sendMessages();
void writeRoutingTable();

int BUFSIZE = 1024;
int myID;
int currentIteration = 0;
int udpSock;


connection* connections;
int connectionCount;
int nodeCount;

void sendACK(connection* conn) {
    char message[1 + sizeof(int)];
    message[0] = 1;
    memcpy(message + 1, &myID, sizeof(int));
    if(sendto(udpSock, &message, 1 + sizeof(int), 0, (struct sockaddr *)&(conn->remaddr), sizeof(conn->remaddr)) < 0) {
        cout << "ERROR SENDING ACK" << endl;
    }
}
bool receivedACKs() {
    for(int i = 0; i < connectionCount; i++) {
        if(!connections[i].recvACK) {
            return false;
        }
    }
    return true;
}








int J_method(int tcp_port, int udp_port) {		//tcp_port is the port of the manager process, udp is passed to the methdod by nick.
    //create a TCP port and connect to the manager process.

    int sock = 0;	// used as the return value for socket call.
    sock = socket(AF_INET, SOCK_STREAM, 0); //s is the socket descriptor if this is
    //successful. else it could not create

    if (sock == -1){
        cout << "Error: could not create socket. Program exiting. \n";
        exit(-1);
    }

    char name[150];
    gethostname(name, sizeof(name));			//host name is used to get ip address of server.

    struct sockaddr_in server_ip;				//struct to hold the ip address of
    server_ip.sin_family=AF_INET;

    server_ip.sin_port = htons(tcp_port);
    server_ip.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY gets the address of the localhost

    int success = connect(sock, (struct sockaddr *) &server_ip, sizeof(server_ip));
    if (success < 0){
        outputToFile("Error: could not connect to given " + to_string(tcp_port) + ". Program exiting.");
        exit(-1);
    }
    outputToFile("Connected to manager!");

    //send the manager the port number of the UDP port and a request for a node address and the associated
    //connectivity table.


    uint16_t out = (udp_port);

    send(sock, &out, sizeof(uint16_t), 0); //Send the UDP port number

    // Receive node number, cost info
    // First read the length
    int32_t length;
    recv(sock, &length, sizeof(length), MSG_WAITALL);
    char* nodeinfo = new char[length];
    bzero(nodeinfo, length);
    recv(sock, nodeinfo, length, 0);

    int num_of_neighbours = (length - sizeof(int)) / (2*sizeof(int) + sizeof(uint16_t));
    int nodenumber = 0;
    memcpy(&nodenumber, nodeinfo, sizeof(int));
    myID = nodenumber;
    string fileName = to_string(myID) + ".out";
    gotID = true;
    outputFile.open(fileName);

    //outputToFile("My ID is now: " + to_string(myID));

    connections = new connection[num_of_neighbours];
    connectionCount = num_of_neighbours;
    for (int i = 0; i < num_of_neighbours; i++)
    {
        int start = sizeof(int) + i * (2*sizeof(int) + sizeof(uint16_t));
        connection* n = &(connections[i]);
        memcpy(&(n->id), nodeinfo + start, sizeof(int));
        start += sizeof(int);
        memcpy(&(n->cost), nodeinfo + start, sizeof(int));
        start += sizeof(int);
        memcpy(&(n->port), nodeinfo + start, sizeof(uint16_t));

        outputToFile("I have a connection to " + to_string(connections[i].id) + " with cost " + to_string(connections[i].cost) + " at port " + to_string(connections[i].port));

        struct sockaddr_in servaddr;
        memset((char*)&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(n->port);
        //servaddr.sin_addr.s_addr = htons(INADDR_ANY);
        inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
        n->remaddr = servaddr;

    }


    //Receive the above information and send a "Ready!" message to the manager, indicating that it is ready to
    //accept link establishment requests from neighbor nodes.

    string message = "Ready";
    length = message.length();
    send(sock, &length, sizeof(int32_t), 0);
    // Then send the actual buffer
    send(sock, message.c_str(), length, 0);

    // Receive safe to reach message
    recv(sock, &length, sizeof(length), MSG_WAITALL);
    char* safetoreachmsg = new char[length+1];
    bzero(safetoreachmsg, length);
    recv(sock, safetoreachmsg, length, 0);
    safetoreachmsg[length] = 0;
    if (strcmp(safetoreachmsg, "Safe to reach") != 0)
    {
        cerr << "Error when receving safe to reach message from manager." << endl;
        delete(safetoreachmsg);
        exit(-1);
    }
    outputToFile("Received safe to reach message from manager");
    delete(safetoreachmsg);

    return sock;
}





int currentSeqNum = 0;
struct packet {
    int seq;
    connection* conn;
    char* message;
    int length;
    clock_t sent;
};
vector<packet*> packets;
void sendPacketUtil(packet* pack) {
    pack->sent = clock();
    sendto(udpSock, pack->message, pack->length, 0, (struct sockaddr *)&(pack->conn->remaddr), sizeof(pack->conn->remaddr));
    outputToFile("sent packet to " + to_string(pack->conn->id) + " with sequence number " + to_string(*((int*)(pack->message + 1))));
}
void sendPacket(packet* pack) {
    packets.push_back(pack);
    sendPacketUtil(pack);
}
void receivedACK(int seq) {
    for(unsigned int i = 0; i < packets.size(); i++) {
        packet* p = packets.at(i);
        if(p->seq == seq) {
            packets.erase(packets.begin() + i);
            break;
        }
        if(i == packets.size() - 1) {
            outputToFile("ERROR! Sequence number " + to_string(seq) + " is not matched with a packet!");
        }
    }
}





double ACKtimeout = 0.1;

void checkPackets() {
    for(unsigned int i = 0; i < packets.size(); i++) {
        packet* p = packets.at(i);
        if((clock() - p->sent) / (double) CLOCKS_PER_SEC > ACKtimeout) {
            outputToFile("Packet number " + to_string(p->seq) + " has not received an ACK, resending");
            sendPacketUtil(p);
        }
    }
}

bool haveAllPaths() {
    for(int i = 0; i < nodeCount; i++) {
        if(!pathsFrom[i]) {
            return false;
        }
    }
    return true;
}

connection* getConnection(int id) {
    for(int i = 0; i < connectionCount; i++) {
        if(connections[i].id == id)
            return &(connections[i]);
    }
    return NULL;
}


void sendLinkState();

int main(int argc, char **argv)
{
    //myID = atoi(argv[1]);
    nodeCount = atoi(argv[3]);

    //initialize the nodes
    pathsFrom = new bool[nodeCount];
    for(int n = 0; n < nodeCount; n++) {
        pathsFrom[n] = false;
    }

    outputToFile("beginning router process");

    int tcpport = atoi(argv[2]);

    int udpport = openUDPSocket();

    int tcpsock = J_method(tcpport, udpport);

    unsigned char buf[BUFSIZE];

    //open all of the connections to the connected nodes
    for(int c = 0; c < connectionCount; c++) {
        openConnection(&(connections[c]));
    }



    //wait until we have received a message from our connections
    for(;;) {
        //start an initial clock timer
        clock_t start = clock();

        for(int i = 0; i < connectionCount; i++) {
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int recvlen = recvfrom(udpSock, buf, BUFSIZE, 0, (struct sockaddr *)&(addr), &len);
            if (recvlen > 0) {
                int sender;
                memcpy(&sender, buf + 1, sizeof(int));

                outputToFile("Received initial packet from node " + to_string(sender));
                //if this is an ACK
                if(buf[0] == 1) {
                    getConnection(sender)->recvACK = true;
                }
            }
        }

        //break when we've recieved ACKs from all our connections
        if(receivedACKs()) {
            outputToFile("Recieved all of the initial ACKs");
            break;
        }
            //otherwise resend the ACKs if we haven't heard back in a while
        else {
            if((clock() - start) / (double) CLOCKS_PER_SEC > ACKtimeout) {
                //reset the clock
                start = clock();
                for(int i = 0; i < connectionCount; i++) {
                    if(!connections[i].recvACK) {
                        sendACK(&(connections[i]));
                        outputToFile("sent initial ACK to node " + to_string(connections[i].id));
                    }
                }
            }
        }
    }


    //send the ready message to the manager
    send_link_up_message(tcpsock);

    //wait until we recieve the go-ahead from the manager
    recv_network_up_message(tcpsock);

    pathsFrom[myID] = true;


    //send your link state to your direct neighbours
    sendLinkState();


    //outer loop to wait infinitely
    for (;;) {
        checkPackets();

        int known = 0;
        for(int i = 0; i < nodeCount; i++)
            if(pathsFrom[i]) known++;
        //outputToFile("outstanding packets: " + to_string(packets.size()) + "   known connections: " + to_string(known));

        struct sockaddr_in addr;

        char* buf = new char[BUFSIZE];

        socklen_t len = sizeof(addr);
        int recvlen = recvfrom(udpSock, buf, BUFSIZE, 0, (struct sockaddr *)&(addr), &len);
        // cout << ntohs(addr.sin_port) << endl;
        if (recvlen > 0) {

            int seq;
            memcpy(&seq, buf + 1, sizeof(int));

            int sender;
            memcpy(&sender, buf + 1 + sizeof(int), sizeof(int));

            connection* conn = getConnection(sender);

            //if this is an ACK
            if(buf[0] == 1) {
                outputToFile("recieved ACK from node " + to_string(sender) + " with sequence number " + to_string(seq));
                receivedACK(seq);
            }
                //otherwise this is data
            else {
                int count;
                int from;

                memcpy(&count, buf + 1 + sizeof(int) * 2, sizeof(int));
                memcpy(&from, buf + 1 + sizeof(int) * 3, sizeof(int));

                //send an ACK for this data
                char message[1 + sizeof(int) * 2];
                message[0] = 1;
                memcpy(message + 1, &seq, sizeof(int));
                memcpy(message + 1 + sizeof(int), &myID, sizeof(int));
                sendto(udpSock, message, sizeof(message), 0, (struct sockaddr *)&(addr), sizeof(addr));


                if(!pathsFrom[from]) {
                    pathsFrom[from] = true;

                    outputToFile("(" + to_string(known) + ") Received link state information about node " + to_string(from) + " sent by node " + to_string(sender) + " seq(" + to_string(seq) +")");

                    for(int link = 0; link < count; link++) {
                        nodePath* path = (nodePath*) (buf + 1 + sizeof(int) * 4 + sizeof(nodePath) * link);
                        nodePath* dup = new nodePath();
                        dup->from = path->from;
                        dup->to = path->to;
                        dup->distance = path->distance;
                        outputToFile("Received connection: " + to_string(dup->to) + " cost: " + to_string(dup->distance));
                        allPaths.push_back(dup);
                    }

                    //send this data on to my other connections
                    for(int c = 0; c < connectionCount; c++) {
                        if(connections[c].id != getConnection(sender)->id) {
                            char* message = new char[recvlen];
                            message[0] = 0;
                            memcpy(message + 1 + sizeof(int), buf + 1 + sizeof(int), recvlen - 1 - sizeof(int));

                            int seq = currentSeqNum++;
                            memcpy(message + 1, &seq, sizeof(int));
                            memcpy(message + 1 + sizeof(int), &myID, sizeof(int));


                            packet* p = new packet();
                            p->message = message;
                            p->length = recvlen;
                            p->seq = seq;
                            p->conn = &(connections[c]);
                            sendPacket(p);
                        }
                    }
                }

                    //discard this packet if we have already had it
                else {
                    outputToFile("discarding duplicate link state information from node " + to_string(from) + " sent by node " + to_string(conn->id));
                }
            }
        } else {
        }


        //if we've receieved all the Data
        if(haveAllPaths()) {
            //break;
            // Do dijkstra
            send_forwarding_table_up_message(tcpsock);

        }
        usleep(10);
    }

    //send_forwarding_table_up_message(tcpsock);



    delete connections;
}

int openUDPSocket() {
    //create a UDP socket
    int port = 50123;

    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return -1;
    }

    int flags = fcntl(udpSock, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(udpSock, F_SETFL, flags);

    for(;;) {
        struct sockaddr_in myaddr;
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(port);

        if(bind(udpSock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            port++;
        } else {
            outputToFile("UDP Socket opened on port " + to_string(port));
            return port;
        }
    }
}

void openConnection(connection* conn) {
    sendACK(conn);
    outputToFile("sent initial ACK to node " + to_string(conn->id));
}

void sendLinkState() {
    outputToFile("sending link state to connections");

    int count = connectionCount - 1;

    for(int c = 0; c < connectionCount; c++) {

        int index = 0;
        int seq = currentSeqNum++;
        int length = sizeof(nodePath) * count + sizeof(int) * 4 + 1;
        char* buffer = (char*) malloc(sizeof(char) * length);
        buffer[0] = 0;
        memcpy(buffer + 1, &seq, sizeof(int));
        memcpy(buffer + 1 + sizeof(int), &myID, sizeof(int));
        memcpy(buffer + 1 + sizeof(int) * 2, &count, sizeof(int));
        memcpy(buffer + 1 + sizeof(int) * 3, &myID, sizeof(int));

        for(int o = 0; o < connectionCount; o++) {
            if(c != o) {
                nodePath* path = (nodePath*) (buffer + 1 + sizeof(int) * 4 + sizeof(nodePath) * index);
                path->from = myID;
                path->to = connections[o].id;
                path->distance = connections[o].cost;
                index++;
            }
        }

        packet* p = new packet();
        p->message = buffer;
        p->length = length;
        p->seq = seq;
        p->conn = &(connections[c]);

        sendPacket(p);
    }
}

