/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.beacons
(
	beacon_stamp		TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	beacon_id			BIGSERIAL					NOT NULL,
	beacon_token		UUID						NOT NULL DEFAULT hash.associate_token('BUOY'),
	device_id			BIGINT						NOT NULL,
	revision			INTEGER						NOT NULL DEFAULT 0,
	coordinate			earth.EARTH					NOT NULL,
	latitude			DOUBLE PRECISION			NOT NULL,
	longitude			DOUBLE PRECISION			NOT NULL,
	range				REAL						NOT NULL,
	beacon_color		journal.RGB					NOT NULL,
	notification_text	TEXT						NOT NULL,

	CONSTRAINT beacons_primary_key
		PRIMARY KEY (beacon_id)
		USING INDEX TABLESPACE plaque_journal,

	CONSTRAINT beacons_token_foreign_key
		FOREIGN KEY (beacon_token)
		REFERENCES hash.repository (token)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT beacons_device_foreign_key
		FOREIGN KEY (device_id)
		REFERENCES auth.devices (device_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT,

	CONSTRAINT beacons_notification_check
		CHECK (LENGTH(notification_text) > 0)
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE plaque_journal;

COMMENT ON COLUMN journal.beacons.coordinate
IS 'Read-only field - the value generated automatically based on latitude and longitude.';

CREATE UNIQUE INDEX beacons_token_key
	ON journal.beacons
	USING BTREE
	(beacon_token)
	WITH (FILLFACTOR = 80)
	TABLESPACE plaque_journal;

CREATE INDEX beacons_coordinate_key
	ON journal.beacons
	USING GiST (coordinate)
	TABLESPACE plaque_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE TABLE journal.plain_texts
(
	plain_text_stamp	TIMESTAMP			NOT NULL DEFAULT CURRENT_TIMESTAMP,
	plain_text_id		BIGSERIAL			NOT NULL,
	beacon_id			BIGINT				NOT NULL,
	background_color	journal.RGB			NOT NULL,
	foreground_color	journal.RGB			NOT NULL,
	content_text		TEXT				NOT NULL,

	CONSTRAINT plain_texts_primary_key
		PRIMARY KEY (plain_text_id)
		USING INDEX TABLESPACE plaque_journal,

	CONSTRAINT plain_texts_beacon_foreign_key
		FOREIGN KEY (beacon_id)
		REFERENCES journal.beacons (beacon_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE plaque_journal;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

DROP TRIGGER IF EXISTS generate_beacon_coordinate_on_insert
	ON journal.beacons RESTRICT;

DROP TRIGGER IF EXISTS refresh_beacon_coordinate_on_update
	ON journal.beacons RESTRICT;

CREATE OR REPLACE FUNCTION journal.synchronize_beacon_coordinate ()
RETURNS TRIGGER AS
$PLSQL$

BEGIN
	NEW.coordinate = earth.LL_TO_EARTH(NEW.latitude, NEW.longitude);

	RETURN NEW;
END;

$PLSQL$
LANGUAGE plpgsql;

CREATE TRIGGER generate_beacon_coordinate_on_insert
	BEFORE INSERT
	ON journal.beacons
	FOR EACH ROW
	EXECUTE PROCEDURE journal.synchronize_beacon_coordinate();

CREATE TRIGGER refresh_beacon_coordinate_on_update
	BEFORE UPDATE
	ON journal.beacons
	FOR EACH ROW
	WHEN ((OLD.latitude IS DISTINCT FROM NEW.latitude) OR (OLD.longitude IS DISTINCT FROM NEW.longitude))
	EXECUTE PROCEDURE journal.synchronize_beacon_coordinate();
