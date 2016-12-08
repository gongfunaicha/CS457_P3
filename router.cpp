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
#include "cost_info.h"
#include "dijkstra.h"
#include <cerrno>

using namespace std;

struct connection {
    int id;
    int cost;
    uint16_t port;
    bool recvACK = false;

    struct sockaddr_in myaddr;      		/* my address */
    struct sockaddr_in remaddr;     		/* remote address */
};

void* enforceack(void* ptr);
int recv_message_from_manager(int tcpsock, int udpsock, uint16_t selfport);
void* tcpthread(void* input);
int recv_message_from_peer(int currentnode,int udpsock, vector<int> port, vector<int> forwardingtable);

//message format that gets sent to the other nodes
struct nodePath {
    int from;
    int to;
    int distance;
};

struct dijkstrainput{
    int currentnode;
    vector<cost_info> v_cost_info;
    int numofnodes;
    vector<int>& forwarding_table;
    int tcpsock;
    int udpsock;
    int udpport;
    ofstream& outputFile;
    dijkstrainput(int currnode, int numnodes, vector<cost_info> costinfo, vector<int>& forwardtable, int sock, int port, ofstream& output, int inputtcpsock) : forwarding_table(forwardtable), outputFile(output)
    {
        currentnode = currnode;
        numofnodes = numnodes;
        udpsock = sock;
        udpport = port;
        tcpsock = inputtcpsock;
        for (int i = 0 ; i < costinfo.size(); i++)
            v_cost_info.push_back(costinfo.at(i));
    }
};

struct tcpthreadinput
{
    int tcpsock;
    int udpsock;
    uint16_t selfport;
};

vector<nodePath*> allPaths;
bool* pathsFrom;
bool gotID = false;
bool printededgeinfo = false;

ofstream outputFile;
string tempbuffer = "";
void outputToFile(string message) {
    time_t result = time(nullptr);
    if (!gotID)
    {
        tempbuffer += (message + "   at " + asctime(localtime(&result)));
    }
    else
    {
        outputFile << tempbuffer << message << "   at " << asctime(localtime(&result)) << flush;
        tempbuffer = "";
    }
}


void send_forwarding_table_up_message(int sock)
{
    int32_t  length;
    string message = "Forwarding table up";
    /*
     * Packet format:
     * First send a int32_t packet with the size of the total packet.
     * Then send a packet using the following format:
     * "Forwarding table up" without terminating zero
     */
    length = message.length();
    send(sock, &length, sizeof(int32_t), 0);
    // Then send the actual buffer
    send(sock, message.c_str(), length, 0);
    outputToFile("Dijkstra algorithm complete");
}



