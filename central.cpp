#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include "backend.h"
#include <poll.h>
#include <cstring>
#include <cstdlib>

#define BACKLOG 2     // how many pending TCP connections queue will hold

using namespace std;

int sockfd_A;
int sockfd_B;
int newfd_A;
int newfd_B;

string src = "";
string dest = "";
Graph graph;
string nameList[MAX_USER_NUM];
PathInfo pathInfo;

char recv_buf[BUF_SIZE];

/**
 * variable for central UDP
 */
int sockfd_udp_central;
vector<struct addrinfo *> backend_serverinfo; // index 0 for server T, 1 for S, 2 for P.

void sigchld_handler(int s) {
    (void) s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/listener.c
 */
void bootUpCentralUDPListener() {
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(localhost, UDP_PORT_C, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd_udp_central = socket(p->ai_family, p->ai_socktype,
                                         p->ai_protocol)) == -1) {
            perror("Central UDP listener: socket");
            continue;
        }

        if (::bind(sockfd_udp_central, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd_udp_central);
            perror("Central UDP listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Central UDP listener: failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 */
void bootUpServerUDPTalker(const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(localhost, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        exit(2);
    }

    backend_serverinfo.push_back(servinfo);
}

void bootUpUDP() {
    bootUpCentralUDPListener();
    bootUpServerUDPTalker(UDP_PORT_T);
    bootUpServerUDPTalker(UDP_PORT_S);
    bootUpServerUDPTalker(UDP_PORT_P);
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 * contact server T via Graph structure
 */
void contactServerT() {
    sockaddr *server_T_addr = backend_serverinfo.at(0)->ai_addr;
    socklen_t server_T_addrlen = backend_serverinfo.at(0)->ai_addrlen;

    int srclen = src.length();
    sendto(sockfd_udp_central, &srclen, sizeof(int), 0, server_T_addr, server_T_addrlen);
    sendto(sockfd_udp_central, src.c_str(), src.length(), 0, server_T_addr, server_T_addrlen);
    int destlen = dest.length();
    sendto(sockfd_udp_central, &destlen, sizeof(int), 0, server_T_addr, server_T_addrlen);
    sendto(sockfd_udp_central, dest.c_str(), dest.length(), 0, server_T_addr, server_T_addrlen);
    cout << "The Central server sent a request to Backend-Server T." << endl;

    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;

    int graphlen = 0;
    recvfrom(sockfd_udp_central, &graphlen, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memset(&graph, 0, graphlen);
    cout << "graphlen=" << graphlen << endl;
    recvfrom(sockfd_udp_central, &recv_buf, graphlen, FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memcpy(&graph, recv_buf, sizeof(graph));
    cout << "graph.size=" << graph.size << endl;

    memset(recv_buf, 0, BUF_SIZE);
    recvfrom(sockfd_udp_central, &recv_buf, BUF_SIZE, FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memset(&nameList, 0, sizeof(nameList));
    memcpy(&nameList, recv_buf, sizeof(nameList));
    cout << "The Central server received information from Backend-Server T using UDP over port " << UDP_PORT_T
         << "." << endl;
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 * contact server S via vector<User> userList
 */
void contactServerS() {
    sockaddr *server_S_addr = backend_serverinfo.at(1)->ai_addr;
    socklen_t server_S_addrlen = backend_serverinfo.at(1)->ai_addrlen;

    int length = sizeof(graph.userList);
    sendto(sockfd_udp_central, &length, sizeof(int), 0, server_S_addr, server_S_addrlen);
    sendto(sockfd_udp_central, &graph.userList, sizeof(graph.userList), 0, server_S_addr, server_S_addrlen);
    cout << "The Central server sent a request to Backend-Server S." << endl;

    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int recvlen = 0;
    recvfrom(sockfd_udp_central, &recvlen, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
    recvfrom(sockfd_udp_central, &recv_buf, recvlen, FLAG, (struct sockaddr *) &their_addr, &addr_len);

    memset(&graph.userList, 0, sizeof(graph.userList));
    memcpy(&graph.userList, recv_buf, recvlen);
    cout << "The Central server received information from Backend-Server S using UDP over port " << UDP_PORT_S
         << "." << endl;
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 * contact server P via Graph amd Path structure
 */
void contactServerP() {
    sockaddr *server_P_addr = backend_serverinfo.at(2)->ai_addr;
    socklen_t server_P_addrlen = backend_serverinfo.at(2)->ai_addrlen;

    int length = sizeof(graph);
    sendto(sockfd_udp_central, &length, sizeof(int), 0, server_P_addr, server_P_addrlen);
    sendto(sockfd_udp_central, &graph, sizeof(graph), 0, server_P_addr, server_P_addrlen);
    cout << "The Central server sent a request to Backend-Server P." << endl;

    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int recvlen = 0;
    recvfrom(sockfd_udp_central, &recvlen, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
    recvfrom(sockfd_udp_central, &recv_buf, recvlen, FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memset(&pathInfo, 0, sizeof pathInfo);
    memcpy(&pathInfo, recv_buf, recvlen);
    cout << "The Central server received information from Backend-Server P using UDP over port " << UDP_PORT_P
         << "." << endl;
}

int bootUpTcpListener(const char* PORT) {
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (::bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

/**
 * Add a new file descriptor to the set
 * Reference: https://beej.us/guide/bgnet/examples/pollserver.c
 */
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = static_cast<pollfd *>(realloc(*pfds, sizeof(**pfds) * (*fd_size)));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

/**
 * Remove an index from the set
 * Reference: https://beej.us/guide/bgnet/examples/pollserver.c
 */
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

void replyClient(int sockfd, int newfd, char client) {
    int length = sizeof(pathInfo);
    send(newfd, &length, sizeof(int), 0);
    send(newfd, &pathInfo, sizeof pathInfo, 0);
    close(newfd);
    cout << "The Central server sent the results to client " << client << "." << endl;
}

int main() {
    bootUpUDP();
    sockfd_A = bootUpTcpListener(TCP_PORT_A);
    sockfd_B = bootUpTcpListener(TCP_PORT_B);

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = static_cast<pollfd *>(malloc(sizeof *pfds * fd_size));

    // Add the listeners to set
    pfds[0].fd = sockfd_A;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection
    pfds[1].fd = sockfd_B;
    pfds[1].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 2; // For the listener

    cout << "The Central server is up and running." << endl;

    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    char buf[BUF_SIZE];    // Buffer for client data

    /**
     * Reference: https://beej.us/guide/bgnet/examples/pollserver.c
     */
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {
            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!
                if (pfds[i].fd == sockfd_A) {
                    // If listener is ready to read, handle new connection
                    addrlen = sizeof remoteaddr;
                    newfd_A = accept(sockfd_A, (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd_A == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd_A, &fd_count, &fd_size);
                        unsigned short portNumber = ntohs(((struct sockaddr_in *) &remoteaddr)->sin_port);

                        int length = 0;
                        read(newfd_A, &length, sizeof(int));
                        char* message = (char*)malloc(length+1);
                        memset(message, 0, length+1);
                        read(newfd_A,message,length);
                        src = message;
                        free(message);

                        cout << "The Central server received input=\"" << src << "\" from the client using TCP over port " << portNumber
                             << "." << endl;
                    }
                }
                else if (pfds[i].fd == sockfd_B) {
                    // If listener is ready to read, handle new connection
                    addrlen = sizeof remoteaddr;
                    newfd_B = accept(sockfd_B, (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd_B == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd_B, &fd_count, &fd_size);
                        unsigned short portNumber = ntohs(((struct sockaddr_in *) &remoteaddr)->sin_port);

                        int length = 0;
                        read(newfd_B, &length, sizeof(int));
                        char* message = (char*)malloc(length+1);
                        memset(message, 0, length+1);
                        read(newfd_B,message,length);
                        dest = message;
                        free(message);;
                        cout << "The Central server received input=\"" << dest << "\" from the client using TCP over port " << portNumber
                             << "." << endl;
                    }
                }// END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors

        // process
        if (src == "" || dest == "") {
            continue;
        }
        contactServerT();
        contactServerS();
        contactServerP();
        src = "";
        dest = "";

        replyClient(sockfd_A, newfd_A, 'A');
        replyClient(sockfd_B, newfd_B, 'B');

    } // END for(;;)--and you thought it would never end!
}