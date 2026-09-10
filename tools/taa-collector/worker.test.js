import test from 'node:test';
import assert from 'node:assert/strict';
import worker, {normalize,digest} from './worker.js';
const record={vs:'87a76ceaf1eaec11',ps:'0000000000000000',width:3840,height:2160,slot:-1,candidates:32,flags:19,rejection:0,draws:200};
const body={schema:1,build:'0.5.0-taa-collection-1',backend:'vulkan',gpu:'NVIDIA GeForce RTX 5080',driver:'2584608768',records:[record]};
const position={version:1,kind:1,slot:7,issues:17,outputs:5};
const recordV2={...record,position,guards:23};
const bodyV2={...body,schema:2,records:[recordV2]};
test('schema 1 canonical bytes and hash remain pinned',async()=>{
 const diagnostic='{"schema":1,"namespace":"renderer-byte-fnv1a64","build":"0.5.0-taa-collection-1","backend":"vulkan","gpu":"NVIDIA GeForce RTX 5080","driver":"2584608768","vs":"87a76ceaf1eaec11","ps":"0000000000000000","width":3840,"height":2160,"slot":-1,"candidates":32,"flags":19,"rejection":0}';
 assert.equal(normalize(body)[0].diagnostic,diagnostic);
 assert.equal(await digest(diagnostic),'de9b8f5948bf0ce9c855a5ebd2310c62d6aad00f60a386feb02a3300e20533d8');
});
test('dedup key ignores count and property order, preserves backend and shader state',async()=>{
 const a=normalize(body)[0];const b=normalize({...body,records:[{...record,draws:999}]})[0];
 assert.equal(await digest(a.diagnostic),await digest(b.diagnostic));
 assert.notEqual(a.diagnostic,normalize({...body,backend:'d3d12'})[0].diagnostic);
 assert.notEqual(a.diagnostic,normalize({...body,records:[{...record,candidates:0}]})[0].diagnostic);
});
test('schema 2 canonicalization ignores input order and draws but separates position evidence',async()=>{
 const a=normalize(bodyV2)[0];
 const reordered={records:[{guards:23,position:{outputs:5,issues:17,slot:7,kind:1,version:1},draws:999,rejection:0,flags:19,candidates:32,slot:-1,height:2160,width:3840,ps:'0000000000000000',vs:'87a76ceaf1eaec11'}],driver:body.driver,gpu:body.gpu,backend:body.backend,build:body.build,schema:2};
 const b=normalize(reordered)[0];
 assert.equal(a.diagnostic,b.diagnostic);
 assert.equal(await digest(a.diagnostic),await digest(b.diagnostic));
 assert.notEqual(await digest(a.diagnostic),await digest(normalize({...bodyV2,records:[{...recordV2,position:{...position,outputs:7}}]})[0].diagnostic));
 assert.notEqual(await digest(a.diagnostic),await digest(normalize({...bodyV2,records:[{...recordV2,guards:22}]})[0].diagnostic));
});
test('rejects raw logs, paths, extra fields, invalid counters and excess records',()=>{
 for(const bad of [{...body,build:['bad']},{...body,path:'C:/Users/test'}, {...body,gpu:'C:/Users/test'}, {...body,records:Array(33).fill(record)},
 {...body,records:[{...record,draws:-1}]},{...body,records:[{...record,source:'hlsl'}]}])assert.throws(()=>normalize(bad));
});
test('rejects unknown schemas, cross-schema fields, extras and invalid position bounds',()=>{
 for(const bad of [
  {...body,schema:3},
  {...body,records:[recordV2]},
  {...bodyV2,records:[record]},
  {...bodyV2,raw:'log'},
  {...bodyV2,records:[{...recordV2,source:'hlsl'}]},
  {...bodyV2,records:[{...recordV2,position:{...position,raw:'expr'}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,version:2}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,kind:0}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,kind:2}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,slot:253}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,issues:64}}]},
  {...bodyV2,records:[{...recordV2,position:{...position,outputs:65536}}]},
  {...bodyV2,records:[{...recordV2,guards:32}]}
 ]) assert.throws(()=>normalize(bad));
 assert.doesNotThrow(()=>normalize({...bodyV2,records:[{...recordV2,position:{...position,kind:0,slot:-1}}]}));
 assert.doesNotThrow(()=>normalize({...bodyV2,records:[{...recordV2,position:{...position,kind:2,slot:-1}}]}));
});
test('bounded body and rate limit reject before database writes',async()=>{
 const env={UPLOAD_LIMIT:{limit:async()=>({success:true})},DB:{batch:()=>{throw Error('must not write')}}};
 let req=new Request('https://lo.dotslash.pro/v1/taa',{method:'POST',headers:{'Content-Type':'application/json'},body:' '.repeat(65537)});
 assert.equal((await worker.fetch(req,env)).status,413);
 env.UPLOAD_LIMIT.limit=async()=>({success:false});
 req=new Request('https://lo.dotslash.pro/v1/taa',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
 assert.equal((await worker.fetch(req,env)).status,429);
});
test('handler stores schema 1 and 2 through the same D1 batch and rejects invalid HTTP payloads',async()=>{
 const batches=[];
 const env={UPLOAD_LIMIT:{limit:async()=>({success:true})},DB:{
  prepare(sql){return {bind(...values){return {sql,values};}};},
  async batch(statements){batches.push(statements);}
 }};
 for(const payload of [body,bodyV2]){
  const req=new Request('https://lo.dotslash.pro/v1/taa',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
  const response=await worker.fetch(req,env);
  assert.equal(response.status,200);
  assert.deepEqual(await response.json(),{accepted:1});
 }
 assert.equal(batches.length,2);
 assert.match(batches[0][0].sql,/INSERT INTO observations/);
 assert.equal(batches[0][0].sql,batches[1][0].sql);
 assert.equal(batches[0][0].values[1],normalize(body)[0].diagnostic);
 assert.equal(batches[1][0].values[1],normalize(bodyV2)[0].diagnostic);
 for(const [payload,headers={}] of [[{...body,schema:3}], [body,{'Content-Encoding':'gzip'}]]){
  const req=new Request('https://lo.dotslash.pro/v1/taa',{method:'POST',headers:{'Content-Type':'application/json',...headers},body:JSON.stringify(payload)});
  assert.equal((await worker.fetch(req,env)).status,headers['Content-Encoding'] ? 415 : 400);
 }
 assert.equal(batches.length,2);
 const health=await worker.fetch(new Request('https://lo.dotslash.pro/health'),env);
 assert.deepEqual(await health.json(),{service:'lost-odyssey-taa-collector',schema:1,schemas:[1,2,3,4],temporal:1,shaderSources:1});
});
