/******************************************************************************/
/*                                                                            */
/*  Look for pending plaque request.                                          */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.get_one_plaque_request ()
RETURNS BOOLEAN AS
$PLSQL$

DECLARE
	var_request_id		BIGINT;

BEGIN
	-- Search for pending request to process (follow FIFO rule).
	--
	SELECT request_id
	FROM reception.requests
	WHERE processed IS FALSE
	  AND kind_of_request = 'PLAQUE'
	ORDER BY request_id ASC
	LIMIT 1
	INTO var_request_id;

	IF var_request_id IS NULL THEN
		RAISE NOTICE 'Nothing to do';
		RETURN FALSE;
	END IF;

	RETURN reception.process_one_plaque_request(var_request_id);
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Process one plaque request.                                               */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.process_one_plaque_request (IN parm_request_id BIGINT)
RETURNS BOOLEAN AS
$PLSQL$

DECLARE
	var_device_id		BIGINT;
	var_plaque_xml		XML;
	xml_parts			XML[];
	var_text			CHARACTER VARYING;
	latitude			DOUBLE PRECISION;
	longitude			DOUBLE PRECISION;
	var_coordinate		EARTH;

BEGIN
	SELECT device_id, request_xml
	FROM reception.requests
	WHERE request_id = parm_request_id
	INTO var_device_id, var_plaque_xml;

	IF XPATH_EXISTS('/request/plaque/coordinate2d', var_plaque_xml) IS FALSE THEN
		PERFORM reception.flag_request_processed(parm_request_id);
		PERFORM reception.change_request_status(parm_request_id, 'DEFERRED', 'No coordinate statements');
	END IF;

	IF XPATH_EXISTS('/request/plaque/coordinate2d/text()', var_plaque_xml) IS FALSE THEN
		PERFORM reception.flag_request_processed(parm_request_id);
		PERFORM reception.change_request_status(parm_request_id, 'DEFERRED', 'No coordinate values');
	END IF;

	xml_parts := XPATH('/request/plaque/coordinate2d/text()', var_plaque_xml);
	IF ARRAY_LENGTH(xml_parts, 1) != 1 THEN
		PERFORM reception.flag_request_processed(parm_request_id);
		PERFORM reception.change_request_status(parm_request_id, 'DEFERRED', 'Too many coordinate statements');
		RETURN FALSE;
	END IF;

	BEGIN
		var_text := xml_parts[1];
		latitude := CAST(SPLIT_PART(var_text, ';', 1) AS DOUBLE PRECISION);
		longitude := CAST(SPLIT_PART(var_text, ';', 2) AS DOUBLE PRECISION);
		var_coordinate := earth.LL_TO_EARTH(latitude, longitude);

	EXCEPTION
		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.flag_request_processed(parm_request_id);
			PERFORM reception.change_request_status(parm_request_id, 'DEFERRED', 'Cannot convert coordinates');
			RETURN FALSE;
	END;

	BEGIN
		INSERT INTO journal.plaques (device_id, kind_of_content, coordinate)
		VALUES (var_device_id, 'PLAINTEXT', var_coordinate);

	EXCEPTION
		WHEN OTHERS THEN
			RAISE NOTICE '%', SQLSTATE;
			PERFORM reception.flag_request_processed(parm_request_id);
			PERFORM reception.change_request_status(parm_request_id, 'DEFERRED', 'Cannot insert plaque');
			RETURN FALSE;
	END;

	PERFORM reception.flag_request_processed(parm_request_id);
	PERFORM reception.flag_request_completed(parm_request_id);

	RETURN TRUE;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
