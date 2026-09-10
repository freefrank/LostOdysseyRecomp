// Schema 3 is deliberately separate so existing schema 1/2 canonical bytes stay stable.
const integer = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
const exact = (obj, fields) => obj && typeof obj === 'object' && !Array.isArray(obj) &&
  Object.keys(obj).length === fields.length && fields.every(k => Object.hasOwn(obj, k));
const vector = (v, length, hi = 0xffffffff) => Array.isArray(v) && v.length === length &&
  Array.from(v).every(x => integer(x, 0, hi));

function transform(value) {
  if (!exact(value, ['slot','phase','applied','guestVP','uploadedVP','viewport','jitterNdc']) ||
      !integer(value.slot,-1,252) || !integer(value.phase,0,32) || typeof value.applied !== 'boolean' ||
      !vector(value.guestVP,16) || !vector(value.uploadedVP,16) ||
      !vector(value.viewport,4) || !vector(value.jitterNdc,2)) throw Error('transform');
  return {slot:value.slot,phase:value.phase,applied:value.applied,
    guestVP:[...value.guestVP],uploadedVP:[...value.uploadedVP],
    viewport:[...value.viewport],jitterNdc:[...value.jitterNdc]};
}

function texture(value) {
  if (!exact(value, ['slot','kind','bank','guestFormat','hostFormat','dimension','swizzle','sourceMip','sign','swapRedBlue',
      'sampler','guestExtent','hostExtent','parentExtent','resolveRect','producerFrameAge','resolveFrameAge',
      'resolveGap','producerState','producerDraws','producer']) ||
      !integer(value.slot,0,31) || !integer(value.kind,0,6) || !integer(value.bank,0,2) ||
      !integer(value.guestFormat,0,63) || !integer(value.hostFormat,0,65535) ||
      !integer(value.dimension,0,3) || !integer(value.swizzle,0,4095) ||
      !integer(value.sourceMip,0,15) || !integer(value.sign,0,255) || typeof value.swapRedBlue !== 'boolean' ||
      !vector(value.sampler,6,7) ||
      !vector(value.guestExtent,2,16384) || !vector(value.hostExtent,2,16384) ||
      !vector(value.parentExtent,2,16384) || !vector(value.resolveRect,4,16384) ||
      !integer(value.producerFrameAge,-1,255) || !integer(value.resolveFrameAge,-1,255) ||
      !integer(value.resolveGap,-1,65535) || !integer(value.producerState,0,3) ||
      !integer(value.producerDraws,0,1000000000)) throw Error('texture');
  const producer = transform(value.producer);
  if ((value.producerState === 1 && producer.applied) ||
      (value.producerState === 2 && !producer.applied)) throw Error('producer');
  return {slot:value.slot,kind:value.kind,bank:value.bank,guestFormat:value.guestFormat,
    hostFormat:value.hostFormat,dimension:value.dimension,swizzle:value.swizzle,
    sourceMip:value.sourceMip,sign:value.sign,swapRedBlue:value.swapRedBlue,sampler:[...value.sampler],
    guestExtent:[...value.guestExtent],hostExtent:[...value.hostExtent],parentExtent:[...value.parentExtent],
    resolveRect:[...value.resolveRect],producerFrameAge:value.producerFrameAge,
    resolveFrameAge:value.resolveFrameAge,resolveGap:value.resolveGap,
    producerState:value.producerState,producerDraws:value.producerDraws,producer};
}

export function normalizeBinding(body) {
  if (!exact(body, ['schema','build','backend','gpu','driver','records']) || body.schema !== 3 ||
      body.build !== '0.5.2-taa-bindings-1' || !['vulkan','d3d12'].includes(body.backend) ||
      typeof body.gpu !== 'string' || !/^[a-zA-Z0-9 ()_.+-]{1,100}$/.test(body.gpu) ||
      typeof body.driver !== 'string' || !/^[0-9]{1,20}$/.test(body.driver) ||
      !Array.isArray(body.records) || body.records.length < 1 || body.records.length > 8) throw Error('schema');
  return body.records.map(r => {
    if (!exact(r, ['vs','ps','width','height','slot','candidates','flags','rejection','draws',
        'position','guards','consumer','psC0','texture']) ||
        typeof r.vs !== 'string' || !/^[0-9a-f]{16}$/.test(r.vs) ||
        typeof r.ps !== 'string' || !/^[0-9a-f]{16}$/.test(r.ps) ||
        !integer(r.width,1,7680) || !integer(r.height,1,4320) ||
        ![-1,0,4,7,8,230,233].includes(r.slot) || !integer(r.candidates,0,63) ||
        !integer(r.flags,0,31) || !integer(r.rejection,0,255) || !integer(r.draws,1,1000000000) ||
        !exact(r.position,['version','kind','slot','issues','outputs']) || r.position.version !== 1 ||
        !integer(r.position.kind,0,2) || !integer(r.position.slot,-1,252) ||
        (r.position.kind === 1 ? r.position.slot < 0 : r.position.slot !== -1) ||
        !integer(r.position.issues,0,63) || !integer(r.position.outputs,0,65535) ||
        !integer(r.guards,0,31) || !vector(r.psC0,4)) throw Error('record');
    return {diagnostic:JSON.stringify({schema:3,namespace:'renderer-byte-fnv1a64',
      build:body.build,backend:body.backend,gpu:body.gpu,driver:body.driver,
      vs:r.vs,ps:r.ps,width:r.width,height:r.height,slot:r.slot,candidates:r.candidates,
      flags:r.flags,rejection:r.rejection,
      position:{version:r.position.version,kind:r.position.kind,slot:r.position.slot,
        issues:r.position.issues,outputs:r.position.outputs},guards:r.guards,
      consumer:transform(r.consumer),psC0:[...r.psC0],texture:texture(r.texture)}),draws:r.draws};
  });
}
