/* 
 * File:   main.cpp
 * Author: aaron
 *
 * Created on June 18, 2012, 10:30 AM
 */

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

#include <mysql/mysql.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "files.h"

using namespace std;

MYSQL *conn = NULL;

MYSQL_RES* runQueryMain (char* query) {
    if (mysql_query(conn, query)) {
        cerr << "Error executing query '" << query << "': " << mysql_error(conn) << endl;
        exit(1);
    }
    return mysql_use_result(conn);
}

MYSQL_RES* runQuery (const char* query) {
    return runQueryMain((char*)query);
}

MYSQL_RES* runQuery (char* query) {
    return runQueryMain(query);
}

MYSQL_RES* runQuery (string query) {
    return runQuery(query.c_str());
}

string toupper(string s) {
    string ret;
    for ( int i = 0; i < s.size(); i++ )
        ret += toupper(s[i]);
    return ret;
}

string tocapcase(string s) {
    string ret;
    for ( int i = 0; i < s.size(); i++ )
        ret += tolower(s[i]);
    ret[0] = toupper(s[0]);
    return ret;
}

string convertTypeMain(char* type) {
    assert(sizeof(int) == 4);
    assert(sizeof(char) == 1);
    if ( !strcmp(type,"int(10)") )
        return "int";
    if ( !strcmp(type,"int(10) unsigned") )
        return "unsigned int";
    if ( !strcmp(type,"int(11)") )
        return "int";
    if ( !strcmp(type,"int(11) unsigned") )
        return "unsigned int";
    if ( !strcmp(type,"tinyint(1)") )
        return "bool";
    if ( !strcmp(type,"tinyint(3)") )
        return "int";
    if ( !strcmp(type,"tinyint(4)") )
        return "int";
    if ( !strcmp(type,"tinyint(3) unsigned") )
        return "unsigned int";
    if ( !strcmp(type,"bigint") )
        return "long";
    
    if ( !strcmp(type,"text") )
        return "string";
    if ( !strcmp(type,"mediumtext") )
        return "string";
    if ( !strcmp(type,"longtext") )
        return "string";
    if ( !strcmp(type,"tinytext") )
        return "string";
        
    if ( !strcmp(type,"double") )
        return "double";
    if ( !strcmp(type,"float") )
        return "float";

    if ( (strlen(type)>7) && (!strncmp(type,"varchar",7)) ) {
        //int size;
        //sscanf (type,"varchar(%d)", &size);
        //char ret[32];
        //sprintf (ret, "char[%d]", size);
        return "string";
    }
    
    if ( (strlen(type)>7) && (!strncmp(type,"decimal",7)) ) {
        return "string";
    }
    
    if ( !strcmp(type,"datetime") )
        return "string";
    if ( !strcmp(type,"time") )
        return "string";
    if ( !strcmp(type,"date") )
        return "string";
    
    if ( !strncmp(type,"enum(",5) )
      return "string";

    return "NONE";
}

string convertType (string type) {
    return convertTypeMain((char*)type.c_str());
}

string convertType (const char* type) {
    return convertTypeMain((char*)type);
}

string convertType (char* type) {
    return convertTypeMain(type);
}

// this also sets 'ret' to 1 if the parse was successful
string readInTypeFromCStr (string field, string type) {
    if ( (type == "int") || (type == "unsigned int") )
        return "int ret = sscanf (x, \"%d\", &" + field + ");";
    if ( type == "bool" )
        return field + " = ( ( !strcmp(x,\"1\") || !strcmp(x,\"true\") ) ) ? 1 : 0; int ret = 1;";
    if ( type == "float" )
        return "int ret = sscanf (x, \"%f\", &" + field + ");";
    if ( type == "double" )
        return "int ret = sscanf (x, \"%lf\", &" + field + ");";
    if ( type == "string" )
        return field + " = x; int ret = 1;";
    return "\n#error ACK";
}

string getNullValueForType (string type) {
    if ( (type == "int") || (type == "unsigned int") )
        return "0";
    if ( type == "bool" )
        return "false";
    if ( type == "float" )
        return "0.0f";
    if ( type == "double" )
        return "0.0";
    if ( type == "char*" )
        return "(char*)\"NULL\"";
    if ( type == "string" )
        return "\"NULL\"";
    stringstream str;
    str << "\n#error ACK ('" << type << "')";
    return str.str();
}

