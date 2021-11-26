#include <string>
#include "backend.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <netdb.h>
#include <unistd.h>

using namespace std;

string src;
string dest;

map<string, int> originalIndexMap;
vector<string> allNameList;
int allEdges[MAX_USER_NUM][MAX_USER_NUM];

Graph graph;

int sockfd;
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

    if ((rv = getaddrinfo(NULL, UDP_PORT_T, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                         p->ai_protocol)) == -1) {
            perror("Server T UDP listener: socket");
            continue;
        }

        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server T UDP listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Server T UDP listener: failed to bind socket\n");
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
    cout << "The ServerT is up and running using UDP on port " << UDP_PORT_T << "." << endl;
}

void receive() {
    string queriedUsernames[2];
    memset(recv_buf, 0, BUF_SIZE);
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    int recvfromResult = recvfrom(sockfd, &recv_buf, BUF_SIZE, FLAG, (struct sockaddr *)&their_addr, &addr_len);
    if (recvfromResult == -1) {
        perror("recvfrom error");
        exit(1);
    }

    memset(&queriedUsernames, 0, sizeof(queriedUsernames));
    memcpy(&queriedUsernames, recv_buf, sizeof(queriedUsernames));

    src = queriedUsernames[0];
    dest = queriedUsernames[1];
    graph.src = src;
    graph.dest = dest;

    cout << "The ServerT received a request from Central to get the topology." << endl;
}

int getOriginalIndex(const string& username) {
    int originalIndex;
    map<string, int>::iterator it = originalIndexMap.find(username);
    if (it != originalIndexMap.end()) {
        originalIndex = it->second;
    } else {
        originalIndex = originalIndexMap.size();
        originalIndexMap.insert(pair<string, int>(username, originalIndex));
        allNameList.push_back(username);
    }
    return originalIndex;
}

void parseEdgeList() {
    ifstream fileIn;
    fileIn.open(string(EDGE_LIST_FILENAME));
    string name1;
    string name2;
    while (!fileIn.eof()) {
        fileIn >> name1;
        fileIn >> name2;
        int i = getOriginalIndex(name1);
        int j = getOriginalIndex(name2);
        allEdges[i][j] = 1;
        allEdges[j][i] = 1;
    }
    fileIn.close();
}

map<string, User> userMap;
set<string> accessibleNameSet;

User* getUser(const string& username) {
    User *u = nullptr;
    map<string, User>::iterator it = userMap.find(username);
    if (it != userMap.end()) {
        u = &it->second;
    } else {
        int id = userMap.size();
        User newUser;
        newUser.username = username;
        newUser.id = id;
        newUser.score = -1;
        newUser.distance = DBL_MAX;
        userMap.insert(pair<string, User>(username, newUser));
        accessibleNameSet.insert(username);
        u = &userMap.find(username)->second;
    }
    return u;
}

bool isAccessible(string username) {
    return accessibleNameSet.find(username) != accessibleNameSet.end();
}

void generateGraph() {
    vector<string> q;
    User* srcUser = getUser(src);
    srcUser->distance = 0;

    // find all usernames that accessible by src
    q.push_back(src);
    while(!q.empty()) {
        string temp = q.back();
        q.pop_back();
        int i = getOriginalIndex(temp);
        for (int j = 0; j < allNameList.size(); j++) {
            string usernameJ = allNameList.at(j);
            if (allEdges[i][j] == 1 && !isAccessible(usernameJ)) {
                User* u = getUser(usernameJ);
                q.push_back(usernameJ);
            }
        }
    }

    int graphSize = accessibleNameSet.size();
    graph.size = graphSize;

    // fill in graph.distance
    for (int i = 0; i < allNameList.size(); i++) {
        for (int j = i + 1; j < allNameList.size(); j++) {
            if (allEdges[i][j] == 1) {
                string nameI = allNameList.at(i);
                string nameJ = allNameList.at(j);
                if (isAccessible(nameI) && isAccessible(nameJ)) {
                    int newI = getUser(nameI)->id;
                    int newJ = getUser(nameJ)->id;
                    graph.distance[newI][newJ] = 1;
                    graph.distance[newJ][newI] = 1;
                }
            }
        }
    }

    // fill in graph.userList
    for (map<string, User>::iterator it = userMap.begin(); it != userMap.end(); it++) {
        User u = it->second;
        graph.userList[u.id] = u;
    }

    // fill in graph.destId;
    graph.destId = getUser(dest)->id;
}

void sendBack() {
    if (sendto(sockfd, &graph, sizeof(graph), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    cout << "The ServerT finished sending the topology to Central." << endl;
}

int main() {
    bootUp();
    while (true) {
        receive();
        parseEdgeList();
        generateGraph();
        sendBack();
    }
    return 0;
}