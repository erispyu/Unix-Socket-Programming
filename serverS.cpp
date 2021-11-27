#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <iostream>
#include <netdb.h>
#include "backend.h"
#include <unistd.h>
#include <cstring>
#include <cstdlib>

using namespace std;

int graphSize;
string nameList[MAX_USER_NUM];
int scoreList[MAX_USER_NUM];
map<string, int> scoreMap;

int sockfd;
int sockfd_central;
char recv_buf[BUF_SIZE];
struct addrinfo* central_serverinfo;

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

    if ((rv = getaddrinfo(NULL, UDP_PORT_S, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("Server S UDP listener: socket");
            continue;
        }

        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server S UDP listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Server S UDP listener: failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 */
void bootUpServerUDPTalker() {
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(localhost, UDP_PORT_C, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd_central = socket(p->ai_family, p->ai_socktype,
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

    central_serverinfo = servinfo;
}

void bootUp() {
    bootUpCentralUDPListener();
    bootUpServerUDPTalker();
    cout << "The ServerS is up and running using UDP on port " << UDP_PORT_S << "." << endl;
}

void receive() {
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;

    graphSize = 0;
    recvfrom(sockfd, &graphSize, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);

    for (int i = 0; i < graphSize; i++) {
        // receive
        string username;
        int length = 0;
        recvfrom(sockfd, &length, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
        char* message = (char*)malloc(length+1);
        memset(message, 0, length+1);
        recvfrom(sockfd, message, length, FLAG, (struct sockaddr *) &their_addr, &addr_len);
        username = message;
        free(message);
        // fill in
        nameList[i] = username;
    }

    cout << "The ServerS received a request from Central to get the scores." << endl;
}

void parseFile() {
    ifstream fileIn;
    fileIn.open(SCORES_FILENAME);

    string username;
    string score;
    while (!fileIn.eof()) {
        fileIn >> username;
        fileIn >> score;
        int socreInt;
        sscanf(score.c_str(), "%d", &socreInt);
        scoreMap.insert(pair<string, int>(username, socreInt));
    }
    fileIn.close();
}

void setScores() {
    for (int i = 0; i < MAX_USER_NUM; i++) {
        string name = nameList[i];
        if (name == "") {
            return;
        }
        scoreList[i] = scoreMap.find(name)->second;
    }
}

void sendBack() {
    int length = sizeof scoreList;
    sendto(sockfd_central, &length, sizeof(int), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
    sendto(sockfd_central, &scoreList, length, 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
    cout << "The ServerS finished sending the scores to Central." << endl;
}

int main() {
    bootUp();
    while (true) {
        receive();
        parseFile();
        setScores();
        sendBack();
    }
    return 0;
}