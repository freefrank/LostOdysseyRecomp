import test from 'node:test';
import assert from 'node:assert/strict';
import worker, {normalize,digest} from './worker.js';

const transform = () => ({slot:7,phase:3,applied:true,
  guestVP:[1065353216,0,0,0,0,1065353216,0,0,0,0,1065353216,0,0,0,0,1065353216],
  uploadedVP:[1065353216,0,0,0,0,1065353216,0,0,0,0,1065353216,0,3120562176,974078464,0,1065353216],
  viewport:[0,0,1164967936,1158086656],jitterNdc:[3120562176,974078464]});
const record = () => ({vs:'e810cfacc107fd3c',ps:'ddba24ad35effb9c',width:3840,height:2160,
  slot:-1,candidates:8,flags:3,rejection:0,draws:200,
  position:{version:1,kind:1,slot:7,issues:0,outputs:17},guards:31,
  consumer:transform(),psC0:[1056964608,3204448256,1056964608,1056964608],
  texture:{slot:0,kind:1,bank:0,guestFormat:6,hostFormat:28,dimension:1,swizzle:1672,
    sourceMip:0,sign:0,swapRedBlue:false,sampler:[2,2,2,1,1,0],guestExtent:[1280,720],hostExtent:[3840,2160],
    parentExtent:[3840,2160],resolveRect:[0,0,3840,2160],producerFrameAge:0,resolveFrameAge:0,
    resolveGap:3,producerState:2,producerDraws:145,producer:transform()}});
const body = () => ({schema:3,build:'0.5.2-taa-bindings-1',backend:'vulkan',
  gpu:'NVIDIA GeForce RTX 5080',driver:'2584608768',records:[record()]});
const request = (payload, headers = {}) => new Request('https://lo.dotslash.pro/v1/taa',
  {method:'POST',headers:{'Content-Type':'application/json',...headers},
    body:typeof payload === 'string' ? payload : JSON.stringify(payload)});
function envWithBatches() {
  const batches=[];
  return {batches,UPLOAD_LIMIT:{limit:async()=>({success:true})},DB:{
    prepare(sql) {return {bind(...values) {return {sql,values};}};},
    async batch(statements) {batches.push(statements);}
  }};
}
function reverseProperties(value) {
  if (Array.isArray(value)) return value.map(reverseProperties);
  if (value && typeof value === 'object') return Object.fromEntries(Object.entries(value)
    .reverse().map(([key,item])=>[key,reverseProperties(item)]));
  return value;
}

test('binding canonicalization preserves IEEE bits and ignores property order and consumer draw count',async()=>{
  const a=normalize(body())[0];
  const reordered=reverseProperties(body());reordered.records[0].draws=999;
  const b=normalize(reordered)[0];
  assert.equal(a.diagnostic,b.diagnostic);
  assert.equal(await digest(a.diagnostic),await digest(b.diagnostic));
  const parsed=JSON.parse(a.diagnostic);
  assert.equal(parsed.schema,3);
  assert.equal(parsed.namespace,'renderer-byte-fnv1a64');
  assert.equal(Object.hasOwn(parsed,'draws'),false);
  assert.deepEqual(parsed.consumer,transform());
  assert.deepEqual(parsed.psC0,record().psC0);
  assert.deepEqual(parsed.texture,record().texture);
  assert.equal(a.draws,200);assert.equal(b.draws,999);
});

test('content key distinguishes source provenance, temporal alignment, sampler and actual upload bits',async()=>{
  const base=normalize(body())[0].diagnostic;
  const changes=[
    r=>r.consumer.uploadedVP[12]++,r=>r.consumer.phase++,r=>r.psC0[0]++,
    r=>r.texture.producer.guestVP[0]++,r=>r.texture.producerDraws++,
    r=>r.texture.producerFrameAge++,r=>r.texture.resolveFrameAge++,r=>r.texture.resolveGap++,
    r=>r.texture.parentExtent[0]++,r=>r.texture.resolveRect[0]++,r=>r.texture.sampler[0]++,
    r=>r.texture.bank=1,r=>r.texture.sourceMip++,r=>r.texture.sign++,r=>r.texture.swapRedBlue=true,
    r=>{r.texture.producerState=1;r.texture.producer.applied=false;},
    r=>r.texture.producerState=3
  ];
  for (const change of changes) {
    const candidate=body();change(candidate.records[0]);
    assert.notEqual(await digest(normalize(candidate)[0].diagnostic),await digest(base));
  }
});

