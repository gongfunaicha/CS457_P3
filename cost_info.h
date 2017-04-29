//
// Created by Chen Wang on 12/3/16.
//

#ifndef CS457_P3_COST_INFO_H
#define CS457_P3_COST_INFO_H


class cost_info {
public:
    int src;
    int dest;
    int cost;
    cost_info(int source, int destination, int numcost);
    cost_info(const cost_info& other);
};


#endif //CS457_P3_COST_INFO_H
