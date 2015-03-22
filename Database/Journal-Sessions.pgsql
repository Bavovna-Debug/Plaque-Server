/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.sessions
(
	session_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	session_id			BIGSERIAL					NOT NULL,
	session_token		UUID						NOT NULL DEFAULT operator.peek_token('SION'),
	device_id			BIGINT						NOT NULL,
	in_sight_revision	INTEGER						NOT NULL DEFAULT 0,
	in_cache_revision	INTEGER						NOT NULL DEFAULT 0,
	satellite_id        BIGINT						NULL DEFAULT NULL,

	CONSTRAINT sessions_primary_key
		PRIMARY KEY (session_id)
		USING INDEX TABLESPACE vp_journal,

	CONSTRAINT sessions_token_foreign_key
		FOREIGN KEY (session_token)
		REFERENCES operator.tokens (token)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT sessions_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT sessions_satellite_foreign_key
		FOREIGN KEY (satellite_id)
		REFERENCES operator.satellites (satellite_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX sessions_token_key
	ON journal.sessions
	USING BTREE
	(session_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE UNIQUE INDEX sessions_device_key
	ON journal.sessions
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS sessions_drop_token_on_delete
	ON journal.sessions RESTRICT;

CREATE OR REPLACE FUNCTION journal.sessions_drop_token_on_delete ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	PERFORM operator.poke_token(OLD.session_token);

	RETURN OLD;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER sessions_drop_token_on_delete
	AFTER DELETE
	ON journal.sessions
	FOR EACH ROW
	EXECUTE PROCEDURE journal.sessions_drop_token_on_delete();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TYPE journal.session AS
(
    session_id          BIGINT,
	session_token		UUID
);

CREATE OR REPLACE FUNCTION journal.get_session (
    IN parm_device_id       BIGINT,
    IN parm_session_token   UUID
)
RETURNS journal.session AS
$PLSQL$

DECLARE
	var_session_id  	BIGINT;
	var_session_token	UUID;

BEGIN
    SELECT session_id, session_token
    FROM journal.sessions
    WHERE device_id = parm_device_id
    INTO var_session_id, var_session_token;

    IF NOT FOUND THEN
	    BEGIN
			INSERT INTO journal.sessions (device_id)
			VALUES (parm_device_id)
			RETURNING session_id, session_token
			INTO var_session_id, var_session_token;

		EXCEPTION
			WHEN OTHERS THEN
				RAISE EXCEPTION 'Cannot create session: % (%)', SQLSTATE, SQLERRM;
		END;
--	ELSE
--DELETE FROM journal.session_plaques
--WHERE session_id = var_session_id;
	END IF;

	RETURN (var_session_id, var_session_token);
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