test('schema 3 rejects extra fields at every depth and metadata outside the fixed contract',()=>{
  for (const target of [b=>b,b=>b.records[0],b=>b.records[0].position,
    b=>b.records[0].consumer,b=>b.records[0].texture,b=>b.records[0].texture.producer]) {
    const candidate=body();target(candidate).serial='sensitive';assert.throws(()=>normalize(candidate));
    const missing=body();delete target(missing)[Object.keys(target(missing))[0]];assert.throws(()=>normalize(missing));
  }
  for (const fields of [{schema:4},{build:'0.5.2'},{build:'0.5.2-taa-bindings-1/path'},
    {gpu:'C:/Users/player'},{gpu:'mail@example.com'},{gpu:['GPU']},{gpu:''},
    {driver:'serial-number'},{driver:'1'.repeat(21)},{backend:'other'},
    {records:[]},{records:Array.from({length:9},record)}]) {
    assert.throws(()=>normalize({...body(),...fields}));
  }
  assert.doesNotThrow(()=>normalize({...body(),records:Array.from({length:8},record)}));
});

test('schema 3 enforces all texture numeric bounds and producer applied consistency',()=>{
  const ranges={slot:[0,31],kind:[0,6],bank:[0,2],guestFormat:[0,63],hostFormat:[0,65535],
    dimension:[0,3],swizzle:[0,4095],sourceMip:[0,15],sign:[0,255],
    producerFrameAge:[-1,255],resolveFrameAge:[-1,255],resolveGap:[-1,65535],
    producerState:[0,3],producerDraws:[0,1000000000]};
  for (const [field,[lo,hi]] of Object.entries(ranges)) {
    for (const value of [lo-1,hi+1,0.5,'0',null,NaN,Infinity]) {
      const candidate=body();candidate.records[0].texture[field]=value;
      assert.throws(()=>normalize(candidate),`${field}=${value}`);
    }
    for (const value of [lo,hi]) {
      const candidate=body();candidate.records[0].texture[field]=value;
      if(field==='producerState')candidate.records[0].texture.producer.applied=value!==1;
      assert.doesNotThrow(()=>normalize(candidate),`${field}=${value}`);
    }
  }
  for (const [state,applied] of [[1,true],[2,false]]) {
    const candidate=body();candidate.records[0].texture.producerState=state;
    candidate.records[0].texture.producer.applied=applied;assert.throws(()=>normalize(candidate));
  }
  for (const value of [0,1,null,'false','true',[],{}]) {
    const candidate=body();candidate.records[0].texture.swapRedBlue=value;
    assert.throws(()=>normalize(candidate));
  }
  for (const state of [0,3]) for (const applied of [false,true]) {
    const candidate=body();candidate.records[0].texture.producerState=state;
    candidate.records[0].texture.producer.applied=applied;assert.doesNotThrow(()=>normalize(candidate));
  }
});

test('schema 3 requires exact dense integer vector lengths without float coercion',()=>{
  const paths=[['psC0',4,0xffffffff],['consumer.guestVP',16,0xffffffff],
    ['consumer.uploadedVP',16,0xffffffff],['consumer.viewport',4,0xffffffff],
    ['consumer.jitterNdc',2,0xffffffff],['texture.producer.guestVP',16,0xffffffff],
    ['texture.producer.uploadedVP',16,0xffffffff],['texture.producer.viewport',4,0xffffffff],
    ['texture.producer.jitterNdc',2,0xffffffff],['texture.sampler',6,7],
    ['texture.guestExtent',2,16384],['texture.hostExtent',2,16384],
    ['texture.parentExtent',2,16384],['texture.resolveRect',4,16384]];
  for (const [path,length,hi] of paths) {
    const parts=path.split('.');const field=parts.pop();
    for (const values of [Array(length-1).fill(0),Array(length+1).fill(0),Array(length),
      Array(length).fill(-1),Array(length).fill(hi+1),Array(length).fill(1.5),
      Array(length).fill('0'),Array(length).fill(null),Array(length).fill(NaN),{}]) {
      const candidate=body();const target=parts.reduce((value,key)=>value[key],candidate.records[0]);
      target[field]=values;assert.throws(()=>normalize(candidate),path);
    }
    const candidate=body();parts.reduce((value,key)=>value[key],candidate.records[0])[field]=Array(length).fill(hi);
    assert.doesNotThrow(()=>normalize(candidate),path);
  }
  for (const get of [r=>r.consumer,r=>r.texture.producer]) {
    for (const [field,values] of [['slot',[-2,253,0.5,'7']],['phase',[-1,33,0.5,'3']],['applied',[0,1,null,'true']]]) {
      for(const value of values) {const candidate=body();get(candidate.records[0])[field]=value;assert.throws(()=>normalize(candidate));}
    }
  }
});

