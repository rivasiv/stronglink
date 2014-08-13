-- Unfortunately, the output of `sqlite3 .dump` is ugly, so this file should be generated by hand.
-- Not to mention, SQLite3 barely supports ALTER TABLE.

PRAGMA application_id=9; -- TODO

PRAGMA page_size=4096;
PRAGMA journal_mode=WAL;


CREATE TABLE users (
    user_id INTEGER PRIMARY KEY NOT NULL,
    username TEXT NOT NULL,
    password_hash TEXT NOT NULL,
    token TEXT
);
CREATE UNIQUE INDEX users_unique ON users (username);

CREATE TABLE sessions (
    session_id INTEGER PRIMARY KEY NOT NULL,
    session_hash TEXT NOT NULL,
    user_id INTEGER NOT NULL
);
CREATE INDEX sessions_index ON sessions (user_id);

-- In theory, file_id should be AUTOINCREMENT to prevent wrap-around, which would break sorting. However, 64-bit wrap-around doesn't seem worth getting worked up about. See "SQLite Autoincrement".
CREATE TABLE files (
    file_id INTEGER PRIMARY KEY NOT NULL,
    internal_hash TEXT NOT NULL,
    file_type TEXT NOT NULL,
    file_size INTEGER NOT NULL
);
CREATE UNIQUE INDEX files_hash_unique ON files (internal_hash, file_type);
CREATE INDEX file_type_index ON files (file_type);
CREATE INDEX file_size_index ON files (file_size);

CREATE TABLE meta_files (
	meta_file_id INTEGER PRIMARY KEY NOT NULL,
	file_id INTEGER NOT NULL,
	target_uri TEXT NOT NULL
);
CREATE UNIQUE INDEX meta_files_index ON meta_files (file_id, target_uri);
CREATE INDEX meta_files_reverse_index ON meta_files (target_uri, file_id);

CREATE TABLE meta_data (
	meta_data_id INTEGER PRIMARY KEY NOT NULL,
	meta_file_id INTEGER NOT NULL,
	field TEXT NOT NULL,
	value TEXT NOT NULL
);
CREATE UNIQUE INDEX meta_data_index ON meta_data (meta_file_id, field, value);
CREATE INDEX meta_data_reverse_index ON meta_data (meta_file_id, value, field);

CREATE TABLE file_uris (
	file_uri_id INTEGER PRIMARY KEY NOT NULL,
	file_id INTEGER NOT NULL,
	uri TEXT NOT NULL
);
CREATE UNIQUE INDEX file_uris_unique ON file_uris (uri, file_id);
CREATE INDEX file_uris_index ON file_uris (file_id);

-- FTS column and table names are heavily restricted. No underscores, etc.
CREATE VIRTUAL TABLE fulltext USING "fts4" (
	content="",
	value TEXT
);
CREATE TABLE meta_fulltext (
	meta_content_id INTEGER PRIMARY KEY NOT NULL,
	meta_file_id INTEGER NOT NULL,
	docid INTEGER NOT NULL
);
CREATE UNIQUE INDEX meta_fulltext_unique ON meta_fulltext (docid, meta_file_id);

-- TODO: We need a much better way to handle permissions, especially in order to determine accurate sort orders.
--CREATE TABLE file_permissions (
--	file_permission_id INTEGER PRIMARY KEY NOT NULL,
--	file_id INTEGER NOT NULL,
--	user_id INTEGER NOT NULL,
--	meta_file_id INTEGER NOT NULL
--);
--CREATE UNIQUE INDEX file_permissions_unique ON file_permissions (user_id, file_id, meta_file_id);

CREATE TABLE pulls (
	pull_id INTEGER PRIMARY KEY NOT NULL,
	user_id INTEGER NOT NULL,
	host TEXT NOT NULL,
	username TEXT NOT NULL,
	password TEXT NOT NULL,
	cookie TEXT NOT NULL,
	query TEXT NOT NULL
);

