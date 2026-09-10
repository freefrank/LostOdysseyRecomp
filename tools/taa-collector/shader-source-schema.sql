CREATE TABLE IF NOT EXISTS shader_sources (
  stage TEXT NOT NULL CHECK(stage IN ('vs','ps')),
  sha256 TEXT NOT NULL CHECK(length(sha256) = 64),
  renderer_hash TEXT NOT NULL CHECK(length(renderer_hash) = 16),
  byte_length INTEGER NOT NULL CHECK(byte_length BETWEEN 4 AND 65536 AND byte_length % 4 = 0),
  payload BLOB NOT NULL CHECK(length(payload) = byte_length),
  first_seen INTEGER NOT NULL,
  last_seen INTEGER NOT NULL,
  PRIMARY KEY(stage, sha256)
);
CREATE INDEX IF NOT EXISTS shader_sources_renderer_hash ON shader_sources(stage, renderer_hash);

CREATE TABLE IF NOT EXISTS shader_source_observations (
  stage TEXT NOT NULL,
  sha256 TEXT NOT NULL,
  build TEXT NOT NULL,
  backend TEXT NOT NULL CHECK(backend IN ('d3d12','vulkan')),
  gpu_key TEXT NOT NULL,
  gpu TEXT NOT NULL,
  driver TEXT NOT NULL,
  first_seen INTEGER NOT NULL,
  last_seen INTEGER NOT NULL,
  PRIMARY KEY(stage, sha256, build, backend, gpu_key, driver),
  FOREIGN KEY(stage, sha256) REFERENCES shader_sources(stage, sha256)
);
CREATE INDEX IF NOT EXISTS shader_source_observations_expiry ON shader_source_observations(last_seen);
CREATE INDEX IF NOT EXISTS shader_source_observations_gpu ON shader_source_observations(gpu_key, backend, driver, build);