int main(int argc, char** argv) {
    char *dbname = NULL, *dbuser = NULL, *dbpass = NULL, *dbhost = NULL;
    string buffer;
    vector<string> hfiles;
    hfiles.push_back("dbobject.h");
    
    if ( (argc != 5) && (argc != 4) ) {
        cerr << "Usage: " << argv[0] << " <hostname> <database> <username> [<password>]" << endl;
        exit(0);
    } else {
        dbhost = argv[1];
        dbname = argv[2];
        dbuser = argv[3];
    }

    if ( argc == 5 )
      dbpass = argv[4];
    else {
      cout << "Enter password for user '" << dbuser << "' for database '" << dbname << "' on host '" << dbhost << "': ";
      cin >> buffer;
      dbpass = (char*) buffer.c_str();
    }
    
    // connect to DB
    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, dbhost, dbuser, dbpass, dbname, 0, NULL, 0)) {
        cerr << "Error connecting to the DB: " << mysql_error(conn) << endl;
        exit(1);
    }
    cout << "Successfully connected to the database..." << endl;

    MYSQL_RES *res;
    MYSQL_ROW row;
    
    res = runQuery("show tables");
    vector<string> tables;
    while ((row = mysql_fetch_row(res)) != NULL)
        tables.push_back(row[0]);
    mysql_free_result(res);
    
    system("mkdir -p dbcpp");
    
    string ofilelist = "dbobject.o";

    for (vector<string>::iterator it = tables.begin(); it != tables.end(); ++it) {
        cout << "Working on table " << *it << "..." << endl;
        string query = "describe " + *it;
        res = runQuery(query);
        vector<string> fields, types;
	vector<bool> canbenull;
        while ((row = mysql_fetch_row(res)) != NULL) {
            fields.push_back(row[0]);
            types.push_back(row[1]);
	    if ( !strcmp(row[2],"YES") )
	      canbenull.push_back(true);
	    else
	      canbenull.push_back(false);
        }
        mysql_free_result(res);
        
	hfiles.push_back(*it + ".h");
        string hfilename = "dbcpp/" + *it + ".h";
        string cfilename = "dbcpp/" + *it + ".cpp";
        ofilelist += " " + *it + ".o";

	// h file header
        cout << "\tWriting to " << hfilename << endl;
        ofstream hfile(hfilename.c_str());
	hfile << "// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework\n\n";
        hfile << "#ifndef DB_" << toupper(*it) << "_H\n#define DB_" << toupper(*it) << "_H\n";
        hfile << "\n#include <iostream>\n#include <string>\n#include <mysql/mysql.h>\n\n"
                << "#include \"dbobject.h\"\n\nusing namespace std;\n\n";
	hfile << "namespace db {\n\n";
        hfile << "class " << *it << " : public dbobject {\n";
        hfile << "\n  public:\n";
        // the constructors and setAll()
	hfile << "    " << *it << "();\n";
	hfile << "    " << *it << " (" << *it << "& other);\n";
        hfile << "    " << *it << " (";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n";
        hfile << "    " << *it << " (";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n\n";
        hfile << "    void setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n";
        hfile << "    void setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n";
	// save()
        hfile << "\n    virtual void save(MYSQL *conn = NULL);\n\n";
        // enter()
        hfile << "    static void enter(";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n";
        hfile << "    static void enter(";
        for ( int i = 0; i < fields.size(); i++ )
            hfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        hfile << ");\n";
	hfile << "    static vector<" << *it << "*> loadAll(MYSQL *conn = NULL);\n";
	hfile << "    static vector<" << *it << "*> load(string constraint, int count = 0, MYSQL *conn = NULL);\n";
	hfile << "    static " << *it << "* loadFirst(string constraint = \"\", MYSQL *conn = NULL);\n";
	hfile << "    static void freeVector(vector<" << *it << "*> vec);\n";
        // protected methods
        hfile << "\n  protected:\n    virtual dbobject* readInFullRow(MYSQL_ROW row);\n"
                << "    virtual string getTableName();\n"
                << "    virtual ostream& put (ostream& out);\n"
                << "    virtual bool isUpdate();\n\n";
        // fields, getters and setters
        for ( int i = 0; i < fields.size(); i++ ) {
	    hfile << "  // " << fields[i] << ": " << types[i] << "\n";
            hfile << "  private:\n    " << convertType(types[i]) << " " << fields[i] << ";\n";
	    if ( canbenull[i] )
	      hfile << "    bool _" << fields[i] << "_is_null;\n";
	    else
	      hfile << "    // " << fields[i] << " cannot be null, so no " 
		    << fields[i] << "_is_null field\n";
	    hfile << "  public:\n";
            hfile << "    const " << convertType(types[i]) << "* get_" << fields[i] << "() const;\n" <<
                    "    void set_" << fields[i] << " (" << convertType(types[i]) << " x);\n";
            if ( convertType(types[i]) != "char*" )
                hfile << "    void set_" << fields[i] << " (char* x);\n";
	    if ( canbenull[i] )
	      hfile << "    void " << "setToNull_" << fields[i] << "();\n";
            hfile << "    bool " << "getIsNull_" << fields[i] << "() const;\n\n";
        }
	// end of h file
        hfile << "};\n\n"; // class close
	hfile << "} // namespace db\n\n";
        hfile << "#endif" << endl;
        hfile.close();
        
        cout << "\tWriting to " << cfilename << endl;
        ofstream cfile(cfilename.c_str());
	cfile << "// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework\n\n";
        cfile << "#include \"" << *it << ".h\"\n\n#include <stdio.h>\n"
                << "#include <string.h>\n#include <stdlib.h>\n#include <sstream>\n\n";
	cfile << "namespace db {\n\n";
        // default constructor
        cfile << *it << "::" << *it << "() {\n";
        for ( int i = 0; i < fields.size(); i++ ) {
	  if ( canbenull[i] )
            cfile << "  _" << fields[i] << "_is_null = true;\n";
	  else
            cfile << "  " << fields[i] << " = " << getNullValueForType(convertType(types[i]))
		  << "; // can't be null, so must set value\n";
	}
        cfile << "}\n\n";
        // copy constructor
        cfile << *it << "::" << *it << "(" << *it << "& other) {\n";
        for ( int i = 0; i < fields.size(); i++ ) {
	  cfile << "  this->" << fields[i] << " = other." << fields[i] << ";\n";
	  if ( canbenull[i] )
	    cfile << "  this->_" << fields[i] << "_is_null = other._" << fields[i] << "_is_null;\n";
	  else
	    cfile << "  // " << fields[i] << " cannot be null, so no setting of _is_null field\n";
	}
        cfile << "}\n\n";
        // the specific constructor with strings
        cfile << *it << "::" << *it << " (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n  setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ");\n}\n\n";
        // the specific constructor with char*
        cfile << *it << "::" << *it << " (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n  setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ");\n}\n\n";
        // setAll() with char*
        cfile << "void " << *it << "::setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "  set_" << fields[i] << " (_" << fields[i] << ");\n";
        cfile << "}\n\n";
        // setAll() with strings
        cfile << "void " << *it << "::setAll (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n";
        for ( int i = 0; i < fields.size(); i++ )
	  if ( canbenull[i] )
            cfile << "  if ( _" << fields[i] << " == \"NULL\" )\n"
		  << "    setToNull_" << fields[i] << "();\n"
		  << "  else\n"
		  << "    set_" << fields[i] << " ((char*)_" << fields[i] << ".c_str());\n";
	  else
            cfile << "  if ( _" << fields[i] << " == \"NULL\" )\n"
		  << "    raise_error(\"cannot pass 'NULL' into " << *it << "::setAll() for field '" << fields[i] << "'\");\n"
		  << "  else\n"
		  << "    set_" << fields[i] << " ((char*)_" << fields[i] << ".c_str());\n";
        cfile << "}\n\n";
        // enter() with strings
        cfile << "void " << *it << "::enter(";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "string _" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n  " << *it << " x (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ");\n  x.save();\n}\n\n";
        // enter() with char*
        cfile << "void " << *it << "::enter(";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "char *_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ") {\n  " << *it << " x (";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "_" << fields[i] << ((i!=fields.size()-1)?", ":"");
        cfile << ");\n  x.save();\n}\n\n";
	// loadAll() and loadFirst()
        cfile << "vector<" << *it << "*> " << *it << "::loadAll(MYSQL *conn) {\n"
	      << "  return load(\"\",0,conn);\n"
	      << "}\n\n";
	cfile << "" << *it << "* " << *it << "::loadFirst(string constraint, MYSQL *conn) {\n"
	      << "  vector<" << *it << "*> foo = load(constraint,1,conn);\n"
	      << "  if ( foo.size() == 0 )\n"
	      << "    return NULL;\n"
	      << "  else\n"
	      << "    return foo[0];\n"
	      << "}\n\n";
	cfile << "vector<" << *it << "*> " << *it << "::load(string constraint, int count, MYSQL *conn) {\n"
	      << "  " << *it << " o;\n"
	      << "  if ( conn == NULL )\n"
	      << "    conn = getMySQLConnection();\n"
	      << "  if ( conn == NULL ) {\n"
	      << "    cerr << \"Ack!  conn is null in " << *it << "::load()\" << endl;\n"
	      << "    exit(1);\n"
	      << "  }\n"
	      << "  stringstream querystream;\n"
	      << "  querystream << \"select * from \" << o.getTableName();\n"
	      << "  if ( constraint != \"\" )\n"
	      << "    querystream << \" where \" << constraint;\n"
	      << "  if ( count != 0 )\n"
	      << "    querystream << \" limit \" << count;\n"
	      << "  string query = querystream.str();\n"
	      << "  if ( _verbose )\n"
	      << "    cout << query << endl;\n"
	      << "  int isbad;\n"
	      << "  vector<" << *it << "*> ret;\n"
	      << "#pragma omp critical\n"
	      << "  {\n"
	      << "    isbad = mysql_query(conn, query.c_str());\n"
	      << "    increment_query_count();\n"
	      << "    if ( !isbad ) {\n"
	      << "      MYSQL_RES *res = mysql_use_result(conn);\n"
	      << "      MYSQL_ROW row;\n"
	      << "      while ((row = mysql_fetch_row(res)))\n"
	      << "	ret.push_back((" << *it << "*)(o.readInFullRow(row)));\n"
	      << "      mysql_free_result(res);\n"
	      << "    } else\n"
	      << "      cerr << mysql_error(conn) << \" in " << *it << "::load() on query: \" << query << endl;\n"
	      << "  }\n"
	      << "  if ( isbad ) {\n"
	      << "    bool recret;\n"
	      << "#pragma omp critical\n"
	      << "    recret = reconnect();\n"
	      << "    if ( recret ) {\n"
	      << "      possibly_call_reconnect_callback();\n"
	      << "      cerr << \"Calling method again...\" << endl;\n"
	      << "      return load(constraint,count); // does NOT send in the 'conn' parameter\n"
	      << "    } else {\n"
	      << "      cerr << \"Unable to reconnect to database.\" << endl;\n"
	      << "      exit(1);\n"
	      << "    }\n"
	      << "  }\n"
	      << "  return ret;\n"
	      << "}\n\n";
	cfile << "void " << *it << "::freeVector(vector<" << *it << "*> vec) {\n"
	      << "  for ( unsigned int i = 0; i < vec.size(); i++ )\n"
	      << "    delete vec[i];\n"
	      << "  vec.clear();\n"
	      << "}\n\n";
        // getters and setters
        for ( int i = 0; i < fields.size(); i++ ) {
	    cfile << "\n// " << fields[i] << ": " << types[i] << "\n\n";
            // check if we have to handle a NULL parameter
            string extraCheck = "";
            if ( convertType(types[i]) == "char*" )
                extraCheck = "if ( x == NULL ) " + fields[i] + " = (char*)\"NULL\"; else ";
            // getter
            cfile << "const " << convertType(types[i]) << "* " << *it << "::get_" << fields[i]
		  << "() const {\n";
	    if ( canbenull[i] )
	      cfile << "  if (_" << fields[i] << "_is_null )\n    return NULL;\n";
	    cfile << "  return &" << fields[i] << ";\n}\n\n";
            cfile << "void " << *it << "::set_" << fields[i] << " (" 
                    << convertType(types[i]) << " x) {\n  " << extraCheck << fields[i] 
		  << " = x;\n";
	    if ( canbenull[i] )
	      cfile << "  _" << fields[i] << "_is_null = false;\n";
	    cfile << "}\n" << endl;
            // set to NULL
	    if ( canbenull[i] )
	      cfile << "void " << *it << "::setToNull_" << fields[i] << "() {\n  _"
                    << fields[i] << "_is_null = true;\n}\n\n";
            // get is NULL
	    if ( canbenull[i] )
	      cfile << "bool " << *it << "::getIsNull_" << fields[i] << "() const {\n  return _"
                    << fields[i] << "_is_null;\n}\n\n";
	    else
	      cfile << "bool " << *it << "::getIsNull_" << fields[i] 
		    << "() const {\n  return false;\n}\n\n";
            // setting from a char* when the field type is not char*
            if ( convertType(types[i]) != "char*" ) {
                cfile << "void " << *it << "::set_" << fields[i] << " (char* x) {\n"
		      << "  if ( x == NULL )\n";
		if ( canbenull[i] )
		  cfile << "    _" << fields[i] << "_is_null = true;\n";
		else
		  cfile << "    raise_error(\"cannot pass NULL to set_" << fields[i] << "()\");\n";
		cfile << "  else {\n"
		      << "    " << readInTypeFromCStr(fields[i],convertType(types[i])) << "\n"
		      << "    if ( ret != 1 )\n"
		      << "      raise_error(\"parse error in set_" << fields[i] << "()\");\n";
		if ( canbenull[i] )
		  cfile << "    _" << fields[i] << "_is_null = false;\n";
		cfile << "  }\n}\n\n";
	    }
        }
        // getTableName()
        cfile << "string " << *it << "::getTableName() {\n"
                << "  return \"" << *it << "\";\n}\n\n";
        // readInFullRow()
        cfile << "dbobject* " << *it << "::readInFullRow(MYSQL_ROW row) {\n"
                << "  " << *it << " *ret = new " << *it << "();\n";
        for ( int i = 0; i < fields.size(); i++ )
            cfile << "  ret->set_" << fields[i] << "(row[" << i << "]);\n";
        cfile << "  return ret;\n}\n\n";
        // put()
        cfile << "ostream& " << *it << "::put (ostream& out) {\n  stringstream ret;\n  ret << \""
                << *it << "{\";\n";
        for ( int i = 0; i < fields.size(); i++ )
	  if ( canbenull[i] )
            cfile << "  if ( _" << fields[i] << "_is_null )\n    ret << \"" << fields[i] 
                    << "=NULL\"" << ((i!=fields.size()-1)?" << \",\";\n":";\n")
                    << "  else\n    ret << \"" << fields[i] << "=\" << " << fields[i]
                    << ((i!=fields.size()-1)?" << \",\";\n":";\n");
	  else
	    cfile << "  ret << \"" << fields[i] << "=\" << " << fields[i]
		  << ((i!=fields.size()-1)?" << \",\";\n":";\n");
        cfile << "  ret << \"}\";\n  return (out << ret.str());\n}\n\n";
        // isUpdate()
	if ( canbenull[0] )
	  // TODO: this won't work for string types...
	  cfile << "bool " << *it << "::isUpdate() {\n"
	        << "  return (!_" << fields[0] << "_is_null);\n"
                << "}\n\n";
	else {
	  cfile << "bool " << *it << "::isUpdate() {\n"
	        << "  return (" << fields[0] << " != " << getNullValueForType(convertType(types[0])) << ");\n"
                << "}\n\n";
	}
        // save()
        cfile << "void " << *it << "::save(MYSQL *conn) {\n"
                << "  if ( conn == NULL )\n"
                << "    conn = theconn;\n"
                << "  if ( conn == NULL ) {\n"
                << "    cerr << \"Ack!  conn is null in " << *it << "::save()\" << endl;\n"
                << "    exit(1);\n"
                << "  }\n"
                << "  stringstream query;\n"
                << "  if ( isUpdate() )\n"
                << "    query << \"update \" << getTableName() << \" set\";\n"
                << "  else\n"
                << "    query << \"insert into \" << getTableName() << \" set\";\n";
        for ( int i = 1; i < fields.size(); i++ ) { // skipping first (primary key) column
            cfile << "  // " << types[i] << " field '" << fields[i] << "'" << endl;
	    if ( canbenull[i] ) {
	      cfile << "  if ( _" << fields[i] << "_is_null )\n    query << \" " << fields[i]
                    << "=NULL\" << ";
	      if ( i == fields.size()-1 )
                cfile << "\"\";\n";
	      else
                cfile << "\",\";\n";
	      if ( types[i] == "datetime" ) {
                cfile << "  else if ( " << fields[i] << " == \"now()\" )\n    query << \" "
		      << fields[i] << "=now()\"";
                if ( i != fields.size()-1 )
		  cfile << " << \",\";\n";
                else
		  cfile << ";\n";
	      }
	      cfile << "  else\n  ";
	    }
	    cfile << "  query << \" " << fields[i] << "='\" << " << fields[i] << " << ";
            if ( i == fields.size()-1 )
                cfile << "\"'\";\n";
            else
                cfile << "\"',\";\n";
        }
        cfile << "  // finishing up the query...\n  if ( isUpdate() )\n"
	      << "    query << \" where " << fields[0] << "=\" << " << fields[0] << ";\n"
	      << "  if ( _verbose )\n    cout << query.str() << endl;\n"
	      << "  int isbad;\n"
	      << "#pragma omp critical\n"
	      << "  {\n"
	      << "    isbad = mysql_query(conn, query.str().c_str());\n"
	      << "    increment_query_count();\n"
	      << "    if ( isbad )\n"
	      << "      cerr << mysql_error(conn) << \" in " << *it << "::save() on query: \" << query.str() << endl;\n"
	      << "  }\n"
	      << "  if ( isbad ) {\n"
	      << "    bool recret;\n"
	      << "#pragma omp critical\n"
	      << "    recret = reconnect();\n"
	      << "    if ( recret ) {\n"
	      << "      possibly_call_reconnect_callback();\n"
	      << "      cerr << \"Calling method again...\" << endl;\n"
	      << "      return save(); // does NOT send in the 'conn' parameter\n"
	      << "    } else {\n"
	      << "      cerr << \"Unable to reconnect to database.\" << endl;\n"
	      << "      exit(1);\n"
	      << "    }\n"
	      << "  }\n"
	      << "}\n\n";
	cfile << "} // namespace db\n\n";
        // close file
        cfile.close();

    }
    
    cout << "Writing dbobject.h..." << endl;
    ofstream dbobjh("dbcpp/dbobject.h");
    dbobjh << dbobject_h;
    dbobjh.close();

    cout << "Writing dbobject.cpp..." << endl;
    ofstream dbobjc("dbcpp/dbobject.cpp");
    dbobjc << dbobject_c;
    dbobjc.close();

    cout << "Writing Makefile..." << endl;
    ofstream makefile("dbcpp/Makefile");
    makefile << "CXX = g++ -O2 -Wall -fopenmp\n\n"
            << "OFILES = " << ofilelist << "\n\n"
            << "OFILESWITHMAIN = " << ofilelist << " main.o\n\n"
            << ".SUFFIXES: .o .cpp\n\n"
            << "ofiles:	$(OFILES)\n\n" 
            << "main:	$(OFILESWITHMAIN)\n"
            << "\t$(CXX) $(OFILESWITHMAIN) -lmysqlclient\n\n"
            << "clean:\n\t/bin/rm -f *.o *~\n\n"
            << "wc:\n\twc -l *.cpp *.h" << endl;
    makefile.close();

    cout << "Writing all_db_h_files.h..." << endl;
    ofstream allhfiles("dbcpp/all_db_h_files.h");
    allhfiles << "// This file generated by the C++ DB framework: https://github.com/aaronbloomfield/cpp-db-framework\n\n";
    for (vector<string>::iterator it = hfiles.begin(); it != hfiles.end(); ++it)
      allhfiles << "#include \"" << *it << "\"\n";
    allhfiles.close();

    return 0;
}
