//--------------------------------------------------------------------------------
// dbobject.h

const char *dbobject_h = R"DBOBJH(//
// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework

#ifndef DB_DBOBJECT_H
#define DB_DBOBJECT_H

#include <iostream>
#include <vector>
#include <string>
#include <mysql/mysql.h>
#include <sqlite3.h>

#define DBOBJECT_CONNECTION_NONE 0
#define DBOBJECT_CONNECTION_MYSQL 1
#define DBOBJECT_CONNECTION_SQLITE 2

using namespace std;

namespace db {

class dbobject {
public:
  dbobject();
  virtual ~dbobject();

  static bool connect_mysql(string host, string db, string user, string passwd);
  static bool connect_mysql(char* host, char* db, char* user, char* passwd);
  static bool connect_sqlite3(string filename);
  static void disconnect();

  static void setVerbose (bool which);
  static void setMySQLConnection(MYSQL *conn);
  static void saveAll(vector<dbobject*> vec, MYSQL *conn = NULL);
  static void executeUpdate(string query);
  static unsigned int getLastInsertID();
  static MYSQL *getMySQLConnection();
  static void raise_error(string s);

  virtual void save() = 0;
  virtual string getTableName() = 0;
  virtual unsigned int size_in_bytes() = 0;

  static void enable_reconnects (string host, string name, string user, string pass, int num);
  static void enable_reconnects (int num);
  static void set_reconnect_callback ( void (*f)(int) );
  static int num_reconnects_remaining();

  static unsigned long get_query_count();

protected:
  static bool _verbose;
  static int connection_status;
  static sqlite3 *db;

  virtual dbobject* readInFullRow(MYSQL_ROW row) = 0;
  virtual ostream& put(ostream &out) = 0;
  virtual bool isUpdate() = 0;

  static bool reconnect();
  static void possibly_call_reconnect_callback();
  static void increment_query_count();

  static void executeUpdate_mysql(string query, MYSQL *conn = NULL);
  static void executeUpdate_sqlite3(string query);
  static unsigned int getLastInsertID_mysql(MYSQL *conn = NULL);
  static unsigned int getLastInsertID_sqlite3();

private:
  static vector<MYSQL*> conns;

  dbobject(const dbobject& orig);

  static bool connect_mysql_private(const char* host, const char* db, const char* user, const char* passwd);
  static bool connect_mysql_private();
  static bool connect_sqlite3_private(string filename);
  static int num_reconnects;
  static string dbhost, dbpass, dbuser, dbname;
  static void (*func)(int);
  static unsigned long query_count;
  static void verify_conn_is_open();
  static void set_db_credentials (string host, string name, string user, string pass);
  static void set_db_credentials (char *host, char *name, char *user, char *pass);
 protected:
  static int last_insert_rowid_callback(void *last_id, int argc, char **argv, char **azColName);

  friend ostream& operator<< (ostream& out, dbobject &x);

};

ostream& operator<< (ostream& out, dbobject &x);

} // namespace db

#endif
)DBOBJH";

//--------------------------------------------------------------------------------
// dbobject.cpp

const char *dbobject_c = R"DBOBJC(//
// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework

#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <cstdlib>

#include "myassert.h"
#include "dbobject.h"

using namespace std;

