CREATE TABLE IF NOT EXISTS temporal_sequences (
  id TEXT PRIMARY KEY,
  metadata TEXT NOT NULL,
  summary TEXT NOT NULL,
  payload BLOB NOT NULL,
  first_seen INTEGER NOT NULL,
  last_seen INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS temporal_last_seen ON temporal_sequences(last_seen);
