/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.sessions
(
	session_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	session_id			BIGSERIAL					NOT NULL,
	session_token		UUID						NOT NULL DEFAULT operator.peek_token('SION'),
	device_id			BIGINT						NOT NULL,
	in_cache_revision	INTEGER						NOT NULL DEFAULT 0,
	on_radar_revision	INTEGER						NOT NULL DEFAULT 0,
	in_sight_revision	INTEGER						NOT NULL DEFAULT 0,
	on_map_revision	    INTEGER						NOT NULL DEFAULT 0,
	on_radar_revised    BOOLEAN                     NOT NULL DEFAULT TRUE,
	in_sight_revised    BOOLEAN                     NOT NULL DEFAULT TRUE,
	on_map_revised      BOOLEAN                     NOT NULL DEFAULT TRUE,
	on_radar_coordinate	EARTH						NULL DEFAULT NULL,
	in_sight_coordinate	EARTH						NULL DEFAULT NULL,
	on_radar_range      REAL                        NOT NULL DEFAULT 200000.0,
	in_sight_range      REAL                        NOT NULL DEFAULT 20000.0,
	satellite_id        BIGINT						NULL DEFAULT NULL,
	satellite_task_id   INTEGER                     NULL DEFAULT NULL,

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
		ON DELETE CASCADE,

	CONSTRAINT sessions_satellite_foreign_key
		FOREIGN KEY (satellite_id)
		REFERENCES operator.satellites (satellite_id)
		ON UPDATE NO ACTION
		ON DELETE SET NULL
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

CREATE INDEX sessions_on_radar_coordinate_key
	ON journal.sessions
	USING GiST
	(on_radar_coordinate)
	TABLESPACE vp_journal;

CREATE INDEX sessions_on_radar_coordinate_and_range_key
	ON journal.sessions
	USING BTREE
	(on_radar_coordinate, on_radar_range)
	TABLESPACE vp_journal;

CREATE INDEX sessions_in_sight_coordinate_key
	ON journal.sessions
	USING GiST
	(in_sight_coordinate)
	TABLESPACE vp_journal;

CREATE INDEX sessions_in_sight_coordinate_and_range_key
	ON journal.sessions
	USING BTREE
	(in_sight_coordinate, in_sight_range)
	TABLESPACE vp_journal;

CREATE INDEX sessions_in_sight_satellite_task_key
	ON journal.sessions
	USING BTREE
	(satellite_task_id)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.sessions_for_revision
(
	session_id			BIGINT  					NOT NULL,

	CONSTRAINT sessions_for_revision_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX sessions_for_revision_session_id_key
	ON journal.sessions_for_revision
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE OR REPLACE RULE sessions_for_revision_ignore_duplicates AS
ON INSERT TO journal.sessions_for_revision
WHERE (EXISTS (
    SELECT 1
    FROM journal.sessions_for_revision
    WHERE session_id = NEW.session_id)
) DO INSTEAD NOTHING;

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

DROP TRIGGER IF EXISTS to_do_things_on_session_update
	ON journal.sessions RESTRICT;

CREATE OR REPLACE FUNCTION journal.to_do_things_on_session_update ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	IF (OLD.satellite_task_id IS NULL) AND (NEW.satellite_task_id IS NOT NULL) THEN
    	INSERT INTO journal.sessions_online (session_id, online_since)
	    VALUES (NEW.session_id, NOW());
	ELSEIF (OLD.satellite_task_id IS NOT NULL) AND (NEW.satellite_task_id IS NULL) THEN
    	UPDATE journal.sessions_online
    	SET offline_since = NOW()
    	WHERE session_id = NEW.session_id
    	  AND offline_since IS NULL;
	END IF;

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER to_do_things_on_session_update
	BEFORE UPDATE
	ON journal.sessions
	FOR EACH ROW
	EXECUTE PROCEDURE journal.to_do_things_on_session_update();

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
