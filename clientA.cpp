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
#include <cstring>
#include <cstdlib>

#define localhost "127.0.0.1"
#define PORT "25900"

#define MAXDATASIZE 1024 // max number of bytes we can get at once

using namespace std;

int main(int argc, char *argv[]) {
    const string username = argv[1];
    int sockfd;
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
    for (p = servinfo; p != NULL; p = p->ai_next) {
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

    int name_len = username.length();
    send(sockfd, &name_len, sizeof(int), 0);
    send(sockfd, username.c_str(), username.length(), 0);
    cout << "The client sent " << username << " to the Central server." << endl;

    // receive src
    string src;
    int srclen = 0;
    read(sockfd, &srclen, sizeof(int));
    char *src_msg = (char *) malloc(srclen + 1);
    memset(src_msg, 0, srclen + 1);
    read(sockfd, src_msg, srclen);
    src = src_msg;
    free(src_msg);

    // receive dest
    string dest;
    int destlen = 0;
    read(sockfd, &destlen, sizeof(int));
    char *dest_msg = (char *) malloc(destlen + 1);
    memset(dest_msg, 0, destlen + 1);
    read(sockfd, dest_msg, destlen);
    dest = dest_msg;
    free(dest_msg);

    // receive path
    string path;
    int path_len = 0;
    read(sockfd, &path_len, sizeof(int));
    char *path_msg = (char *) malloc(path_len + 1);
    memset(path_msg, 0, path_len + 1);
    read(sockfd, path_msg, path_len);
    path = path_msg;
    free(path_msg);

    // receive score
    double compatibilityScore;
    read(sockfd, &compatibilityScore, sizeof(double));

    close(sockfd);

    cout << "Found compatibility for " << src << " and " << dest << ":" << endl;
    cout << path << endl;
    printf("Compatibility score: %.2f\n", compatibilityScore);
    return 0;
}

