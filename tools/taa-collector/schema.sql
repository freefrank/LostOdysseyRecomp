CREATE TABLE IF NOT EXISTS observations (id TEXT PRIMARY KEY, diagnostic TEXT NOT NULL, max_draws INTEGER NOT NULL, first_seen INTEGER NOT NULL, last_seen INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS observations_expiry ON observations(last_seen);
