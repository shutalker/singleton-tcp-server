#include "server.h"


int main()
{
    TCPServer::SingletonTCPServer( "127.0.0.1", 9999 );
    TCPServer::SingletonTCPServer().run();

    return 0;
}