const STRIDE=4824, RAW_BYTES=16+32*STRIDE, MAX_BYTES=256*1024;
const text=b=>new TextDecoder().decode(b);
export function decodeTemporal(bytes) {
  let raw;
  const magic=text(bytes.subarray(0,4));
  if(magic==='LOR1') {if(bytes.length!==RAW_BYTES+4)throw Error('size');raw=bytes.slice(4);}
  else if(magic==='LOZ1') {
    raw=new Uint8Array(RAW_BYTES);let p=4,o=0;
    while(p<bytes.length){const token=bytes[p++],n=(token&127)+1;if(o+n>raw.length)throw Error('expansion');
      if(!(token&128)){if(p+n>bytes.length)throw Error('truncated');raw.set(bytes.subarray(p,p+n),o);p+=n;}o+=n;}
    if(o!==raw.length)throw Error('size');
    for(let i=16+STRIDE;i<raw.length;i++)raw[i]^=raw[i-STRIDE];
  } else throw Error('codec');
  const v=new DataView(raw.buffer,raw.byteOffset,raw.byteLength);
  if(text(raw.subarray(0,4))!=='LOMV'||v.getUint32(4,true)!==1||v.getUint32(8,true)!==32||v.getUint32(12,true)!==STRIDE)throw Error('header');
  const summary={frames:32,grid:[32,18],motion:'camera-only',units:'pixels-previous-minus-current-unjittered',depth:'reverse-nonlinear',validMotion:0,invalidDepth:0,maxAbsMotion:0,jitterMin:[Infinity,Infinity],jitterMax:[-Infinity,-Infinity],historyReusableFrames:0};
  const first=v.getBigUint64(16,true),epoch=v.getBigUint64(24,true);let lastTime=-Infinity;
  const half=b=>{const s=b&32768?-1:1,e=(b>>10)&31,m=b&1023;return e===31?NaN:s*(e===0?m*2**-24:(1+m/1024)*2**(e-15));};
  for(let f=0;f<32;f++) {
    const o=16+f*STRIDE,w=v.getUint32(o+24,true),h=v.getUint32(o+28,true),flags=v.getUint32(o+32,true),t=v.getFloat64(o+16,true);
    if(v.getBigUint64(o,true)!==first+BigInt(f)||v.getBigUint64(o+8,true)!==epoch||w<32||w>7680||h<18||h>4320||flags>15||!Number.isFinite(t)||t<lastTime)throw Error('frame');
    if(f&&(w!==summary.width||h!==summary.height))throw Error('extent');lastTime=t;summary.width=w;summary.height=h;
    if(flags&2)summary.historyReusableFrames++;
    for(let j=0;j<4;j++){const x=v.getFloat32(o+36+j*4,true);if(!Number.isFinite(x)||Math.abs(x)>16)throw Error('jitter');if(j<2){summary.jitterMin[j]=Math.min(summary.jitterMin[j],x);summary.jitterMax[j]=Math.max(summary.jitterMax[j],x);}}
    for(let i=52;i<212;i+=4)if(!Number.isFinite(v.getFloat32(o+i,true)))throw Error('camera');
    if(v.getUint32(o+212,true)!==0)throw Error('reserved');
    for(let i=0;i<576;i++){const p=o+216+i*8,d=v.getFloat32(p+4,true),x=half(v.getUint16(p,true)),y=half(v.getUint16(p+2,true));
      if(!Number.isFinite(d)||d<=0||d>1)summary.invalidDepth++;
      if(Number.isFinite(x)&&Number.isFinite(y)){if(!(flags&1)||!Number.isFinite(d)||d<=0||d>1)throw Error('motion');summary.validMotion++;summary.maxAbsMotion=Math.max(summary.maxAbsMotion,Math.abs(x),Math.abs(y));}}
  }
  summary.firstFrame=first.toString();summary.lastFrame=(first+31n).toString();summary.rawBytes=RAW_BYTES;summary.packedBytes=bytes.length;
  return {raw,summary};
}
export async function temporalRequest(request,env) {
  const reply=(x,s=200)=>Response.json(x,{status:s,headers:{'Cache-Control':'no-store'}});
  if(request.method!=='POST')return reply({error:'method'},405);
  if(request.headers.get('Content-Type')!=='application/octet-stream'||request.headers.has('Content-Encoding'))return reply({error:'content_type'},415);
  if(!(await env.UPLOAD_LIMIT.limit({key:request.headers.get('CF-Connecting-IP')||'unknown'})).success)return reply({error:'rate_limit'},429);
  if(Number(request.headers.get('Content-Length'))>MAX_BYTES)return reply({error:'too_large'},413);
  let packed,decoded,meta;
  try {
    meta={build:request.headers.get('X-LO-Build'),backend:request.headers.get('X-LO-Backend'),gpu:request.headers.get('X-LO-GPU'),driver:request.headers.get('X-LO-Driver')};
    if(!meta.build||!/^[a-zA-Z0-9._-]{1,80}$/.test(meta.build)||!['d3d12','vulkan'].includes(meta.backend)||!meta.gpu||!/^[a-zA-Z0-9 ()_.+-]{1,100}$/.test(meta.gpu)||!meta.driver||!/^\d{1,20}$/.test(meta.driver))throw Error('metadata');
    const reader=request.body?.getReader();if(!reader)throw Error('body');let size=0;const parts=[];
    for(;;){const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>MAX_BYTES){await reader.cancel();return reply({error:'too_large'},413);}parts.push(value);}
    packed=new Uint8Array(size);let offset=0;for(const p of parts){packed.set(p,offset);offset+=p.length;}
    decoded=decodeTemporal(packed);
  } catch{return reply({error:'invalid_payload'},400);}
  try {
    const prefix=new TextEncoder().encode(JSON.stringify(meta));const canonical=new Uint8Array(prefix.length+decoded.raw.length);canonical.set(prefix);canonical.set(decoded.raw,prefix.length);
    const id=[...new Uint8Array(await crypto.subtle.digest('SHA-256',canonical))].map(x=>x.toString(16).padStart(2,'0')).join('');
    const now=Math.floor(Date.now()/1000);
    await env.DB.prepare('INSERT INTO temporal_sequences(id,metadata,summary,payload,first_seen,last_seen) VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET last_seen=excluded.last_seen')
      .bind(id,JSON.stringify(meta),JSON.stringify(decoded.summary),packed.buffer,now,now).run();
    console.log(JSON.stringify({event:'temporal_accepted',id,bytes:packed.length,validMotion:decoded.summary.validMotion}));
    return reply({accepted:1,id,bytes:packed.length});
  }catch{console.error(JSON.stringify({event:'temporal_storage_failure'}));return reply({error:'unavailable'},503);}
}
