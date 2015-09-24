/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.device_displacements
(
	device_id			BIGINT						NOT NULL,
	need_on_radar_revision      BOOLEAN             NOT NULL DEFAULT FALSE,
	need_in_sight_revision      BOOLEAN             NOT NULL DEFAULT FALSE,
	need_on_map_revision        BOOLEAN             NOT NULL DEFAULT FALSE,
	revised_on_radar_coordinate	EARTH				NULL DEFAULT NULL,
	revised_in_sight_coordinate	EARTH				NULL DEFAULT NULL,
	revised_on_map_coordinate	EARTH				NULL DEFAULT NULL,
	on_radar_coordinate	EARTH						NULL DEFAULT NULL,
	in_sight_coordinate	EARTH						NULL DEFAULT NULL,
	on_map_coordinate	EARTH						NULL DEFAULT NULL,
	on_radar_range      REAL                        NULL DEFAULT NULL,
	in_sight_range      REAL                        NULL DEFAULT NULL,
	on_map_range        REAL                        NULL DEFAULT NULL,

	CONSTRAINT sessions_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX device_displacements_device_key
	ON journal.device_displacements
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX device_displacements_need_on_radar_revision_key
	ON journal.device_displacements
	USING BTREE
	(need_on_radar_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX device_displacements_need_in_sight_revision_key
	ON journal.device_displacements
	USING BTREE
	(need_in_sight_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX device_displacements_need_on_map_revision_key
	ON journal.device_displacements
	USING BTREE
	(need_on_map_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS to_do_things_on_device_displacements_update
	ON journal.device_displacements RESTRICT;

CREATE OR REPLACE FUNCTION journal.to_do_things_on_device_displacements_update ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
    IF (OLD.on_radar_coordinate IS NULL) AND (NEW.on_radar_coordinate IS NOT NULL) THEN
        NEW.need_on_radar_revision = TRUE;
    ELSEIF EARTH_DISTANCE(NEW.on_radar_coordinate, NEW.revised_on_radar_coordinate) > (NEW.on_radar_range / 4) THEN
        NEW.need_on_radar_revision = TRUE;
        NEW.revised_on_radar_coordinate = NEW.on_radar_coordinate;
    END IF;

    IF OLD.in_sight_coordinate IS NULL THEN
        IF NEW.in_sight_coordinate IS NOT NULL THEN
            NEW.need_in_sight_revision = TRUE;
        END IF;
    ELSE
        IF EARTH_DISTANCE(NEW.in_sight_coordinate, NEW.revised_in_sight_coordinate) > (1000.0 / 1.609) THEN
            NEW.need_in_sight_revision = TRUE;
        END IF;
    END IF;

    IF (OLD.on_map_coordinate IS NULL) AND (NEW.on_map_coordinate IS NOT NULL) THEN
        NEW.need_on_map_revision = TRUE;
    ELSEIF EARTH_DISTANCE(NEW.on_map_coordinate, NEW.revised_on_map_coordinate) > (NEW.on_map_range / 10) THEN
        NEW.need_on_map_revision = TRUE;
        NEW.revised_on_map_coordinate = NEW.on_map_coordinate;
    END IF;

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER to_do_things_on_device_displacements_update
	BEFORE UPDATE
	ON journal.device_displacements
	FOR EACH ROW
	EXECUTE PROCEDURE journal.to_do_things_on_device_displacements_update();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE TABLE journal.displaced_devices
(
	device_id			BIGINT  					NOT NULL,

	CONSTRAINT displaced_devices_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX displaced_devices_device_key
	ON journal.displaced_devices
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE OR REPLACE RULE displaced_devices_ignore_duplicates AS
ON INSERT TO journal.displaced_devices
WHERE (EXISTS (
    SELECT 1
    FROM journal.displaced_devices
    WHERE device_id = NEW.device_id)
) DO INSTEAD NOTHING;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.revision_sessions_for_device_displacement_in_sight ()
RETURNS INTEGER AS
$PLSQL$

DECLARE
    var_processed_sessions  INTEGER;
    var_processed_plaques   INTEGER;
    var_device_id           BIGINT;
    var_session_id          BIGINT;
    var_in_sight_coordinate EARTH;
    var_in_sight_range      REAL;
    var_in_sight_revision   INTEGER;
    var_plaque_id           BIGINT;
    var_device_cube         CUBE;

BEGIN
    var_processed_sessions := 0;

	-- Go through displaced devices.
	--
	FOR var_device_id, var_in_sight_coordinate, var_in_sight_range IN
		UPDATE journal.device_displacements
		SET need_in_sight_revision = FALSE,
            revised_in_sight_coordinate = in_sight_coordinate
		WHERE need_in_sight_revision IS TRUE
        RETURNING device_id, in_sight_coordinate, in_sight_range
	LOOP
	    SELECT session_id, in_sight_revision
	    FROM journal.sessions
	    WHERE device_id = var_device_id
	    INTO var_session_id, var_in_sight_revision;

        IF FOUND THEN
            var_in_sight_revision := var_in_sight_revision + 1;

            var_processed_plaques := 0;

    	    var_device_cube := EARTH_BOX(var_in_sight_coordinate, var_in_sight_range / 1.609);

	        -- Go through plaques that are in the range for this device.
	        --
    	    FOR var_plaque_id IN
        		SELECT plaque_id
	        	FROM surrounding.plaques
		        WHERE coordinate <@ var_device_cube
    		LOOP
    		    BEGIN
                    INSERT INTO journal.session_in_sight_plaques (session_id, in_sight_revision, plaque_id)
                    VALUES (var_session_id, var_in_sight_revision, var_plaque_id);

                EXCEPTION
                    WHEN UNIQUE_VIOLATION THEN
                END;

                RAISE LOG 'SESSION:% PLAQUE:% APPEAERED', var_session_id, var_plaque_id;

                var_processed_plaques := var_processed_plaques + 1;
    	    END LOOP;

	        -- Go through plaques that are not in the range for this device any more.
	        --
    	    FOR var_plaque_id IN
        		SELECT plaque_id
	        	FROM journal.session_in_sight_plaques
	        	JOIN surrounding.plaques
	        	    USING (plaque_id)
		        WHERE session_id = var_session_id
    		      AND coordinate @> var_device_cube
	    	LOOP
                UPDATE journal.session_in_sight_plaques
                SET disappeared = TRUE,
                    in_sight_revision = var_in_sight_revision
                WHERE session_id = var_session_id
                  AND plaque_id = var_plaque_id;

        		RAISE LOG 'SESSION:% PLAQUE:% DISAPPEAERED', var_session_id, var_plaque_id;

                var_processed_plaques := var_processed_plaques + 1;
    	    END LOOP;

    	    IF var_processed_plaques > 0 THEN
    	        UPDATE journal.sessions
    	        SET in_sight_revised = TRUE,
    	            in_sight_revision = var_in_sight_revision
    	        WHERE session_id = var_session_id;

                INSERT INTO journal.revised_sessions (session_id)
                VALUES (var_session_id);

                var_processed_sessions := var_processed_sessions + 1;
    	    END IF;
    	END IF;
	END LOOP;

	RETURN var_processed_sessions;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.revision_sessions_for_device_displacement_on_map ()
RETURNS INTEGER AS
$PLSQL$

DECLARE
    var_processed_sessions  INTEGER;
    var_processed_plaques   INTEGER;
    var_device_id           BIGINT;
    var_session_id          BIGINT;
    var_on_map_coordinate   EARTH;
    var_on_map_range        REAL;
    var_on_map_revision     INTEGER;
    var_plaque_id           BIGINT;
    var_device_cube         CUBE;

BEGIN
    var_processed_sessions := 0;

	-- Go through displaced devices.
	--
	FOR var_device_id, var_on_map_coordinate, var_on_map_range IN
		UPDATE journal.device_displacements
		SET need_on_map_revision = FALSE,
            revised_on_map_coordinate = on_map_coordinate
		WHERE need_on_map_revision IS TRUE
        RETURNING device_id, on_map_coordinate, on_map_range
	LOOP
	    IF var_on_map_range > 50000.0 THEN
	        var_on_map_range := 50000.0;
	    END IF;

	    SELECT session_id, on_map_revision
	    FROM journal.sessions
	    WHERE device_id = var_device_id
	    INTO var_session_id, var_on_map_revision;

        IF FOUND THEN
            var_on_map_revision := var_on_map_revision + 1;

            var_processed_plaques := 0;

    	    var_device_cube := EARTH_BOX(var_on_map_coordinate, var_on_map_range / 1.609);

	        -- Go through plaques that are in the range for this device.
	        --
    	    FOR var_plaque_id IN
        		SELECT plaque_id
	        	FROM surrounding.plaques
		        WHERE coordinate <@ var_device_cube
    		LOOP
    		    BEGIN
                    INSERT INTO journal.session_on_map_plaques (session_id, on_map_revision, plaque_id)
                    VALUES (var_session_id, var_on_map_revision, var_plaque_id);

                EXCEPTION
                    WHEN UNIQUE_VIOLATION THEN
                END;

                RAISE LOG 'SESSION:% PLAQUE:% APPEAERED', var_session_id, var_plaque_id;

                var_processed_plaques := var_processed_plaques + 1;
    	    END LOOP;

	        -- Go through plaques that are not in the range for this device any more.
	        --
    	    FOR var_plaque_id IN
        		SELECT plaque_id
	        	FROM journal.session_on_map_plaques
	        	JOIN surrounding.plaques
	        	    USING (plaque_id)
		        WHERE session_id = var_session_id
    		      AND coordinate @> var_device_cube
	    	LOOP
                UPDATE journal.session_on_map_plaques
                SET disappeared = TRUE,
                    on_map_revision = var_on_map_revision
                WHERE session_id = var_session_id
                  AND plaque_id = var_plaque_id;

        		RAISE LOG 'SESSION:% PLAQUE:% DISAPPEAERED', var_session_id, var_plaque_id;

                var_processed_plaques := var_processed_plaques + 1;
    	    END LOOP;

    	    IF var_processed_plaques > 0 THEN
    	        UPDATE journal.sessions
    	        SET on_map_revised = TRUE,
    	            on_map_revision = var_on_map_revision
    	        WHERE session_id = var_session_id;

                INSERT INTO journal.revised_sessions (session_id)
                VALUES (var_session_id);

                var_processed_sessions := var_processed_sessions + 1;
    	    END IF;
    	END IF;
	END LOOP;

	RETURN var_processed_sessions;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
