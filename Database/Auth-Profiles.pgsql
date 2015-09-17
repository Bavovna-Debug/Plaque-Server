/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE auth.profiles
(
	profile_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	profile_id			BIGSERIAL					NOT NULL,
	profile_token		UUID						NOT NULL DEFAULT operator.peek_token('PROF'),
	profile_revision	INTEGER						NOT NULL DEFAULT 0,
	profile_name		VARCHAR(20)					NOT NULL,
	user_name			VARCHAR(50)					NULL DEFAULT NULL,
	password_md5		CHARACTER(32)				NULL DEFAULT NULL,
	email_address		VARCHAR(200)				NULL DEFAULT NULL,

	CONSTRAINT profiles_primary_key
		PRIMARY KEY (profile_id)
		USING INDEX TABLESPACE vp_auth,

	CONSTRAINT profiles_token_foreign_key
		FOREIGN KEY (profile_token)
		REFERENCES operator.tokens (token)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT profiles_profile_name_check
		CHECK (LENGTH(profile_name) >= 4)
		NOT DEFERRABLE INITIALLY IMMEDIATE,

	CONSTRAINT profiles_email_address_check
		CHECK (email_address LIKE '%@%.%')
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_auth;

CREATE UNIQUE INDEX profiles_token_key
	ON auth.profiles
	USING BTREE
	(profile_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

CREATE UNIQUE INDEX profiles_profile_name_key
	ON auth.profiles
	USING BTREE
	(LOWER(profile_name))
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

CREATE INDEX profiles_user_name_key
	ON auth.profiles
	USING BTREE
	(LOWER(user_name))
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

CREATE UNIQUE INDEX profiles_email_address_key
	ON auth.profiles
	USING BTREE
	(LOWER(email_address))
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_auth;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS profiles_drop_token_on_delete
	ON auth.profiles RESTRICT;

CREATE OR REPLACE FUNCTION auth.profiles_drop_token_on_delete ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	PERFORM operator.poke_token(OLD.profile_token);

	RETURN OLD;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER profiles_drop_token_on_delete
	AFTER DELETE
	ON auth.profiles
	FOR EACH ROW
	EXECUTE PROCEDURE auth.profiles_drop_token_on_delete();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION auth.is_profile_name_free (
	IN parm_profile_name	VARCHAR(40)
)
RETURNS BOOLEAN AS
$PLSQL$

BEGIN
	CASE (
		SELECT COUNT(*)
		FROM auth.profiles
		WHERE LOWER(profile_name) = LOWER(parm_profile_name)
	)
	WHEN 0 THEN
		RAISE NOTICE 'Check profile name "%" - name is free', parm_profile_name;
		RETURN TRUE;
	ELSE
		RAISE NOTICE 'Check profile name "%" - name is already in use', parm_profile_name;
		RETURN FALSE;
	END CASE;
END;

$PLSQL$
LANGUAGE plpgsql
STABLE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE FUNCTION auth.create_profile (
	IN parm_profile_name	VARCHAR(40),
	IN parm_password_md5	CHARACTER(32),
	IN parm_email_address	VARCHAR(200)
)
RETURNS UUID AS
$PLSQL$

DECLARE
	var_profile_id			BIGINT;
	var_profile_token		UUID;

BEGIN
	BEGIN
		INSERT INTO auth.profiles (profile_name, password_md5)
		VALUES (parm_profile_name, parm_password_md5)
		RETURNING profile_id, profile_token
		INTO var_profile_id, var_profile_token;

	EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			RAISE NOTICE 'Profile with name "%" already exists', LOWER(parm_profile_name);
			RETURN NULL;

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot create new profile: % (%)', SQLSTATE, SQLERRM;
	END;

	BEGIN
		UPDATE auth.profiles
		SET email_address = parm_email_address
		WHERE profile_id = var_profile_id;

	EXCEPTION
		WHEN UNIQUE_VIOLATION THEN
			RAISE NOTICE 'Email address "%" already used', LOWER(parm_email_address);
			RETURN NULL;

		WHEN OTHERS THEN
			RAISE EXCEPTION 'Cannot set email address to profile: % (%)', SQLSTATE, SQLERRM;
	END;

	RETURN var_profile_token;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
CALLED ON NULL INPUT
EXTERNAL SECURITY DEFINER;
*/
