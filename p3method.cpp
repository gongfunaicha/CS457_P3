//
// Created by bennywng on 12/5/16.
//

#include "p3method.h"


#define _POSIX_SOURCE
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

using namespace std;

struct node J_method(int * tcp_port, int * udp_port);

struct node {		//size is 8 bytes.
    int id;
    int distance;
    int nextStep;
};

struct neighbour
{
    int id;
    int cost;
    uint16_t port;
};

struct node J_method(int * tcp_port, int * udp_port){		//tcp_port is the port of the manager process, udp is passed to the methdod by nick.
    struct node n;		//node to return.

//create a TCP port and connect to the manager process.

    int o = 0;	// used to check for bind errors.
    int sock = 0;	// used as the return value for socket call.
    sock = socket(AF_INET, SOCK_STREAM, 0); //s is the socket descriptor if this is
    //successful. else it could not create

    struct addrinfo hints, *s_info;		  //this is used in case we want to print out debug info.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (sock == -1){
        cout << "Error: could not create socket. Program exiting. \n";
        return n;
    }

    char name[150];
    gethostname(name, sizeof(name));			//host name is used to get ip address of server.

    struct sockaddr_in server_ip;				//struct to hold the ip address of
    server_ip.sin_family=AF_INET;

    int p_no = (*tcp_port);
    server_ip.sin_port = htons(p_no);
    server_ip.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY gets the address of the localhost

    int success = connect(sock, (struct sockaddr *) &server_ip, sizeof(server_ip));
    if (success < 0){
        cout << "Error: could not connect to given ip/port. Program exiting. \n";
        return n;
    }
    cout << "Connected to manager! \n";

//send the manager the port number of the UDP port and a request for a node address and the associated
//connectivity table.


    uint16_t out = (*udp_port);

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

    vector<neighbour> v_of_neighbour;
    for (int i = 0; i < num_of_neighbours; i++)
    {
        int start = sizeof(int) + i * (2*sizeof(int) + sizeof(uint16_t));
        neighbour n;
        memcpy(&n.id, nodeinfo + start, sizeof(int));
        start += sizeof(int);
        memcpy(&n.cost, nodeinfo + start, sizeof(int));
        start += sizeof(int);
        memcpy(&n.port, nodeinfo + start, sizeof(uint16_t));
        v_of_neighbour.push_back(n);
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
        _exit(-1);
    }
    //output_file << "Received safe to reach message from manager at " << getcurrenttime().c_str() << endl;
    delete(safetoreachmsg);

    return n;
}

void send_link_up_message(int sock)
{
    int32_t length;
    string message = "Link up";
    length = message.length();
    send(sock, &length, sizeof(int32_t), 0);
    // Then send the actual buffer
    send(sock, message.c_str(), length, 0);
}

void recv_network_up_message(int sock)
{
    int32_t length;
    recv(sock, &length, sizeof(int32_t), MSG_WAITALL);
    char* networkupmsg = new char[length+1];
    bzero(networkupmsg, length);
    recv(sock, networkupmsg, length, 0);
    networkupmsg[length] = 0;
    if (strcmp(networkupmsg, "Network up") != 0)
    {
        cerr << "Error when receving network up message from manager." << endl;
        delete(networkupmsg);
        _exit(-1);
    }
    //output_file << "Received safe to reach message from manager at " << getcurrenttime().c_str() << endl;
    delete(networkupmsg);
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

bool ack = true;
char* buffer = 0;
struct sockaddr_in myaddr;
int udpsocket = 0;
pthread_t thread;
void enforceack()
{
    usleep(500);
    if (ack)
    {
        // ACK received, delete the buffer
        delete(buffer);
        buffer = 0;
    }
    else
    {
        // ACK not receive, try resend
        sendto(udpsocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&myaddr, sizeof(myaddr));
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_detach(thread);
    }
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
        cout << "Received packet initiate request to node " << dest << " from manager" << endl;
        // Send udp packet to own udp server
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family=AF_INET;
        inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
        myaddr.sin_port=htons(selfport);
        buffer = new char[1 + sizeof(int)];
        buffer[0] = 'I'; // I for initiate
        memcpy(buffer + sizeof(char), &dest, sizeof(int));
        sendto(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&myaddr, sizeof(myaddr));
        ack = false;
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_detach(thread);
        return 0;
    }
    else
    {
        char* quitmsg = new char[length+1];
        bzero(quitmsg, length);
        recv(tcpsock, quitmsg, length, 0);
        quitmsg[length] = 0;
        if (strcmp(quitmsg, "Quit") != 0)
        {
            cerr << "Error when receving quit message from manager." << endl;
            delete(quitmsg);
            _exit(-1);
        }
        delete(quitmsg);
        // Send udp packet to own udp server
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family=AF_INET;
        inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
        myaddr.sin_port=htons(selfport);
        buffer = new char[5];
        strcpy(buffer, "Quit");
        sendto(udpsock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&myaddr, sizeof(myaddr));
        ack = false;
        pthread_create(&thread, NULL, enforceack, NULL);
        pthread_join(thread, NULL);
        return -1;
    }
}

