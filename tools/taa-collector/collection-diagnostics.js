// Compact CPU windows are a separate canonical schema. Their content ID links
// only evidence inside this window, never a player, installation or device.
const exact = (v, keys) => v && typeof v === 'object' && !Array.isArray(v) &&
  Object.keys(v).length === keys.length && keys.every(k => Object.hasOwn(v, k));
const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
const bool = v => typeof v === 'boolean';
const fail = () => { throw Error('compact'); };
const counterKeys = {
  source:['queued','known','invalid','full','disabled','busy'],
  summary:['queued','counted','dropped','disabled','busy'],
  binding:['queued','counted','full','busy','disabled','stale'],
  sparse:['queued','duplicate','full','busy','disabled','stale','cooldown','discontinuous'],
  compact:['pairDropped','bindingDropped','lockBusy','frameDiscontinuity']
};
function counts(v, keys) {
  if (!exact(v, keys) || !keys.every(k => int(v[k],0,0xffffffff))) fail();
  return Object.fromEntries(keys.map(k => [k,v[k]]));
}
function capabilities(v, backend) {
  const fixed = {version:1,cpuWindowFrames:32,cpuCooldownSeconds:180,pairCapacity:24,
    bindingCapacity:8,bindingTextureSlot:0,bindingPerPairPerFrame:1,
    sourceMaxProgramBytes:65536,sparseSupported:backend === 'd3d12',sparseFrames:32,
    sparseCooldownSeconds:300,sparseWindowLinked:false,gpuCompletion:false,colorImages:false,
    sourceScope:'draw-program-observe',summaryScope:'taa-draw-observe',pendingWindowCapacity:1};
  if (!exact(v,[...Object.keys(fixed),'runtimeVersion','runtimeCommit','bindingPairs','strictAck']) ||
      !Object.entries(fixed).every(([k,value])=>v[k] === value) ||
      typeof v.runtimeVersion !== 'string' || !/^0\.5\.[23](?:[-+][a-zA-Z0-9._-]+)?$/.test(v.runtimeVersion) || v.runtimeVersion.length > 80 ||
      typeof v.runtimeCommit !== 'string' || !/^(?:[0-9a-f]{7,40}|unknown)$/.test(v.runtimeCommit) ||
      JSON.stringify(v.bindingPairs) !== JSON.stringify(['e810cfacc107fd3c:5b11f88a8bb293df','e810cfacc107fd3c:78a5c96b2d7eaa91']) ||
      JSON.stringify(v.strictAck) !== '["compact"]') fail();
  return {...fixed,runtimeVersion:v.runtimeVersion,runtimeCommit:v.runtimeCommit,
    bindingPairs:[...v.bindingPairs],strictAck:[...v.strictAck]};
}
const frameKeys = ['offset','taa','ready','completed','reused','sceneRejection','historyCaptured',
  'historyRejection','cameraChecks','previousFrameDelta','sameEpoch','resetAfterFrame','sparseReady'];
function frame(v, span) {
  if (!exact(v,frameKeys) || !int(v.offset,0,span-1) || !int(v.sceneRejection,0,255) ||
      !int(v.historyRejection,0,1023) || !int(v.previousFrameDelta,-1,65535) ||
      !['taa','ready','completed','reused','historyCaptured','cameraChecks','sameEpoch','resetAfterFrame','sparseReady'].every(k=>bool(v[k]))) fail();
  return Object.fromEntries(frameKeys.map(k=>[k,v[k]]));
}
// Reuse the schema 2/3 validators so nested records cannot smuggle free text or
// relax existing numeric limits. Keep draws within the immutable window.
function record(v, schema, body, normalize) {
  const canonical = JSON.parse(normalize({schema,build:schema===3?'0.5.2-taa-bindings-1':body.build,
    backend:body.backend,gpu:body.gpu,driver:body.driver,records:[v]})[0].diagnostic);
  for (const key of ['schema','namespace','build','backend','gpu','driver']) delete canonical[key];
  return {...canonical,draws:v.draws};
}
export function normalizeCompact(body, normalize) {
  if (!exact(body,['schema','build','backend','gpu','driver','records']) || body.schema !== 4 ||
      body.build !== '0.5.2-collection-diagnostics-1' || !['d3d12','vulkan'].includes(body.backend) ||
      typeof body.gpu !== 'string' || !/^[a-zA-Z0-9 ()_.+-]{1,100}$/.test(body.gpu) ||
      typeof body.driver !== 'string' || !/^[0-9]{1,20}$/.test(body.driver) ||
      !Array.isArray(body.records) || body.records.length !== 1) fail();
  const w = body.records[0];
  if (!exact(w,['capabilities','complete','frameSpan','counters','delivery','pending','frames','pairs','bindings']) ||
      !bool(w.complete) || !int(w.frameSpan,1,32) ||
      !Array.isArray(w.frames) || w.frames.length < 1 || w.frames.length > 32 ||
      !Array.isArray(w.pairs) || w.pairs.length > 24 || !Array.isArray(w.bindings) || w.bindings.length > 8 ||
      !exact(w.counters,Object.keys(counterKeys)) ||
      !exact(w.delivery,['scope','summary','source','binding','sparse','compact']) || w.delivery.scope !== 'since-consent-reset') fail();
  const caps = capabilities(w.capabilities,body.backend);
  const counters = Object.fromEntries(Object.entries(counterKeys).map(([k,keys])=>[k,counts(w.counters[k],keys)]));
  const delivery = {scope:w.delivery.scope,...Object.fromEntries(['summary','source','binding','sparse','compact']
    .map(k=>[k,counts(w.delivery[k],['accepted','transportFailed','httpRejected'])]))};
  const pending = counts(w.pending,['summary','source','binding','sparseFrames']);
  if (pending.summary > 2048 || pending.source > 8192 || pending.binding > 64 || pending.sparseFrames > 32) fail();
  const frames = w.frames.map(v=>frame(v,w.frameSpan));
  if (frames.some((v,i)=>i && frames[i-1].offset >= v.offset) ||
      (w.complete && (w.frameSpan !== 32 || frames.length !== 32 || frames.some((v,i)=>v.offset !== i)))) fail();
  const pairs = w.pairs.map(v=>{
    if (!exact(v,['first','last','record']) || !int(v.first,0,w.frameSpan-1) || !int(v.last,v.first,w.frameSpan-1)) fail();
    return {first:v.first,last:v.last,record:record(v.record,2,body,normalize)};
  });
  const bindings = w.bindings.map(v=>{
    if (!exact(v,['offset','record']) || !int(v.offset,0,w.frameSpan-1)) fail();
    const r = record(v.record,3,body,normalize);
    if (!caps.bindingPairs.includes(`${r.vs}:${r.ps}`) || r.texture.slot !== caps.bindingTextureSlot) fail();
    return {offset:v.offset,record:r};
  });
  // Offsets can name draws whose end-frame observation was dropped, but must
  // remain inside the sampled span; the completeness counters expose that gap.
  const diagnostic = JSON.stringify({schema:4,namespace:'renderer-byte-fnv1a64',
    build:body.build,backend:body.backend,gpu:body.gpu,driver:body.driver,
    capabilities:caps,complete:w.complete,frameSpan:w.frameSpan,counters,delivery,pending,frames,pairs,bindings});
  // The limit applies to the request; adding the namespace during canonical
  // storage must not reject an otherwise valid request at the exact boundary.
  if (new TextEncoder().encode(JSON.stringify(body)).length > 32768) fail();
  return [{diagnostic,draws:1}];
}
