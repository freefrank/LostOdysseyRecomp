import test from 'node:test';
import assert from 'node:assert/strict';
import worker, {normalize,digest} from './worker.js';

const counts = keys => Object.fromEntries(keys.split(',').map(k=>[k,0]));
const transform = () => ({slot:7,phase:0,applied:false,guestVP:Array(16).fill(0),
  uploadedVP:Array(16).fill(0),viewport:Array(4).fill(0),jitterNdc:[0,0]});
const pair = () => ({vs:'e810cfacc107fd3c',ps:'5b11f88a8bb293df',width:1280,height:720,
  slot:-1,candidates:8,flags:3,rejection:2,draws:1,
  position:{version:1,kind:1,slot:7,issues:0,outputs:17},guards:31});
const binding = () => ({...pair(),consumer:transform(),psC0:[0,0,0,0],texture:{
  slot:0,kind:0,bank:0,guestFormat:0,hostFormat:0,dimension:0,swizzle:0,sourceMip:0,
  sign:0,swapRedBlue:false,sampler:[0,0,0,0,0,0],guestExtent:[0,0],hostExtent:[0,0],
  parentExtent:[0,0],resolveRect:[0,0,0,0],producerFrameAge:-1,resolveFrameAge:-1,
  resolveGap:-1,producerState:0,producerDraws:0,producer:transform()}});
export function fixture() {
  return {schema:4,build:'0.5.2-collection-diagnostics-1',backend:'d3d12',gpu:'Test GPU',driver:'0',records:[{
    capabilities:{version:1,runtimeVersion:'0.5.2',runtimeCommit:'d1bb801',cpuWindowFrames:32,
      cpuCooldownSeconds:180,pairCapacity:24,bindingCapacity:8,
      bindingPairs:['e810cfacc107fd3c:5b11f88a8bb293df','e810cfacc107fd3c:78a5c96b2d7eaa91'],
      bindingTextureSlot:0,bindingPerPairPerFrame:1,sourceMaxProgramBytes:65536,
      sparseSupported:true,sparseFrames:32,sparseCooldownSeconds:300,sparseWindowLinked:false,
      gpuCompletion:false,colorImages:false,sourceScope:'draw-program-observe',summaryScope:'taa-draw-observe',
      pendingWindowCapacity:1,strictAck:['compact']},
    complete:false,frameSpan:1,counters:{source:counts('queued,known,invalid,full,disabled,busy'),
      summary:counts('queued,counted,dropped,disabled,busy'),binding:counts('queued,counted,full,busy,disabled,stale'),
      sparse:counts('queued,duplicate,full,busy,disabled,stale,cooldown,discontinuous'),
      compact:counts('pairDropped,bindingDropped,lockBusy,frameDiscontinuity')},
    delivery:{scope:'since-consent-reset',...Object.fromEntries(['summary','source','binding','sparse','compact']
      .map(k=>[k,counts('accepted,transportFailed,httpRejected')]))},
    pending:{summary:1,source:2,binding:1,sparseFrames:0},
    frames:[{offset:0,taa:true,ready:true,completed:true,reused:false,sceneRejection:0,historyCaptured:true,
      historyRejection:1,cameraChecks:true,previousFrameDelta:1,sameEpoch:true,resetAfterFrame:false,sparseReady:true}],
    pairs:[{first:0,last:0,record:pair()}],bindings:[{offset:0,record:binding()}]
  }]};
}
const request = body => new Request('https://lo.dotslash.pro/v1/taa',{
  method:'POST',headers:{'Content-Type':'application/json'},body:typeof body==='string'?body:JSON.stringify(body)});
