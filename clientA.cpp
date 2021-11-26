#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define localhost "127.0.0.1"
#define PORT "25900"

#define MAXDATASIZE 1024 // max number of bytes we can get at once

using namespace std;

struct PathInfo {
    std::string src;
    std::string dest;
    std::string pathStr;
    double distance;
};

int main(int argc, char *argv[])
{
    const string username = argv[1];
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(localhost, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    cout << "The client is up and running." << endl;

    if (send(sockfd, &username, sizeof username, 0) == -1)
        perror("send");
    cout << "The client sent " << username << " to the Central server." << endl;

    read(sockfd, buf, MAXDATASIZE);
    PathInfo pathInfo;
    memcpy(&pathInfo, buf, sizeof pathInfo);
    close(sockfd);

    cout << "Found compatibility for " << pathInfo.src << " and " << pathInfo.dest << ":" << endl;
    cout << pathInfo.pathStr << endl;
    printf("Compatibility score: %.2f\n", pathInfo.distance);
    return 0;
}

