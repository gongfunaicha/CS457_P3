#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include "cost_info.h"
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <cstring>
#include <ctime>
#include <stdlib.h>


using namespace std;

int ReadNumberFromString(string s)
{
    stringstream ss(s);
    int num;
    ss >> num;
    return num;
}

pair<int,int> getsrcdest(string line)
{
    stringstream ss(line);
    int src, dest;
    ss >> src >> dest;
    return make_pair(src, dest);
};


string getcurrenttime()
{
    time_t result = time(nullptr);
    return asctime(localtime(&result));
}

void PutCostinfoIntoVector(string s, vector<cost_info>& v_cost_info)
{
    stringstream ss(s);
    int source = 0, destination = 0, numcost = 0;
    ss >> source >> destination >> numcost;
    v_cost_info.push_back(cost_info(source, destination, numcost));
    v_cost_info.push_back(cost_info(destination, source, numcost));
}

int main(int argc, char* argv[]) {
    if (argc != 2)
    {
        cerr << "Wrong number of arguments specified." << endl;
        cerr << "Usage: manager <topology file>" << endl;
        return -1;
    }
    // Correct number of arguments
    // First open output file
    ofstream output_file;
    output_file.open("manager.out");
    if (!output_file.is_open())
    {
        cerr << "Error creating manager.out." << endl;
        return -1;
    }
    // Then open input file
    ifstream topology_file;
    topology_file.open(argv[1]);
    if (!topology_file.is_open())
    {
        cerr << "Error opening the file. Please check whether the file exists or has the right permission." << endl;
        return -1;
    }
    // File opened
    string line;
    // First read the number of routers
    if (!getline(topology_file, line))
    {
        // Getline failed
        cerr << "Error reading the first line." << endl;
        return -1;
    }
    int num_of_routers = ReadNumberFromString(line);
    vector<cost_info> v_cost_info;
    while (getline(topology_file, line))
    {
        if (line.at(0) == '-')
        {
            // Possible "-1" line
            int number;
            number = ReadNumberFromString(line);
            if (number == -1)
                break;
            else
            {
                // Shouldn't have string starting with "-" and not a "-1"
                cerr << "Error reading the file." << endl;
                return -1;
            }
        }
        else
        {
            // Should be cost_info line
            PutCostinfoIntoVector(line, v_cost_info);
            // Already duplicate infomation in v_cost_info
        }
    }
    // Start a TCP server
    int listensock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_ip;				//struct to hold the ip address of server
    server_ip.sin_family=AF_INET;
    server_ip.sin_addr.s_addr = INADDR_ANY;
    uint16_t port = 0;
    for(port = 50000; port < 64000; port++) {
        server_ip.sin_port = htons(port);        //htons makes sure that the port number is converted to big.
        server_ip.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY gets the address of the server
        int ret = ::bind(listensock, (struct sockaddr *) &server_ip, sizeof(server_ip)); //binds the socket to the server's ip address--for some stupid reason it requires that you typecast the sockaddr_in to sock_addr.
        if (ret == -1) {
            continue;
        } else {
            break;
        }
    }
    listen(listensock, 10);
    cout << "Server listen on port: " << port << endl;
    output_file << "Server listen on port: " << port << " at " << getcurrenttime().c_str() << flush;

    // Start creating router process
    vector<pid_t> vector_of_children;
    bool ischildren = false;
    int i = 0;
    for (; i < num_of_routers; i++)
    {
        pid_t current_pid = fork();
        if (current_pid < 0)
        {
            cerr << "Error when doing fork." << endl;
            return -1;
        }
        else if (current_pid == 0)
        {
            // Child process
            ischildren = true;
            break;
        }
        else
        {
            // Parent process
            vector_of_children.push_back(current_pid);
        }
    }
    if (ischildren)
    {
        cout << "executing router process: " << i << endl;
        execlp("router", "router", to_string(i).c_str(), to_string(port).c_str(), to_string(num_of_routers).c_str(), (char*) 0);
    }
    else
    {
        // Parent process here

        // Accept connection from children
        vector<int> vector_of_socket;
        for (int i = 0; i < num_of_routers; i++)
        {
            struct sockaddr_in client_ip;
            socklen_t len = sizeof(client_ip);
            int connected_sock = 0;
            connected_sock = accept(listensock, (struct sockaddr *) &client_ip,  &len);
            if (connected_sock == -1)
            {
                // Error when accepting
                cerr << "Error when accepting TCP connection from router." << endl;
                return -1;
            }
            vector_of_socket.push_back(connected_sock);
            output_file << "Node " << i << " just connected at " << getcurrenttime().c_str() << flush;
        }

        // Get port information from all the routers
        vector<uint16_t> vector_of_ports;
        for (int i = 0; i < num_of_routers; i++)
        {
            uint16_t portinfo;
            recv(vector_of_socket.at(i), &portinfo, sizeof(portinfo), MSG_WAITALL);
            vector_of_ports.push_back(portinfo);
            output_file << "Received port number: " << portinfo << " from node " << i << " at " << getcurrenttime().c_str() << flush;
        }

        // Send node number, neighbour info, cost info, port info to each of the routers
        for (int i = 0; i < num_of_routers; i++)
        {
            vector<cost_info> vector_adj;
            output_file << "Sending message: ";
            for (int j = 0; j < v_cost_info.size(); j++)
            {
                if (v_cost_info.at(j).src == i)
                    vector_adj.push_back(v_cost_info[j]);
            }
            int size = sizeof(int) + vector_adj.size()*(2*sizeof(int) + sizeof(uint16_t));
            char* sendbuffer = new char[size];
            bzero(sendbuffer, size);
            // Copy the node number into sendbuffer
            memcpy(sendbuffer, &i, sizeof(int));
            output_file << "Assign node number " << i << ", having neighbour:\n";
            for (int j = 0; j < vector_adj.size(); j++)
            {
                // Start point of copy
                int start = sizeof(int) + j * (2*sizeof(int) + sizeof(uint16_t));
                int neighbour = vector_adj.at(j).dest;
                int cost = vector_adj.at(j).cost;
                uint16_t port = vector_of_ports.at(neighbour);
                memcpy(sendbuffer + start, &neighbour, sizeof(int));
                start += sizeof(int);
                memcpy(sendbuffer + start, &cost, sizeof(int));
                start += sizeof(int);
                memcpy(sendbuffer + start, &port, sizeof(uint16_t));
                output_file << neighbour << " at port " << port << " with link cost " << cost << endl;
            }
            /*
             * Packet format:
             * First send a int32_t packet indicating the size of the total packet.
             * Then send a packet using the following format:
             * <node_number>(int)(<neighbour_node_number>(int)<cost>(int)<port>(uint16_t)) * num_of_neighbours
             */
            // Sendbuffer should be ready, now send the length
            int32_t sendsize = size;
            send(vector_of_socket.at(i), &sendsize, sizeof(int32_t), 0);
            // Then send the actual buffer
            send(vector_of_socket.at(i), sendbuffer, sendsize, 0);
            output_file << "at " << getcurrenttime().c_str() << flush;
        }

        // Receive ready message from all the routers
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            recv(vector_of_socket.at(i), &length, sizeof(length), MSG_WAITALL);
            char* readymsg = new char[length+1];
            bzero(readymsg, length);
            recv(vector_of_socket.at(i), readymsg, length, 0);
            readymsg[length] = 0;
            if (strcmp(readymsg, "Ready") != 0)
            {
                cerr << "Error when receving Ready message from router." << endl;
                delete(readymsg);
                return -1;
            }
            output_file << "Received ready message from node " << i << " at " << getcurrenttime().c_str() << flush;
            delete(readymsg);
        }

        // Send info that is safe to reach neighbours
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            string message = "Safe to reach";
            length = message.length();
            /*
             * Packet format:
             * First send a int32_t packet indicating the size of the total packet.
             * Then send a packet using the following format:
             * "Safe to reach" without terminating zero
             */
            send(vector_of_socket.at(i), &length, sizeof(int32_t), 0);
            // Then send the actual buffer
            send(vector_of_socket.at(i), message.c_str(), length, 0);
            output_file << "Sent it is safe to reach neighbours to node " << i << " at " << getcurrenttime().c_str() << flush;
        }

        // Receive link up message from all the routers
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            recv(vector_of_socket.at(i), &length, sizeof(length), MSG_WAITALL);
            char* linkupmsg = new char[length+1];
            bzero(linkupmsg, length);
            recv(vector_of_socket.at(i), linkupmsg, length, 0);
            linkupmsg[length] = 0;
            if (strcmp(linkupmsg, "Link up") != 0)
            {
                cerr << "Error when receving link is up message from router." << endl;
                delete(linkupmsg);
                return -1;
            }
            delete(linkupmsg);
            output_file << "Recieved link up message from node " << i << " at " << getcurrenttime().c_str() << flush;
        }

        // Send info that network is up
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            string message = "Network up";
            length = message.length();
            /*
             * Packet format:
             * First send a int32_t packet indicating the size of the total packet.
             * Then send a packet using the following format:
             * "Network up" without terminating zero
             */
            send(vector_of_socket.at(i), &length, sizeof(int32_t), 0);
            // Then send the actual buffer
            send(vector_of_socket.at(i), message.c_str(), length, 0);
            output_file << "Sent network up message to node " << i << " at " << getcurrenttime().c_str() << flush;
        }

        // Receive forwarding table up message from all the routers
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            recv(vector_of_socket.at(i), &length, sizeof(length), MSG_WAITALL);
            char* forwardingtableupmsg = new char[length+1];
            bzero(forwardingtableupmsg, length);
            recv(vector_of_socket.at(i), forwardingtableupmsg, length, 0);
            forwardingtableupmsg[length] = 0;
            if (strcmp(forwardingtableupmsg, "Forwarding table up") != 0)
            {
                cerr << "Error when receving forwarding table is up message from router." << endl;
                delete(forwardingtableupmsg);
                return -1;
            }
            delete(forwardingtableupmsg);
            output_file << "Received forwarding table up message from node " << i << " at " << getcurrenttime().c_str() << flush;
        }

        // Send info that network is ready to send packet
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            string message = "Ready to send packet";
            length = message.length();
            /*
             * Packet format:
             * First send a int32_t packet indicating the size of the total packet.
             * Then send a packet using the following format:
             * "Ready to send packet" without terminating zero
             */
            send(vector_of_socket.at(i), &length, sizeof(int32_t), 0);
            // Then send the actual buffer
            send(vector_of_socket.at(i), message.c_str(), length, 0);
            output_file << "Sent network is ready message to node " << i << " at " << getcurrenttime().c_str() << flush;
        }


        while (getline(topology_file, line))
        {
            if (line.at(0) == '-')
            {
                // Might be -1, use method to check
                int number = ReadNumberFromString(line);
                if (number != -1)
                    cerr << "Error when reading packet transmit information." << endl;
                break;
            }
            else
            {
                // Should be data transmit
                pair<int,int> src_dest = getsrcdest(line);
                int src = src_dest.first;
                int dest = src_dest.second;
                /*
                 * Packet format:
                 * First send a int32_t packet with value "-1" indicating transmission
                 * Then send a int packet with value (destination)
                 */
                // send length = -1 to indicate transmit
                int32_t length = -1;
                send(vector_of_socket.at(src), &length, sizeof(int32_t), 0);
                // Then send the actual buffer
                send(vector_of_socket.at(src), &dest, sizeof(int), 0);
                output_file << "Sent command to initiate a packet from node " << dest << " to node " << src <<" at " << getcurrenttime().c_str() << flush;
                usleep(500000);
                output_file << "Sleep for half second for router to process the transmission at " << getcurrenttime().c_str() << flush;
            }
        }

        // Signal all routers need to quit
        for (int i = 0; i < num_of_routers; i++)
        {
            int32_t length;
            string message = "Quit";
            /*
             * Packet format:
             * First send a int32_t packet with the size of the total packet.
             * Then send a packet using the following format:
             * "Quit" without terminating zero
             */
            length = message.length();
            send(vector_of_socket.at(i), &length, sizeof(int32_t), 0);
            // Then send the actual buffer
            send(vector_of_socket.at(i), message.c_str(), length, 0);

            output_file << "Sent quit message to node " << i << " at " << getcurrenttime().c_str() << flush;

            // Close the socket of that router
            close(vector_of_socket.at(i));

            // Then wait for that exact pid to quit
            int status;
            usleep(500);
        }

        // Manager can now terminate
        close(listensock);
        topology_file.close();
        // Wait for router to cout
        usleep(50000);
        output_file << "Manager quit at " << getcurrenttime().c_str() << flush;
        cout << "Manager quit at " << getcurrenttime().c_str() << flush;
        output_file.close();
        return 0;
    }
    return 0;
}
