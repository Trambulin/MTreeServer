#include<iostream>
#include<unistd.h>
#include<ctime>
#include"poolConnecter.hpp"
#include"jobManager.hpp"
#include"applog.hpp"

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace std;

int main(int argc, char** argv)
{
    char b;
    try {
        sql::Driver *driver;
        sql::Connection *con;
        sql::Statement *stmt;
        sql::PreparedStatement *prep_stmt;
        sql::ResultSet *res;

        /* Create a connection */
        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", "hhhh");
        /* Connect to the MySQL test database */
        con->setSchema("testDb");

        stmt = con->createStatement();
        res = stmt->executeQuery("SELECT * FROM autoIncTest");
        while (res->next()) {
            cout << "\t... MySQL replies: ";
            /* Access column data by alias or column name */
            //cout << res->getString("_message") << endl;
            //cout << "\t... MySQL says it again: ";
            /* Access column data by numeric offset, 1 is the first column */
            //cout << res->getString(1) << endl;
            cout << "id: " << res->getInt("id") << ", data: " << res->getString("data") << endl;
        }
        /*prep_stmt = con->prepareStatement("INSERT INTO autoIncTest(data) VALUES (?)");
        prep_stmt->setString(1, "preparedTest");
        prep_stmt->executeUpdate();
        prep_stmt->setString(1, "preparedTestSecond");
        prep_stmt->executeUpdate();*/
        //int ff = stmt->executeUpdate("INSERT INTO autoIncTest(data) VALUES (\"fromCode\")");
        //cout << "replied number: "<<ff<<endl;
        delete res;
        //delete prep_stmt;
        delete stmt;
        delete con;

    } catch (sql::SQLException &e) {
        cout << "# ERR: SQLException in " << __FILE__;
        cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
        cout << "# ERR: " << e.what();
        cout << " (MySQL error code: " << e.getErrorCode();
        cout << ", SQLState: " << e.getSQLState() << " )" << endl;
    }

    string ff="letsThis";
    hash<string> hFF;
    unsigned long hashedFF=hFF(ff);
    cout << endl << hashedFF << endl;

    time_t now = time(0);
    tm *ltm = localtime(&now);
    cout << 1900 + ltm->tm_year <<"-"<<  1 + ltm->tm_mon <<"-"<< ltm->tm_mday << " ";
    cout << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec <<endl;
    //cin >> b;
    return 0;


    applog::init();
    applog::logFile.open("test.txt"); //emptying file
    applog::logFile.close();
    char url[29]="http://dash.suprnova.cc:9995";
    char user[15]="trambulin.test";
    char pass[2]="x";
    int n;
    pthread_t pthClient;
    pthread_attr_t attrClient;
    /*char url[29]="http://ltc.suprnova.cc:4444";
    char user[15]="trambulin.tram";
    char pass[2]="x";*/
    jobManager::startNewPoolConnection(url,user,pass,0);
    int port=27015;
    
    pthread_attr_init(&attrClient);
    n = pthread_create(&pthClient, &attrClient, jobManager::startClientListen, &port);
    pthread_attr_destroy(&attrClient);
    if(n) {
        applog::log(LOG_ERR,"jobManager thread create failed");
    }
    char f[256];
    std::cout << "teszt";
    while(1){
        std::cin >> f;
        std::cout << f << "\n";
        //jobManager::clients[0]->connOver=true; //close connection
    }
    return 0;
}