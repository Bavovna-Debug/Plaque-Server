/******************************************************************************/
/*                                                                            */
/*  Check whether this is new request or ticket status check.                 */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.anticipant (IN parm_exigence_xml XML)
RETURNS XML AS
$PLSQL$

BEGIN
	IF XPATH_EXISTS('/exigence/ticket', parm_exigence_xml) IS FALSE THEN
		RETURN reception.check_anticipant_ticket(parm_exigence_xml);
	ELSE
		RETURN reception.enqueue_anticipant(parm_exigence_xml);
	END IF;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Check status of anticipant ticket. If ticket is already processed         */
/*  then send device authentication information back.                         */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.check_anticipant_ticket (IN parm_exigence_xml XML)
RETURNS XML AS
$PLSQL$

DECLARE
	var_anticipant_token	UUID;
	var_device_id			BIGINT;
	var_device_uuid			UUID;

BEGIN
	-- Get ticket number.
	--
	BEGIN
		var_anticipant_token := UNNEST(XPATH('/exigence/ticket/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple anticipant tickets.';

		WHEN INVALID_TEXT_REPRESENTATION THEN
			RAISE EXCEPTION 'Anticipant ticket in wrong format.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process anticipant ticket (sqlstate=%).', SQLSTATE;
	END;

	SELECT device_id
	FROM reception.anticipants
	WHERE anticipant_token = var_anticipant_token
	INTO var_device_id;

	IF NOT FOUND THEN
		RAISE EXCEPTION 'No such anticipant ticket.';
	END IF;

	IF var_device_id IS NULL THEN
		RETURN XMLELEMENT(NAME ticket, var_anticipant_token);
	END IF;

	SELECT device_uuid
	FROM auth.devices
	WHERE device_id = var_device_id
	INTO var_device_uuid;

	RETURN XMLELEMENT(NAME device, var_device_uuid);
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Process anticipant request and give anticipant ticket back.               */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.enqueue_anticipant (IN parm_exigence_xml XML)
RETURNS XML AS
$PLSQL$

DECLARE
	var_device_token		UUID;
	var_device_name			CHARACTER VARYING(50);
	var_device_model		CHARACTER VARYING(20);
	var_system_name			CHARACTER VARYING(20);
	var_system_version		CHARACTER VARYING(20);
	var_anticipant_token	UUID;

BEGIN
	IF XPATH_EXISTS('/exigence/device_token', parm_exigence_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing device token.';
	END IF;

	IF XPATH_EXISTS('/exigence/device_name', parm_exigence_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing device name.';
	END IF;

	IF XPATH_EXISTS('/exigence/device_model', parm_exigence_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing device model.';
	END IF;

	IF XPATH_EXISTS('/exigence/system_name', parm_exigence_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing system name.';
	END IF;

	IF XPATH_EXISTS('/exigence/system_version', parm_exigence_xml) IS FALSE THEN
		RAISE EXCEPTION 'Missing system version.';
	END IF;

	-- Convert and check device token.
	--
	BEGIN
		var_device_token := UNNEST(XPATH('/exigence/device_token/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple device tokens.';

		WHEN INVALID_TEXT_REPRESENTATION THEN
			RAISE EXCEPTION 'Device token in wrong format.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process device token (sqlstate=%).', SQLSTATE;
	END;

	-- Convert and check device name.
	--
	BEGIN
		var_device_name := UNNEST(XPATH('/exigence/device_name/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple device names.';

		WHEN STRING_DATA_RIGHT_TRUNCATION THEN
			RAISE NOTICE 'Device name truncated.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process device name (sqlstate=%).', SQLSTATE;
	END;

	-- Convert and check device model.
	--
	BEGIN
		var_device_model := UNNEST(XPATH('/exigence/device_model/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple device models.';

		WHEN STRING_DATA_RIGHT_TRUNCATION THEN
			RAISE NOTICE 'Device model truncated.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process device model (sqlstate=%).', SQLSTATE;
	END;

	-- Convert and check system name.
	--
	BEGIN
		var_system_name := UNNEST(XPATH('/exigence/system_name/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple system names.';

		WHEN STRING_DATA_RIGHT_TRUNCATION THEN
			RAISE NOTICE 'System name truncated.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process system name (sqlstate=%).', SQLSTATE;
	END;

	-- Convert and check system version.
	--
	BEGIN
		var_system_version := UNNEST(XPATH('/exigence/system_version/text()', parm_exigence_xml));

	EXCEPTION
		WHEN CARDINALITY_VIOLATION THEN
			RAISE EXCEPTION 'Multiple system versions.';

		WHEN STRING_DATA_RIGHT_TRUNCATION THEN
			RAISE NOTICE 'System version truncated.';

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot process system version (sqlstate=%).', SQLSTATE;
	END;

	BEGIN
		INSERT INTO reception.anticipants (device_token, device_name, device_model, system_name, system_version)
		VALUES (var_device_token, var_device_name, var_device_model, var_system_name, var_system_version)
		RETURNING anticipant_token
		INTO var_anticipant_token;

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot insert anticipant (sqlstate=%).', SQLSTATE;
	END;

	RETURN XMLELEMENT(NAME ticket, var_anticipant_token);
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/*  Process anticipant.                                                       */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.dequeue_anticipant ()
RETURNS XML AS
$PLSQL$

DECLARE
	var_anticipant_id		BIGINT;
	go_through_anticipants	REFCURSOR;

BEGIN
	OPEN go_through_anticipants FOR
	SELECT anticipant_id
	FROM reception.anticipants
	WHERE anticipant_stamp < NOW() - INTERVAL '20 SECONDS';

	<<go_through_anticipants>>
	LOOP
		FETCH NEXT FROM go_through_anticipants
		INTO var_anticipant_id;

		EXIT go_through_anticipants
		WHEN NOT FOUND;

		BEGIN
			WITH device AS (
				INSERT INTO auth.devices (device_token, device_name, device_model, system_name, system_version)
				SELECT device_token, device_name, device_model, system_name, system_version
				FROM reception.anticipants
				WHERE anticipant_id = var_anticipant_id
				RETURNING device_id
			)
			UPDATE reception.anticipants
			SET device_id = (
				SELECT device_id
				FROM device
			);

		EXCEPTION
			WHEN OTHERS THEN
				RAISE EXCEPTION 'Cannot register device from anticipant (sqlstate=%).', SQLSTATE;
		END;
    END LOOP go_through_anticipants;

	CLOSE go_through_anticipants;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
