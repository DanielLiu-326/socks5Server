#include <iostream>
#include"Socks5Server.h"
#include<unistd.h>
#include <signal.h>
#include"ServerConfigs.h"
int main(int argc ,char * argv[]) {
    if(argc == 1)
    {
        initServerConfig("./server.config");
    }
    else
    {
        initServerConfig(argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    Socks5Server server;
    server.id_key = ACCOUNTS;
    server.init(THREADS,PORT,NULL);
    server.start();
    char inputBuffer[256];
    while(true)
    {
        std::cin.getline(inputBuffer,sizeof(inputBuffer));
        std::string in(inputBuffer);
        if(in=="stop")
        {
            server.stop();
            return 0;
        }else if(in == "connections")
        {
            server.printConNum();
        }
    }
    return -1;
}
