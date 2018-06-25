output: main.o poolConnecter.o applog.o
	g++ -g main.o poolConnecter.o applog.o -ljansson -lcurl -lpthread -o server.out

main.o: main.cpp
	g++ -g -c main.cpp	#-c means make .o file from .cpp file

poolConnecter.o: poolConnecter.cpp poolConnecter.hpp
	g++ -g -c poolConnecter.cpp

applog.o: applog.cpp applog.hpp
	g++ -g -c applog.cpp

clean:
	rm *.o server.out