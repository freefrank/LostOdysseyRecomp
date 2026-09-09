import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {decodeTemporal,temporalRequest} from './temporal.js';
const packed=readFileSync(new URL('../../out/v0.5.0/performance-fix/temporal-fixture.bin',import.meta.url));
const raw=readFileSync(new URL('../../out/v0.5.0/performance-fix/temporal-fixture.raw',import.meta.url));
test('C++ packet roundtrip and camera translation statistics',()=>{
  const d=decodeTemporal(packed);assert.deepEqual(Buffer.from(d.raw),raw);
  assert.equal(d.summary.validMotion,32*576);assert.equal(d.summary.maxAbsMotion,1);
  assert.equal(d.summary.invalidDepth,0);assert.deepEqual(d.summary.jitterMin,[-.25,0]);assert.deepEqual(d.summary.jitterMax,[.25,0]);
  assert.deepEqual(Buffer.from(decodeTemporal(Buffer.concat([Buffer.from('LOR1'),raw])).raw),raw);
});
test('reject truncated runs, decompression overflow, invalid frame and jitter',()=>{
  assert.throws(()=>decodeTemporal(packed.subarray(0,packed.length-1)));
  assert.throws(()=>decodeTemporal(Buffer.concat([packed,Buffer.from([255])])));
  for(const [offset,value] of [[16+24,0],[16+36,0x7f800000]]) {
    const b=Buffer.from(raw);b.writeUInt32LE(value,offset);assert.throws(()=>decodeTemporal(Buffer.concat([Buffer.from('LOR1'),b])));
  }
});
const request=(bytes=packed)=>new Request('https://lo.dotslash.pro/v1/temporal',{method:'POST',headers:{'Content-Type':'application/octet-stream','X-LO-Build':'probe-temporal-1','X-LO-Backend':'vulkan','X-LO-GPU':'Synthetic','X-LO-Driver':'0'},body:bytes});
test('both encodings share content hash; bounded request before storage',async()=>{
  const rows=[];const env={UPLOAD_LIMIT:{limit:async()=>({success:true})},DB:{prepare:()=>({bind:(...args)=>({run:async()=>rows.push(args)})})}};
  assert.equal((await temporalRequest(request(),env)).status,200);
  assert.equal((await temporalRequest(request(Buffer.concat([Buffer.from('LOR1'),raw])),env)).status,200);
  assert.equal(rows[0][0],rows[1][0]);assert.ok(rows[0][3] instanceof ArrayBuffer);
  assert.equal((await temporalRequest(request(new Uint8Array(256*1024+1)),env)).status,413);assert.equal(rows.length,2);
});
