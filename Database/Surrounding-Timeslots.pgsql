/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE surrounding.time_slots
(
	time_slot_stamp     TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	time_slot_id		BIGSERIAL					NOT NULL,
	time_slot_begin     TIMESTAMP WITH TIME ZONE	NULL,
	time_slot_end       TIMESTAMP WITH TIME ZONE	NULL,

	CONSTRAINT time_slots_primary_key
		PRIMARY KEY (time_slot_id)
		USING INDEX TABLESPACE vp_surrounding,

	CONSTRAINT time_slots_range_check
		CHECK ((time_slot_begin IS NOT NULL) OR (time_slot_end IS NOT NULL))
		NOT DEFERRABLE INITIALLY IMMEDIATE
)
TABLESPACE vp_surrounding;