int recv_message_from_peer(int currentnode,int udpsock, vector<int> port, vector<int> forwardingtable)
{
    struct sockaddr_in recvfromaddr;
    socklen_t  addrlen = sizeof(recvfromaddr);
    char BUF[1000];
    bzero(BUF,1000);
    recvfrom(udpsock, BUF, 1000,0,(struct sockaddr*) &recvfromaddr, &addrlen );
    if (BUF[0] == 'A')
    {
        // ACK received
        ack = true;
    }
    else if (BUF[0] == 'I')
    {
        // Initiate
        // First send ACK
        sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen);
        int dest;
        memcpy(&dest, BUF + sizeof(char), sizeof(int));
        if (currentnode == dest)
        {
            // Is the destination
            cout << "Packet initiated at the destination node, transmit complete." << endl;
        }
        else if (forwardingtable.at(dest) == -1)
        {
            // Unreachable
            cout << "Destination of the packet is unreachable at the current node. Transmit abort." << endl;
        }
        else
        {
            // Send the packet to nexthop
            memset(&myaddr, 0, sizeof(myaddr));
            myaddr.sin_family=AF_INET;
            inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
            myaddr.sin_port=htons(port.at(forwardingtable.at(dest)));
            buffer = new char[sizeof(char) + 3*sizeof(int)];
            buffer[0] = 'I'; // I for initiate
            memcpy(buffer + sizeof(char), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 1 * sizeof(int), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 2 * sizeof(int), &dest, sizeof(int));
            sendto(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&myaddr, sizeof(myaddr));
            ack = false;
            cout << "Packet to node " << dest << " initiated at current node has been forwarded to nexthop:" << forwardingtable.at(dest) << endl;
            pthread_create(&thread, NULL, enforceack, NULL);
            pthread_detach(thread);
        }
    }
    else if (BUF[0] == 'T')
    {
        // Transfer
        // First send ACK
        sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen);
        int src, transfer, dest;
        memcpy(&src, BUF + sizeof(char), sizeof(int));
        memcpy(&transfer, BUF + sizeof(char) + 1 * sizeof(int), sizeof(int));
        memcpy(&dest, BUF + sizeof(char) + 2 * sizeof(int), sizeof(int));
        if (currentnode == dest)
        {
            // Is the destination
            cout << "Packet initiated at node " << src << " received from " << transfer << " has reached its destination: node " << dest << ". Transmit complete." << endl;
        }
        else if (forwardingtable.at(dest) == -1)
        {
            // Unreachable
            cout << "Destination of the packet is unreachable at the current node. Transmit abort." << endl;
        }
        else
        {
            // Send the packet to nexthop
            memset(&myaddr, 0, sizeof(myaddr));
            myaddr.sin_family=AF_INET;
            inet_pton(AF_INET,"127.0.0.1",&myaddr.sin_addr.s_addr);
            myaddr.sin_port=htons(port.at(forwardingtable.at(dest)));
            buffer = new char[sizeof(char) + 3*sizeof(int)];
            buffer[0] = 'I'; // I for initiate
            memcpy(buffer + sizeof(char), &src, sizeof(int));
            memcpy(buffer + sizeof(char) + 1 * sizeof(int), &currentnode, sizeof(int));
            memcpy(buffer + sizeof(char) + 2 * sizeof(int), &dest, sizeof(int));
            sendto(udpsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&myaddr, sizeof(myaddr));
            ack = false;
            pthread_create(&thread, NULL, enforceack, NULL);
            pthread_detach(thread);
            cout << "Packet received from node " << transfer << " initiated at node " << src << " heading toward node " << dest << " has been forwarded to nexthop:" << forwardingtable.at(dest) << endl;
        }
    }
    else
    {
        // Quit
        // First send ACK
        sendto(udpsock, "ACK", 3, 0, (struct sockaddr*) &recvfromaddr, addrlen);
        cout << "Received quit message from manager." << endl;
        return -1;
    }
    return 0;
    // -1 for quit, 0 for normal
}