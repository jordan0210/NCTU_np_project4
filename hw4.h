#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <fstream>
#include <string.h>

using boost::asio::ip::tcp;
using namespace std;

typedef struct requestData{
    string url;
    string port;
    string testfile;
}requestData;

string socksIP, socksPort;
requestData requestDatas[5];

void parse_QUERY_STRING(string &QUERY_STRING);

void send_default_HTML();

void send_dafault_table(string index, string Msg);

void send_command(string index, string Msg);

void send_shell(string index, string Msg);

void refactor(string &Msg);

void set_request(int ID, unsigned char* req);