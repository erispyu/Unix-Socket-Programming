# the compiler: gcc for C program, define as g++ for C++
CC = g++

all: clean serverC.out serverS.out serverT.out serverP.out clientA.out clientB.out

serverC: serverC.out
	@ ./serverC

serverT: serverT.out
	@ ./serverT

serverP: serverP.out
	@ ./serverP

serverS: serverS.out
	@ ./serverS

serverC.out: central.cpp backend.h
	@ $(CC) -o serverC central.cpp

serverS.out: serverS.cpp backend.h
	@ $(CC) -o serverS serverS.cpp

serverT.out: serverT.cpp backend.h
	@ $(CC) -o serverT serverT.cpp

serverP.out: serverP.cpp backend.h
	@ $(CC) -o serverP serverP.cpp

clientA.out: clientA.cpp
	@ $(CC) -o clientA clientA.cpp

clientB.out: clientB.cpp
	@ $(CC) -o clientB clientB.cpp

clean:
	@ $(RM) serverC serverT serverS serverP clientA clientB

.PHONY: all run serverC serverP serverS serverT