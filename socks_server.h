#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <string.h>
#include <vector>
#include <fstream>
#include <regex>
#include <sys/wait.h>

using boost::asio::ip::tcp;
using namespace std;

typedef struct socket4_request{
    unsigned char VN;
    unsigned char CD;
    unsigned int dstPort;
    string dstIP;
    unsigned char *userId;
    int userIdLength;
    unsigned char *domainName;
    int domainNameLength;
    string url;
}Request;

typedef struct firewall_config{
    unsigned char mode;
    string rule;
}config;

char **test_argv;
boost::asio::io_context io_context;

void parseRequest(unsigned char *data, Request &req);

bool checkFireWall(unsigned char CD, string url);