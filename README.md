C++ Database Framework
======================

This is a C++ program that creates a series of C++ classes that can be used to access a MySQL database.  In particular, for each table in the DB, a separate C++ class is created, which represents a record from that table.  Functions for loading and saving the data are also included.  It is meant to be a simple utility that creates a series of C++ classes that can easily be used to access a DB; the design goal was quickly writing this code, not having complete functionality.

It is released under the the GPL, v2.  It was created by [Aaron Bloomfield](http://www.cs.virginia.edu/~asb) ([aaron@virginia.edu](<mailto:aaron@virginia.edu>), [@bloomfieldaaron](http://twitter.com/bloomfieldaaron)).

It is assumed that each table has an integer `id` field (likely `auto_increment`, although that is technically not a requirement) such that each record can be uniquely identified by that `id` field.  This is necessary for any saving of a row.

Todos / Limitations
-------------------

This is a list of features yet to be implemented (i.e., the limitations of this code)

- Some MySQL data types are not yet supported, such as blobs, enums, and decimal.  Note that text, double, floats, and integer types are all supported.
- The various integer sizes in C++ should be made to match the same size (and singed/unsigned) as the MySQL verions -- right now, all MySQL integer ytpes are a C++ `int`, execpt for a MySQL `bitint`, which is a C++ `long`.
- A methods to return `last_insert_id()`, which would be called after `enter()`.  Or perhaphs `enter()` could return that value.
- A way to get the MySQL warnings after an update/entry.  Or have it somehow return the error/warning status.

Classes
-------

There is one parent class, `dboject`, and the rest of the classes (one per table) are all sub-classes of `dbobject`.

### dbobject ###

This class provides much of the functionality for loading and saving the data, while deferring table-specific methods to the sub-classes.  As it is an abstract class (since it has virtual pure methods -- meaning methods that have no bodies), it can not be instantiated.

##### Public methods #####

These methods all have their impelementation in the dbobject.cpp file.  All these methods are `public`.  All of the methods below allow passing of the MySQL connection entity; if left out (as it has a default parameter value) or set to `NULL`, then the value passed into `setMySQLConnection()` is used instead.

- `dbobject()`: constructor; does nothing.
- `dbobject(const dbobject& orig)`: copy constructor; does nothing.
- `virtual ~dbobject()`: destructor; does nothing.
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

These subroutines are protected in dboject (for `operator<< ()`) or the sublcasses (for all the rest).  Thus, they are not part of the public interface, and are only included here for those who want to delve into how the code works.

- `ostream& operator<< (ostream& out, dbobject &x)`: allows printing of the object via `cout`.  As it is declared for dbobject, it calls `put()` on the sub-class to allow the sub-class to print itself properly.
- `virtual dbobject* readInFullRow(MYSQL_ROW row)`: reads in a full row, entering each column into a field in the class.
- `virtual string getTableName()`: gets the table name that this class is pulling data out of (or writing data to).  This will be the same name as the class.
- `virtual ostream& put(ostream &out)`: this allows the sub-class to print itself, and is only called by `operator<< ()`.
- `virtual bool isUpdate()`: whether the current query to be executed is an update or not.

### sub-classes ###

Each table in the DB has it's own class, and the class name is the same as the table name.  For each column in the table, the following are included in the class:

- a field with an equivalent C++ type (all `text` fields are represented via a C++ `string`)
- a setter method that takes in the type of the field, and sets the (private) field in the class to the passed value
- a setter method that takes in a `char*`, and parses that (based on the field type) into the (private) field in the class
- a getter method, which gets the value of the field
- a method to set that field to NULL

For the naming convention for the setters/getters, the first letter of the field is capitalized, with 'set' or 'get' pre-pended to it.  For the set-to-NULL methods, the setter method name is appended with "ToNull".

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

The `dboject` class is in its own files (dboject.h and dboject.cpp).  Each class has it's own .h and .cpp files, with the class name (and thus the file base name) being the same as the table name.

A "all\_db\_h\_files.h" file is created, which just \#include's all the other .h files.

A sample Makefile is created, which compiles all of the .cpp files created by this utility.  The 'main' target will create an executable, but a main.cpp file needs to be provided for that.

Connecting to MySQL
-------------------

The following code will connect to MySQL; this needs `#include <mysql/mysql.h>` line, as well as linking to the mysqlclient library (`-lmysqlclient` to the compiler).  These files can be installed on an Ubuntu system via the libmysqlclient-dev package.  Change the first four lines to match your credentials.


```
char *dbhost = (char*) "localhost";
char *dbname = (char*) "database";
char *dbuser = (char*) "username";
char *dbpass = (char*) "password";
MYSQL *conn = mysql_init(NULL);
if (!mysql_real_connect(conn, dbhost, dbuser, dbpass, dbname, 0, NULL, 0)) {
    cerr << "Error connecting to the DB: " << mysql_error(conn) << endl;
    exit(1);
}
```

Once connected, the dbobject connection must be set:

```
dbobject::setMySQLConnection(conn);
```

At that point, the various methods in the generated classes should work.
