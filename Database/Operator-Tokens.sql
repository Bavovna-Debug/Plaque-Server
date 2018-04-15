/******************************************************************************/
/*                                                                            */
/*  List of hash codes for all types of objects.                              */
/*                                                                            */
/******************************************************************************/

CREATE TABLE operator.tokens
(
	token_stamp			TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	token				UUID						NOT NULL,
	proprietor			CHARACTER(4)				NOT NULL,
	retries				INTEGER						NOT NULL,
	in_use				BOOLEAN						NOT NULL DEFAULT TRUE,

	CONSTRAINT tokens_primary_key
		PRIMARY KEY (token)
		USING INDEX TABLESPACE vp_operator
)
TABLESPACE vp_operator;

/******************************************************************************/
/*                                                                            */
/*  Create a new hash entry. Try several times if necessary.                  */
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION hash.allocate_hash_code (IN parm_proprietor hash.HASHOWNER)
RETURNS BIGINT AS
$PLSQL$

DECLARE
	var_hash_id		BIGINT DEFAULT NULL;
	var_hash_code	CHARACTER(32);

	try_number		INTEGER DEFAULT 0;
	max_retries		CONSTANT INTEGER := 32;

BEGIN
	var_hash_code := TO_CHAR(CURRENT_TIMESTAMP, 'YYYYMMDDHHMISSUS');

	<<try_hash_code>>
	LOOP
		try_number := try_number + 1;
		IF (try_number = max_retries) THEN
			RAISE NOTICE 'Cannot create unique hash code after % retries.',
				try_number;
			RETURN NULL;
		END IF;

		var_hash_code := MD5(var_hash_code);
		var_hash_code := UPPER(var_hash_code);

		BEGIN
			INSERT INTO hash.repository (hash_code, proprietor)
			VALUES (var_hash_code, parm_proprietor)
			RETURNING hash_id
			INTO var_hash_id;

		EXCEPTION
			WHEN UNIQUE_VIOLATION THEN
				RAISE DEBUG 'Cannot create unique hash code. Retrying.';
				var_hash_code := CONCAT(
					SUBSTRING(var_hash_code FOR 1),
					SUBSTRING(var_hash_code FROM 2));
				CONTINUE try_hash_code;

			WHEN OTHERS THEN
				RAISE NOTICE '%', SQLSTATE;
				PERFORM reception.cancel_request('Cannot create hash code.');
				RETURN NULL;
		END;

		EXIT try_hash_code
		WHEN var_hash_id IS NOT NULL;
	END LOOP;

	RETURN var_hash_id;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/

/******************************************************************************/
/*                                                                            */
/*  Procedure to be called by triggers for tables                             */
/*  that are referencing the hash.repository.                                 */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION operator.peek_token (IN parm_proprietor CHARACTER(4))
RETURNS UUID AS
$PLSQL$

DECLARE
	try_number		INTEGER DEFAULT 0;
	max_retries		CONSTANT INTEGER := 100;
	var_token		UUID;

BEGIN
	<<try_associate_hash>>
	LOOP
		var_token := UUID_GENERATE_V4();

		BEGIN
			INSERT INTO operator.tokens (token, proprietor, retries)
			VALUES (var_token, parm_proprietor, try_number);

		EXCEPTION
			WHEN UNIQUE_VIOLATION THEN
				try_number := try_number + 1;
				IF (try_number < max_retries) THEN
					RAISE NOTICE 'Cannot peek token. Retrying.';
					CONTINUE;
				ELSE
					RAISE NOTICE 'Cannot peek token after % retries.', try_number;
					RETURN NULL;
				END IF;

			WHEN OTHERS THEN
				RAISE EXCEPTION 'Cannot peek token: % (%)', SQLSTATE, SQLERRM;
		END;

		EXIT try_associate_hash
		WHEN var_token IS NOT NULL;
	END LOOP;

	RETURN var_token;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION operator.poke_token (IN parm_token UUID)
RETURNS VOID AS
$PLSQL$

BEGIN
	BEGIN
		UPDATE operator.tokens
		SET in_use = FALSE
		WHERE token = parm_token;

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot poke token: % (%)', SQLSTATE, SQLERRM;
	END;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
