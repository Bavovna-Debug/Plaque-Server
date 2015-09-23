/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TYPE surrounding.plaque_on_radar AS
(
	plaque_token		UUID,
	plaque_revision		INTEGER
);

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION surrounding.query_plaques_in_sight (
	IN parm_session_id			BIGINT,
	IN parm_in_sight_revision	INTEGER,
	IN parm_latitude			DOUBLE PRECISION,
	IN parm_longitude			DOUBLE PRECISION,
	IN parm_range				REAL
)
RETURNS SETOF surrounding.plaque_on_radar AS
$PLSQL$

DECLARE
	var_next_in_sight_revision	INTEGER;
	var_plaque_id				BIGINT;
	var_plaque_token			UUID;
	var_plaque_revision			INTEGER;
	var_range					REAL;

BEGIN
	RAISE NOTICE 'Query plaques in sight for % x % last revision %',
		parm_latitude,
		parm_longitude,
		parm_in_sight_revision;

	IF (parm_latitude = 0.0) OR (parm_longitude = 0.0) THEN
		RETURN;
	END IF;

	-- Get revision number for this run.
	--
	SELECT in_sight_revision, in_sight_range
	FROM journal.sessions
	WHERE session_id = parm_session_id
	INTO var_next_in_sight_revision, var_range;

	IF parm_range != 0.0 THEN
		IF parm_range > 20000.0 THEN
			var_range = 20000.0;
		ELSE
			var_range = parm_range;
		END IF;
	END IF;

	-- Clear session journal from disappeared plaques those disappearance has being confirmed by device.
	--
	DELETE FROM journal.session_in_sight_plaques
	WHERE session_id = parm_session_id
	  AND in_sight_revision <= parm_in_sight_revision
	  AND plaque_revision = -1;

	-- Prepare temporary tables.
	--
	CREATE TEMPORARY TABLE known_plaques
	(
		plaque_id			BIGINT		NOT NULL,
		plaque_revision		INTEGER		NOT NULL,
		revision_changed	BOOLEAN		NOT NULL DEFAULT FALSE
	)
	ON COMMIT DROP;

	CREATE TEMPORARY TABLE appeared_plaques
	(
		plaque_id			BIGINT		NOT NULL,
		plaque_revision		INTEGER		NOT NULL
	)
	ON COMMIT DROP;

	-- Fill temporary table with plaques already known to device.
	--
	INSERT INTO known_plaques (plaque_id, plaque_revision)
	SELECT plaque_id, plaque_revision
	FROM journal.session_in_sight_plaques
	WHERE session_id = parm_session_id;

	-- Fetch a list of plaques around device.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM surrounding.plaques
		WHERE coordinate <@ EARTH_BOX(LL_TO_EARTH(parm_latitude, parm_longitude), parm_range / 1.609)
	LOOP
		-- Check whether plaque with such id and revision is already known to device.
		--
		DELETE FROM known_plaques
		WHERE plaque_id = var_plaque_id
		  AND plaque_revision = var_plaque_revision;

		-- If plaque with such id and revision is not found in a list of known plaques then
		-- either it is not known to device yet or its revision did change.
		--
		IF NOT FOUND THEN
			--
			-- Check whether this plaque is known to device.
			--
			UPDATE known_plaques
			SET revision_changed = TRUE,
				plaque_revision = var_plaque_revision
			WHERE plaque_id = var_plaque_id;

			-- If plaque with such id is not found in a list of known plaques then
			-- this plaque is not known to device yet.
			--
			IF NOT FOUND THEN
				--
				-- Add it to a list of appeared plaques.
				--
				INSERT INTO appeared_plaques (plaque_id, plaque_revision)
				VALUES (var_plaque_id, var_plaque_revision);
			END IF;
		END IF;
	END LOOP;

	-- At this point all plaques in a list of known plaques that are not flagged as "revision changed"
	-- are those that did disappear from in sight.

	-- Go through disappeared plaques.
	--
	FOR var_plaque_id IN
		SELECT plaque_id
		FROM known_plaques
		WHERE revision_changed IS FALSE
	LOOP
		-- Mark this plaque in session journal as disappeared.
		--
		UPDATE journal.session_in_sight_plaques
		SET in_sight_revision = var_next_in_sight_revision,
			plaque_revision = -1
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DISAPPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through appeared plaques.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM appeared_plaques
	LOOP
		-- Add this plaque to session journal.
		--
		INSERT INTO journal.session_in_sight_plaques (session_id, in_sight_revision, plaque_id, plaque_revision)
		VALUES (parm_session_id, var_next_in_sight_revision, var_plaque_id, var_plaque_revision);

		RAISE NOTICE 'SESSION:% PLAQUE:% APPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through changed plaques.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM known_plaques
		WHERE revision_changed IS TRUE
	LOOP
		-- Update revision of this plaque in session journal.
		--
		UPDATE journal.session_in_sight_plaques
		SET in_sight_revision = var_next_in_sight_revision,
			plaque_revision = var_plaque_revision
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DID CHANGE', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through appeared plaques.
	--
	FOR var_plaque_token, var_plaque_revision IN
		SELECT plaques.plaque_token, radar.plaque_revision
		FROM journal.session_in_sight_plaques AS radar
		JOIN surrounding.plaques AS plaques
			USING (plaque_id)
		WHERE in_sight_revision >= parm_in_sight_revision
	LOOP
		RETURN NEXT (var_plaque_token, var_plaque_revision);

		RAISE NOTICE 'SESSION:% PLAQUE:% REVISION:%', parm_session_id, var_plaque_token, var_plaque_revision;
	END LOOP;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION surrounding.query_plaques_on_radar (
	IN parm_session_id			BIGINT,
	IN parm_on_radar_revision	INTEGER,
	IN parm_latitude			DOUBLE PRECISION,
	IN parm_longitude			DOUBLE PRECISION,
	IN parm_range				REAL
)
RETURNS SETOF surrounding.plaque_on_radar AS
$PLSQL$

