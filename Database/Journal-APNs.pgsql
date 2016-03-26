/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.apns_tokens
(
	apns_token_stamp	TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	apns_token_id		BIGSERIAL					NOT NULL,
	device_id			BIGINT						NOT NULL,
	apns_token          BYTEA                       NOT NULL,

	CONSTRAINT apns_tokens_primary_key
		PRIMARY KEY (apns_token_id)
		USING INDEX TABLESPACE vp_journal,

	CONSTRAINT apns_tokens_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE CASCADE
		ON DELETE RESTRICT,

	CONSTRAINT apns_tokens_token_check
		CHECK (LENGTH(apns_token) = 32)
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

/*
INSERT INTO journal.notifications (device_id, message_key, message_arguments)
VALUES (11840788, 'YOUR_PLAQUE_WAS_DESTROYED', '{'Crazy Tom', 'Ich weiss nicht was soll das bedeuten...'}');
*/

CREATE TABLE journal.notifications
(
	notification_stamp	TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	notification_id		BIGSERIAL					NOT NULL,
	in_messanger        BOOLEAN                     NOT NULL DEFAULT FALSE,
	sent                BOOLEAN                     NOT NULL DEFAULT FALSE,
	device_id			BIGINT						NOT NULL,
	message_key         CHARACTER VARYING(64)		NOT NULL,
	message_arguments   CHARACTER VARYING[] 		NULL DEFAULT NULL,

	CONSTRAINT notifications_primary_key
		PRIMARY KEY (notification_id)
		USING INDEX TABLESPACE vp_journal,

	CONSTRAINT notifications_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE CASCADE
		ON DELETE SET NULL
)
TABLESPACE vp_journal;

CREATE INDEX notifications_device_key
	ON journal.notifications
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION journal.set_apns_token (
	parm_device_id      BIGINT,
	parm_apns_token     BYTEA
)
RETURNS BOOLEAN AS
$PLSQL$

BEGIN
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
