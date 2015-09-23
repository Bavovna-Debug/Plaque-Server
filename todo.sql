ALTER TABLE journal.session_in_sight_plaques ADD COLUMN appear_revision INTEGER NULL DEFAULT NULL;
ALTER TABLE journal.session_in_sight_plaques ADD COLUMN change_revision INTEGER NULL DEFAULT NULL;
ALTER TABLE journal.session_in_sight_plaques ADD COLUMN disappear_revision INTEGER NULL DEFAULT NULL;
DROP INDEX session_in_sight_plaques_session_in_sight_revision_key;
ALTER TABLE journal.session_in_sight_plaques DROP COLUMN in_sight_revision;
