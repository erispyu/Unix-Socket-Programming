#include <cstdlib>
#include "backend.h"
#include <queue>
#include <string>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <stdio.h>

using namespace std;

Graph graph;

int sockfd;
int sockfd_central;
char recv_buf[BUF_SIZE];
struct addrinfo* central_serverinfo;

string nameList[MAX_USER_NUM];
int scoreList[MAX_USER_NUM];
string path;
double compatibilityScore;

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
    cout << "The ServerP is up and running using UDP on port " << UDP_PORT_P << "." << endl;
}

void receive() {
    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;

    // receive graph
    int graphlen = 0;
    recvfrom(sockfd, &graphlen, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memset(&graph, 0, graphlen);
    recvfrom(sockfd, &recv_buf, graphlen, FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memcpy(&graph, recv_buf, sizeof(graph));

    if (IS_DEBUG) {
        cout << "graph.size = " << graph.size << endl;
        cout << "destId = " << graph.destId << endl;
        cout << "Adj Matrix:" << endl;
        for (int i = 0; i < graph.size; i++) {
            for (int j = 0; j < graph.size; j++) {
                cout << graph.distance[i][j] << "\t";
            }
            cout << endl;
        }
        cout << "UserList:" << endl;
        for (int i = 0; i < graph.size; i++) {
            User u = graph.userList[i];
            cout << "id=" << u.id << ", pre=" << u.preId << ", distance=" << u.distance << endl;
        }
    }

    // receive scoreList
    int recvlen = 0;
    recvfrom(sockfd, &recvlen, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memset(&scoreList, 0, recvlen);
    recvfrom(sockfd, &recv_buf, recvlen, FLAG, (struct sockaddr *) &their_addr, &addr_len);
    memcpy(&scoreList, recv_buf, recvlen);

    // receive nameList
    int graphSize = graph.size;
    for (int i = 0; i < graphSize; i++) {
        // receive
        string username;
        int length = 0;
        recvfrom(sockfd, &length, sizeof(int), FLAG, (struct sockaddr *) &their_addr, &addr_len);
        char *message = (char *) malloc(length + 1);
        memset(message, 0, length + 1);
        recvfrom(sockfd, message, length, FLAG, (struct sockaddr *) &their_addr, &addr_len);
        username = message;
        free(message);
        // fill in
        nameList[i] = username;
    }

    if (IS_DEBUG) {
        for (int i = 0; i < graphSize; i++) {
            cout << i << "\t" << nameList[i] << "\t" << scoreList[i] << endl;
        }
    }

    cout << "The ServerP received the topology and score information." << endl;
}

double getMatchingGap(int score1, int score2) {
    return abs(score1 - score2) / double(score1 + score2);
}

void setDistance() {
    int size = graph.size;
    for (int i = 0; i < size; i++) {
        for (int j = i + 1; j > i && j < size && (graph.distance[i][j] > 0); j++) {
            double distance = getMatchingGap(scoreList[i], scoreList[j]);
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
    string src= nameList[0];

    if (graph.destId >= graph.size){
        return;
    }
    User u = graph.userList[graph.destId];
    string username = nameList[u.id];

    while (username != src) {
        path = " --- " + username + path;
        u = graph.userList[u.preId];
        username = nameList[u.id];
    }
    path = src + path;
    compatibilityScore = graph.userList[graph.destId].distance;
}

void sendBack() {
    // send path
    int pathlen = path.length();
    sendto(sockfd_central, &pathlen, sizeof(int), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
    sendto(sockfd_central, path.c_str(), path.length(), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);

//    // send score
//    sendto(sockfd_central, &compatibilityScore, sizeof(double), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);

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