namespace db {

string dbobject::dbpass = string();
string dbobject::dbname = string();
string dbobject::dbuser = string();
string dbobject::dbhost = string();
void (*dbobject::func)(int) = NULL;
int dbobject::num_reconnects = 0;
bool dbobject::_verbose = false;
unsigned long dbobject::query_count = 0;
vector<MYSQL*> dbobject::conns = vector<MYSQL*>();
int dbobject::connection_status = DBOBJECT_CONNECTION_NONE;
sqlite3* dbobject::db = NULL;

dbobject::dbobject() {
}

dbobject::dbobject(const dbobject& orig) {
}

dbobject::~dbobject() {
}

void dbobject::set_db_credentials (string host, string name, string user, string pass) {
  dbpass = pass;
  dbhost = host;
  dbname = name;
  dbuser = user;
}

void dbobject::set_db_credentials (char *host, char *name, char *user, char *pass) {
  set_db_credentials(string(host),string(name),string(user),string(pass));
}

void dbobject::enable_reconnects (string host, string name, string user, string pass, int num) {
  set_db_credentials(host,name,user,pass);
  num_reconnects = num;
}

void dbobject::enable_reconnects (int num) {
  num_reconnects = num;
}

bool dbobject::reconnect() {
  if ( num_reconnects == 0 ) {
#pragma omp critical(output)
    cerr << "No more reconnect attempts allowed in thread " << omp_get_thread_num() << endl;
    return false;
  }
#pragma omp critical(output)
  cerr << "Attempting to reconnect to the DB in thread " << omp_get_thread_num() << " (" << num_reconnects-- << " more attempts allowed)..." << endl;
  setMySQLConnection(NULL);
  if ( connect_mysql_private() ) {
#pragma omp critical(output)
    cerr << "Reconnection attempt successful in thread " << omp_get_thread_num() << endl;
    return true;
  } else {
#pragma omp critical(output)
    cerr << "Reconnection attempt failed in thread " << omp_get_thread_num() << endl;
    return false;
  }
}

int dbobject::num_reconnects_remaining() {
  return num_reconnects;
}

void dbobject::set_reconnect_callback ( void (*f)(int) ) {
  func = f;
}

void dbobject::possibly_call_reconnect_callback() {
  if ( func != NULL )
    (*func)(num_reconnects);
}

void dbobject::verify_conn_is_open() {
  if ( connection_status == DBOBJECT_CONNECTION_MYSQL ) {
    unsigned int thread_num = omp_get_thread_num();
    if ( thread_num >= conns.size() ) {
#pragma omp critical(dbcppconns)
      while ( thread_num >= conns.size() )
	conns.push_back(NULL);
    }
    if ( conns[thread_num] == NULL )
      connect_mysql_private();
    myassert(conns[thread_num]!=NULL);
  } else if ( connection_status == DBOBJECT_CONNECTION_SQLITE ) {
    myassert(db != NULL);
  } else {
    myassert(0);
  }
}

unsigned long dbobject::get_query_count() {
  return query_count;
}

void dbobject::increment_query_count() {
  // all calls to this already are in a #pragma critical
  query_count++;
}

bool dbobject::connect_mysql_private() {
  //myassert(dbpass!=""); // pasword can be empty
  myassert(dbhost!="");
  myassert(dbuser!="");
  myassert(dbname!="");
  return connect_mysql_private(dbhost.c_str(),dbname.c_str(),dbuser.c_str(),dbpass.c_str());
}

bool dbobject::connect_mysql_private(const char* host, const char* db, const char* user, const char* passwd) {
  myassert((connection_status == DBOBJECT_CONNECTION_NONE) || (connection_status == DBOBJECT_CONNECTION_MYSQL));
  unsigned int thread_num = omp_get_thread_num();
  if ( (connection_status == DBOBJECT_CONNECTION_MYSQL) &&
       (conns.size() > thread_num) &&
       (conns[thread_num] != NULL) )
    return true;
  MYSQL *conn = mysql_init(NULL);
  if (!mysql_real_connect(conn, dbhost.c_str(), dbuser.c_str(), dbpass.c_str(), dbname.c_str(), 0, NULL, 0)) {
    cout << "\tconnection error: " << mysql_error(conn) << endl;
    return false;
  }
  setMySQLConnection(conn); // sets connection_status also
  return true;
}

bool dbobject::connect_mysql(char* host, char* db, char* user, char* passwd) {
  set_db_credentials(host,db,user,passwd);
  bool ret;
#pragma omp critical(dbcpp)
  ret = connect_mysql_private(host, db, user, passwd);
  return ret;
}

bool dbobject::connect_mysql(string host, string db, string user, string passwd) {
  set_db_credentials(host,db,user,passwd);
  bool ret;
#pragma omp critical(dbcpp)
  ret = connect_mysql_private(host.c_str(),db.c_str(),user.c_str(),passwd.c_str());
  return ret;
}

bool dbobject::connect_sqlite3(string filename) {
  bool ret;
#pragma omp critical(dbcpp)
  ret = connect_sqlite3_private(filename.c_str());
  return ret;
}

bool dbobject::connect_sqlite3_private(string filename) {
  if ( connection_status == DBOBJECT_CONNECTION_SQLITE )
    return true;
  myassert( connection_status == DBOBJECT_CONNECTION_NONE );
  myassert(db == NULL);
  int rc = sqlite3_open(filename.c_str(), &db);
  if ( rc ) {
    cerr << "Can't open sqlite3 database '" << filename << "': " << sqlite3_errmsg(db) << endl;
    return false;
  }
  connection_status = DBOBJECT_CONNECTION_SQLITE;
  return true;
}

void dbobject::disconnect() {
#pragma omp critical(dbcppconns)
  {
    if ( connection_status == DBOBJECT_CONNECTION_SQLITE ) {
      myassert(db != NULL);
      sqlite3_close(db);
      db = NULL;
    } else if ( connection_status == DBOBJECT_CONNECTION_MYSQL ) {
      for ( unsigned int i = 0; i < conns.size(); i++ ) {
	if ( conns[i] != NULL )
	  mysql_close(conns[i]);
	conns[i] = NULL;
      }
    } else if ( connection_status == DBOBJECT_CONNECTION_NONE ) {
      // do nothing
    } else {
      myassert(0);
    }
    conns.clear();
    connection_status = DBOBJECT_CONNECTION_NONE;
  }
}

void dbobject::setVerbose(bool which) {
  _verbose = which;
}

void dbobject::setMySQLConnection(MYSQL *conn) {
  myassert((connection_status == DBOBJECT_CONNECTION_NONE) || (connection_status == DBOBJECT_CONNECTION_MYSQL));
  unsigned int thread_num = omp_get_thread_num();
  if ( thread_num >= conns.size() ) {
#pragma omp critical(dbcppconns)
    while ( thread_num >= conns.size() )
      conns.push_back(NULL);
  }
  conns[thread_num] = conn;
  connection_status = DBOBJECT_CONNECTION_MYSQL;
}

MYSQL* dbobject::getMySQLConnection() {
  if ( connection_status != DBOBJECT_CONNECTION_MYSQL )
    return NULL;
  myassert(connection_status == DBOBJECT_CONNECTION_MYSQL);
  unsigned int thread_num = omp_get_thread_num();
  if ( thread_num >= conns.size() ) {
#pragma omp critical(dbcppconns)
    while ( thread_num >= conns.size() )
      conns.push_back(NULL);
  }
  verify_conn_is_open();
  return conns[thread_num];
}

unsigned int dbobject::getLastInsertID() {
  if ( connection_status == DBOBJECT_CONNECTION_MYSQL )
    return getLastInsertID_mysql();
  else if ( connection_status == DBOBJECT_CONNECTION_SQLITE )
    return getLastInsertID_sqlite3();
  else {
    myassert(0);
    return 0;
  }
}

int dbobject::last_insert_rowid_callback(void *last_id, int argc, char **argv, char **azColName){
  myassert(argc == 1);
  myassert(last_id != NULL);
  int *id = (int*) last_id;
  *id = atoi(argv[0]);
  return 0;
}

unsigned int dbobject::getLastInsertID_sqlite3() {
  myassert(connection_status == DBOBJECT_CONNECTION_SQLITE);
  char* errorMsg;
  unsigned int last_id = 0;
  int rc = 0;
#pragma omp critical(dbcpp)
  rc = sqlite3_exec(db, "select last_insert_rowid()", last_insert_rowid_callback, &last_id, &errorMsg);
  if ( rc != SQLITE_OK ) {
    cout << "SQLite3 error: " << errorMsg << endl;
    sqlite3_free(errorMsg);
  }
  return last_id;
}

unsigned int dbobject::getLastInsertID_mysql(MYSQL *conn) {
  unsigned int thread_num = omp_get_thread_num();
  myassert(connection_status == DBOBJECT_CONNECTION_MYSQL);
  verify_conn_is_open();
  if ( conn == NULL )
    conn = getMySQLConnection();
  string query = "select last_insert_id()";
  unsigned int id = 0, isbad;
  string error;
#pragma omp critical(dbcpp)
  {
    isbad = mysql_query(conn, query.c_str());
    increment_query_count();
    if ( !isbad ) {
      MYSQL_RES *res = mysql_use_result(conn);
      MYSQL_ROW row = mysql_fetch_row(res);
      sscanf (row[0], "%d", &id);
      mysql_free_result(res);
    } else
      error = string(mysql_error(conn));
  }
  if ( isbad ) {
#pragma omp critical(output)
    cerr << error << " in dbobject::getLastInsertID()" << endl;
    bool recret = false;
#pragma omp critical(dbcpp)
    recret = reconnect();
    if ( recret ) {
      possibly_call_reconnect_callback();
      cerr << "Calling method again in thread " << thread_num << "..." << endl;
      return getLastInsertID(); // does NOT send in the 'conn' parameter
    } else {
      cerr << "Unable to reconnect to database." << endl;
      exit(1);
    }
  }
  return id;
}

void dbobject::executeUpdate(string query) {
  if ( connection_status == DBOBJECT_CONNECTION_MYSQL )
    executeUpdate_mysql(query);
  else if ( connection_status == DBOBJECT_CONNECTION_SQLITE )
    executeUpdate_sqlite3(query);
  else
    myassert(0);
}

void dbobject::executeUpdate_sqlite3(string query) {
  myassert(connection_status == DBOBJECT_CONNECTION_SQLITE);
  char* errorMsg;
  int rc = 0;
#pragma omp critical(dbcpp)
  rc = sqlite3_exec(db, query.c_str(), NULL, NULL, &errorMsg);
  if ( rc != SQLITE_OK ) {
    cout << "SQLite3 error: " << errorMsg << endl;
    sqlite3_free(errorMsg);
  }
}

void dbobject::executeUpdate_mysql(string query, MYSQL *conn) {
  unsigned int thread_num = omp_get_thread_num();
  myassert(connection_status == DBOBJECT_CONNECTION_MYSQL);
  verify_conn_is_open();
  if ( conn == NULL )
    conn = getMySQLConnection();
  if ( conn == NULL ) {
    cerr << "Ack!  conn is null in dbobject::executeUpdate()" << endl;
    exit(1);
  }
  int myret = -1;
#pragma omp critical(dbcpp)
  myret = mysql_query(conn,query.c_str());
  increment_query_count();
  bool isgood = (myret == 0);
  if ( !isgood ) {
    string error = mysql_error(conn);
#pragma omp critical(output)
    cerr << error << " in thread " << thread_num << " in dbobject::executeUpdate() on query: " << query << endl;
    bool recret = false;
#pragma omp critical(dbcpp)
    recret = reconnect();
    if ( recret ) {
      possibly_call_reconnect_callback();
      cerr << "Calling method again in thread " << thread_num << "..." << endl;
      return executeUpdate(query); // does NOT send in the 'conn' parameter
    } else {
      cerr << "Unable to reconnect to database." << endl;
      exit(1);
    }
  }
}

void dbobject::raise_error(string s) {
  cout << s << endl;
  // change this to something else, if desired
  myassert(0);
}

void dbobject::saveAll(vector<dbobject*> vec, MYSQL *conn) {
  verify_conn_is_open();
  if ( conn == NULL )
    conn = getMySQLConnection();
  if ( conn == NULL ) {
    cerr << "Ack!  conn is null in dbobject::saveAll()" << endl;
    exit(1);
  }
  for ( unsigned int i = 0; i < vec.size(); i++ )
    vec[i]->save(); // already in its own critical section
}

ostream& operator<< (ostream& out, dbobject &x) {
  return x.put(out);
}

} // namespace db
)DBOBJC";
