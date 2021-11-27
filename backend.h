#ifndef UNIX_SOCKET_PROGRAMMING_CENTRAL_H
#define UNIX_SOCKET_PROGRAMMING_CENTRAL_H

#include <vector>
#include <map>
#include <string>
#include <float.h>

#define EDGE_LIST_FILENAME "./edgelist.txt"
#define SCORES_FILENAME "./scores.txt"

#define localhost "127.0.0.1"
#define UDP_PORT_T "21900"
#define UDP_PORT_P "22900"
#define UDP_PORT_S "23900"
#define UDP_PORT_C "24900"
#define TCP_PORT_A "25900"
#define TCP_PORT_B "26900"

#define BUF_SIZE 4096
#define FLAG 0

#define MAX_USER_NUM 20

struct User {
    int id;
    double distance;
    int preId;
};

struct Graph {
    int destId;
    int size;
    User userList[MAX_USER_NUM];
    double distance[MAX_USER_NUM][MAX_USER_NUM];
};

#endif //UNIX_SOCKET_PROGRAMMING_CENTRAL_H
