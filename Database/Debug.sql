/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE debug.reports
(
	report_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	report_id			BIGSERIAL					NOT NULL,
	device_id			BIGINT						NOT NULL,
	message     		TEXT						NOT NULL,

	CONSTRAINT reports_primary_key
		PRIMARY KEY (report_id)
		USING INDEX TABLESPACE vp_debug,

	CONSTRAINT reports_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE CASCADE
		ON DELETE RESTRICT,

	CONSTRAINT reports_message_check
		CHECK (LENGTH(message) > 0)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_debug;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE TABLE debug.succeeded_bonjours
(
	bonjour_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	bonjour_id			BIGSERIAL					NOT NULL,
	ip_address			INET						NOT NULL,
	port_number			INTEGER						NOT NULL,
	request_xml			XML							NOT NULL,
	response_xml		XML							NOT NULL,

	CONSTRAINT succeeded_bonjours_primary_key
		PRIMARY KEY (bonjour_id)
		USING INDEX TABLESPACE plaque_debug
)
TABLESPACE plaque_debug;

CREATE TABLE debug.failed_bonjours
(
	bonjour_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	bonjour_id			BIGSERIAL					NOT NULL,
	ip_address			INET						NOT NULL,
	port_number			INTEGER						NOT NULL,
	request_xml			XML							NOT NULL,
	response_xml		XML							NOT NULL,
	error_message		TEXT						NOT NULL,

	CONSTRAINT failed_bonjours_primary_key
		PRIMARY KEY (bonjour_id)
		USING INDEX TABLESPACE plaque_debug
)
TABLESPACE plaque_debug;

CREATE TABLE debug.intruders
(
	intruder_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	intruder_id			BIGSERIAL					NOT NULL,
	ip_address			INET						NOT NULL,
	port_number			INTEGER						NOT NULL,
	request				TEXT						NOT NULL,

	CONSTRAINT intruders_primary_key
		PRIMARY KEY (intruder_id)
		USING INDEX TABLESPACE plaque_debug
)
TABLESPACE plaque_debug;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/
/*
CREATE TYPE debug.str1_t AS
(
	s		CHARACTER VARYING(50),
	lat		DOUBLE PRECISION,
	alt		REAL,
	token	UUID
);

CREATE OR REPLACE FUNCTION debug.str1 (
	IN parm_latitude	DOUBLE PRECISION,
	IN parm_longitude	DOUBLE PRECISION,
	IN parm_range		REAL
)
RETURNS SETOF debug.str1_t AS
-- RETURNS TABLE (
--	ret_name	CHARACTER VARYING(50),
--	ret_lat		DOUBLE PRECISION,
--	ret_alt		REAL
-- ) AS
$PLSQL$

DECLARE
	ret_name	CHARACTER VARYING(50);
	ret_lat		DOUBLE PRECISION;
	ret_alt		REAL;
	ret_token	UUID;

BEGIN
	FOR ret_name, ret_lat, ret_alt, ret_token IN
		SELECT s, lat, alt, token
		FROM debug.strings
	LOOP
		RETURN NEXT (ret_name, ret_lat, ret_alt, ret_token)::debug.str1_t;
	END LOOP;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION debug.intruder (
	IN parm_ip_address		INET,
	IN parm_port_number		INTEGER,
	IN parm_request			TEXT
)
RETURNS VOID AS
$PLSQL$

BEGIN
	BEGIN
		INSERT INTO debug.intruders (ip_address, port_number, request)
		VALUES (parm_ip_address, parm_port_number, parm_request);

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot report intruder (sqlstate=%)',
				SQLSTATE;
	END;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION debug.create_random_plaques (IN parm_number_of_plaques BIGINT)
RETURNS VOID AS
$PLSQL$

DECLARE
	var_plaque_id			BIGINT;
	var_latitude			DOUBLE PRECISION;
	var_longitude			DOUBLE PRECISION;
	var_background_color	journal.RGB;
	var_foreground_color	journal.RGB;
	var_content_text		TEXT;

BEGIN
	LOOP
		var_latitude := ROUND(CAST(RANDOM() * 180 - 90 AS NUMERIC), 6);
		var_longitude := ROUND(CAST(RANDOM() * 360 - 180 AS NUMERIC), 6);

		INSERT INTO journal.plaques (device_id, kind_of_content, latitude, longitude)
		VALUES (5968002, 'PLAINTEXT', var_latitude, var_longitude)
		RETURNING plaque_id
		INTO var_plaque_id;

		var_background_color := ROW(ROUND(RANDOM() * 255), ROUND(RANDOM() * 255), ROUND(RANDOM() * 255))::journal.RGB;
		var_foreground_color := ROW(ROUND(RANDOM() * 255), ROUND(RANDOM() * 255), ROUND(RANDOM() * 255))::journal.RGB;
		var_content_text := 'Hello from ' || var_latitude || ' x ' || var_longitude;

		INSERT INTO journal.plain_texts (plaque_id, background_color, foreground_color, content_text)
		VALUES (var_plaque_id, var_background_color, var_foreground_color, var_content_text);

		parm_number_of_plaques := parm_number_of_plaques - 1;

		EXIT WHEN parm_number_of_plaques = 0;
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
CREATE OR REPLACE FUNCTION debug.create_random_beacons (IN parm_number_of_beacons BIGINT)
RETURNS VOID AS
$PLSQL$

DECLARE
	var_beacon_id			BIGINT;
	var_latitude			DOUBLE PRECISION;
	var_longitude			DOUBLE PRECISION;
	var_range				REAL;
	var_beacon_color		journal.RGB;
	var_notification_text	TEXT;

BEGIN
	LOOP
		var_latitude := ROUND(8.2 + CAST(RANDOM() * 2 AS NUMERIC), 6);
		var_longitude := ROUND(47.6 + CAST(RANDOM() * 2 AS NUMERIC), 6);
		var_range := 50.0 + ROUND(CAST(RANDOM() * 9 AS NUMERIC), 0) * 50.0;
		var_beacon_color := ROW(ROUND(RANDOM() * 255), ROUND(RANDOM() * 255), ROUND(RANDOM() * 255))::journal.RGB;
		var_notification_text := 'Boje auf ' || var_latitude || ' x ' || var_longitude;

		INSERT INTO journal.beacons (device_id, latitude, longitude, beacon_color, range, notification_text)
		VALUES (5968002, var_latitude, var_longitude, var_beacon_color, var_range, var_notification_text)
		RETURNING beacon_id
		INTO var_beacon_id;

		parm_number_of_beacons := parm_number_of_beacons - 1;

		EXIT WHEN parm_number_of_beacons = 0;
	END LOOP;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/
