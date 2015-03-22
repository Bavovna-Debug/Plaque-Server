/**************************************************************/
/*                                                            */
/*  Create role(s).                                           */
/*                                                            */
/**************************************************************/

CREATE ROLE guardian WITH
	LOGIN
	CONNECTION LIMIT 2020
	PASSWORD 'nVUcDYDVZCMaRdCfayWrG23w';

/**************************************************************/
/*                                                            */
/*  Define database and table spaces.                         */
/*                                                            */
/**************************************************************/

CREATE TABLESPACE guardian_root
	OWNER guardian
	LOCATION '/var/opt/database/guardian_root';

CREATE TABLESPACE guardian_pool
	OWNER guardian
	LOCATION '/var/opt/database/guardian_pool';

CREATE DATABASE guardian WITH
	OWNER guardian
	TEMPLATE template0
	ENCODING 'UTF8'
	LC_COLLATE 'de_DE.UTF-8'
	LC_CTYPE 'de_DE.UTF-8'
	TABLESPACE guardian_root;

/**************************************************************/
/*                                                            */
/*  Activate extensions.                                      */
/*                                                            */
/**************************************************************/

CREATE LANGUAGE plpgsql;