void* dijkstrathread(void* input)
{
    dijkstrainput* dinput = (dijkstrainput*)input;
    dinput->forwarding_table = dijkstra(dinput->currentnode, dinput->v_cost_info, dinput->numofnodes, dinput->outputFile);
    send_forwarding_table_up_message(dinput->tcpsock);
    // Dijkstra over, time to stop
    // First wait for 0.5 second for all the ACKs to stop
    usleep(500);
    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
    myaddr.sin_port=htons(dinput->udpport);
    /*
     * Packet format:
     * Send a char packet with value (2) to self udp port to signal dijkstra ends
     */
    char tosend = 2;
    while (sendto(dinput->udpsock, &tosend, sizeof(char), 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
        usleep(10);
    return 0;
}








void send_link_up_message(int sock)
{
    int32_t length;
    string message = "Link up";
    /*
     * Packet format:
     * First send a int32_t packet with the size of the total packet.
     * Then send a packet using the following format:
     * "Link up" without terminating zero
     */
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
    recv(sock, networkupmsg, length, MSG_WAITALL);
    networkupmsg[length] = 0;
    if (strcmp(networkupmsg, "Network up") != 0)
    {
        cerr << "Error when receving network up message from manager. " << networkupmsg << endl;
        // delete networkupmsg;
        exit(-1);
    }
    // delete(networkupmsg);
}
















int openUDPSocket(int);
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
    /*
     * Packet format:
     * 1(char)ID(int)
     */
    int num_try = 0;
    while ((num_try < 10) && (sendto(udpSock, &message, 1 + sizeof(int), 0, (struct sockaddr *)&(conn->remaddr), sizeof(conn->remaddr)) < 0)) {
        num_try ++ ;
    }
    if (num_try == 10)
    {
        perror("ERROR SENDING ACK");
        outputToFile("ERROR SENDING ACK");
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

    /*
     * Packet format:
     * Send a uint16_t packet with value (udp_port).
     */

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
    string fileName = "router" + to_string(myID) + ".out";
    gotID = true;
    outputFile.open(fileName);

    outputToFile("Received NodeID from manager: " + to_string(myID));

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
    /*
     * Packet format:
     * First send a int32_t packet with the size of the total packet.
     * Then send a packet using the following format:
     * "Ready" without terminating zero
     */
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
        // delete safetoreachmsg;
        exit(-1);
    }
    outputToFile("Received safe to reach message from manager");
    // delete safetoreachmsg;

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
    /*
     * Packet format:
     * See comments when packing the packet
     */
    while (sendto(udpSock, pack->message, pack->length, 0, (struct sockaddr *)&(pack->conn->remaddr), sizeof(pack->conn->remaddr)) < 0)
        usleep(10);
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
    int ID = atoi(argv[1]);
    nodeCount = atoi(argv[3]);

    //initialize the nodes
    pathsFrom = new bool[nodeCount];
    for(int n = 0; n < nodeCount; n++) {
        pathsFrom[n] = false;
    }

    outputToFile("Beginning router process");

    int tcpport = atoi(argv[2]);

    int udpport = openUDPSocket(ID);

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

    vector<int> forwarding_table;

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

            if (buf[0] == 2)
            {
                // Break message
                break;
            }

            int seq;
            memcpy(&seq, buf + 1, sizeof(int));

            int sender;
            memcpy(&sender, buf + 1 + sizeof(int), sizeof(int));

            connection* conn = getConnection(sender);

            //if this is an ACK
            if(buf[0] == 1) {
                if (!printededgeinfo)
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
                /*
                 * Packet format:
                 * 1(char)sequence_number(int)myID(int)
                 */
                while (sendto(udpSock, message, sizeof(message), 0, (struct sockaddr *)&(addr), sizeof(addr)) < 0)
                    usleep(10);

                if(!pathsFrom[from]) {
                    pathsFrom[from] = true;

                    if (!printededgeinfo)
                        outputToFile("(" + to_string(known) + ") Received link state information about node " + to_string(from) + " sent by node " + to_string(sender) + " seq(" + to_string(seq) +")");

                    for(int link = 0; link < count; link++) {
                        nodePath* path = (nodePath*) (buf + 1 + sizeof(int) * 4 + sizeof(nodePath) * link);
                        nodePath* dup = new nodePath();
                        dup->from = path->from;
                        dup->to = path->to;
                        dup->distance = path->distance;
                        if (!printededgeinfo)
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

                            /*
                             * Packet format:
                             * 0(char)sequence_number(int)myID(int)rest_of_received_link_state_packet(various length)
                             */

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
                    if (!printededgeinfo)
                        outputToFile("discarding duplicate link state information from node " + to_string(from) + " sent by node " + to_string(conn->id));
                }
            }
        } else {
        }


        //if we've receieved all the Data
        if(haveAllPaths()) {
            // Gather the edge info
            if (!printededgeinfo)
            {
                vector<cost_info> v_cost_info;
                for (int j = 0; j < allPaths.size(); j++)
                {
                    nodePath* p = allPaths.at(j);
                    v_cost_info.push_back(cost_info(p->from, p->to, p->distance));

                }
                for (int j = 0; j < connectionCount; j++)
                    v_cost_info.push_back(cost_info(myID, connections[j].id, connections[j].cost));
                string edgeoutput;
                for (int j = 0; j < v_cost_info.size(); j++)
                {
                    if (v_cost_info.at(j).src < v_cost_info.at(j).dest)
                        edgeoutput += (to_string(v_cost_info.at(j).src) + " " + to_string(v_cost_info.at(j).dest) + " " + to_string(v_cost_info.at(j).cost) + "\n");
                }
                outputFile << endl;
                outputToFile("Gathered edge information:");
                outputFile << edgeoutput << endl;
                printededgeinfo = true;

                // Start dijkstra thread
                pthread_t thread;
                dijkstrainput* input = new dijkstrainput(myID, nodeCount, v_cost_info, forwarding_table, udpSock, udpport, outputFile, tcpsock);
                pthread_create(&thread, NULL, dijkstrathread, (void*)input);
                pthread_detach(thread);
            }
        }
        usleep(250);
    }

    outputFile << endl;
    outputToFile("Forwarding table: ");
    string tempoutput;
    for (int i = 0; i < forwarding_table.size(); i++)
    {
        if (i == myID)
        {
            tempoutput += ("Node: " + to_string(i) + " Nexthop: -\n");
        }
        else if (forwarding_table.at(i) == -1)
        {
            tempoutput += ("Node: " + to_string(i) + " Nexthop: Unreachable\n");
        }
        else
        {
            tempoutput += ("Node: " + to_string(i) + " Nexthop: " + to_string(forwarding_table.at(i)) + "\n");
        }
    }
    outputFile << tempoutput << endl;

    // Getting ready for input packet
    int32_t length;
    recv(tcpsock, &length, sizeof(length), MSG_WAITALL);
    char* readytosendpacketmsg = new char[length+1];
    bzero(readytosendpacketmsg, length);
    recv(tcpsock, readytosendpacketmsg, length, MSG_WAITALL);
    readytosendpacketmsg[length] = 0;
    if (strcmp(readytosendpacketmsg, "Ready to send packet") != 0)
    {
        cerr << "Error when receving ready to send packet message from manager. " << readytosendpacketmsg << endl;
        // delete readytosendpacketmsg;
        exit(-1);
    }
    // delete readytosendpacketmsg;

    // Setup tcp recv

    pthread_t thread;
    tcpthreadinput tinput;
    tinput.udpsock = udpSock;
    tinput.selfport = (uint16_t)udpport;
    tinput.tcpsock = tcpsock;
    pthread_create(&thread, NULL, tcpthread, (void*)&tinput);

    // Setup udp recv
    // First create a vector<int> for the port
    vector<int> port;
    port.resize(nodeCount);
    // Initialize the vector
    for (int j = 0; j < port.size(); j++)
        port[j] = 0;
    for (int j = 0; j < connectionCount; j ++)
        port[connections[j].id] = connections[j].port;
    while (recv_message_from_peer(myID, udpSock, port,forwarding_table) != -1);

    pthread_join(thread, NULL);

    //// delete connections;

    outputToFile("Router quit");
    outputFile << endl;
    close(tcpsock);
    close(udpSock);
    //outputFile.close();
}

