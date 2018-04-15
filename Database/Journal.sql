SET search_path = public, journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.movements
(
	movement_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	movement_id			BIGSERIAL					NOT NULL,
	device_id			BIGINT						NOT NULL,
	coordinate			EARTH						NOT NULL,
	latitude			DOUBLE PRECISION			NOT NULL,
	longitude			DOUBLE PRECISION			NOT NULL,
	altitude			REAL						NOT NULL,
	course				REAL						NULL DEFAULT NULL,
	floor_level			INTEGER						NULL DEFAULT NULL,

	CONSTRAINT movements_primary_key
		PRIMARY KEY (movement_id)
		USING INDEX TABLESPACE vp_journal,

	CONSTRAINT movements_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE CASCADE
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

COMMENT ON COLUMN journal.movements.coordinate
IS 'Read-only field - the value generated automatically based on latitude and longitude.';

CREATE INDEX movements_coordinate_key
	ON journal.movements
	USING GiST
	(coordinate)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS generate_movement_coordinate_on_insert
	ON journal.movements RESTRICT;

CREATE OR REPLACE FUNCTION journal.synchronize_movement_coordinate ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	NEW.coordinate = LL_TO_EARTH(NEW.latitude, NEW.longitude);

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER generate_movement_coordinate_on_insert
	BEFORE INSERT
	ON journal.movements
	FOR EACH ROW
	EXECUTE PROCEDURE journal.synchronize_movement_coordinate();
