output: main.o poolConnecter.o
	g++ -g main.o poolConnecter.o -ljansson -lcurl -o server.out

main.o: main.cpp
	g++ -g -c main.cpp	#-c means make .o file from .cpp file

poolConnecter.o: poolConnecter.cpp poolConnecter.hpp
	g++ -g -c poolConnecter.cpp

clean:
	rm *.o server.out