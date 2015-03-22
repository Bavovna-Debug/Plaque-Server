/******************************************************************************/
/*                                                                            */
/*  Process anticipant.                                                       */
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION auth.process_anticipant (IN parm_exigence_xml XML)
RETURNS XML AS
$PLSQL$

DECLARE
	var_device_token	UUID;
	var_device_uuid		UUID;
	var_device_name		CHARACTER VARYING(50);
	device_token_xml	XML;

	response_xml		XML;

BEGIN
	IF NOT XML_IS_WELL_FORMED_DOCUMENT(parm_request_text) THEN
		RAISE EXCEPTION 'Not XML';
	END IF;

	request_xml := XMLPARSE(DOCUMENT parm_request_text);

	IF XPATH_EXISTS('/anticipant/device_token/text()', request_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing device token.';
	END IF;

	-- Convert and check device token.
	--
	BEGIN
		var_device_token := UNNEST(XPATH('/anticipant/device_token/text()', request_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple device tokens.';

		WHEN INVALID_TEXT_REPRESENTATION THEN
			RAISE EXCEPTION 'Device token in wrong format.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process device token (sqlstate=%).', SQLSTATE;
	END;

	IF XPATH_EXISTS('/anticipant/device_name/text()', request_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing device name.';
	END IF;

	-- Convert and check device name.
	--
	BEGIN
		device_name_xml := UNNEST(XPATH('/anticipant/device_name/text()', request_xml));
		var_device_name := CAST(device_name_xml AS CHARACTER VARYING);

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple device names.';

		WHEN STRING_DATA_RIGHT_TRUNCATION THEN
			RAISE NOTICE 'Device name truncated.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process device name (sqlstate=%).', SQLSTATE;
	END;

	BEGIN
		INSERT INTO auth.devices (device_token, device_name)
		VALUES (var_device_token, var_device_name)
		RETURNING device_uuid
		INTO var_device_uuid;

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot register new device (sqlstate=%).', SQLSTATE;
	END;

	response_xml := XMLCONCAT(
		XMLPI(NAME plaque, 'version="1.0"'),
		XMLELEMENT(NAME anticipant,
			XMLELEMENT(NAME device, var_device_uuid)
		)
	);

	response_text := CAST(response_xml AS TEXT);

	RETURN response_text;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/
