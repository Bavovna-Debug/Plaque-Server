/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.session_in_cache_plaques
(
	session_id			BIGINT  					NOT NULL,
	in_cache_revision   INTEGER                     NOT NULL,
	plaque_id			BIGINT						NOT NULL,

	CONSTRAINT session_in_cache_plaques_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE,

	CONSTRAINT session_in_cache_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_in_cache_revision_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id, in_cache_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_plaque_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id, plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.session_in_sight_plaques
(
	session_id			BIGINT  					NOT NULL,
	in_sight_revision   INTEGER                     NOT NULL,
	plaque_id			BIGINT						NOT NULL,
	disappeared         BOOLEAN                     NOT NULL DEFAULT FALSE,

	CONSTRAINT session_in_sight_plaques_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE,

	CONSTRAINT session_in_sight_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE INDEX session_in_sight_plaques_session_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_sight_plaques_session_in_sight_revision_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id, in_sight_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE UNIQUE INDEX session_in_sight_plaques_session_plaque_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id, plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.session_on_radar_plaques
(
	session_id			BIGINT  					NOT NULL,
	on_radar_revision   INTEGER                     NOT NULL,
	plaque_id			BIGINT						NOT NULL,
	disappeared         BOOLEAN                     NOT NULL DEFAULT FALSE,

	CONSTRAINT session_on_radar_plaques_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE,

	CONSTRAINT session_on_radar_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE INDEX session_on_radar_plaques_session_key
	ON journal.session_on_radar_plaques
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_on_radar_plaques_session_in_cache_revision_key
	ON journal.session_on_radar_plaques
	USING BTREE
	(session_id, on_radar_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_on_radar_plaques_session_plaque_key
	ON journal.session_on_radar_plaques
	USING BTREE
	(session_id, plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.revised_sessions
(
	session_id			BIGINT  					NOT NULL,

	CONSTRAINT revised_sessions_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX revised_sessions_session_key
	ON journal.revised_sessions
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE OR REPLACE RULE revised_sessions_ignore_duplicates AS
ON INSERT TO journal.revised_sessions
WHERE (EXISTS (
    SELECT 1
    FROM journal.revised_sessions
    WHERE session_id = NEW.session_id)
) DO INSTEAD NOTHING;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.modified_plaques
(
	plaque_id			BIGINT						NOT NULL,
/*
    event_stamp 		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
    recognized_stamp 	TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,
    broadcasted_stamp 	TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,
*/
	CONSTRAINT modified_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX modified_plaques_plaque_key
	ON journal.modified_plaques
	USING BTREE
	(plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE OR REPLACE RULE modified_plaques_ignore_duplicates AS
ON INSERT TO journal.modified_plaques
WHERE (EXISTS (
    SELECT 1
    FROM journal.modified_plaques
    WHERE plaque_id = NEW.plaque_id)
) DO INSTEAD NOTHING;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.moved_plaques
(
	plaque_id			BIGINT						NOT NULL,

	CONSTRAINT moved_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX moved_plaques_plaque_key
	ON journal.moved_plaques
	USING BTREE
	(plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE OR REPLACE RULE moved_plaques_ignore_duplicates AS
ON INSERT TO journal.moved_plaques
WHERE (EXISTS (
    SELECT 1
    FROM journal.moved_plaques
    WHERE plaque_id = NEW.plaque_id)
) DO INSTEAD NOTHING;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.revision_sessions_for_modified_plaques ()
RETURNS INTEGER AS
$PLSQL$

DECLARE
    var_processed_sessions  INTEGER;
    var_plaque_id           BIGINT;
    var_session_id          BIGINT;
    var_in_sight_revision   INTEGER;
    var_on_radar_revision   INTEGER;

BEGIN
    var_processed_sessions := 0;

	-- Go through appeared plaques.
	--
	FOR var_plaque_id IN
		DELETE FROM journal.modified_plaques
        RETURNING plaque_id
	LOOP
	    -- Process "in sight"
	    --
	    FOR var_session_id IN
	        SELECT DISTINCT session_id
            FROM journal.session_in_sight_plaques
            WHERE plaque_id = var_plaque_id
        LOOP
            UPDATE journal.sessions
            SET in_sight_revised = TRUE,
                in_sight_revision = in_sight_revision + 1
            WHERE session_id = var_session_id
            RETURNING in_sight_revision
            INTO var_in_sight_revision;

            UPDATE journal.session_in_sight_plaques
            SET in_sight_revision = var_in_sight_revision
            WHERE session_id = var_session_id
              AND plaque_id = var_plaque_id;

            INSERT INTO journal.revised_sessions (session_id)
            VALUES (var_session_id);

    		RAISE LOG 'Plaque has changed in sight: session=% plaqueId=%',
    		    var_session_id, var_plaque_id;

    		var_processed_sessions := var_processed_sessions + 1;
	    END LOOP;

        -- Process "on radar"
        --
	    FOR var_session_id IN
	        SELECT DISTINCT session_id
            FROM journal.session_on_radar_plaques
            WHERE plaque_id = var_plaque_id
        LOOP
            UPDATE journal.sessions
            SET on_radar_revised = TRUE,
                on_radar_revision = on_radar_revision + 1
            WHERE session_id = var_session_id
            RETURNING on_radar_revision
            INTO var_on_radar_revision;

            UPDATE journal.session_on_radar_plaques
            SET on_radar_revision = var_on_radar_revision
            WHERE session_id = var_session_id
              AND plaque_id = var_plaque_id;

            INSERT INTO journal.revised_sessions (session_id)
            VALUES (var_session_id);

    		RAISE LOG 'Plaque has changed on radar: session=% plaqueId=%',
    		    var_session_id, var_plaque_id;

    		var_processed_sessions := var_processed_sessions + 1;
	    END LOOP;
	END LOOP;

	RETURN var_processed_sessions;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
