/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE auth.devices
(
	device_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	device_id			BIGSERIAL					NOT NULL,
	confirmed			BOOLEAN						NOT NULL DEFAULT FALSE,
	profile_id			BIGINT						NULL DEFAULT NULL,
	device_token		UUID						NOT NULL DEFAULT operator.peek_token('DEVI'),
	vendor_token		UUID						NOT NULL,
	device_name			CHARACTER VARYING(40)		NOT NULL,
	device_model		CHARACTER VARYING(20)		NOT NULL,
	system_name			CHARACTER VARYING(20)		NOT NULL,
	system_version		CHARACTER VARYING(20)		NOT NULL,

	CONSTRAINT devices_primary_key
		PRIMARY KEY (device_id)
		USING INDEX TABLESPACE vp_auth,

	CONSTRAINT devices_token_foreign_key
		FOREIGN KEY (device_token)
		REFERENCES operator.tokens (token)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT devices_profile_foreign_key
		FOREIGN KEY (profile_id)
		REFERENCES auth.profiles (profile_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT devices_device_name_check
		CHECK (LENGTH(device_name) > 0)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_auth;

CREATE UNIQUE INDEX devices_token_key
	ON auth.devices
	USING BTREE
	(device_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

CREATE INDEX devices_profile_key
	ON auth.devices
	USING BTREE
	(profile_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS devices_create_displacement_on_insert
	ON auth.devices RESTRICT;

CREATE OR REPLACE FUNCTION auth.devices_create_displacement_on_insert ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	INSERT INTO journal.device_displacements (device_id)
	VALUES (NEW.device_id);

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER devices_create_displacement_on_insert
	AFTER INSERT
	ON auth.devices
	FOR EACH ROW
	EXECUTE PROCEDURE auth.devices_create_displacement_on_insert();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS devices_drop_token_on_delete
	ON auth.devices RESTRICT;

CREATE OR REPLACE FUNCTION auth.devices_drop_token_on_delete ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	PERFORM operator.poke_token(OLD.device_token);

	RETURN OLD;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER devices_drop_token_on_delete
	AFTER DELETE
	ON auth.devices
	FOR EACH ROW
	EXECUTE PROCEDURE auth.devices_drop_token_on_delete();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.register_device (
	IN parm_vendor_token	UUID,
	IN parm_device_name		CHARACTER VARYING(40),
	IN parm_device_model	CHARACTER VARYING(20),
	IN parm_system_name		CHARACTER VARYING(20),
	IN parm_system_version	CHARACTER VARYING(20)
)
RETURNS UUID AS
$SQL$

	INSERT INTO auth.devices (vendor_token, device_name, device_model, system_name, system_version)
	VALUES (parm_vendor_token, parm_device_name, parm_device_model, parm_system_name, parm_system_version)
	RETURNING device_token;

$SQL$
LANGUAGE SQL
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