DECLARE
	var_next_on_radar_revision	INTEGER;
	var_plaque_id				BIGINT;
	var_plaque_token			UUID;
	var_plaque_revision			INTEGER;
	var_range					REAL;

BEGIN
	IF (parm_latitude = 0.0) OR (parm_longitude = 0.0) THEN
		RETURN;
	END IF;

	-- Get revision number for this run.
	--
	SELECT on_radar_revision, on_radar_range
	FROM journal.sessions
	WHERE session_id = parm_session_id
	INTO var_next_on_radar_revision, var_range;

	IF parm_range != 0.0 THEN
		IF parm_range > 200000.0 THEN
			var_range = 200000.0;
		ELSE
			var_range = parm_range;
		END IF;
	END IF;

	-- Clear session journal from disappeared plaques those disappearance has being confirmed by device.
	--
	DELETE FROM journal.session_on_radar_plaques
	WHERE session_id = parm_session_id
	  AND on_radar_revision <= parm_on_radar_revision
	  AND plaque_revision = -1;

	-- Prepare temporary tables.
	--
	CREATE TEMPORARY TABLE known_plaques
	(
		plaque_id			BIGINT		NOT NULL,
		plaque_revision		INTEGER		NOT NULL,
		revision_changed	BOOLEAN		NOT NULL DEFAULT FALSE
	)
	ON COMMIT DROP;

	CREATE TEMPORARY TABLE appeared_plaques
	(
		plaque_id			BIGINT		NOT NULL,
		plaque_revision		INTEGER		NOT NULL
	)
	ON COMMIT DROP;

	-- Fill temporary table with plaques already known to device.
	--
	INSERT INTO known_plaques (plaque_id, plaque_revision)
	SELECT plaque_id, plaque_revision
	FROM journal.session_on_radar_plaques
	WHERE session_id = parm_session_id;

	-- Fetch a list of plaques around device.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM surrounding.plaques
		WHERE coordinate <@ EARTH_BOX(LL_TO_EARTH(parm_latitude, parm_longitude), parm_range / 1.609)
	LOOP
		-- Check whether plaque with such id and revision is already known to device.
		--
		DELETE FROM known_plaques
		WHERE plaque_id = var_plaque_id
		  AND plaque_revision = var_plaque_revision;

		-- If plaque with such id and revision is not found in a list of known plaques then
		-- either it is not known to device yet or its revision did change.
		--
		IF NOT FOUND THEN
			--
			-- Check whether this plaque is known to device.
			--
			UPDATE known_plaques
			SET revision_changed = TRUE,
				plaque_revision = var_plaque_revision
			WHERE plaque_id = var_plaque_id;

			-- If plaque with such id is not found in a list of known plaques then
			-- this plaque is not known to device yet.
			--
			IF NOT FOUND THEN
				--
				-- Add it to a list of appeared plaques.
				--
				INSERT INTO appeared_plaques (plaque_id, plaque_revision)
				VALUES (var_plaque_id, var_plaque_revision);
			END IF;
		END IF;
	END LOOP;

	-- Go through disappeared plaques.
	--
	FOR var_plaque_id IN
		SELECT plaque_id
		FROM known_plaques
		WHERE revision_changed IS FALSE
	LOOP
		-- Mark this plaque in session journal as disappeared.
		--
		UPDATE journal.session_on_radar_plaques
		SET on_radar_revision = var_next_on_radar_revision,
			plaque_revision = -1
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DISAPPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through appeared plaques.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM appeared_plaques
	LOOP
		-- Add this plaque to session journal.
		--
		INSERT INTO journal.session_on_radar_plaques (session_id, on_radar_revision, plaque_id, plaque_revision)
		VALUES (parm_session_id, var_next_on_radar_revision, var_plaque_id, var_plaque_revision);

		RAISE NOTICE 'SESSION:% PLAQUE:% APPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through changed plaques.
	--
	FOR var_plaque_id, var_plaque_revision IN
		SELECT plaque_id, plaque_revision
		FROM known_plaques
		WHERE revision_changed IS TRUE
	LOOP
		-- Update revision of this plaque in session journal.
		--
		UPDATE journal.session_on_radar_plaques
		SET on_radar_revision = var_next_on_radar_revision,
			plaque_revision = var_plaque_revision
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DID CHANGE', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through appeared plaques.
	--
	FOR var_plaque_token, var_plaque_revision IN
		SELECT plaques.plaque_token, radar.plaque_revision
		FROM journal.session_on_radar_plaques AS radar
		JOIN surrounding.plaques AS plaques
			USING (plaque_id)
		WHERE on_radar_revision >= parm_on_radar_revision
	LOOP
		RETURN NEXT (var_plaque_token, var_plaque_revision);

		--RAISE NOTICE 'SESSION:% PLAQUE:% REVISION:%', parm_session_id, var_plaque_token, var_plaque_revision;
	END LOOP;

	-- At this point all plaques in a list of known plaques that are not flagged as "revision changed"
	-- are those that did disappear from radar.

	-- Go through disappeared plaques.
	--
	FOR var_plaque_id, var_plaque_token IN
		SELECT plaque_id, plaque_token
		FROM surrounding.plaques
		WHERE plaque_id IN (
			SELECT plaque_id
			FROM known_plaques
			WHERE revision_changed IS FALSE
		)
	LOOP
		-- Mention this plaque as disappeared.
		--
		RETURN NEXT (var_plaque_token, -1);

		-- Remove this plaque from radar.
		--
		DELETE FROM journal.session_plaques
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DISAPPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through appeared plaques.
	--
	FOR var_plaque_id, var_plaque_token, var_plaque_revision IN
		SELECT plaque_id, plaque_token, plaque_revision
		FROM surrounding.plaques
		WHERE plaque_id IN (
			SELECT plaque_id
			FROM appeared_plaques
		)
	LOOP
		-- Mention this plaque as appeared.
		--
		RETURN NEXT (var_plaque_token, var_plaque_revision);

		-- Add this plaque to radar.
		--
		INSERT INTO journal.session_plaques (session_id, plaque_id, plaque_revision)
		VALUES (parm_session_id, var_plaque_id, var_plaque_revision);

		RAISE NOTICE 'SESSION:% PLAQUE:% APPEAERED', parm_session_id, var_plaque_id;
	END LOOP;

	-- Go through changed plaques.
	--
	FOR var_plaque_id, var_plaque_token, var_plaque_revision IN
		SELECT plaque_id, plaque_token, plaque_revision
		FROM surrounding.plaques
		WHERE plaque_id IN (
			SELECT plaque_id
			FROM known_plaques
			WHERE revision_changed IS TRUE
		)
	LOOP
		-- Mention this plaque as disappeared.
		--
		RETURN NEXT (var_plaque_token, var_plaque_revision);

		-- Update current revision of this plaque on radar.
		--
		UPDATE journal.session_plaques
		SET plaque_revision = var_plaque_revision
		WHERE session_id = parm_session_id
		  AND plaque_id = var_plaque_id;

		RAISE NOTICE 'SESSION:% PLAQUE:% DID CHANGE', parm_session_id, var_plaque_id;
	END LOOP;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION journal.download_plaques (IN parm_exigence_xml XML)
