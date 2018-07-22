output: main.o poolConnecter.o applog.o clientConnecter.o jobManager.o
	g++ -g main.o poolConnecter.o applog.o clientConnecter.o jobManager.o -ljansson -lcurl -lpthread -o server.out

main.o: main.cpp
	g++ -g -c main.cpp	#-c means make .o file from .cpp file

jobManager.o: jobManager.cpp jobManager.hpp
	g++ -g -c jobManager.cpp

poolConnecter.o: poolConnecter.cpp poolConnecter.hpp
	g++ -g -c poolConnecter.cpp

applog.o: applog.cpp applog.hpp
	g++ -g -c applog.cpp

clientConnecter.o: clientConnecter.cpp clientConnecter.hpp
	g++ -g -c -std=c++11 clientConnecter.cpp

clean:
	rm *.o server.out