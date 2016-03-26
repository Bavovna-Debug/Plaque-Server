/******************************************************************************/
/*                                                                            */
/*  TAN record.                                                               */
/*                                                                            */
/******************************************************************************/

CREATE TYPE auth.TAN AS
(
	tan_number			SMALLINT,
	tan_code			CHARACTER(8),
	used_stamp			TIMESTAMP WITHOUT TIME ZONE
);

/******************************************************************************/
/*                                                                            */
/*  TAN lists.                                                                */
/*                                                                            */
/******************************************************************************/

CREATE TABLE auth.tan_lists
(
	tan_list_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	tan_list_id			BIGSERIAL					NOT NULL,
	tan_list_token		UUID						NOT NULL DEFAULT hash.peek_token('TANS'),
	device_id			BIGINT						NOT NULL,
	sent				BOOLEAN						NOT NULL DEFAULT FALSE,
	confirmed			BOOLEAN						NOT NULL DEFAULT FALSE,
	valid				BOOLEAN						NOT NULL DEFAULT FALSE,
	tans				auth.TAN ARRAY[100]			NOT NULL,

	CONSTRAINT tan_lists_primary_key
		PRIMARY KEY (tan_list_id)
		USING INDEX TABLESPACE plaque_auth,

	CONSTRAINT tan_lists_token_foreign_key
		FOREIGN KEY (tan_list_token)
		REFERENCES operator.tokens (token)
		ON UPDATE CASCADE
		ON DELETE RESTRICT,

	CONSTRAINT tan_lists_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE CASCADE
		ON DELETE RESTRICT
)
TABLESPACE plaque_auth;

