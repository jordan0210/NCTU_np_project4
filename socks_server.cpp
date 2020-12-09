#include "socks_server.h"

class session
    :public std::enable_shared_from_this<session>{
    private:
        tcp::resolver resolver;
        tcp::socket client_socket;
        tcp::socket server_socket;
        boost::asio::io_context& io_context;
        boost::asio::ip::tcp::resolver::results_type endpoints;
        string url, port;
        enum { max_length = 1024};
        unsigned char UL_buf[max_length];
        unsigned char DL_buf[max_length];

    public:
        session(boost::asio::io_context& io_context,
            tcp::socket socket,
            string url, string port)
            :resolver(io_context), server_socket(io_context), client_socket(std::move(socket)), io_context(io_context),
            url(url), port(port){
        }

        void start(){
            do_resolve();
        }

    private:
        void do_resolve(){
            auto self(shared_from_this());
            cout << "Start resolve." << endl;
            resolver.async_resolve(url, port,
                [this, self](const boost::system::error_code &ec,
                    const boost::asio::ip::tcp::resolver::results_type results){
                    if (!ec){
                        cout << "Success resolve." << endl;
                        endpoints = results;
                        do_connect();
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }

        void do_connect(){
            auto self(shared_from_this());
            cout << "Start connect." << endl;
            boost::asio::async_connect(server_socket, endpoints,
                [this, self](const boost::system::error_code &ec, tcp::endpoint ed){
                    if (!ec){
                        cout << "Success connect." << endl;
                        do_UL_read();
                        do_DL_read();
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }

        void do_UL_read(){
            auto self(shared_from_this());
            bzero(UL_buf, sizeof(UL_buf));
            client_socket.async_read_some(boost::asio::buffer(UL_buf, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        cout << "Success UL read." << endl;
                        string data = (char*)UL_buf;
                        cout << "****************************" << endl;
                        cout << data << endl;
                        cout << "****************************" << endl;
                        do_UL_write(length);
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }

        void do_UL_write(std::size_t length){
            auto self(shared_from_this());
            boost::asio::async_write(server_socket, boost::asio::buffer(UL_buf, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec){
                        cout << "Success UL write." << endl;
                        do_UL_read();
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }

        void do_DL_read(){
            auto self(shared_from_this());
            bzero(DL_buf, sizeof(DL_buf));
            server_socket.async_read_some(boost::asio::buffer(DL_buf, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        cout << "Success DL read." << endl;
                        string data = (char*)DL_buf;
                        cout << "****************************" << endl;
                        cout << data << endl;
                        cout << "****************************" << endl;
                        do_DL_write(length);
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }

        void do_DL_write(std::size_t length){
            auto self(shared_from_this());
            boost::asio::async_write(client_socket, boost::asio::buffer(DL_buf, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec){
                        cout << "Success DL write." << endl;
                        do_DL_read();
                    } else {
                        client_socket.close();
                        server_socket.close();
                    }
                }
            );
        }
};

class socks
    : public std::enable_shared_from_this<socks>{
    public:
        socks(tcp::socket socket)
            : socket_(std::move(socket)){
        }

        void start(){
            do_read();
        }

    private:
        void do_read(){
            auto self(shared_from_this());
            bzero(data_, sizeof(data_));
            socket_.async_read_some(boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length){
                    if (!ec){
                        Request req;
                        parseRequest(data_, req, data_);
                        string reply;
                        if (req.VN != 0x04){
                            *(data_+1) = 0x5b;
                            reply = "Reject";
                        } else {
                            if (checkFireWall(req.dstIP)){
                                *(data_+1) = 0x5a;
                                reply = "Accept";
                            } else {
                                *(data_+1) = 0x5b;
                                reply = "Reject";
                            }
                        }

                        cout << "<S_IP>: " << socket_.local_endpoint().address().to_string() << endl;
                        cout << "<S_PORT>: " << to_string(htons(socket_.local_endpoint().port())) << endl;
                        cout << "<D_IP>: " << req.dstIP << endl;
                        cout << "<D_PORT>: " << req.dstPort << endl;
                        if (req.CD == 0x01)
                            cout << "<Command>: CONNECT" << endl;
                        else if (req.CD == 0x02)
                            cout << "<Command>: BIND" << endl;
                        else
                            cout << "<Command>: UNKNOWN -- CD = " << (int)req.CD << endl;
                        cout << "<Reply>: " << reply << endl;

                        cout << "---------------" << endl;
                        for (int i=0; i<8; i++){
                            cout << i << ": ";
                            printf("%2x ", *(data_+i));
                            cout << endl;
                        }

                        do_write(req, sizeof(unsigned char)*8);
                    } else {
                        socket_.close();
                    }
                }
            );
        }

        void do_write(Request req, std::size_t length){
            auto self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
                [this, self, req](boost::system::error_code ec, std::size_t /*length*/){
                    if (!ec){
                        if (*(this->data_+1) == 0x5a){
                            string url;
                            if (req.dstIP == "0.0.0.1")
                                url = (char*)req.domainName;
                            else
                                url = req.dstIP;
                            cout << url << endl;
                            make_shared<session>(io_context, move(socket_), url, to_string(req.dstPort))->start();
                            io_context.run();
                        }
                    } else {
                        socket_.close();
                    }
            });
        }

        bool checkFireWall(string dstIp){
            return true;
        }

        tcp::socket socket_;
        enum { max_length = 1024 };
        unsigned char data_[max_length];
};

class server{
    public:
        server(boost::asio::io_context& io_context, short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
            do_accept();
        }

    private:
        void do_accept(){
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket){
                if (!ec){
                    std::make_shared<socks>(std::move(socket))->start();
                }

                do_accept();
            });
        }

        tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]){
    test_argv = argv;
    try{
        if (argc != 2){
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    } catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << "\n";
    }

  return 0;
}

void parseRequest(unsigned char *data, Request &req, unsigned char *res){
    req.VN = data[0];
    req.CD = data[1];
    req.dstPort = ntohs(*(short *)(data + 2));
    req.dstIP = to_string((int)data[4]) + "." + to_string((int)data[5]) + "." + to_string((int)data[6]) + "." + to_string((int)data[7]);
    req.userId = data + 8;
    req.userIdLength = 0;
    while (data[8 + req.userIdLength] != 0x00){
        req.userIdLength++;
    }
    if (req.dstIP == "0.0.0.1"){
        req.domainName = data + 9 + req.userIdLength;
        req.domainNameLength = 0;
        while(data[9 + req.userIdLength + req.domainNameLength] != 0x00){
            req.domainNameLength++;
        }
    }
    res[0] = 0x00;
    // bzero(res, sizeof(unsigned char)*8);
    // memcpy(data+2, res+2, sizeof(char)*6);
}
