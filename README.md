C++ Database Framework
======================

This is a utility that creates a series of C++ classes that can be used to access a MySQL database.  In particular, for each table in the DB, a separate C++ class is created, which represents a record from that table.  Functions for loading and saving the data are also included.  It is meant to be a simple utility that creates a series of C++ classes that can easily be used to access a DB; the design goal was quickly writing this code, not having complete functionality.

It is released under the the GPL, v2.  It was created by [Aaron Bloomfield](http://aaronbloomfield.me) ([aaron@virginia.edu](<mailto:aaron@virginia.edu>), [@bloomfieldaaron](http://twitter.com/bloomfieldaaron)).

It is assumed that each table has an integer id field (likely `auto_increment`, although that is technically not a requirement) as the **first** column in the table.  This is necessary for any saving of data to the DB.  If this is not the case, then there will be errors when saving a records (and it may corrupt the data in the DB!), but the non-saving features will certainly work.  Note that this field can be named anything ("id", "fooid", etc.).  In particular, it must be such that each record can be uniquely identified by said id field.  

Introduction
------------

To call this utility, you pass either three or four parameters on the command line: hostname, database name, user name, and an optional password, in that order.  If the password is not specified, then the program will prompt for one.  The utility will create a dbcpp/ directory, in which all of the files it create will be written.

The `connect()` methods in dbobject will connect to the database; dbobject.h has a `#include <mysql/mysql.h>` line.  To compile, it must be linked with the mysqlclient library (pass `-lmysqlclient` to the compiler).

All classes are in a `db` namespace.


Classes
-------

There is one parent class created, `dbobject`, and the rest of the created classes (one per table) are all sub-classes of `dbobject`.

### dbobject ###

This class provides much of the functionality for loading and saving the data, while deferring table-specific methods to the sub-classes.  As it is an abstract class (since it has virtual pure methods -- meaning methods that have no bodies), it can not be instantiated.

##### Public methods #####

These methods all have their impelementation in the dbobject.cpp file.  All these methods are `public`.  All of the methods below allow passing of the MySQL connection entity; if left out (as it has a default parameter value) or set to `NULL`, then the value passed into `setMySQLConnection()` is used instead.

- `dbobject()`: constructor; does nothing.
- `dbobject(const dbobject& orig)`: copy constructor; does nothing.
- `virtual ~dbobject()`: destructor; does nothing.
- `connect (string host, string db, string user, string passwd)`: this connects to the DB; there is also a version of this method that takes in `char*` parameters.
- `void setVerbose(bool which)`: when `true`, this will cause the sub-classes to print queries on some actions.
- `void setMySQLConnection(MYSQL *conn = NULL)`: this sets the MySQL connection that is used by all the code produced by this utility; see below for how to create a `MYSQL*` connection.
- `vector<dbobject*> loadAll(MYSQL *conn = NULL)`: loads all the entries from the table into a vector.  The individual classes are the specific sub-classes, represented in the vector as `dbobject*` pointers.
- `void freeVector(vector<dbobject*> vec)`: properlly deallocates a vector obtained via the `freeAll()` method.
- `vector<dbobject*> load(string constraint, int count = 0, MYSQL *conn = NULL)`: loads rows from the DB, but with the passed constraint (which becomes the `where` clause in the DB query).  If count is 0, then it will load all rows that match the constraint; otherwise, it loads `count` entries.
- `dbobject* loadFirst(string constraint, MYSQL *conn = NULL)`: load the first row that matches the passed constraint (which becomes the `where` clause in the DB query).
- `void executeUpdate(string query, MYSQL *conn = NULL)`: this executes a MySQL `update` command, which is assumed to not return a value.
- `virtual void save(MYSQL *conn = NULL)`: this method executes a MySQL `update` command (based on the `id` field) to overwrite the existing entry in the DB.  This is a pure virtual method.
- `void saveAll(vector<dbobject*> vec, MYSQL *conn = NULL)`: this calls the `save()` method on each of the objects in the vector.


##### Protected methods #####

These methods are all `protected` in the sub-classes; there is also one function (`operator<< ()`) in dbobject.h/cpp.  Thus, they are not part of the public interface, and are only included here for those who want to delve into how the code works.

- `ostream& operator<< (ostream& out, dbobject &x)`: allows printing of the object via `cout`.  As it is declared for dbobject, it calls `put()` on the sub-class to allow the sub-class to print itself properly.
- `virtual dbobject* readInFullRow(MYSQL_ROW row)`: reads in a full row, entering each column into a field in the class.
- `virtual string getTableName()`: gets the table name that this class is pulling data out of (or writing data to).  This will be the same name as the class.
- `virtual ostream& put(ostream &out)`: this allows the sub-class to print itself, and is only called by `operator<< ()`.
- `virtual bool isUpdate()`: whether the current query to be executed is an update or not.

### sub-classes ###

Each table in the DB has it's own class, and the class name is the same as the table name.  For each column in the table, the following are included in the class:

- A field with an equivalent C++ type (all `text` fields are represented via a C++ `string`), as well as a field to indicate if that value is set to NULL; note that all fields are `private`
- A setter method that takes in the type of the field, and sets the (`private`) field in the class to the passed value; this method's name has `set_` pre-pended to the field name
- A setter method that takes in a `char*`, and parses that (based on the field type) into the (private) field in the class; this method's name also has `set_` pre-pended to the field name (if this parameter is `NULL`, then the field is set to NULL)
- A getter method, which gets the value of the field; this method's name has `get_` pre-pended to the field name (this returns a pointer to the field in the object, and this pointer is `NULL` if the field is NULL; note that this allows direct access to the field)
- A method to set that field to NULL; this method's name has `setToNull_` pre-pended to the field name
- A method to get if that field is NULL; this method's name has `getIsNull_` pre-pended to the field name

In addition, there are few other methods included in each sub-class:

- Default constructor, which sets each field to NULL
- Two specific constructors that take in one parameter for *each* column in the table -- this can cause there to be a *very* long parameter list.  The difference between these two constructors is that one takes in the actual types of the fields (similar to the first setter method described above), and one takes in `char*` fields, which are then parsed into the field (similar to the second setter method described above).
- Two `setAll()` methods.  These methods set all the fields of the object.  They also take in one parameter for each column in the table, and thus can have very long parameter lists.  The two versions are similar to the two specific constructor versions: one takes in each field type, and one takes in `char*` fields which are parsed into the field.
- Two `enter()` methods.  These methods directly enter a record into the DB (note that if you have already loaded the fields via `loadAll()`, then that vector is *not* modified.  They also take in one parameter for each column in the table, and thus can have very long parameter lists.  The two versions are similar to the two specific constructor versions: one takes in each field type, and one takes in `char*` fields which are parsed into the field.
- The `save()` methods, described in the "public methods" section, above.
- A number of `protected` methods, described in the "protected methods" section, above.


Files Created
-------------

All files are created into a dbcpp/ sub-directory.  Any files of the same name in that directory will be overwritten.

The `dbobject` class is in its own files (dbobject.h and dbobject.cpp).  Each class has it's own .h and .cpp files, with the class name (and thus the file base name) being the same as the table name.

A "all\_db\_h\_files.h" file is created, which just \#include's all the other .h files.

A sample Makefile is created, which compiles all of the .cpp files created by this utility.  The 'main' target will create an executable, but a main.cpp file needs to be provided for that.


Todos / Limitations
-------------------

This is a list of features yet to be implemented (i.e., the limitations of this code)

- Some MySQL data types are not yet supported, such as blobs, enums, and decimal.  Note that text, double, floats, and integer types are all supported.
- The various integer sizes in C++ should be made to match the same size (and singed/unsigned) as the MySQL verions -- right now, all MySQL integer ytpes are a C++ `int`, execpt for a MySQL `bitint`, which is a C++ `long`.
- A methods to return `last_insert_id()`, which would be called after `enter()`.  Or perhaphs `enter()` could return that value.
- A way to get the MySQL warnings after an update/entry.  Or have it somehow return the error/warning status.
- Enums are just stored as a `string`, and can be set (through the C++ object) to any value, and is not restricted to the enum value
- Likewise, decimal types are just stored as `string` objects
- If there is no auto_increment ID field in a table (or at least as the first column in a table), then it should detect this and prevent saving of records
