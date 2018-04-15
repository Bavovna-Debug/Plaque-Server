/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.sessions_online
(
	session_id			BIGINT  					NOT NULL,
	online_since		TIMESTAMP WITHOUT TIME ZONE	NOT NULL,
	offline_since		TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,

	CONSTRAINT sessions_online_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE CASCADE
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE INDEX sessions_online_session_key
	ON journal.sessions_online
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX sessions_online_journaling_key
	ON journal.sessions_online
	USING BTREE
	(session_id, (offline_since IS NULL))
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;
