#include <string>
#include "backend.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

using namespace std;

string src;
string dest;
Graph graph;
map<string, User> userMap;
set<string> accessibleNameSet;
string nameList[MAX_USER_NUM];

map<string, int> originalIndexMap;
vector<string> allNameList;
int allEdges[MAX_USER_NUM][MAX_USER_NUM];

int sockfd;
int sockfd_central;
char recv_buf[BUF_SIZE];
struct addrinfo *central_serverinfo;

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
    for (p = servinfo; p != NULL; p = p->ai_next) {
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
    for (p = servinfo; p != NULL; p = p->ai_next) {
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
    cout << "The ServerT is up and running using UDP on port " << UDP_PORT_T << "." << endl;
}

void receive() {
    int length = 0;
    recv(sockfd, &length, sizeof(int), 0);
    char *src_msg = (char *) malloc(length + 1);
    memset(src_msg, 0, length + 1);
    recv(sockfd, src_msg, length, 0);
    src = src_msg;
    free(src_msg);

    length = 0;
    recv(sockfd, &length, sizeof(int), 0);
    char *dest_msg = (char *) malloc(length + 1);
    memset(dest_msg, 0, length + 1);
    recv(sockfd, dest_msg, length, 0);
    dest = dest_msg;
    free(dest_msg);

    if (IS_DEBUG) {
        cout << "src = " << src << endl;
        cout << "dest = " << dest << endl;
    }

    cout << "The ServerT received a request from Central to get the topology." << endl;
}

int getOriginalIndex(const string &username) {
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
    fileIn.open(EDGE_LIST_FILENAME);
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

User *getUser(const string &username) {
    User *u = NULL;
    map<string, User>::iterator it = userMap.find(username);
    if (it != userMap.end()) {
        u = &it->second;
    } else {
        int id = userMap.size();
        User newUser;
        nameList[id] = username;
        newUser.id = id;
        newUser.preId = -1;
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
    User *srcUser = getUser(src);
    srcUser->distance = 0;

    // find all usernames that accessible by src
    q.push_back(src);
    while (!q.empty()) {
        string temp = q.back();
        q.pop_back();
        int i = getOriginalIndex(temp);
        for (int j = 0; j < allNameList.size(); j++) {
            string usernameJ = allNameList.at(j);
            if (allEdges[i][j] == 1 && !isAccessible(usernameJ)) {
                User *u = getUser(usernameJ);
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

    // fill in graph.destId
    graph.destId = getUser(dest)->id;
}

void sendBack() {
    int graphSize = graph.size;
    int graphlen = sizeof graph;
    sendto(sockfd_central, &graphlen, sizeof(int), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
    sendto(sockfd_central, &graph, sizeof(graph), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);

    for (int i = 0; i < graphSize; i++) {
        string username = nameList[i];
        int length = username.length();
        sendto(sockfd_central, &length, sizeof(int), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
        sendto(sockfd_central, username.c_str(), username.length(), 0, central_serverinfo->ai_addr, central_serverinfo->ai_addrlen);
    }

    cout << "The ServerT finished sending the topology to Central." << endl;
}

int main() {
    bootUp();
    parseEdgeList();
    while (true) {
        receive();
        generateGraph();
        sendBack();

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

        graph = Graph();
        memset(&nameList, 0 ,sizeof nameList);
        src = "";
        dest = "";
        userMap = map<string, User>();
        accessibleNameSet = set<string>();
    }
    return 0;
}