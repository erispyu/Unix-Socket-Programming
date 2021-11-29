1. Full name:
2. ID:

3. What I have done:
(1) Create a UDP socket for each server, to receive and send messages.
(2) Create two TCP sockets for the central server, to serve incoming clients.
(3) Use the poll() to multiplex client connections.
(4) In server T, implement a BFS like algorithm to generate a minimum graph, covering the input usernames.
(5) In server P, implement the Dijkstra algorithm, plus with a priority queue comparator.

4. Format of messages exchanged:
For each message, I would also forward its size in bytes to avoid segment failure.
(1) Clients to central server: string
(2) central to T: two strings
(3) T to central: a Graph(defined in backend.h), an array of string(nameList)
(4) central to S: an array of string(nameList)
(5) S to central: an array of int(scoreList)
(6) central to P: a Graph(defined in backend.h), an array of string(nameList), an array of int(scoreList)
(7) P to central: two path strings(one for client A, the other for B), a double(final score)
(8) central to clients: a string(path), a double(final score)
You can set the IS_DEBUG field in backend.h to see what these servers received and forwarded.

5. Idiosyncrasy:
(1) Invalid input. If the input name from the clients is not in the DB, the server would fail.
(2) No path. If the two input user cannot reach each other, the server may fail.

6. Reused codes:
(1) Create UDP socket: https://beej.us/guide/bgnet/examples/listener.c, https://beej.us/guide/bgnet/examples/listener.c
(2) Create TCP server: https://beej.us/guide/bgnet/examples/server.c
(3) Create TCP clients: https://beej.us/guide/bgnet/examples/client.c
(4) Use of poll() in central.cpp: https://beej.us/guide/bgnet/examples/pollserver.c