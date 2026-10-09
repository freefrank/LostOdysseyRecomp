CREATE TABLE IF NOT EXISTS runtime_logs (
  id TEXT PRIMARY KEY CHECK(length(id) = 64),
  build TEXT NOT NULL,
  platform TEXT NOT NULL CHECK(platform IN ('windows')),
  text TEXT NOT NULL,
  first_seen INTEGER NOT NULL,
  last_seen INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS runtime_logs_expiry ON runtime_logs(last_seen);
