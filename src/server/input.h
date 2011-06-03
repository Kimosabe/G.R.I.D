#ifndef INPUT_H_
#define INPUT_H_

#include <string>
#include <vector>
#include <stack>
#include <map>

#define NET_DESCRIPTION_FILE "node_net.dat"
#define CONFIG_FILE "..\\test\\config.txt"

bool read_net(std::vector<std::string> &addresses, std::vector< std::stack<int> > &ports);

typedef std::map<std::string, std::string> config_t;

bool read_config(config_t &config);

#endif //INPUT_H_
