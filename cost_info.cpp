//
// Created by Chen Wang on 12/3/16.
//

#include "cost_info.h"

cost_info::cost_info(int source, int destination, int numcost)
{
    src = source;
    dest = destination;
    cost = numcost;
}

cost_info::cost_info(const cost_info &other)
{
    src = other.src;
    dest = other.dest;
    cost = other.cost;
}