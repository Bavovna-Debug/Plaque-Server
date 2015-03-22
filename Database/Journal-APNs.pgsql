/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.apns_tokens
(
	apns_token_stamp	TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	apns_token_id		BIGSERIAL					NOT NULL,
	device_id			BIGINT						NOT NULL,
	apns_token          CHARACTER VARYING(64)		NOT NULL,

	CONSTRAINT apns_tokens_primary_key
		PRIMARY KEY (apns_token_id)
		USING INDEX TABLESPACE vp_journal,

	CONSTRAINT apns_tokens_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT apns_tokens_token_check
		CHECK (LENGTH(apns_token) = 64)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_journal;

CREATE UNIQUE INDEX apns_tokens_device_key
	ON journal.apns_tokens
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.set_apns_token (
	parm_device_id      BIGINT,
	parm_apns_token     CHARACTER VARYING(64)
)
RETURNS BOOLEAN AS
$PLSQL$

BEGIN
    RAISE NOTICE 'APNS token: %', parm_apns_token;

    DELETE FROM journal.apns_tokens
    WHERE device_id = parm_device_id;

    INSERT INTO journal.apns_tokens (device_id, apns_token)
    VALUES (parm_device_id, parm_apns_token);

	RETURN TRUE;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