RETURNS XML AS
$PLSQL$

DECLARE
	var_plaque_id		BIGINT;
	var_plaque_uuid		UUID;
	var_revision		INTEGER;
	var_kind_of_content	journal.KINDOFCONTENT;
	var_latitude		DOUBLE PRECISION;
	var_longitude		DOUBLE PRECISION;
	var_altitude		REAL;
	var_direction		REAL;
	var_width			REAL;
	var_height			REAL;
	content_xml			XML;
	plaque_xml			XML;
	replique_xml		XML DEFAULT NULL;

DECLARE
	go_through_plaques_in_query REFCURSOR;

BEGIN
	OPEN go_through_plaques_in_query FOR
	SELECT UNNEST(XPATH('/exigence/plaque/@uuid', parm_exigence_xml));

	<<go_through_plaques_in_query>>
	LOOP
		FETCH NEXT FROM go_through_plaques_in_query
		INTO var_plaque_uuid;

		EXIT go_through_plaques_in_query
		WHEN NOT FOUND;

		SELECT plaque_id, revision, kind_of_content, latitude, longitude, altitude, direction, width, height
		FROM journal.plaques
		WHERE plaque_uuid = var_plaque_uuid
		INTO var_plaque_id, var_revision, var_kind_of_content, var_latitude, var_longitude, var_altitude, var_direction, var_width, var_height;

		IF NOT FOUND THEN
			RAISE EXCEPTION 'Plaque download request with UUID that does not exist';
		END IF;

 		CASE var_kind_of_content
		WHEN 'PLAINTEXT' THEN
			SELECT XMLELEMENT(NAME plain_text,
				XMLELEMENT(NAME background_color, (background_color).red || ';' || (background_color).green||';'||(background_color).blue),
				XMLELEMENT(NAME foreground_color, (foreground_color).red||';'||(foreground_color).green||';'||(foreground_color).blue),
				XMLELEMENT(NAME content_text, content_text))
			FROM journal.plain_texts
			WHERE plaque_id = var_plaque_id
			INTO content_xml;
		END CASE;

		plaque_xml := XMLELEMENT(NAME coordinate2d, var_latitude || ';' || var_longitude);
		IF var_direction IS NOT NULL THEN
			plaque_xml := XMLCONCAT(plaque_xml, XMLELEMENT(NAME direction, var_direction));
		END IF;
		plaque_xml := XMLCONCAT(plaque_xml, XMLELEMENT(NAME altitude, var_altitude));
		plaque_xml := XMLCONCAT(plaque_xml, XMLELEMENT(NAME size, var_width || ';' || var_height));
		plaque_xml := XMLCONCAT(plaque_xml, content_xml);

		replique_xml := XMLCONCAT(replique_xml,
			XMLELEMENT(NAME plaque,
				XMLATTRIBUTES(var_plaque_uuid AS uuid, var_revision AS revision),
				plaque_xml
			)
		);
	END LOOP go_through_plaques_in_query;

	CLOSE go_through_plaques_in_query;

	RETURN replique_xml;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/
