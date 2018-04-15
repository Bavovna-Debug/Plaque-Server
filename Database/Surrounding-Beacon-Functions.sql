/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION journal.compile_record (IN input_xml XML)
RETURNS XML AS
$PLSQL$

BEGIN
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

CREATE OR REPLACE FUNCTION journal.query_plaques (IN parm_query_xml XML)
RETURNS XML AS
$PLSQL$

DECLARE
	var_value			CHARACTER VARYING;
	user_latitude		DOUBLE PRECISION;
	user_longitude		DOUBLE PRECISION;
	var_range			REAL;
	var_plaque_uuid		UUID;
	var_revision		INTEGER;
	result_xml			XML DEFAULT NULL;

DECLARE
	search_by_coordinate_and_range CURSOR (
		key_latitude	DOUBLE PRECISION,
		key_longitude	DOUBLE PRECISION,
		key_range		INTEGER
	) IS
	SELECT plaque_uuid, revision
	FROM journal.plaques
	WHERE coordinate <@ EARTH_BOX(earth.LL_TO_EARTH(key_latitude, key_longitude), key_range / 1.609);

BEGIN
	IF XPATH_EXISTS('/query/coordinate2d', parm_query_xml) IS FALSE THEN
		RAISE EXCEPTION 'No coordinate specified';
	END IF;

	BEGIN
		var_value := UNNEST(XPATH('/query/coordinate2d/text()', parm_query_xml));
		user_latitude := CAST(SPLIT_PART(var_value, ';', 1) AS DOUBLE PRECISION);
		user_longitude := CAST(SPLIT_PART(var_value, ';', 2) AS DOUBLE PRECISION);

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Wrong coordinate specified';
	END;

	IF XPATH_EXISTS('/query/range', parm_query_xml) IS FALSE THEN
		RAISE EXCEPTION 'No range specified';
	END IF;

	BEGIN
		var_value := UNNEST(XPATH('/query/range/text()', parm_query_xml));
		var_range := CAST(var_value AS REAL);

	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'Wrong range specified';
	END;

	result_xml := NULL;

	OPEN search_by_coordinate_and_range(user_latitude, user_longitude, var_range);

	<<go_through_plaques>>
	LOOP
		FETCH NEXT FROM search_by_coordinate_and_range
		INTO var_plaque_uuid, var_revision;

		EXIT go_through_plaques
		WHEN NOT FOUND;

		result_xml := XMLCONCAT(result_xml,
			XMLELEMENT(NAME plaque,
				XMLATTRIBUTES(var_plaque_uuid AS uuid, var_revision AS revision)
			)
		);
	END LOOP go_through_plaques;

	CLOSE search_by_coordinate_and_range;

	result_xml := XMLELEMENT(NAME radar, result_xml);

	RETURN result_xml;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.download_plaques (IN parm_query_xml XML)
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
	result_xml			XML DEFAULT NULL;

DECLARE
	go_through_plaques_in_query REFCURSOR;

BEGIN
	OPEN go_through_plaques_in_query FOR
	SELECT UNNEST(XPATH('/query/plaque/@uuid', parm_query_xml));

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
				XMLELEMENT(NAME background_color, (background_color).red || ';' || (background_color).green || ';' || (background_color).blue),
				XMLELEMENT(NAME foreground_color, (foreground_color).red || ';' || (foreground_color).green || ';' || (foreground_color).blue),
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

		result_xml := XMLCONCAT(result_xml,
			XMLELEMENT(NAME plaque,
				XMLATTRIBUTES(var_plaque_uuid AS uuid, var_revision AS revision),
				plaque_xml
			)
		);
	END LOOP go_through_plaques_in_query;

	CLOSE go_through_plaques_in_query;

	result_xml := XMLELEMENT(NAME plaques, result_xml);

	RETURN result_xml;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
