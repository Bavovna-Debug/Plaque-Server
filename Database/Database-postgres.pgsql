/**************************************************************/
/*                                                            */
/*  Create role(s).                                           */
/*                                                            */
/**************************************************************/

CREATE ROLE vp WITH
	LOGIN
	CONNECTION LIMIT 8020
	PASSWORD 'vi79HRhxbFahmCKFUKMAACrY';

/**************************************************************/
/*                                                            */
/*  Define database and table spaces.                         */
/*                                                            */
/**************************************************************/

CREATE TABLESPACE vp_root
	OWNER vp
	LOCATION '/var/opt/database/vp_root';

CREATE TABLESPACE vp_debug
	OWNER vp
	LOCATION '/var/opt/database/vp_debug';

CREATE TABLESPACE vp_operator
	OWNER vp
	LOCATION '/var/opt/database/vp_operator';

CREATE TABLESPACE vp_auth
	OWNER vp
	LOCATION '/var/opt/database/vp_auth';

CREATE TABLESPACE vp_reception
	OWNER vp
	LOCATION '/var/opt/database/vp_reception';

CREATE TABLESPACE vp_surrounding
	OWNER vp
	LOCATION '/var/opt/database/vp_surrounding';

CREATE TABLESPACE vp_journal
	OWNER vp
	LOCATION '/var/opt/database/vp_journal';

CREATE DATABASE vp WITH
	OWNER vp
	TEMPLATE template0
	ENCODING 'UTF8'
	LC_COLLATE 'de_DE.UTF-8'
	LC_CTYPE 'de_DE.UTF-8'
	TABLESPACE vp_root;

/**************************************************************/
/*                                                            */
/*  Activate extensions.                                      */
/*                                                            */
/**************************************************************/

CREATE LANGUAGE plpgsql;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp" WITH SCHEMA public;

CREATE EXTENSION IF NOT EXISTS "cube" WITH SCHEMA public;

CREATE EXTENSION IF NOT EXISTS "earthdistance" WITH SCHEMA public;
