import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {DatabaseSync} from 'node:sqlite';
import worker from './worker.js';
import {rendererHash,normalizeShaderSources,expireShaderSources} from './shader-source.js';

const MAX_BYTES = 256 * 1024;
const fnvReference = bytes => {
  let hash = 0xcbf29ce484222325n;
  for (const byte of bytes) hash = BigInt.asUintN(64, (hash ^ BigInt(byte)) * 0x100000001b3n);
  return hash.toString(16).padStart(16, '0');
};
const program = (bytes=Buffer.from([0,1,2,3]),stage='vs') => ({stage,hash:fnvReference(bytes),data:bytes.toString('base64')});
const payload = (programs=[program()]) => ({schema:1,build:'0.5.0-shader-sources-1',backend:'d3d12',gpu:'NVIDIA GeForce RTX 5080',driver:'2584608768',programs});
const request = (body=payload(), headers={}, method='POST') => new Request('https://lo.dotslash.pro/v1/shader-sources', {
  method, headers:{'Content-Type':'application/json',...headers}, ...(method==='POST' ? {body:typeof body==='string' || body instanceof Uint8Array ? body : JSON.stringify(body)} : {})
});

// Executes the endpoint's real parameterized SQL, with D1's documented batch
// transaction behavior and per-statement parameter bound enforced by the adapter.
function database(t) {
  const sqlite = new DatabaseSync(':memory:');
  t.after(() => sqlite.close());
  sqlite.exec('PRAGMA foreign_keys=ON');
  for (const schema of ['schema.sql','temporal-schema.sql','shader-source-schema.sql'])
    sqlite.exec(readFileSync(new URL(schema,import.meta.url), 'utf8'));
  const adapter = {
    batches:[], failAt:-1,
    prepare(sql) {
      const statement = {sql,values:[],bind(...values) {return {...this,values};},
        async run() {return sqlite.prepare(this.sql).run(...this.values.map(v => v instanceof ArrayBuffer ? new Uint8Array(v) : v));}};
      return statement;
    },
    async batch(statements) {
      this.batches.push(statements);
      sqlite.exec('BEGIN IMMEDIATE');
      try {
        const results = statements.map((statement,index) => {
          assert.ok(statement.values.length <= 100, 'D1 bound parameter limit');
          if (index===this.failAt) throw Error('injected storage failure');
          return sqlite.prepare(statement.sql).run(...statement.values.map(v => v instanceof ArrayBuffer ? new Uint8Array(v) : v));
        });
        sqlite.exec('COMMIT');
        return results;
      } catch (error) {sqlite.exec('ROLLBACK');throw error;}
    }
  };
  return {sqlite,adapter,env:{DB:adapter,UPLOAD_LIMIT:{limit:async()=>({success:true})}}};
}
const count = (sqlite,table) => sqlite.prepare(`SELECT COUNT(*) AS n FROM ${table}`).get().n;

test('renderer byte hash matches standard FNV reference including overflow and maximum bytes',()=>{
  assert.equal(rendererHash(new Uint8Array()),'cbf29ce484222325');
  for (const size of [4,12,256,65536]) {
    const bytes = Uint8Array.from({length:size},(_,i)=>(i*137+251)%256);
    assert.equal(rendererHash(bytes),fnvReference(bytes));
  }
});

test('stores original bytes and stage/SHA identity independently of hardware metadata',async t=>{
  const {sqlite,env}=database(t);
  const bytes=Buffer.from([0,1,2,3]), p=program(bytes), sha256=createHash('sha256').update(bytes).digest('hex');
  const response=await worker.fetch(request(payload([p,p,program(bytes,'ps')])),env);
  assert.equal(response.status,200);
  assert.deepEqual(await response.json(),{accepted:3,programs:[{stage:'vs',hash:p.hash,sha256},{stage:'vs',hash:p.hash,sha256},{stage:'ps',hash:p.hash,sha256}]});
  assert.equal(count(sqlite,'shader_sources'),2);
  assert.equal(count(sqlite,'shader_source_observations'),2);
  for (const row of sqlite.prepare('SELECT * FROM shader_sources').all()) {
    assert.equal(row.sha256,sha256);assert.equal(row.renderer_hash,p.hash);assert.deepEqual(Buffer.from(row.payload),bytes);
  }
  assert.equal(response.headers.get('Cache-Control'),'no-store');
});

test('concurrent retries are idempotent while GPU models, backend, build and driver remain distinct',async t=>{
  const {sqlite,env}=database(t);
  const body=payload();
  const variants=[body,{...body,gpu:'  nvidia  geforce RTX 5080 '},{...body,gpu:'NVIDIA GeForce RTX 4060 Ti'},
    {...body,backend:'vulkan'},{...body,driver:'2584608769'},{...body,build:'0.5.0-shader-sources-2'}];
  const responses=await Promise.all([...variants,...variants].map(b=>worker.fetch(request(b),env)));
  assert.ok(responses.every(r=>r.status===200));
  assert.equal(count(sqlite,'shader_sources'),1);
  assert.equal(count(sqlite,'shader_source_observations'),5);
  assert.deepEqual(sqlite.prepare('SELECT DISTINCT gpu_key FROM shader_source_observations ORDER BY gpu_key').all().map(r=>r.gpu_key),
    ['nvidia geforce rtx 4060 ti','nvidia geforce rtx 5080']);
});

test('accepts exactly 32 programs with bounded statements and a 64 KiB program',async t=>{
  const {sqlite,adapter,env}=database(t);
  const programs=Array.from({length:32},(_,i)=>program(Buffer.from([i,0,0,0]),i%2 ? 'ps' : 'vs'));
  assert.equal((await worker.fetch(request(payload(programs)),env)).status,200);
  assert.equal(count(sqlite,'shader_sources'),32);
  assert.equal(adapter.batches[0].length,8);
  assert.equal((await worker.fetch(request(payload([program(Buffer.alloc(65536,255))])),env)).status,200);
  assert.equal(count(sqlite,'shader_sources'),33);
});

