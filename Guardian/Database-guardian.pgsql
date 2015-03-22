/**************************************************************/
/*                                                            */
/*  Define database permissions.                              */
/*                                                            */
/**************************************************************/

REVOKE CONNECT
	ON DATABASE guardian
	FROM PUBLIC;

GRANT CONNECT
	ON DATABASE guardian
	TO guardian;

/**************************************************************/
/*                                                            */
/*  Define name spaces.                                       */
/*                                                            */
/**************************************************************/

CREATE SCHEMA pool
	AUTHORIZATION guardian;

REVOKE ALL PRIVILEGES
	ON SCHEMA pool
	FROM PUBLIC;

/**************************************************************/
/*                                                            */
/*  Set search path.                                          */
/*                                                            */
/**************************************************************/

ALTER USER guardian
	SET search_path
	TO public, pool;
