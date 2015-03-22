/******************************************************************************/
/*                                                                            */
/*  Check subject of incoming bonjour and call appropriate routine            */
/*  to handle this bonjour.                                                   */
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION reception.bonjour (IN parm_bonjour_text TEXT)
RETURNS TEXT AS
$PLSQL$

DECLARE
	in_bonjour_xml		XML;
	out_bonjour_xml		XML DEFAULT NULL;
	exigence_xml		XML;
	replique_xml		XML;
	var_subject			CHARACTER VARYING;

	go_through_bonjour	REFCURSOR;

BEGIN
	IF NOT XML_IS_WELL_FORMED_DOCUMENT(parm_bonjour_text) THEN
		RAISE EXCEPTION 'Not XML';
	END IF;

	in_bonjour_xml := XMLPARSE(DOCUMENT parm_bonjour_text);

	OPEN go_through_bonjour FOR
	SELECT UNNEST(XPATH('/bonjour/exigence', in_bonjour_xml));

	<<go_through_bonjour>>
	LOOP
		FETCH NEXT FROM go_through_bonjour
		INTO exigence_xml;

		EXIT go_through_bonjour
		WHEN NOT FOUND;

		var_subject := UNNEST(XPATH('/exigence/@subject', exigence_xml));

		IF var_subject IS NULL THEN
			RAISE EXCEPTION 'Subject statement missing';
		END IF;

		IF LENGTH(var_subject) = 0 THEN
			RAISE EXCEPTION 'No subject specified';
		END IF;

		CASE var_subject
		WHEN 'query_plaques' THEN
			replique_xml := journal.query_plaques(exigence_xml);

		WHEN 'download_plaques' THEN
			replique_xml := journal.download_plaques(exigence_xml);

		WHEN 'server_list' THEN
			replique_xml := general.process_anticipant(exigence_xml);

		WHEN 'anticipant' THEN
			replique_xml := reception.anticipant(exigence_xml);

		WHEN 'plaque_approval' THEN
			replique_xml := reception.take_request(exigence_xml);

		ELSE
			RAISE EXCEPTION 'Unknown subject';
		END CASE;

		out_bonjour_xml := XMLCONCAT(out_bonjour_xml,
			XMLELEMENT(NAME replique, XMLATTRIBUTES(var_subject AS subject),
				replique_xml
			)
		);
    END LOOP go_through_bonjour;

	CLOSE go_through_bonjour;

	out_bonjour_xml := XMLCONCAT(
		XMLPI(NAME plaque, 'version="1.0"'),
		XMLELEMENT(NAME bonjour, out_bonjour_xml)
	);

	RETURN CAST(out_bonjour_xml AS TEXT);
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