test('strict allowlists, metadata, stage, renderer hash and canonical base64 reject before writes',async t=>{
  const {sqlite,adapter,env}=database(t), base=payload(), p=program();
  const invalid=[null,[],{...base,schema:2},{...base,programs:[]},{...base,programs:Array(33).fill(p)},
    {...base,backend:'dx11'},{...base,gpu:'  '},{...base,gpu:'GPU/serial'},{...base,driver:123},{...base,build:'path/name'},
    {...base,programs:[{...p,stage:'VS'}]},{...base,programs:[{...p,hash:'0000000000000000'}]},
    {...base,programs:[{...p,data:''}]},{...base,programs:[{...p,data:'AAAA'}]},
    {...base,programs:[{...p,data:'AAAAAB=='}]},{...base,programs:[{...p,data:'AAECAw'}]},
    {...base,programs:[{...p,data:'AAECAw==\n'}]},{...base,programs:[{...p,data:'AAE-__=='}]},
    {...base,programs:[program(Buffer.alloc(65540))]},
    {...base,programs:[p,{...p,data:'bad'}]}];
  for (const field of ['serial','uuid','user','session','path','ip','source','hlsl']) {
    invalid.push({...base,[field]:'unwanted'}, {...base,programs:[{...p,[field]:'unwanted'}]});
  }
  for (const body of invalid) assert.equal((await worker.fetch(request(body),env)).status,400,JSON.stringify(body)?.slice(0,150));
  assert.equal(adapter.batches.length,0);assert.equal(count(sqlite,'shader_sources'),0);
});

test('HTTP limits validate actual stream length, type, UTF8, methods and rate limit before storage',async t=>{
  const {adapter,env}=database(t), json=JSON.stringify(payload());
  assert.equal((await worker.fetch(request(json+' '.repeat(MAX_BYTES-json.length)),env)).status,200);
  const before=adapter.batches.length;
  assert.equal((await worker.fetch(request(' '.repeat(MAX_BYTES+1)),env)).status,413);
  assert.equal((await worker.fetch(request(payload(),{'Content-Length':String(MAX_BYTES+1)}),env)).status,413);
  assert.equal((await worker.fetch(request(payload(),{'Content-Encoding':'gzip'}),env)).status,415);
  assert.equal((await worker.fetch(request(payload(),{'Content-Type':'application/json-bogus'}),env)).status,415);
  assert.equal((await worker.fetch(request(payload(),{},'GET'),env)).status,405);
  assert.equal((await worker.fetch(request(new Uint8Array([255])),env)).status,400);
  assert.equal((await worker.fetch(request('{'),env)).status,400);
  env.UPLOAD_LIMIT.limit=async()=>({success:false});
  assert.equal((await worker.fetch(request(),env)).status,429);
  assert.equal(adapter.batches.length,before);
});

test('storage failure rolls back every program and returns retryable failure',async t=>{
  const {sqlite,adapter,env}=database(t);
  adapter.failAt=3;
  const body=payload(Array.from({length:12},(_,i)=>program(Buffer.from([i,1,2,3]))));
  assert.equal((await worker.fetch(request(body),env)).status,503);
  assert.equal(count(sqlite,'shader_sources'),0);assert.equal(count(sqlite,'shader_source_observations'),0);
  adapter.failAt=-1;
  assert.equal((await worker.fetch(request(body),env)).status,200);
  assert.equal(count(sqlite,'shader_sources'),12);
});

test('retention removes expired hardware references, keeps shared content, then removes orphan bytes',async t=>{
  const {sqlite,adapter,env}=database(t);
  await worker.fetch(request(payload()),env);
  await worker.fetch(request({...payload(),gpu:'AMD Radeon RX 9060 XT'}),env);
  sqlite.prepare("UPDATE shader_source_observations SET last_seen=10 WHERE gpu_key LIKE 'nvidia%'").run();
  await expireShaderSources(adapter,11);
  assert.equal(count(sqlite,'shader_source_observations'),1);assert.equal(count(sqlite,'shader_sources'),1);
  sqlite.prepare('UPDATE shader_source_observations SET last_seen=10').run();
  await worker.scheduled({},env);
  assert.equal(count(sqlite,'shader_source_observations'),0);assert.equal(count(sqlite,'shader_sources'),0);
});

test('normalization never retains request identity metadata and there is no public source download',async t=>{
  const {sqlite,env}=database(t), keys=[];
  env.UPLOAD_LIMIT.limit=async arg=>{keys.push(arg.key);return {success:true};};
  const res=await worker.fetch(request(payload(),{'CF-Connecting-IP':'192.0.2.1','X-Forwarded-For':'192.0.2.2','User-Agent':'private-agent'}),env);
  assert.equal(res.status,200);assert.deepEqual(keys,['192.0.2.1']);
  const metadata=(await normalizeShaderSources(payload())).metadata;
  assert.deepEqual(Object.keys(metadata),['build','backend','gpu','gpuKey','driver']);
  const observations=sqlite.prepare('SELECT * FROM shader_source_observations').all();
  assert.doesNotMatch(JSON.stringify(observations),/192\.0\.2\.|private-agent/);
  assert.equal((await worker.fetch(new Request('https://lo.dotslash.pro/v1/shader-sources/example'),env)).status,404);
  const landing=await worker.fetch(new Request('https://lo.dotslash.pro/'),env);
  assert.match(await landing.text(),/original VS\/PS microcode/);
});
