output: main.o poolConnecter.o applog.o clientConnecter.o jobManager.o sha256Abstract.o
	g++ -g -no-pie main.o poolConnecter.o applog.o clientConnecter.o jobManager.o sha256Abstract.o -ljansson -lcurl -lpthread -lmysqlcppconn -o server.out

main.o: main.cpp
	g++ -I /usr/include/mysql-cppconn-8/jdbc -g -c main.cpp	#-c means make .o file from .cpp file

sha256Abstract.o: sha256Abstract.cpp sha256Abstract.hpp
	g++ -g -c -std=c++11 sha256Abstract.cpp

jobManager.o: jobManager.cpp jobManager.hpp
	g++ -I /usr/include/mysql-cppconn-8/jdbc -g -c jobManager.cpp

poolConnecter.o: poolConnecter.cpp poolConnecter.hpp
	g++ -g -c poolConnecter.cpp

applog.o: applog.cpp applog.hpp
	g++ -g -c applog.cpp

clientConnecter.o: clientConnecter.cpp clientConnecter.hpp
	g++ -I /usr/include/mysql-cppconn-8/jdbc -g -c -std=c++11 clientConnecter.cpp

clean:
	rm *.o server.out