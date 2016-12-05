#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include "cost_info.h"
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int ReadNumberFromString(string s)
{
    stringstream ss(s);
    int num;
    ss >> num;
    return num;
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
        int ret = bind(listensock, (struct sockaddr *) &server_ip,
                 sizeof(server_ip)); //binds the socket to the server's ip address--for some stupid reason it requires that you typecast the sockaddr_in to sock_addr.
        if (ret == -1) {
            continue;
        } else {
            break;
        }
    }
    listen(listensock, 10);
    cout << "Server listen on port: " << port << endl;

    // Start creating router process
    vector<pid_t> vector_of_children;
    bool ischildren = false;
    for (int i = 0; i < num_of_routers; i++)
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
        // ******************
        // Child process here
        // ******************
    }
    else
    {
        // Parent process here
    }
    return 0;
}