/**************************************************************/
/*                                                            */
/*  Define database permissions.                              */
/*                                                            */
/**************************************************************/

REVOKE CONNECT
	ON DATABASE vp
	FROM PUBLIC;

GRANT CONNECT
	ON DATABASE vp
	TO vp;

/**************************************************************/
/*                                                            */
/*  Define name spaces.                                       */
/*                                                            */
/**************************************************************/

CREATE SCHEMA auth
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA auth
	FROM PUBLIC;

CREATE SCHEMA debug
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA debug
	FROM PUBLIC;

CREATE SCHEMA journal
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA journal
	FROM PUBLIC;

CREATE SCHEMA operator
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA operator
	FROM PUBLIC;

CREATE SCHEMA reception
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA reception
	FROM PUBLIC;

CREATE SCHEMA surrounding
	AUTHORIZATION vp;

REVOKE ALL PRIVILEGES
	ON SCHEMA surrounding
	FROM PUBLIC;

/**************************************************************/
/*                                                            */
/*  Set search path.                                          */
/*                                                            */
/**************************************************************/

ALTER USER vp
	SET search_path
	TO public, auth, debug, journal, operator, reception, surrounding;
