import {temporalRequest} from './temporal.js';
import {shaderSourceRequest,expireShaderSources} from './shader-source.js';
const MAX_BYTES = 65536;
const reply = (body, status = 200) => Response.json(body, {status, headers: {'Cache-Control': 'no-store'}});
const integer = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
function exact(obj, fields) {
  return obj && typeof obj === 'object' && !Array.isArray(obj) &&
    Object.keys(obj).length === fields.length && fields.every(k => Object.hasOwn(obj, k));
}
export function normalize(body) {
  if (!exact(body, ['schema','build','backend','gpu','driver','records']) || ![1,2].includes(body.schema) ||
      typeof body.build !== 'string' || !/^[a-zA-Z0-9._-]{1,80}$/.test(body.build) || !['vulkan','d3d12'].includes(body.backend) ||
      typeof body.gpu !== 'string' || !/^[a-zA-Z0-9 ()_.+-]{1,100}$/.test(body.gpu) ||
      typeof body.driver !== 'string' || !/^[0-9]{1,20}$/.test(body.driver) ||
      !Array.isArray(body.records) || body.records.length < 1 || body.records.length > 32) throw Error('schema');
  return body.records.map(r => {
    const fields=body.schema===1 ? ['vs','ps','width','height','slot','candidates','flags','rejection','draws'] :
      ['vs','ps','width','height','slot','candidates','flags','rejection','draws','position','guards'];
    if (!exact(r, fields) ||
        typeof r.vs !== 'string' || !/^[0-9a-f]{16}$/.test(r.vs) ||
        typeof r.ps !== 'string' || !/^[0-9a-f]{16}$/.test(r.ps) ||
        !integer(r.width,1,7680) || !integer(r.height,1,4320) ||
        ![-1,0,4,7,8,230,233].includes(r.slot) || !integer(r.candidates,0,63) ||
        !integer(r.flags,0,31) || !integer(r.rejection,0,255) || !integer(r.draws,1,1000000000)) throw Error('record');
    if (body.schema===1) return {diagnostic: JSON.stringify({schema:1, namespace:'renderer-byte-fnv1a64',
      build:body.build,backend:body.backend,gpu:body.gpu,driver:body.driver,
      vs:r.vs,ps:r.ps,width:r.width,height:r.height,slot:r.slot,candidates:r.candidates,flags:r.flags,rejection:r.rejection}), draws:r.draws};
    if (!exact(r.position,['version','kind','slot','issues','outputs']) || r.position.version!==1 ||
        !integer(r.position.kind,0,2) || !integer(r.position.slot,-1,252) ||
        (r.position.kind===1 ? r.position.slot<0 : r.position.slot!==-1) ||
        !integer(r.position.issues,0,63) || !integer(r.position.outputs,0,65535) || !integer(r.guards,0,31)) throw Error('position');
    return {diagnostic: JSON.stringify({schema:2, namespace:'renderer-byte-fnv1a64',
      build:body.build,backend:body.backend,gpu:body.gpu,driver:body.driver,
      vs:r.vs,ps:r.ps,width:r.width,height:r.height,slot:r.slot,candidates:r.candidates,flags:r.flags,rejection:r.rejection,
      position:{version:r.position.version,kind:r.position.kind,slot:r.position.slot,issues:r.position.issues,outputs:r.position.outputs},guards:r.guards}), draws:r.draws};
  });
}
export async function digest(text) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text)))].map(x=>x.toString(16).padStart(2,'0')).join('');
}
export default {
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (path === '/health' && request.method === 'GET') return reply({service:'lost-odyssey-taa-collector',schema:1,schemas:[1,2],temporal:1,shaderSources:1});
    if (path === '/' && request.method === 'GET') return new Response('Lost Odyssey optional TAA diagnostics. Opt-in only. Structured shader hashes, original VS/PS microcode, GPU/driver, dimensions, compact position-use evidence, jitter/camera matrices, sparse depth and camera-only motion sequences; no raw logs, paths or saves. Shader programs are deduplicated by content and associated with GPU model, backend, driver and build. Records expire after 30 days without an observation; unreferenced shader programs are removed. Disable collection in game Settings. No public diagnostic download. Cloudflare processes connection metadata for delivery and abuse prevention.');
    if (path === '/v1/temporal') return temporalRequest(request,env);
    if (path === '/v1/shader-sources') return shaderSourceRequest(request,env);
    if (path !== '/v1/taa') return reply({error:'not_found'},404);
    if (request.method !== 'POST') return reply({error:'method'},405);
    if (!request.headers.get('Content-Type')?.toLowerCase().startsWith('application/json') || request.headers.has('Content-Encoding')) return reply({error:'content_type'},415);
    const {success} = await env.UPLOAD_LIMIT.limit({key:request.headers.get('CF-Connecting-IP') || 'unknown'});
    if (!success) return reply({error:'rate_limit'},429);
    if (Number(request.headers.get('Content-Length')) > MAX_BYTES) return reply({error:'too_large'},413);
    let records;
    try {
      const reader = request.body?.getReader(); if (!reader) throw Error('body');
      let size=0; const chunks=[];
      for (;;) {const {done,value}=await reader.read();if(done)break;size+=value.length;
        if(size>MAX_BYTES){await reader.cancel();return reply({error:'too_large'},413);}chunks.push(value);}
      const bytes=new Uint8Array(size);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
      records=normalize(JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes)));
    } catch {return reply({error:'invalid_payload'},400);}
    try {
      const now=Math.floor(Date.now()/1000);
      const statements=await Promise.all(records.map(async r=>env.DB.prepare(
        'INSERT INTO observations(id,diagnostic,max_draws,first_seen,last_seen) VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET max_draws=MAX(observations.max_draws,excluded.max_draws),last_seen=excluded.last_seen'
      ).bind(await digest(r.diagnostic),r.diagnostic,r.draws,now,now)));
      await env.DB.batch(statements);
      return reply({accepted:records.length});
    } catch {console.error(JSON.stringify({event:'storage_failure'}));return reply({error:'unavailable'},503);}
  },
  async scheduled(_event,env) {
    const cutoff=Math.floor(Date.now()/1000)-30*86400;
    await env.DB.prepare('DELETE FROM temporal_sequences WHERE last_seen < ?').bind(cutoff).run();
    await env.DB.prepare('DELETE FROM observations WHERE last_seen < ?').bind(cutoff).run();
    await expireShaderSources(env.DB,cutoff);
  }
};
