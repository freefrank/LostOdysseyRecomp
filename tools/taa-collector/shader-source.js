const MAX_BYTES = 256 * 1024;
const MAX_PROGRAM_BYTES = 64 * 1024;
const MAX_PROGRAMS = 32;
const reply = (body, status = 200) => Response.json(body, {status, headers: {'Cache-Control':'no-store'}});
const exact = (obj, fields) => obj && typeof obj === 'object' && !Array.isArray(obj) &&
  Object.keys(obj).length === fields.length && fields.every(key => Object.hasOwn(obj, key));
const hex = bytes => [...bytes].map(x => x.toString(16).padStart(2, '0')).join('');

// The renderer hashes the original bytes with standard FNV-1a64. Use two
// 32-bit limbs so a maximum-size request does not require BigInt per byte.
export function rendererHash(bytes) {
  let hi = 0xcbf29ce4, lo = 0x84222325;
  for (const byte of bytes) {
    lo = (lo ^ byte) >>> 0;
    const product = lo * 0x1b3;
    hi = (Math.imul(hi, 0x1b3) + Math.imul(lo, 0x100) + Math.floor(product / 0x100000000)) >>> 0;
    lo = product >>> 0;
  }
  return hi.toString(16).padStart(8, '0') + lo.toString(16).padStart(8, '0');
}

function decodeProgram(data) {
  if (typeof data !== 'string' || data.length < 8 || data.length > 4 * Math.ceil(MAX_PROGRAM_BYTES / 3) ||
      data.length % 4 !== 0 || !/^[A-Za-z0-9+/]*={0,2}$/.test(data)) throw Error('base64');
  const binary = atob(data);
  // Re-encoding checks padding and unused low bits as well as alphabet spelling.
  if (btoa(binary) !== data || !binary.length || binary.length > MAX_PROGRAM_BYTES || binary.length % 4) throw Error('bytes');
  return Uint8Array.from(binary, c => c.charCodeAt(0));
}

export async function normalizeShaderSources(body) {
  if (!exact(body, ['schema','build','backend','gpu','driver','programs']) || body.schema !== 1 ||
      typeof body.build !== 'string' || !/^[a-zA-Z0-9._-]{1,80}$/.test(body.build) ||
      !['d3d12','vulkan'].includes(body.backend) ||
      typeof body.gpu !== 'string' || !/^[a-zA-Z0-9 ()_.+-]{1,100}$/.test(body.gpu) ||
      typeof body.driver !== 'string' || !/^\d{1,20}$/.test(body.driver) ||
      !Array.isArray(body.programs) || !body.programs.length || body.programs.length > MAX_PROGRAMS) throw Error('schema');
  const gpu = body.gpu.trim().replace(/ +/g, ' ');
  if (!gpu) throw Error('gpu');
  const metadata = {build:body.build, backend:body.backend, gpu, gpuKey:gpu.toLowerCase(), driver:body.driver};
  const programs = [];
  for (const item of body.programs) {
    if (!exact(item, ['stage','hash','data']) || !['vs','ps'].includes(item.stage) ||
        typeof item.hash !== 'string' || !/^[0-9a-f]{16}$/.test(item.hash)) throw Error('program');
    const bytes = decodeProgram(item.data);
    if (rendererHash(bytes) !== item.hash) throw Error('hash');
    const sha256 = hex(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)));
    programs.push({stage:item.stage, hash:item.hash, sha256, bytes});
  }
  return {metadata, programs};
}

function insertStatements(db, programs, metadata, now) {
  // All statements run in the same D1 transaction. Ten rows keep each statement
  // below 100 bound parameters (70 for sources, 90 for observations).
  const unique = [...new Map(programs.map(p => [`${p.stage}:${p.sha256}`, p])).values()];
  const statements = [];
  for (let start = 0; start < unique.length; start += 10) {
    const group = unique.slice(start, start + 10);
    statements.push(db.prepare(`INSERT INTO shader_sources(stage,sha256,renderer_hash,byte_length,payload,first_seen,last_seen)
      VALUES ${group.map(() => '(?,?,?,?,?,?,?)').join(',')}
      ON CONFLICT(stage,sha256) DO UPDATE SET last_seen=MAX(shader_sources.last_seen,excluded.last_seen)`)
      .bind(...group.flatMap(p => [p.stage,p.sha256,p.hash,p.bytes.length,p.bytes.buffer,now,now])));
    statements.push(db.prepare(`INSERT INTO shader_source_observations(stage,sha256,build,backend,gpu_key,gpu,driver,first_seen,last_seen)
      VALUES ${group.map(() => '(?,?,?,?,?,?,?,?,?)').join(',')}
      ON CONFLICT(stage,sha256,build,backend,gpu_key,driver) DO UPDATE SET last_seen=MAX(shader_source_observations.last_seen,excluded.last_seen)`)
      .bind(...group.flatMap(p => [p.stage,p.sha256,metadata.build,metadata.backend,metadata.gpuKey,metadata.gpu,metadata.driver,now,now])));
  }
  return statements;
}

export async function shaderSourceRequest(request, env) {
  if (request.method !== 'POST') return reply({error:'method'}, 405);
  if (!/^application\/json(?:\s*;|$)/i.test(request.headers.get('Content-Type') || '') || request.headers.has('Content-Encoding'))
    return reply({error:'content_type'}, 415);
  if (!(await env.UPLOAD_LIMIT.limit({key:request.headers.get('CF-Connecting-IP') || 'unknown'})).success)
    return reply({error:'rate_limit'}, 429);
  if (Number(request.headers.get('Content-Length')) > MAX_BYTES) return reply({error:'too_large'}, 413);
  let decoded;
  try {
    const reader = request.body?.getReader();
    if (!reader) throw Error('body');
    let size = 0;
    const chunks = [];
    for (;;) {
      const {done,value} = await reader.read();
      if (done) break;
      size += value.length;
      if (size > MAX_BYTES) { await reader.cancel(); return reply({error:'too_large'}, 413); }
      chunks.push(value);
    }
    const bytes = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    decoded = await normalizeShaderSources(JSON.parse(new TextDecoder('utf-8', {fatal:true}).decode(bytes)));
  } catch { return reply({error:'invalid_payload'}, 400); }
  try {
    await env.DB.batch(insertStatements(env.DB, decoded.programs, decoded.metadata, Math.floor(Date.now() / 1000)));
    return reply({accepted:decoded.programs.length, programs:decoded.programs.map(({stage,hash,sha256}) => ({stage,hash,sha256}))});
  } catch {
    console.error(JSON.stringify({event:'shader_source_storage_failure'}));
    return reply({error:'unavailable'}, 503);
  }
}

export async function expireShaderSources(db, cutoff) {
  await db.batch([
    db.prepare('DELETE FROM shader_source_observations WHERE last_seen < ?').bind(cutoff),
    db.prepare(`DELETE FROM shader_sources WHERE NOT EXISTS (
      SELECT 1 FROM shader_source_observations o WHERE o.stage=shader_sources.stage AND o.sha256=shader_sources.sha256)`)
  ]);
}