function environment() {
  const rows = new Map();
  return {rows,UPLOAD_LIMIT:{limit:async()=>({success:true})},DB:{
    prepare(sql) {return {bind(...values) {return {sql,values};}};},
    async batch(stmts) {for(const s of stmts)rows.set(s.values[0],s.values);}
  }};
}
function reverse(v) {
  return Array.isArray(v)?v.map(reverse):v&&typeof v==='object'?
    Object.fromEntries(Object.entries(v).reverse().map(([k,x])=>[k,reverse(x)])):v;
}
test('window content canonicalization links nested records and ignores JSON property order',async()=>{
  const a=normalize(fixture())[0],b=normalize(reverse(fixture()))[0];
  assert.equal(a.diagnostic,b.diagnostic);assert.equal(a.draws,1);
  const d=JSON.parse(a.diagnostic);assert.equal(d.schema,4);
  assert.equal(d.bindings[0].record.texture.producerFrameAge,-1);
  const changed=fixture();changed.records[0].frames[0].reused=true;
  assert.notEqual(await digest(a.diagnostic),await digest(normalize(changed)[0].diagnostic));
});
test('strict allowlists reject identity fields, paths and extra nested data',()=>{
  for(const get of [b=>b,b=>b.records[0],b=>b.records[0].capabilities,b=>b.records[0].counters.source,
    b=>b.records[0].frames[0],b=>b.records[0].delivery,b=>b.records[0].pending,
    b=>b.records[0].pairs[0],b=>b.records[0].bindings[0].record.texture]) {
    const b=fixture();get(b).serial='private';assert.throws(()=>normalize(b));
  }
  for(const change of [b=>b.gpu='C:/Users/person',b=>b.records[0].capabilities.runtimeCommit='host1234',
    b=>b.records[0].capabilities.colorImages=true,b=>b.records[0].capabilities.gpuCompletion=true,
    b=>b.records[0].capabilities.sparseWindowLinked=true,b=>b.records[0].capabilities.bindingPairs.push('0:0')]) {
    const b=fixture();change(b);assert.throws(()=>normalize(b));
  }
});
test('backend capabilities preserve Vulkan sparse unsupported state',()=>{
  const b=fixture();b.backend='vulkan';assert.throws(()=>normalize(b));
  b.records[0].capabilities.sparseSupported=false;assert.doesNotThrow(()=>normalize(b));
});
test('partial and complete windows enforce ordered relative offsets and bounded references',()=>{
  const b=fixture(),w=b.records[0];w.complete=true;assert.throws(()=>normalize(b));
  w.frameSpan=32;w.frames=Array.from({length:32},(_,offset)=>({...w.frames[0],offset}));
  assert.doesNotThrow(()=>normalize(b));w.frames[3].offset=2;assert.throws(()=>normalize(b));
  for(const mutate of [w=>w.pairs[0].last=1,w=>w.pairs[0].first=-1,w=>w.bindings[0].offset=1,
    w=>w.frameSpan=33,w=>w.frames=[],w=>w.pairs=Array(25).fill(w.pairs[0]),
    w=>w.bindings=Array(9).fill(w.bindings[0])]) {
    const c=fixture();mutate(c.records[0]);assert.throws(()=>normalize(c));
  }
});
test('observation counters and pending counts do not accept coercion or overflow',()=>{
  for(const bad of [-1,1.5,'1',null,4294967296]) {
    const b=fixture();b.records[0].counters.source.busy=bad;assert.throws(()=>normalize(b));
  }
  for(const [key,value] of [['summary',2049],['source',8193],['binding',65],['sparseFrames',33]]) {
    const b=fixture();b.records[0].pending[key]=value;assert.throws(()=>normalize(b));
  }
  const b=fixture();b.records[0].frames[0].historyRejection=1024;assert.throws(()=>normalize(b));
});
test('nested records preserve existing position/binding bounds and exact pair coverage',()=>{
  for(const mutate of [w=>w.pairs[0].record.position.issues=64,w=>w.pairs[0].record.vs='bad',
    w=>w.bindings[0].record.ps='0000000000000000',w=>w.bindings[0].record.texture.slot=1,
    w=>w.bindings[0].record.consumer.guestVP[0]=-1]) {
    const b=fixture();mutate(b.records[0]);assert.throws(()=>normalize(b));
  }
});
test('HTTP receipts identify committed canonical content and identical retry deduplicates',async()=>{
  const env=environment();const expected=await digest(normalize(fixture())[0].diagnostic);
  for(const body of [fixture(),reverse(fixture())]) {
    const r=await worker.fetch(request(body),env);assert.equal(r.status,200);
    assert.deepEqual(await r.json(),{accepted:1,id:expected});
  }
  assert.equal(env.rows.size,1);assert.equal(env.rows.get(expected)[2],1);
});
test('compact HTTP input is bounded at 32KiB and failures never acknowledge',async()=>{
  const env=environment(),raw=JSON.stringify(fixture());
  assert.equal((await worker.fetch(request(raw+' '.repeat(32769-raw.length)),env)).status,413);
  assert.equal(env.rows.size,0);
  const invalid=fixture();invalid.records[0].frames[0].path='private';
  assert.equal((await worker.fetch(request(invalid),env)).status,400);
  env.UPLOAD_LIMIT.limit=async()=>({success:false});
  assert.equal((await worker.fetch(request(fixture()),env)).status,429);
  env.UPLOAD_LIMIT.limit=async()=>({success:true});env.DB.batch=async()=>{throw Error('offline');};
  const r=await worker.fetch(request(fixture()),env);assert.equal(r.status,503);
  assert.deepEqual(await r.json(),{error:'unavailable'});
});
test('health advertises schema4 while old-schema receipt stays compatible',async()=>{
  const env=environment();
  const health=await worker.fetch(new Request('https://lo.dotslash.pro/health'),env);
  assert.deepEqual((await health.json()).schemas,[1,2,3,4]);
  const old={schema:2,build:'0.5.0-taa-collection-1',backend:'d3d12',gpu:'Test GPU',driver:'0',records:[pair()]};
  const r=await worker.fetch(request(old),env);assert.deepEqual(await r.json(),{accepted:1});
});
