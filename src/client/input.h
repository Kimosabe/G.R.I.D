#ifndef INPUT_H_
#define INPUT_H_

#include <string>
#include <vector>
#include <stack>
#include <iostream>

class grid_task;

bool read_net(std::vector<std::string> &addresses, std::vector< std::stack<int> > &ports);

bool parse_task(grid_task &gt, std::istream &fin);

#endif //INPUT_H_
