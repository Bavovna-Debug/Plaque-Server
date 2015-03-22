/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.session_in_sight_plaques
(
	session_id			BIGINT  					NOT NULL,
	in_sight_revision   INTEGER                     NOT NULL,
	plaque_id			BIGINT						NOT NULL,
	plaque_revision		INTEGER						NOT NULL,

	CONSTRAINT session_in_sight_plaques_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE,

	CONSTRAINT session_in_sight_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE INDEX session_in_sight_plaques_session_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_sight_plaques_session_in_sight_revision_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id, in_sight_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE UNIQUE INDEX session_in_sight_plaques_session_plaque_key
	ON journal.session_in_sight_plaques
	USING BTREE
	(session_id, plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.session_in_cache_plaques
(
	session_id			BIGINT  					NOT NULL,
	in_cache_revision   INTEGER                     NOT NULL,
	plaque_id			BIGINT						NOT NULL,
	plaque_revision		INTEGER						NOT NULL,

	CONSTRAINT session_in_cache_plaques_session_foreign_key
		FOREIGN KEY (session_id)
		REFERENCES journal.sessions (session_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE,

	CONSTRAINT session_in_cache_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_in_cache_revision_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id, in_cache_revision)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

CREATE INDEX session_in_cache_plaques_session_plaque_key
	ON journal.session_in_cache_plaques
	USING BTREE
	(session_id, plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE journal.modified_plaques
(
	plaque_id			BIGINT						NOT NULL,

	CONSTRAINT modified_plaques_plaque_foreign_key
		FOREIGN KEY (plaque_id)
		REFERENCES surrounding.plaques (plaque_id)
		ON UPDATE NO ACTION
		ON DELETE CASCADE
)
TABLESPACE vp_journal;

CREATE INDEX modified_plaques_plaque_key
	ON journal.modified_plaques
	USING BTREE
	(plaque_id)
	WITH (FILLFACTOR = 80)
	TABLESPACE vp_journal;
