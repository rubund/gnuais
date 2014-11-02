
The first time GNU AIS is started, a configuration file is created in ~/.config/gnuais/config.
An alternative configuration file can be specified with the command line argument -c <filename>.
If an alternative configuration file is specified the first time you run gnuais, the file ~/.config/gnuais/config
will not be generated.
It is explained in ~/.config/gnuais/config how you will have to edit it in order to fullfill your wishes.

You will also have to create a table in the database if the mysql option is desired:


Make new database (if needed) with command:
  mysqladmin create <databasename>

You can also use any existing database adding table 'ais':
  mysql <databasename>  < create\_table.sql
  
Or if you have specified username and password for your database:
  mysql -u <username> -p <databasename>  < create\_table.sql
  
 

The file create\_table.sql can be found in this folder.
