SET search_path = public, surrounding;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TYPE surrounding.DIMENSION AS ENUM
(
	'2D',
	'3D',
	'4D'
);

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE surrounding.plaques
(
	plaque_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	plaque_id			BIGSERIAL					NOT NULL,
	plaque_token		UUID						NOT NULL DEFAULT operator.peek_token('PLAQ'),
	plaque_revision		INTEGER						NOT NULL DEFAULT 0,
	device_id			BIGINT						NOT NULL,
	profile_id			BIGINT						NOT NULL,
	time_slot_id		BIGINT						NULL DEFAULT NULL,
	dimension			DIMENSION					NOT NULL,
	coordinate			EARTH						NOT NULL,
	latitude			DOUBLE PRECISION			NOT NULL,
	longitude			DOUBLE PRECISION			NOT NULL,
	altitude			REAL						NULL DEFAULT NULL,
	direction			REAL						NULL DEFAULT NULL,
	tilt				REAL						NULL DEFAULT NULL,
	width				REAL						NOT NULL,
	height				REAL						NOT NULL,
	background_color	INTEGER						NOT NULL,
	foreground_color	INTEGER						NOT NULL,
	font_size			REAL						NOT NULL,
	inscription			TEXT						NOT NULL,

	CONSTRAINT plaques_primary_key
		PRIMARY KEY (plaque_id)
		USING INDEX TABLESPACE vp_surrounding,

	CONSTRAINT plaques_token_foreign_key
		FOREIGN KEY (plaque_token)
		REFERENCES operator.tokens (token)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT plaques_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT plaques_profile_foreign_key
		FOREIGN KEY (profile_id)
		REFERENCES auth.profiles (profile_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT plaques_time_slot_foreign_key
		FOREIGN KEY (time_slot_id)
		REFERENCES surrounding.time_slots (time_slot_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT plaques_direction_check
		CHECK (direction BETWEEN 0.0 AND 359.999)
		NOT DEFERRABLE INITIALLY IMMEDIATE,

	CONSTRAINT plaques_tilt_check
		CHECK (tilt BETWEEN -90.0 AND 90.0)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_surrounding;

COMMENT ON COLUMN surrounding.plaques.coordinate
IS 'Read-only field - the value generated automatically based on latitude and longitude.';

COMMENT ON COLUMN surrounding.plaques.altitude
IS 'Altitude of the most lower corner of the plaque over default niveau.';

COMMENT ON COLUMN surrounding.plaques.direction
IS 'If not specified then the plaque should always be turned to observer.';

COMMENT ON COLUMN surrounding.plaques.tilt
IS 'If not specified then the plaque stays upright, which is equal to tilt 0 grad. Tilt 90 grad means look upside, -90 grad downside.';

CREATE UNIQUE INDEX plaques_token_key
	ON surrounding.plaques
	USING BTREE
	(plaque_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_surrounding;

CREATE INDEX plaques_device_key
	ON surrounding.plaques
	USING BTREE
	(device_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_surrounding;

CREATE INDEX plaques_profile_key
	ON surrounding.plaques
	USING BTREE
	(profile_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_surrounding;

CREATE INDEX plaques_coordinate_key
	ON surrounding.plaques
	USING GiST
	(coordinate)
	TABLESPACE vp_surrounding;

CREATE INDEX plaques_inscription_key
	ON surrounding.plaques
	USING BTREE
	(LOWER(inscription))
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_surrounding;
/*
CREATE INDEX plaques_inscription_key2
	ON surrounding.plaques
	USING GIN
	(TO_TSVECTOR(inscription))
	WITH (FILLFACTOR = 80)
	TABLESPACE plaque_surrounding;
*/
/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS plaques_drop_token_on_delete
	ON surrounding.plaques RESTRICT;

CREATE OR REPLACE FUNCTION surrounding.plaques_drop_token_on_delete ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	PERFORM operator.poke_token(OLD.plaque_token);

	RETURN OLD;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER plaques_drop_token_on_delete
	AFTER DELETE
	ON surrounding.plaques
	FOR EACH ROW
	EXECUTE PROCEDURE surrounding.plaques_drop_token_on_delete();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS generate_plaque_coordinate_on_insert
	ON surrounding.plaques RESTRICT;

CREATE OR REPLACE FUNCTION surrounding.synchronize_plaque_coordinate ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	NEW.coordinate = LL_TO_EARTH(NEW.latitude, NEW.longitude);

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER generate_plaque_coordinate_on_insert
	BEFORE INSERT
	ON surrounding.plaques
	FOR EACH ROW
	EXECUTE PROCEDURE surrounding.synchronize_plaque_coordinate();

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS to_do_things_on_plaque_update
	ON surrounding.plaques RESTRICT;

CREATE OR REPLACE FUNCTION surrounding.to_do_things_on_plaque_update ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	NEW.plaque_revision = NEW.plaque_revision + 1;

	IF (NEW.latitude != OLD.latitude) OR (NEW.longitude != OLD.longitude) THEN
		NEW.coordinate = LL_TO_EARTH(NEW.latitude, NEW.longitude);

		INSERT INTO journal.moved_plaques (plaque_id)
		VALUES (NEW.plaque_id);
	END IF;

	INSERT INTO journal.modified_plaques (plaque_id)
	VALUES (NEW.plaque_id);

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER to_do_things_on_plaque_update
	BEFORE UPDATE
	ON surrounding.plaques
	FOR EACH ROW
	EXECUTE PROCEDURE surrounding.to_do_things_on_plaque_update();

/*
CREATE TRIGGER refresh_plaque_coordinate_on_update
	BEFORE UPDATE
	ON surrounding.plaques
	FOR EACH ROW
	WHEN ((OLD.latitude IS DISTINCT FROM NEW.latitude) OR (OLD.longitude IS DISTINCT FROM NEW.longitude))
	EXECUTE PROCEDURE surrounding.synchronize_plaque_coordinate();
*/
