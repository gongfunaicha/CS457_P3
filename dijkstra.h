//
// Created by bennywng on 12/5/16.
//

#ifndef CS457_P3_DIJKSTRA_H
#define CS457_P3_DIJKSTRA_H
#include <vector>
#include <iostream>
using namespace std;

struct element
{
    int distance;
    int nexthop;
    int destination;
};
bool invector(vector<element> v, int target);
int findsmallest(vector<element>& tentative, vector<element>& confirmed);
int findtargetinvector(vector<element> v, int target);
void update_adj(vector<element>& tentative, vector<element>& confirmed, vector<element>& pool, vector<cost_info>& v_cost_info, int numnode, int distance, int nexthop);
void printout(vector<element>& confirmed, vector<element>& tentative);
vector<int> dijkstra(int current_node, vector<cost_info> v_cost_info, int num_of_nodes);

#endif //CS457_P3_DIJKSTRA_H