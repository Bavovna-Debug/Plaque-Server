/******************************************************************************/
/*                                                                            */
/*  Review status.                                                            */
/*                                                                            */
/******************************************************************************/

/*
CREATE TYPE reception.TICKETSTATUS AS ENUM
(
	'WAITING',
	'WAITING_FOR_UPLOAD',
	'UPLOADED',
	'WAITING_FOR_REVIEW',
	'IN_REVIEW',
	'APPROVED',
	'DEFERRED',
	'REJECTED'
);
*/

/******************************************************************************/
/*                                                                            */
/*  Review event.                                                             */
/*                                                                            */
/******************************************************************************/

/*
CREATE TYPE reception.TICKETEVENT AS
(
	stamp				TIMESTAMP WITHOUT TIME ZONE,
	status				reception.TICKETSTATUS,
	explanation			TEXT
);
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE reception.anticipants
(
	anticipant_stamp	TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	anticipant_id		BIGSERIAL					NOT NULL,
	anticipant_token	UUID						NOT NULL DEFAULT UUID_GENERATE_V4(),
	device_id			BIGINT						NULL DEFAULT NULL,
	device_token		UUID						NOT NULL,
	device_name			CHARACTER VARYING(50)		NOT NULL,
	device_model		CHARACTER VARYING(20)		NOT NULL,
	system_name			CHARACTER VARYING(20)		NOT NULL,
	system_version		CHARACTER VARYING(20)		NOT NULL,

	CONSTRAINT anticipants_primary_key
		PRIMARY KEY (anticipant_id)
		USING INDEX TABLESPACE plaque_reception,

	CONSTRAINT anticipants_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT anticipants_unique
		UNIQUE (device_token)
		USING INDEX TABLESPACE plaque_reception,

	CONSTRAINT anticipants_device_name_check
		CHECK (LENGTH(device_name) > 0)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE plaque_reception;

CREATE UNIQUE INDEX anticipants_token_key
	ON reception.anticipants
	USING BTREE
	(anticipant_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE plaque_reception;