test('schema 3 keeps schema 2 hash, dimension, position and flag validation',()=>{
  for (const fields of [{vs:'a'.repeat(64)},{ps:'ABCDEF0123456789'},{width:7681},{height:4321},
    {width:0},{height:0},{slot:5},{candidates:64},{flags:32},{rejection:256},{draws:0},
    {draws:1000000001},{guards:32}]) {
    const candidate=body();Object.assign(candidate.records[0],fields);assert.throws(()=>normalize(candidate));
  }
  for (const fields of [{version:2},{kind:3},{kind:0},{slot:-1},{slot:253},{issues:64},{outputs:65536}]) {
    const candidate=body();Object.assign(candidate.records[0].position,fields);assert.throws(()=>normalize(candidate));
  }
});

test('schema 3 uses the existing D1 upsert, excludes draws from key and separates metadata',async()=>{
  const env=envWithBatches();
  for (const [draws,backend] of [[200,'vulkan'],[999,'vulkan'],[200,'d3d12']]) {
    const payload=body();payload.records[0].draws=draws;payload.backend=backend;
    const response=await worker.fetch(request(payload),env);
    assert.equal(response.status,200);assert.deepEqual(await response.json(),{accepted:1});
  }
  const [a,b,c]=env.batches.map(batch=>batch[0]);
  assert.match(a.sql,/INSERT INTO observations/);
  assert.match(a.sql,/ON CONFLICT\(id\) DO UPDATE SET max_draws=MAX\(observations.max_draws,excluded.max_draws\),last_seen=excluded.last_seen/);
  assert.equal(a.values[0],await digest(a.values[1]));
  assert.equal(a.values[0],b.values[0]);assert.notEqual(a.values[0],c.values[0]);
  assert.equal(a.values[2],200);assert.equal(b.values[2],999);
  assert.equal(a.values[3],a.values[4]);
});

test('schema 3 rejects invalid batches, rate limiting and oversized bodies before any DB writes',async()=>{
  const env=envWithBatches();
  const invalid=body();invalid.records.push({...record(),path:'C:/Users/player'});
  for (const payload of [invalid,{...body(),records:Array.from({length:9},record)}]) {
    const response=await worker.fetch(request(payload),env);
    assert.equal(response.status,400);assert.deepEqual(await response.json(),{error:'invalid_payload'});
  }
  assert.equal((await worker.fetch(request(JSON.stringify(body())+' '.repeat(65536)),env)).status,413);
  assert.equal((await worker.fetch(request(body(),{'Content-Length':'65537'}),env)).status,413);
  assert.equal((await worker.fetch(request(body(),{'Content-Encoding':'gzip'}),env)).status,415);
  env.UPLOAD_LIMIT.limit=async()=>({success:false});
  assert.equal((await worker.fetch(request(body()),env)).status,429);
  assert.equal(env.batches.length,0);
});

test('eight bounded records fit the HTTP limit and storage errors return a fixed response',async()=>{
  const payload={...body(),records:Array.from({length:8},record)};
  assert.ok(new TextEncoder().encode(JSON.stringify(payload)).length<65536);
  const env=envWithBatches();
  const response=await worker.fetch(request(payload),env);
  assert.equal(response.status,200);assert.deepEqual(await response.json(),{accepted:8});
  assert.equal(env.batches[0].length,8);
  env.DB.batch=async()=>{throw Error('secret must never be echoed');};
  const failed=await worker.fetch(request(body()),env);
  assert.equal(failed.status,503);assert.deepEqual(await failed.json(),{error:'unavailable'});
});
