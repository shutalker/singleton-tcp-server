#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include <iostream>
#include <vector>       // thread_pool
#include <queue>        // request_queues
#include <string>
#include <thread>       // thread_pool
#include <mutex>
#include <condition_variable>

#include <cstring>      // std::memset(), std::strerror()
#include <unistd.h>     // close() socket
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h> // socket(), bind(), listen(), accept(), getaddrinfo()
#include <netdb.h>      
#include <netinet/in.h>
#include <arpa/inet.h>  // inet_ntop()


class TCPServer
{
public:
    static TCPServer & SingletonTCPServer( const std::string &server_address="",
                                           const int server_port=-1 );

    void run();
    void stop();

private:
    TCPServer( const std::string &server_address, const int server_port );
    ~TCPServer();

    // signal handler
    static void stop_server( int signal_num );

    TCPServer( TCPServer const & ) = delete;
    TCPServer & operator= ( TCPServer const & ) = delete;

    int listener_socket;
    int accept_socket;

    int workers_amount;
    int threads_amount;

    std::vector<std::queue<int>>  request_queues;
    std::vector<std::thread>      thread_pool;
    std::vector<std::mutex>       thread_mutex;

    std::vector<std::condition_variable> thread_condition_variable;

    volatile bool terminate_worker_flag;

    void worker( std::queue<int> *request_queue, int thread_id );
    void process_request( int request_socket );
};

#endif // _TCP_SERVER_H_