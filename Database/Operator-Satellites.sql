/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE operator.satellites
(
    satellite_id        BIGSERIAL                   NOT NULL,
	valid_since			TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,
	valid_till			TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,
	ip_address			INET						NOT NULL,
	port_number			INTEGER						NOT NULL,

	CONSTRAINT satellites_primary_key
		PRIMARY KEY (satellite_id)
		USING INDEX TABLESPACE vp_operator
)
TABLESPACE vp_operator;