CREATE UNIQUE INDEX tan_lists_token_key
	ON auth.tan_lists
	USING BTREE
	(tan_list_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE plaque_auth;

CREATE INDEX tan_lists_device_key
	ON auth.tan_lists
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE plaque_auth;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS tan_lists_drop_token_on_delete
	ON auth.tan_lists RESTRICT;

CREATE OR REPLACE FUNCTION auth.tan_lists_drop_token_on_delete ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	PERFORM operator.poke_token(OLD.tan_list_token);

	RETURN OLD;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER tan_lists_drop_token_on_delete
	AFTER DELETE
	ON auth.tan_lists
	FOR EACH ROW
	EXECUTE PROCEDURE auth.tan_lists_drop_token_on_delete();

/******************************************************************************/
/*                                                                            */
/*  Create new TAN list for specified device. After creation TAN list         */
/*  must be activated. It cannot be used until it is sent to device           */
/*  and confirmed by device.                                                  */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.create_tan_list (IN parm_device_id BIGINT)
RETURNS BIGINT AS
$PLSQL$

DECLARE
	var_tan_list_id		BIGINT;
	var_tans			auth.TAN ARRAY[100] DEFAULT '{}';
	tan_number			SMALLINT DEFAULT 1;
	tan_code			CHARACTER(8);

BEGIN
	var_hash_code := TO_CHAR(CURRENT_TIMESTAMP, 'YYYYMMDDHHMISSUS');

	<<generate_tan>>
	LOOP
		tan_code := UPPER(SUBSTRING(CAST(UUID_GENERATE_V4() AS TEXT) FOR 8));

		var_tans := ARRAY_APPEND(var_tans, ROW(tan_number, tan_code, NULL)::auth.TAN);

		EXIT generate_tan
		WHEN ARRAY_LENGTH(var_tans, 1) = 100;

		tan_number := tan_number + 1;
	END LOOP;

	BEGIN
		INSERT INTO auth.tan_lists (device_id, tans)
		VALUES (parm_device_id, var_tans)
		RETURNING tan_list_id
		INTO var_tan_list_id;

	EXCEPTION
		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.cancel_request('Cannot create new TAN list.');
			RETURN NULL;
	END;

	RETURN var_tan_list_id;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Check how many TAN numbers in TAN list are not used.                      */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.number_of_free_tans (IN parm_tan_list_id BIGINT)
RETURNS INTEGER AS
$PLSQL$

DECLARE
	var_tans			auth.TAN ARRAY[100];
	number_of_free_tans	INTEGER;

BEGIN
	SELECT tans
	FROM auth.tan_lists
	WHERE tan_list_id = var_tan_list_id
	INTO var_tans;

	IF NOT FOUND THEN
		PERFORM reception.cancel_request('Cannot find TAN list.');
		RETURN NULL;
	END IF;

	SELECT COUNT(*)
	FROM UNNEST(var_tans)
	WHERE used_stamp IS NULL
	INTO number_of_free_tans;

	RETURN number_of_free_tans;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Flag TAN list as sent to device.                                          */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.flag_tan_list_as_sent (IN parm_tan_list_id BIGINT)
RETURNS BOOLEAN AS
$PLSQL$

BEGIN
	BEGIN
		UPDATE auth.tan_lists
		SET sent = TRUE
		WHERE tan_list_id = parm_tan_list_id
		  AND sent IS FALSE
		  AND confirmed IS FALSE
		  AND valid IS FALSE;

		IF NOT FOUND THEN
			RAISE EXCEPTION NO_DATA_FOUND;
		END IF;

	EXCEPTION
		WHEN NO_DATA_FOUND THEN
			PERFORM reception.cancel_request('No suitable TAN list found to be flagged as sent.');
			RETURN FALSE;

		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.cancel_request('Cannot flag TAN list as sent.');
			RETURN FALSE;
	END;

	RETURN TRUE;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Flag TAN list as confirmed by device and activate it.                     */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.flag_tan_list_as_confirmed (IN parm_tan_list_id BIGINT)
RETURNS BOOLEAN AS
$PLSQL$

BEGIN
	BEGIN
		UPDATE auth.tan_lists
		SET confirmed = TRUE,
			valid = TRUE
		WHERE tan_list_id = parm_tan_list_id
		  AND sent IS TRUE
		  AND confirmed IS FALSE
		  AND valid IS FALSE;

		IF NOT FOUND THEN
			RAISE EXCEPTION NO_DATA_FOUND;
		END IF;

	EXCEPTION
		WHEN NO_DATA_FOUND THEN
			PERFORM reception.cancel_request('No suitable TAN list found to be flagged as confirmed.');
			RETURN FALSE;

		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.cancel_request('Cannot flag TAN list as confirmed.');
			RETURN FALSE;
	END;

	RETURN TRUE;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Check TAN number for specified device. Quit if:                           */
/*    - there is no valid TAN list for specified device;                      */
/*    - specified TAN is flagged as already used;                             */
/*    - TAN with specified number does not exist;                             */
/*    - provided code for specified TAN is wrong.                             */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.validate_tan (
	IN parm_device_id	BIGINT,
	IN parm_tan_number	SMALLINT,
	IN parm_tan_code	CHARACTER(8)
)
RETURNS BOOLEAN AS
$PLSQL$

DECLARE
	var_tan_list_ids	BIGINT ARRAY;
	var_tan_list_id		BIGINT;
	var_tans			auth.TAN ARRAY[100];
	old_tan				auth.TAN;
	new_tan				auth.TAN;

BEGIN
	SELECT ARRAY(
		SELECT tan_list_id
		FROM auth.tan_lists
		WHERE device_id = parm_device_id
	  	  AND valid IS TRUE
	)
	INTO var_tan_list_ids;

	IF ARRAY_LENGTH(var_tan_list_ids, 1) IS NULL THEN
		PERFORM reception.cancel_request('No valid TAN list found.');
		RETURN FALSE;
	END IF;

	IF ARRAY_LENGTH(var_tan_list_ids, 1) != 1 THEN
		PERFORM reception.cancel_request('Multiple valid TAN lists.');
		RETURN FALSE;
	END IF;

	var_tan_list_id := var_tan_list_ids[1];

	SELECT tans
	FROM auth.tan_lists
	WHERE tan_list_id = var_tan_list_id
	INTO var_tans;

	IF NOT FOUND THEN
		PERFORM reception.cancel_request('Cannot load TANs.');
		RETURN FALSE;
	END IF;

	SELECT *
	FROM UNNEST(var_tans)
	WHERE tan_number = parm_tan_number
	INTO old_tan;

	IF NOT FOUND THEN
		PERFORM reception.cancel_request('TAN with specified number does not exist.');
		RETURN FALSE;
	END IF;

	IF parm_tan_code != old_tan.tan_code THEN
		PERFORM reception.cancel_request('TAN code mismatch.');
		RETURN FALSE;
	END IF;

	IF old_tan.used_stamp IS NOT NULL THEN
		PERFORM reception.cancel_request('TAN already used.');
		RETURN FALSE;
	END IF;

	-- Flag this TAN as used.
	--
	new_tan := old_tan;
	new_tan.used_stamp := CURRENT_TIMESTAMP;

	BEGIN
		var_tans := ARRAY_REPLACE(var_tans, old_tan, new_tan);

		UPDATE auth.tan_lists
		SET tans = var_tans
		WHERE tan_list_id = var_tan_list_id;

		IF NOT FOUND THEN
			RAISE EXCEPTION NO_DATA_FOUND;
		END IF;

	EXCEPTION
		WHEN NO_DATA_FOUND THEN
			PERFORM reception.cancel_request('Cannot find TAN list to update.');
			RETURN FALSE;

		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.cancel_request('Cannot update TAN list.');
			RETURN FALSE;
	END;

	RETURN TRUE;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
