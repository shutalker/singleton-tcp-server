#include "server.h"


inline TCPServer & TCPServer::SingletonTCPServer( const std::string &server_address,
                                                  const int server_port )
{
    static TCPServer server( server_address, server_port );
    return *( &server );
}


inline void TCPServer::stop_server( int signal_num )
{
    TCPServer::SingletonTCPServer().stop();
}


TCPServer::TCPServer( const std::string &server_address,
                      const int server_port ):
listener_socket( -1 ),
accept_socket( -1 ),
workers_amount( 1024 ),
threads_amount( 2 ),
request_queues( threads_amount  ),
thread_pool(threads_amount ),
thread_mutex( threads_amount ),
thread_condition_variable( threads_amount ),
terminate_worker_flag( false )
{
    // set signal handler for main thread
    struct sigaction main_action;

    std::memset( &main_action, 0, sizeof( main_action ) );
    main_action.sa_handler = stop_server;

    sigemptyset( &main_action.sa_mask );                                                             
    sigaddset( &main_action.sa_mask, SIGINT );
    sigaddset( &main_action.sa_mask, SIGTERM ); 
    sigaction( SIGINT, &main_action, nullptr );
    sigaction( SIGTERM, &main_action, nullptr );

    sockaddr_in server_addr;

    // initializing sockaddr_in structure
    std::memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( server_port );
    inet_pton( AF_INET, server_address.c_str(), &( server_addr.sin_addr ) );

    listener_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if( listener_socket < 0 )
    {
        std::cerr << "unable to create socket" << std::endl;
        stop();
    }

    int enable_option_flag = 1;
    setsockopt( listener_socket, SOL_SOCKET, SO_REUSEADDR,
                static_cast<void *>( &enable_option_flag ),
                static_cast<socklen_t>( sizeof( enable_option_flag ) ) );

    if( bind( listener_socket, ( struct sockaddr * ) &server_addr,
            sizeof( server_addr ) ) < 0 )
    {
        std::cerr << "unable to create socket" << std::endl;
        stop();
    }

    // creating request handling threads
    for( auto i = 0; i < threads_amount; i++ )
    {
        thread_pool[i] = std::thread( std::bind( &TCPServer::worker, this,
                                    &request_queues[i], i ) );
    }

    std::cout << "Server started at " << server_address << ":" << server_port
            << std::endl;
}


TCPServer::~TCPServer()
{
}


void TCPServer::run()
{
    sockaddr  request_host;
    socklen_t address_size;
    
    int status; // status of listen() syscall
    int current_thread = 0;

    std::memset( &request_host, 0, sizeof( request_host ) );

    status  = listen( listener_socket, workers_amount );

    if( status < 0 )
    {
        std::cerr << std::strerror( status ) << std::endl;
        stop();
    }

    while( !terminate_worker_flag )
    {
        accept_socket = accept( listener_socket, &request_host, &address_size );

        if( accept_socket != -1 )
        {
            std::cout << "Incoming connection" << std::endl;
            {
                std::lock_guard<std::mutex> guard( thread_mutex[current_thread] );
                request_queues[current_thread].push( accept_socket );

                // prevent from closing accepted request by listening thread
                // it should be closed by worker thread!
                accept_socket = -1;
            }

            thread_condition_variable[current_thread++].notify_one();

            if( current_thread > (thread_pool.size() - 1) )
                current_thread = 0;
        }
    }
}


void TCPServer::worker( std::queue<int> *request_queue, int thread_id )
{
    std::queue<int> *queue = request_queue;

    int id = thread_id;
    int request_socket;

    // requests processing loop
    while( !terminate_worker_flag )
    {
        {
            std::unique_lock<std::mutex> request_lock( thread_mutex[id] );

            // handling suprious wakeup
            while( queue->empty()  && ( !terminate_worker_flag ) )
                thread_condition_variable[id].wait( request_lock );
            
            if( terminate_worker_flag )
                break;

            request_socket = queue->front();
            queue->pop();
        }

        process_request( request_socket );
    }

    // cleaning thread's request queue
    if( !queue->empty() )
    {
        {
            std::lock_guard<std::mutex> guard( thread_mutex[id] );
            
            while( !queue->empty() )
            {
                request_socket = queue->front();

                send( request_socket, "UNEXPECTEDLY CLOSED\n", 21 , 0);
                close( request_socket );

                queue->pop();
            }
        }
    }
}


void TCPServer::process_request( int request_socket )
{

    sleep( 10 );
    send( request_socket, "HELLO\n", 7, 0 );
    close( request_socket );
}


void TCPServer::stop()
{
    terminate_worker_flag = true;

    // close listener socket on recieving requests
    if( listener_socket )
        shutdown( listener_socket, SHUT_RD );

    // if request was accepted bun wasn't pushed into worker thread's queue
    if( accept_socket )
    {
        send( accept_socket, "UNEXPECTEDLY CLOSED\n", 21 , 0);
        close( accept_socket );
    }

    std::cerr << "\nServer is shutting down..." << std::endl;

    for( auto i = 0; i < thread_condition_variable.size(); i++ )
    {
        thread_condition_variable[i].notify_one();
        if( thread_pool[i].joinable() )
            thread_pool[i].join();
    }

    // destroy listener socket
    if( listener_socket )
        close( listener_socket );
    
    request_queues.clear();
    thread_condition_variable.clear();
    thread_mutex.clear();
    thread_pool.clear();

    std::cerr << "Server was terminated by ^C" << std::endl;
}