int openUDPSocket(int ID) {
    //create a UDP socket
    int port = 55555 + ID;

    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return -1;
    }

    int flags = fcntl(udpSock, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(udpSock, F_SETFL, flags);

    while (port <= 65535) {
        struct sockaddr_in myaddr;
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons((uint16_t)port);

        if(bind(udpSock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            port++;
        } else {
            outputToFile("UDP Socket opened on port " + to_string(port));
            return port;
        }
    }
    if (port == 65536)
    {
        cerr << "No available UDP port on this computer, program will now exit." << endl;
        exit(-1);
    }
}

void openConnection(connection* conn) {
    usleep(1000);
    sendACK(conn);
    outputToFile("sent initial ACK to node " + to_string(conn->id));
}

void sendLinkState() {
    outputToFile("sending link state to connections");

    int count = connectionCount;

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
            //if(c != o) {
            nodePath* path = (nodePath*) (buffer + 1 + sizeof(int) * 4 + sizeof(nodePath) * index);
            path->from = myID;
            path->to = connections[o].id;
            path->distance = connections[o].cost;
            index++;
            //}
        }

        /*
         * Packet format:
         * 0(char)sequence_number(int)num_of_neighbours(int)myID(int)(link_state_info(nodePath)*num_of_neighbours)
         */

        packet* p = new packet();
        p->message = buffer;
        p->length = length;
        p->seq = seq;
        p->conn = &(connections[c]);

        sendPacket(p);
    }
}

