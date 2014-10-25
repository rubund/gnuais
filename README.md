
The first time GNU AIS is started, a configuration file is created in ~/.config/gnuais/config.
An alternative configuration file can be specified with the command line argument -c <filename>.
It is explained in ~/.config/gnuais/config how you will have to edit it in order to fullfill your wishes.

(Note: For backwards compatibility, if there is a configuration file in /etc/gnuais.conf, no local
configuration file will be created in ~/.config/gnuais/config. /etc/gnuais.conf will override
user settings.)

You will also have to create a table in the database if the mysql option is desired:


Make new database (if needed) with command:
  mysqladmin create <databasename>

You can also use any existing database adding table 'ais':
  mysql <databasename>  < create\_table.sql
  
Or if you have specified username and password for your database:
  mysql -u <username> -p <databasename>  < create\_table.sql
  
 

The file create\_table.sql can be found in this folder.
