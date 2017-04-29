//
// Created by bennywng on 12/5/16.
//
#include "cost_info.h"
#include "dijkstra.h"
#include <ctime>
#include <fstream>


bool invector(vector<element> v, int target)
{
    bool isin = false;
    for (int i = 0; i < v.size(); i++)
    {
        if (v.at(i).destination == target)
        {
            isin = true;
            break;
        }
    }
    return isin;
}

int findsmallest(vector<element>& tentative, vector<element>& confirmed)
{
    int mindist = 100000000;
    int minnode = -1;
    for (int i = 0 ; i < tentative.size(); i++)
    {
        if (!invector(confirmed, tentative.at(i).destination))
        {
            // Not confirmed, already in tentative, shouldn't be -1
            if (tentative.at(i).distance < mindist)
            {
                minnode = i;
                mindist = tentative.at(i).distance;
            }
        }
    }
    return minnode;
}

int findtargetinvector(vector<element> v, int target)
{
    int index = -1;
    for (int i = 0; i < v.size(); i++)
    {
        if (v.at(i).destination == target)
        {
            index = i;
            break;
        }
    }
    return index;
}

void update_adj(vector<element>& tentative, vector<element>& confirmed, vector<element>& pool, vector<cost_info>& v_cost_info, int numnode, int distance, int nexthop)
{

    for (int i = 0 ; i < v_cost_info.size(); i++)
    {
        if (v_cost_info.at(i).src == numnode)
        {
            int dest = v_cost_info.at(i).dest;

            if (!invector(confirmed, dest))
            {
                // Currently not confirmed, check whether in tentative, if not, add into tentative
                if (!invector(tentative, dest))
                {
                    // update distance, then add into tentative
                    pool[dest].distance = distance + v_cost_info.at(i).cost;
                    if (nexthop != -1)
                        pool[dest].nexthop = nexthop;
                    else
                        pool[dest].nexthop = dest;
                    tentative.push_back(pool[dest]);
                }
                else
                {
                    // already in tentative, compare the current distance and the new distance
                    int index = findtargetinvector(tentative, dest);
                    int olddist = tentative[index].distance;
                    if (olddist > (distance + v_cost_info.at(i).cost))
                    {
                        // Need to update this distance
                        tentative[index].distance = distance + v_cost_info.at(i).cost;
                        if (nexthop != -1)
                            tentative[index].nexthop = nexthop;
                        else
                            tentative[index].nexthop = dest;
                    }
                }
            }
        }
    }
}

void printout(vector<element>& confirmed, vector<element>& tentative, ofstream& outputFile)
{
    outputFile << "Contents in confirmed: " << endl;
    bool first_print = true;
    for (int i = 0 ; i < confirmed.size(); i++)
    {
        if (first_print)
            first_print = false;
        else
            outputFile << ",";

        int destination = confirmed.at(i).destination;
        int distance = confirmed.at(i).distance;
        int nexthop = confirmed.at(i).nexthop;
        string strnexthop;
        if (nexthop == -1)
            strnexthop = "-";
        else
            strnexthop = to_string(nexthop);
        outputFile << "(" << destination << "," << distance << "," << strnexthop << ")";
    }
    if (!confirmed.size())
        outputFile << "Empty";
    outputFile << endl ;
    first_print = true;
    bool printed  = false;
    outputFile << "Contents in tentative: " << endl;
    for (int i = 0 ; i < tentative.size(); i++)
    {
        int destination = tentative.at(i).destination;
        if (invector(confirmed, destination))
            continue;
        int distance = tentative.at(i).distance;
        int nexthop = tentative.at(i).nexthop;
        string strnexthop;
        printed = true;
        if (!first_print)
            outputFile << ",";
        else
            first_print = false;
        if (nexthop == -1)
            strnexthop = "-";
        else
            strnexthop = to_string(nexthop);
        outputFile << "(" << destination << "," << distance << "," << strnexthop << ")";
    }
    if (!printed)
        outputFile << "Empty";
    outputFile << endl << endl ;
}

vector<int> dijkstra(int current_node, vector<cost_info> v_cost_info, int num_of_nodes, ofstream& outputFile)
{
    vector<element> pool;
    vector<element> confirmed;
    vector<element> tentative;
    for (int i = 0; i < num_of_nodes; i++)
    {
        element e;
        e.distance = -1;
        e.nexthop = -1;
        e.destination = i;
        pool.push_back(e);
    }
    // Add the source into tentative
    pool[current_node].distance = 0;
    tentative.push_back(pool[current_node]);

    int i = 1;
    while ((tentative.size() != confirmed.size()) && (confirmed.size() != num_of_nodes))
    {
        int smallestnode = findsmallest(tentative, confirmed);
        int numnode = tentative.at(smallestnode).destination;
        int distance = tentative.at(smallestnode).distance;
        int nexthop = tentative.at(smallestnode).nexthop;
        // put the smallest node into confirmed
        confirmed.push_back(tentative[smallestnode]);
        // Update adjcent distance
        update_adj(tentative, confirmed, pool, v_cost_info, numnode, distance, nexthop);
        time_t result = time(nullptr);
        outputFile << "After round " << i++ << ": " << asctime(localtime(&result)) << flush;
        printout(confirmed, tentative, outputFile);
    }

    vector<int> forward_table(num_of_nodes, -1);
    // put nexthop into the forward_table
    for (int i = 0; i < confirmed.size(); i++)
        forward_table[confirmed.at(i).destination] = confirmed.at(i).nexthop;
    // -1 means either current node, or unreachable
    return forward_table;
}