bool ack = true;
int size;
char* buffer = 0;
struct sockaddr_in myaddr;
int udpsocket = 0;
pthread_t thread;
void* enforceack(void* ptr)
{
    usleep(30000);
    if (ack)
    {
        // ACK received, delete the buffer
        if (buffer != 0)
            // delete buffer;
            buffer = 0;
    }
    else
    {
        // ACK not receive, try resend
        while (sendto(udpsocket, buffer, size, 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
            usleep(10);
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_detach(thread);
    }
    return 0;
}

int recv_message_from_manager(int tcpsock, int udpsock, uint16_t selfport)
{
    int32_t length;
    udpsocket = udpsock;
    recv(tcpsock, &length, sizeof(int32_t), MSG_WAITALL);
    if (length < 0)
    {
        // Send command
        int dest;
        recv(tcpsock, &dest, sizeof(int), MSG_WAITALL);
        outputToFile("Received packet initiate request to node " + to_string(dest) + " from manager");
        // Send udp packet to own udp server
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family=AF_INET;
        inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
        myaddr.sin_port=htons(selfport);
        size = 1 + sizeof(int);
        buffer = new char[size];
        buffer[0] = 'I'; // I for initiate
        memcpy(buffer + sizeof(char), &dest, sizeof(int));
        /*
         * Packet format:
         * 'I'(chat)destination(int)
         */
        while (sendto(udpsock, buffer, size, 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
            usleep(10);
        ack = false;
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_detach(thread);
        return 0;
    }
    else
    {
        char* quitmsg = new char[length+1];
        bzero(quitmsg, length);
        recv(tcpsock, quitmsg, length, MSG_WAITALL);
        quitmsg[length] = 0;
        if (strcmp(quitmsg, "Quit") != 0)
        {
            cout << quitmsg << endl;
            cerr << "Error when receving quit message from manager." << endl;
            exit(-1);
        }
        // Send udp packet to own udp server
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family=AF_INET;
        inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
        myaddr.sin_port=htons(selfport);
        buffer = new char[5];
        strcpy(buffer, "Quit");
        /*
         * Packet format:
         * "Quit"
         */
        while (sendto(udpsock, buffer, 4, 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
            usleep(10);
        ack = false;
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_join(thread, NULL);
        return -1;
    }
}

void* tcpthread(void* input)
{
    tcpthreadinput* tinput = (tcpthreadinput* ) input;
    while (recv_message_from_manager(tinput->tcpsock, tinput->udpsock, tinput->selfport) != -1);
    return 0;
}


int recv_message_from_peer(int currentnode,int udpsock, vector<int> port, vector<int> forwardingtable)
{
    struct sockaddr_in recvfromaddr;
    socklen_t  addrlen = sizeof(recvfromaddr);
    char BUF[1000];
    bzero(BUF,1000);
    recvfrom(udpsock, BUF, 1000, 0,(struct sockaddr*) &recvfromaddr, &addrlen );
    if (BUF[0] == 'A')
    {
        // ACK received
        ack = true;
    }
    else if (BUF[0] == 'I')
    {
        // Initiate
        // First send ACK
        /*
         * Packet format:
         * "ACK"
         */
        while (sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen) < 0)
            usleep(10);
        int dest;
        memcpy(&dest, BUF + sizeof(char), sizeof(int));
        if (currentnode == dest)
        {
            // Is the destination
            outputToFile("Packet initiated at the destination node, transmit complete.");
        }
        else if (forwardingtable.at(dest) == -1)
        {
            // Unreachable
            outputToFile("Destination of the packet is unreachable at the current node. Transmit abort.");
        }
        else
        {
            // Send the packet to nexthop
            memset(&myaddr, 0, sizeof(myaddr));
            myaddr.sin_family=AF_INET;
            inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
            myaddr.sin_port=htons(port.at(forwardingtable.at(dest)));
            size = sizeof(char) + 3*sizeof(int);
            buffer = new char[size];
            buffer[0] = 'T'; // T for transfer
            memcpy(buffer + sizeof(char), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 1 * sizeof(int), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 2 * sizeof(int), &dest, sizeof(int));
            /*
             * Packet format:
             * 'T'(char)source_node(int)transfer_node(int)destination_node(int)
             */
            while (sendto(udpsock, buffer, size, 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
                usleep(10);
            ack = false;
            outputToFile("Packet to node " + to_string(dest) + " initiated at current node has been forwarded to nexthop:" + to_string(forwardingtable.at(dest)));
            pthread_create(&thread, NULL, enforceack, NULL);
            pthread_detach(thread);
        }
    }
    else if (BUF[0] == 'T')
    {
        // Transfer
        // First send ACK
        /*
         * Packet format:
         * "ACK"
         */
        while (sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen) < 0)
            usleep(10);
        int src, transfer, dest;
        memcpy(&src, BUF + sizeof(char), sizeof(int));
        memcpy(&transfer, BUF + sizeof(char) + 1 * sizeof(int), sizeof(int));
        memcpy(&dest, BUF + sizeof(char) + 2 * sizeof(int), sizeof(int));
        //outputToFile("Source: " + to_string(src) + " Transfer: " + to_string(transfer) + " Dest: " + to_string(dest));
        if (currentnode == dest)
        {
            // Is the destination
            outputToFile("Packet initiated at node " + to_string(src) + " received from " + to_string(transfer) + " has reached its destination: node " + to_string(dest) + ". Transmit complete.");
        }
        else if (forwardingtable.at(dest) == -1)
        {
            // Unreachable
            outputToFile("Destination of the packet is unreachable at the current node. Transmit abort.");
        }
        else
        {
            // Send the packet to nexthop
            memset(&myaddr, 0, sizeof(myaddr));
            myaddr.sin_family=AF_INET;
            inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
            myaddr.sin_port=htons(port.at(forwardingtable.at(dest)));
            size = sizeof(char) + 3*sizeof(int);
            buffer = new char[size];
            buffer[0] = 'T';
            memcpy(buffer + sizeof(char), &src, sizeof(int));
            memcpy(buffer + sizeof(char) + 1 * sizeof(int), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 2 * sizeof(int), &dest, sizeof(int));
            /*
             * Packet format:
             * 'T'(char)source_node(int)transfer_node(int)destination_node(int)
             */
            while (sendto(udpsock, buffer, size, 0, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
                usleep(10);
            ack = false;
            pthread_create(&thread, NULL, enforceack, NULL);
            pthread_detach(thread);
            outputToFile("Packet received from node " + to_string(transfer) + " initiated at node " + to_string(src) + " heading toward node " + to_string(dest) + " has been forwarded to nexthop:" + to_string(forwardingtable.at(dest)));
        }
    }
    else if (BUF[0] == 'Q')
    {
        // Quit
        // First send ACK
        /*
         * Packet format:
         * "ACK"
         */
        while (sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen) < 0)
            usleep(10);
        outputToFile("Received quit message from manager");
        return -1;
    }
    else
    {
        // History packet, discard
        return 0;
    }
    return 0;
    // -1 for quit, 0 for normal
}
