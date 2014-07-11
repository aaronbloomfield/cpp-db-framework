//--------------------------------------------------------------------------------
// dbobject.h

const char *dbobject_h = R"DBOBJH(//
// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework

#ifndef DBOBJECT_H
#define DBOBJECT_H

#include <iostream>
#include <vector>
#include <string>
#include <mysql/mysql.h>

using namespace std;

namespace db {

class dbobject {
public:
  dbobject();
  dbobject(const dbobject& orig);
  virtual ~dbobject();

  static bool connect(string host, string db, string user, string passwd);
  static bool connect(char* host, char* db, char* user, char* passwd);

  vector<dbobject*> loadAll(MYSQL *conn = NULL);
  vector<dbobject*> load(string constraint, int count = 0, MYSQL *conn = NULL);
  dbobject* loadFirst(string constraint, MYSQL *conn = NULL);

  static void setVerbose (bool which);
  static void setMySQLConnection(MYSQL *conn);
  static void freeVector(vector<dbobject*> vec);
  static void saveAll(vector<dbobject*> vec, MYSQL *conn = NULL);
  static void executeUpdate(string query, MYSQL *conn = NULL);

  virtual void save(MYSQL *conn = NULL) = 0;

protected:
  static bool _verbose;
  static MYSQL *theconn;

  virtual dbobject* readInFullRow(MYSQL_ROW row) = 0;
  virtual string getTableName() = 0;
  virtual ostream& put(ostream &out) = 0;
  virtual bool isUpdate() = 0;

private:
  static bool connect_private(const char* host, const char* db, const char* user, const char* passwd);

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
#include <stdlib.h>

#include <string>
#include <sstream>

#include "dbobject.h"

using namespace std;

namespace db {

bool dbobject::_verbose = false;
MYSQL* dbobject::theconn = NULL;

dbobject::dbobject() {
}

dbobject::dbobject(const dbobject& orig) {
}

dbobject::~dbobject() {
}

bool dbobject::connect_private(const char* host, const char* db, const char* user, const char* passwd) {
  MYSQL *conn = mysql_init(NULL);
  if (!mysql_real_connect(conn, host, user, passwd, db, 0, NULL, 0))
    return false;
  setMySQLConnection(conn);
  return true;
}

bool dbobject::connect(char* host, char* db, char* user, char* passwd) {
  return connect_private(host, db, user, passwd);
}

bool dbobject::connect(string host, string db, string user, string passwd) {
  return connect_private(host.c_str(),db.c_str(),user.c_str(),passwd.c_str());
}

void dbobject::setVerbose(bool which) {
  _verbose = which;
}

void dbobject::setMySQLConnection(MYSQL *conn) {
  theconn = conn;
}

vector<dbobject*> dbobject::loadAll(MYSQL *conn) {
  return load("",0,conn);
}

void dbobject::freeVector(vector<dbobject*> vec) {
  for ( unsigned int i = 0; i < vec.size(); i++ )
    delete vec[i];
  vec.clear();
}

dbobject* dbobject::loadFirst(string constraint, MYSQL *conn) {
  vector<dbobject*> foo = load("",1,conn);
  return foo[0];
}

void dbobject::executeUpdate(string query, MYSQL *conn) {
  if ( conn == NULL )
    conn = theconn;
  if ( conn == NULL ) {
    cerr << "Ack!  conn is null in dbobject::executeUpdate()" << endl;
    exit(1);
  }
  if (mysql_query(conn, query.c_str())) {
    cerr << mysql_error(conn) << endl;
    exit(1);
  }
}

vector<dbobject*> dbobject::load(string constraint, int count, MYSQL *conn) {
  if ( conn == NULL )
    conn = theconn;
  if ( conn == NULL ) {
    cerr << "Ack!  conn is null in dbobject::load()" << endl;
    exit(1);
  }
  stringstream querystream;
  querystream << "select * from " << getTableName();
  if ( constraint != "" )
    querystream << " where " << constraint;
  if ( count != 0 )
    querystream << " limit " << count;
  string query = querystream.str();
  if (mysql_query(conn, query.c_str())) {
    cerr << mysql_error(conn) << endl;
    exit(1);
  }
  MYSQL_RES *res = mysql_use_result(conn);
  vector<dbobject*> *ret = new vector<dbobject*>();
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res)) != NULL)
    ret->push_back(readInFullRow(row));
  mysql_free_result(res);
  return *ret;
}

void dbobject::saveAll(vector<dbobject*> vec, MYSQL *conn) {
  if ( conn == NULL )
    conn = theconn;
  if ( conn == NULL ) {
    cerr << "Ack!  conn is null in dbobject::load()" << endl;
    exit(1);
  }
  for ( unsigned int i = 0; i < vec.size(); i++ )
    vec[i]->save();
}

ostream& operator<< (ostream& out, dbobject &x) {
  return x.put(out);
}

} // namespace db
)DBOBJC";

//--------------------------------------------------------------------------------