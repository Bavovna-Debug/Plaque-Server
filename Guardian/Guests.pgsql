/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE pool.sources
(
	source_id			BIGSERIAL					NOT NULL,
	ip_address			INET						NOT NULL,
	positive_hits		BIGINT						NOT NULL,
	negative_hits		BIGINT						NOT NULL,
	blocked_until		TIMESTAMP WITHOUT TIME ZONE	NULL DEFAULT NULL,

	CONSTRAINT sources_primary_key
		PRIMARY KEY (source_id)
		USING INDEX TABLESPACE guardian_pool
)
TABLESPACE guardian_pool;

CREATE UNIQUE INDEX sources_ip_address_key
	ON pool.sources
	USING BTREE
	(ip_address)
	WITH (FILLFACTOR = 80)
	TABLESPACE guardian_pool;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE TABLE pool.source_hits
(
	source_hit_id		BIGSERIAL					NOT NULL,
	source_id			BIGINT						NOT NULL,
	accessed			TIMESTAMP WITHOUT TIME ZONE	NOT NULL DEFAULT CURRENT_TIMESTAMP,
	successfull			BOOLEAN						NOT NULL,

	CONSTRAINT source_hits_primary_key
		PRIMARY KEY (source_hit_id)
		USING INDEX TABLESPACE guardian_pool,

	CONSTRAINT source_hits_source_foreign_key
		FOREIGN KEY (source_id)
		REFERENCES pool.sources (source_id)
		ON UPDATE NO ACTION
		ON DELETE RESTRICT
)
TABLESPACE guardian_pool;

CREATE UNIQUE INDEX source_hits_search_key
	ON pool.source_hits
	USING BTREE
	(source_id, accessed)
	WITH (FILLFACTOR = 80)
	TABLESPACE guardian_pool;

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

/*
CREATE OR REPLACE RULE increase_hits AS
ON INSERT TO pool.sources
WHERE (EXISTS (SELECT 1 FROM pool.sources WHERE ip_address = NEW.ip_address))
DO INSTEAD
UPDATE pool.sources
SET hits = hits + 1
WHERE ip_address = NEW.ip_address;
*/

/******************************************************************************/
/*                                                                            */
/******************************************************************************/

CREATE OR REPLACE FUNCTION pool.verify_ip (IN parm_ip_address INET)
RETURNS BOOLEAN AS
$PLSQL$

DECLARE
	var_source_id		BIGINT;
	var_blocked_until	TIMESTAMP;
	number_of_accesses	INTEGER;

BEGIN
	SELECT source_id, blocked_until
	FROM pool.sources
	WHERE ip_address = parm_ip_address
	INTO var_source_id, var_blocked_until;

	IF NOT FOUND THEN
		BEGIN
			WITH source AS (
				INSERT INTO pool.sources (ip_address, positive_hits, negative_hits)
				VALUES (parm_ip_address, 1, 0)
				RETURNING source_id
			)
			INSERT INTO pool.source_hits (source_id, successfull)
			SELECT source_id, TRUE
			FROM source;

		EXCEPTION
			WHEN OTHERS THEN
				RAISE EXCEPTION 'Error on new source: % %', SQLSTATE, SQLERRM;
		END;

		RETURN TRUE;
	ELSE
		IF (var_blocked_until IS NOT NULL) AND (var_blocked_until > NOW()) THEN
			BEGIN
				UPDATE pool.sources
				SET negative_hits = negative_hits + 1
				WHERE source_id = var_source_id;

				INSERT INTO pool.source_hits (source_id, successfull)
				VALUES (var_source_id, FALSE);

			EXCEPTION
				WHEN OTHERS THEN
					RAISE EXCEPTION 'Error on blocked source: % %', SQLSTATE, SQLERRM;
			END;

			RETURN FALSE;
		END IF;

		SELECT COUNT(*)
		FROM pool.source_hits
		WHERE source_id = var_source_id
		  AND accessed > NOW() - INTERVAL '60 SECONDS'
		INTO number_of_accesses;

		IF number_of_accesses < 2 THEN
			BEGIN
				UPDATE pool.sources
				SET positive_hits = positive_hits + 1,
					blocked_until = NULL
				WHERE source_id = var_source_id;

				INSERT INTO pool.source_hits (source_id, successfull)
				VALUES (var_source_id, TRUE);

			EXCEPTION
				WHEN OTHERS THEN
					RAISE EXCEPTION 'Error on source: % %', SQLSTATE, SQLERRM;
			END;

			RETURN TRUE;
		ELSE
			BEGIN
				UPDATE pool.sources
				SET negative_hits = negative_hits + 1,
					blocked_until = NOW() + INTERVAL '60 SECONDS'
				WHERE source_id = var_source_id;

				INSERT INTO pool.source_hits (source_id, successfull)
				VALUES (var_source_id, FALSE);

			EXCEPTION
				WHEN OTHERS THEN
					RAISE EXCEPTION 'Error on blocking the source: % %', SQLSTATE, SQLERRM;
			END;

			RETURN FALSE;
		END IF;
	END IF;
END;

$PLSQL$
LANGUAGE plpgsql
VOLATILE
RETURNS NULL ON NULL INPUT
EXTERNAL SECURITY DEFINER;
