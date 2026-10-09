// Opt-in runtime log collection. The TAA telemetry routes are retired (410);
// their D1 tables keep expiring under the same 30-day rule until empty.
const MAX_TEXT_BYTES = 65536;
const MAX_BODY_BYTES = 2 * MAX_TEXT_BYTES; // JSON escaping of Windows paths
const RETENTION_SECONDS = 30 * 86400;
const RETIRED = new Set(['/v1/taa', '/v1/temporal', '/v1/shader-sources']);
const reply = (body, status = 200) => Response.json(body, {status, headers: {'Cache-Control': 'no-store'}});

export function normalizeLog(body) {
  const fields = ['schema', 'build', 'platform', 'text'];
  if (!body || typeof body !== 'object' || Array.isArray(body) ||
      Object.keys(body).length !== fields.length || !fields.every(k => Object.hasOwn(body, k)) ||
      body.schema !== 1 || typeof body.build !== 'string' || !/^[0-9A-Za-z._+-]{1,80}$/.test(body.build) ||
      body.platform !== 'windows' || typeof body.text !== 'string' || body.text.length === 0 ||
      !body.text.isWellFormed() || /[\u0000-\u0008\u000b-\u001f\u007f]/.test(body.text) ||
      new TextEncoder().encode(body.text).length > MAX_TEXT_BYTES) throw Error('schema');
  return {build: body.build, platform: body.platform, text: body.text};
}

export async function digest(text) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text)))]
    .map(x => x.toString(16).padStart(2, '0')).join('');
}

async function readBounded(request) {
  const reader = request.body?.getReader();
  if (!reader) throw Error('body');
  let size = 0; const chunks = [];
  for (;;) {
    const {done, value} = await reader.read();
    if (done) break;
    size += value.length;
    if (size > MAX_BODY_BYTES) { await reader.cancel(); return null; }
    chunks.push(value);
  }
  const bytes = new Uint8Array(size); let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
  return new TextDecoder('utf-8', {fatal: true}).decode(bytes);
}

export default {
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (path === '/health' && request.method === 'GET') return reply({service: 'lost-odyssey-log-collector', schema: 1, taa: 'retired'});
    if (path === '/' && request.method === 'GET') return new Response('Lost Odyssey Recomp optional log collection. Opt-in only, Windows builds. At startup the game sends a filtered summary of the previous session\'s log: build, Windows version, GPU and driver, settings, map names, errors, crash and hang reports, and TAA/jitter mismatch records with shader IDs. Account names in folder paths are replaced before sending. No saves, screenshots, or personal or device identifiers. D1 records are deleted 30 days after their last upload; a private research archive keeps copies. Disable collection in game Settings. No public download. Cloudflare processes connection metadata for delivery and abuse prevention. The earlier TAA shader collection is retired.');
    if (RETIRED.has(path)) return reply({error: 'retired'}, 410);
    if (path !== '/v1/logs') return reply({error: 'not_found'}, 404);
    if (request.method !== 'POST') return reply({error: 'method'}, 405);
    if (!request.headers.get('Content-Type')?.toLowerCase().startsWith('application/json') || request.headers.has('Content-Encoding')) return reply({error: 'content_type'}, 415);
    const {success} = await env.UPLOAD_LIMIT.limit({key: request.headers.get('CF-Connecting-IP') || 'unknown'});
    if (!success) return reply({error: 'rate_limit'}, 429);
    if (Number(request.headers.get('Content-Length')) > MAX_BODY_BYTES) return reply({error: 'too_large'}, 413);
    let log;
    try {
      const text = await readBounded(request);
      if (text === null) return reply({error: 'too_large'}, 413);
      log = normalizeLog(JSON.parse(text));
    } catch { return reply({error: 'invalid_payload'}, 400); }
    try {
      const now = Math.floor(Date.now() / 1000), id = await digest(log.text);
      await env.DB.prepare('INSERT INTO runtime_logs(id,build,platform,text,first_seen,last_seen) VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET last_seen=excluded.last_seen')
        .bind(id, log.build, log.platform, log.text, now, now).run();
      return reply({accepted: 1, id});
    } catch { console.error(JSON.stringify({event: 'storage_failure'})); return reply({error: 'unavailable'}, 503); }
  },
  async scheduled(_event, env) {
    const cutoff = Math.floor(Date.now() / 1000) - RETENTION_SECONDS;
    await env.DB.prepare('DELETE FROM runtime_logs WHERE last_seen < ?').bind(cutoff).run();
    // Retired TAA tables; absent in a fresh database, which must not stop log expiry.
    try {
      await env.DB.batch([
        env.DB.prepare('DELETE FROM observations WHERE last_seen < ?').bind(cutoff),
        env.DB.prepare('DELETE FROM temporal_sequences WHERE last_seen < ?').bind(cutoff),
        env.DB.prepare('DELETE FROM shader_source_observations WHERE last_seen < ?').bind(cutoff),
        env.DB.prepare(`DELETE FROM shader_sources WHERE NOT EXISTS (
          SELECT 1 FROM shader_source_observations o WHERE o.stage=shader_sources.stage AND o.sha256=shader_sources.sha256)`)
      ]);
    } catch { console.error(JSON.stringify({event: 'retired_expiry_failure'})); }
  }
};
