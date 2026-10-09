import test from 'node:test';
import assert from 'node:assert/strict';
import worker, {normalizeLog, digest} from './worker.js';

const text = '[    0.116 t1234] [note]  source version: 0.9.1\n[   12.000 t1234] [error] renderer: TAA initialization failed; SMAA fallback\n';
const body = {schema: 1, build: '0.9.1', platform: 'windows', text};
const post = (value, headers = {}) => new Request('https://lo.dotslash.pro/v1/logs', {
  method: 'POST', headers: {'Content-Type': 'application/json', ...headers},
  body: typeof value === 'string' ? value : JSON.stringify(value)});
const environment = () => {
  const writes = [];
  return {writes, UPLOAD_LIMIT: {limit: async () => ({success: true})},
    DB: {prepare: sql => ({bind: (...args) => ({run: async () => { writes.push({sql, args}); }})})}};
};

test('accepts exactly the log schema', () => {
  assert.deepEqual(normalizeLog(body), {build: '0.9.1', platform: 'windows', text});
  for (const bad of [
    {...body, schema: 2}, {...body, build: 'a/b'}, {...body, build: ''}, {...body, platform: 'linux'},
    {...body, text: ''}, {...body, text: 'a\u0000b'}, {...body, text: 'a\u007fb'}, {...body, text: '\ud800'},
    {...body, text: 'x'.repeat(65537)}, {...body, gpu: 'x'}, {schema: 1, build: '0.9.1', text}, [body], null
  ]) assert.throws(() => normalizeLog(bad));
  assert.doesNotThrow(() => normalizeLog({...body, text: 'tab\tand\nnewline 中文'}));
});

test('stores the text under its SHA-256 and refreshes last_seen on repeats', async () => {
  const env = environment();
  const response = await worker.fetch(post(body), env);
  assert.equal(response.status, 200);
  const id = await digest(text);
  assert.deepEqual(await response.json(), {accepted: 1, id});
  assert.equal(env.writes.length, 1);
  assert.match(env.writes[0].sql, /INSERT INTO runtime_logs.*ON CONFLICT\(id\) DO UPDATE SET last_seen=excluded.last_seen/);
  assert.deepEqual(env.writes[0].args.slice(0, 4), [id, '0.9.1', 'windows', text]);
});

test('rejects bad requests before writing', async () => {
  const env = environment();
  env.DB.prepare = () => { throw Error('must not write'); };
  assert.equal((await worker.fetch(post(' '.repeat(2 * 65536 + 1)), env)).status, 413);
  assert.equal((await worker.fetch(post('{'), env)).status, 400);
  assert.equal((await worker.fetch(post({...body, extra: 1}), env)).status, 400);
  assert.equal((await worker.fetch(post(body, {'Content-Type': 'text/plain'}), env)).status, 415);
  assert.equal((await worker.fetch(post(body, {'Content-Encoding': 'gzip'}), env)).status, 415);
  assert.equal((await worker.fetch(new Request('https://lo.dotslash.pro/v1/logs'), env)).status, 405);
  env.UPLOAD_LIMIT.limit = async () => ({success: false});
  assert.equal((await worker.fetch(post(body), env)).status, 429);
});

test('retired TAA routes answer 410 and unknown paths 404', async () => {
  const env = environment();
  for (const path of ['/v1/taa', '/v1/temporal', '/v1/shader-sources'])
    assert.equal((await worker.fetch(new Request('https://lo.dotslash.pro' + path, {method: 'POST', body: '{}'}), env)).status, 410);
  assert.equal((await worker.fetch(new Request('https://lo.dotslash.pro/v2/logs'), env)).status, 404);
  assert.equal((await worker.fetch(new Request('https://lo.dotslash.pro/health'), env)).status, 200);
});

test('expiry removes old logs even when the retired tables are missing', async () => {
  const statements = [];
  const env = {DB: {
    prepare: sql => ({bind: (...args) => ({sql, args, run: async () => { statements.push(sql); }})}),
    batch: async () => { throw Error('no such table: observations'); }
  }};
  await worker.scheduled({}, env);
  assert.deepEqual(statements, ['DELETE FROM runtime_logs WHERE last_seen < ?']);
});
