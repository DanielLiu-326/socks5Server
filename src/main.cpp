#include <iostream>
#include"Socks5Server.h"
int main()
{

    Socks5Server server;
    server.setConfig("./server.config");
    server.init();
    server. start();
    std::string command;
    while(true)
    {
        command ="";
        std::cin>>command;
        if(command == "stop"||command == "quit"||command == "exit")
        {
            server.stop();
            std::cout<<"quit correctly"<<std::endl;
            return 0;
        }
        else if(command == "connection")
        {
            auto && load = server.getSessionLoad();
            for(int i = 0;i<load.size();i++)
            {
                std::cout<<"thread"<<i<<" : "<<load[i]<<std::endl;
            }
        }
    }

}
