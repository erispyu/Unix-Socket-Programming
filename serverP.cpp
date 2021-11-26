#include <cstdlib>
#include "backend.h"
#include <queue>
#include <string>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

using namespace std;

Graph graph;

int sockfd;
char recv_buf[BUF_SIZE];
struct addrinfo* central_serverinfo;

PathInfo pathInfo;

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

    if ((rv = getaddrinfo(NULL, UDP_PORT_P, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("Server P UDP listener: socket");
            continue;
        }

        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server P UDP listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Server P UDP listener: failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
}

/**
 * Reference: https://beej.us/guide/bgnet/examples/talker.c
 */
void bootUpServerUDPTalker() {
    int sockfd;
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

    central_serverinfo = servinfo;
}

void bootUp() {
    bootUpCentralUDPListener();
    bootUpServerUDPTalker();
    cout << "The ServerP is up and running using UDP on port " << UDP_PORT_P << "." << endl;
}

void receive() {
    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int recvfromResult = recvfrom(sockfd, &recv_buf, BUF_SIZE, FLAG, (struct sockaddr *)&their_addr, &addr_len);
    if (recvfromResult == -1) {
        perror("recvfrom error");
        exit(1);
    }

    memset(&graph, 0, sizeof(graph));
    memcpy(&graph, recv_buf, sizeof(graph));
    cout << "The ServerP received the topology and score information." << endl;
}

double getMatchingGap(int score1, int score2) {
    return abs(score1 - score2) / double(score1 + score2);
}

void setDistance() {
    int size = graph.size;
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j > i && j < size && (graph.distance[i][j] > 0); j++) {
            double distance = getMatchingGap(graph.userList[i].score, graph.userList[j].score);
            graph.distance[i][j] = distance;
            graph.distance[j][i] = distance;
        }
    }
}

struct UserComparator {
    bool operator()(const int id_1, const int id_2) {
        return graph.userList[id_1].distance > graph.userList[id_2].distance;
    }
};

void relax(User *u, User *v) {
    double interDistance = graph.distance[u->id][v->id];
    if (v->distance > u->distance + interDistance) {
        v->preId = u->id;
        v->distance = u->distance + interDistance;
    }
}

void dijkstra() {
    priority_queue<int, vector<int>, UserComparator> minHeap;
    for (int i = 0; i < graph.size; i++) {
        User u = graph.userList[i];
        minHeap.push(u.id);
    }
    while (!minHeap.empty()) {
        int top = minHeap.top();
        minHeap.pop();
        User *u = &graph.userList[top];
        for (int i = 0; i < graph.size; i++) {
            if (graph.distance[top][i] > 0) {
                User *v = &graph.userList[i];
                relax(u, v);
            }
        }
    }
}

void generateShortestPath() {
    if (graph.destId >= graph.size){
        return;
    }
    User u = graph.userList[graph.destId];
    string pathSrc;
    while (u.username != graph.src) {
        pathSrc = " --- " + u.username + pathSrc;
        u = graph.userList[u.preId];
    }
    pathSrc = graph.src + pathSrc;

    pathInfo.src = graph.src;
    pathInfo.dest = graph.dest;
    pathInfo.pathStr = pathSrc;
    pathInfo.distance = graph.userList[graph.destId].distance;
}

void sendBack() {
    if (sendto(sockfd, &pathInfo, sizeof(pathInfo), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    cout << "The ServerP finished sending the results to the Central." << endl;
}

int main() {
    bootUp();
    while (true) {
        receive();
        setDistance();
        dijkstra();
        generateShortestPath();
        sendBack();
    }
    return 0;
}
