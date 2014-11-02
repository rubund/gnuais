
drop table if exists `ais_nmea`;
drop table if exists `ais_position`;
drop table if exists `ais_vesseldata`;
drop table if exists `ais_basestation`;

create table ais_nmea (
	id int not null primary key auto_increment,
	time bigint, message varchar(200)
);

create table ais_position (
	id int not null primary key auto_increment,
	time bigint,
	mmsi int,
	latitude float,
	longitude float,
	heading float,
	course float,
	speed float
);

create table ais_vesseldata (
	id int not null primary key auto_increment,
	time bigint,
	mmsi int,
	name varchar(21),
	destination varchar(21),
	draught float,
	A int, B int, C int, D int
);

create table ais_basestation (
	id int not null primary key auto_increment,
	time bigint,
	mmsi int,
	latitude float, longitude float
);

