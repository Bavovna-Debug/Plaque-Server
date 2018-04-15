/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TYPE general.KINDOFSERVER AS ENUM
(
	'GENERAL',
	'QUERY'
);

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE general.servers
(
	server_id			BIGSERIAL					NOT NULL,
	server_type			general.KINDOFSERVER		NOT NULL,
	active				BOOLEAN						NOT NULL,
	ip_address			INET						NOT NULL,
	port_number			INTEGER						NOT NULL,
	priority			INTEGER						NOT NULL,
	available_from		TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,
	available_till		TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,

	CONSTRAINT servers_primary_key
		PRIMARY KEY (server_id)
		USING INDEX TABLESPACE plaque_general,

	CONSTRAINT servers_unique
		UNIQUE (ip_address, port_number)
		USING INDEX TABLESPACE plaque_general
)
TABLESPACE plaque_general;

/******************************************************************************/
/*                                                                            */
/*  Create an XML document describing all available servers.                  */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION hash.associate_hash (IN parm_proprietor CHARACTER(4))
RETURNS UUID AS
$PLSQL$

DECLARE
	try_number		INTEGER DEFAULT 0;
	max_retries		CONSTANT INTEGER := 100;
	var_hash_code	UUID;

BEGIN
	<<try_associate_hash>>
	LOOP
		var_hash_code := UUID_GENERATE_V4();

		BEGIN
			INSERT INTO hash.repository (hash_code, proprietor, retries)
			VALUES (var_hash_code, parm_proprietor, try_number);

		EXCEPTION
			WHEN UNIQUE_VIOLATION THEN
				try_number := try_number + 1;
				IF (try_number < max_retries) THEN
					RAISE NOTICE 'Cannot create hash code. Retrying.';
					CONTINUE;
				ELSE
					RAISE NOTICE 'Cannot create hash code after % retries.', try_number;
					RETURN NULL;
				END IF;

			WHEN OTHERS THEN
				RAISE NOTICE '%', SQLSTATE;
				PERFORM reception.cancel_request('Cannot associate hash code.');
				RETURN NULL;
		END;

		EXIT try_associate_hash
		WHEN var_hash_code IS NOT NULL;
	END LOOP;

	RETURN var_hash_code;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
