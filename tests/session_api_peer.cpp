// Exercise exactly the four unchanged FoggyTCP functions, including blocking EOF.
#include "foggy_tcp.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <stdexcept>
std::string unhex(const std::string &s) {
    std::string out;
    for (size_t i=0;i<s.size();i+=2) out.push_back(std::stoi(s.substr(i,2),nullptr,16));
    return out;
}
std::string hex(const char *data, int count) {
    const char *digits="0123456789abcdef";
    std::string out;
    for (int i=0;i<count;++i) {
        auto c=static_cast<unsigned char>(data[i]);
        out+=digits[c>>4];out+=digits[c&15];
    }
    return out;
}
int main(int argc,char **argv) {
    if (argc!=4) return 2;
    void *io=foggy_socket(std::string(argv[1])=="listen"?TCP_LISTENER:TCP_INITIATOR,argv[3],argv[2]);
    if (!io) return 1;
    std::cout<<"READY "<<static_cast<foggy_socket_t *>(io)->my_port<<std::endl;
    std::string line;
    while (std::getline(std::cin,line)) {
        auto at=line.find(' ');
        auto cmd=line.substr(0,at), value=at==std::string::npos?"":line.substr(at+1);
        if (cmd=="SEND") {
            auto bytes=unhex(value);
            std::cout<<(foggy_write(io,bytes.data(),bytes.size())==0?"SENT":"ERROR")<<std::endl;
        } else if (cmd=="READ") {
            char data[4096];
            int size=std::min(4096,std::stoi(value));
            int n=foggy_read(io,data,size);
            if(n>0)std::cout<<"DATA "<<hex(data,n)<<std::endl;
            else std::cout<<(n==0?"EOF":"ERROR")<<std::endl;
        } else if (cmd=="CLOSE") {
            int result=foggy_close(io);io=nullptr;
            std::cout<<"CLOSED "<<result<<std::endl;
            break;
        }
    }
    if(io)foggy_close(io);
}
