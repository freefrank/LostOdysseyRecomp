"""Compare opt-in completed postprocess snapshots to original-shader math.
Output is bounded numerical evidence, not full quality or P2 acceptance.
"""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from postprocess_mask_reference import (vertex_outputs,raster_interpolants,
    blur_reference,tonemap_reference,area_max_reference)


def floats(words):
    return np.asarray(words,dtype='<u4').view('<f4')


def sampler(item):
    key=int(item['effective_sampler_key'])
    if key!=int(item['sampler_key']):raise ValueError('requested sampler differs from actual palette binding')
    if int(item['mip_levels'])!=1:raise ValueError('implicit LOD not proven level zero')
    if key & (1<<15):raise ValueError('anisotropic footprint unsupported')
    if (key>>6)&7 not in (2,4) or (key>>9)&7 not in (2,4):raise ValueError('non-clamp address mode')
    mag=(key&3)!=0;mini=((key>>2)&3)!=0
    if mag!=mini:raise ValueError('mixed min/mag requires derivative qualification')
    return mag


def load_snapshot(root,snapshot):
    if snapshot['status']!='complete':raise ValueError('snapshot not complete')
    path=(root/snapshot['file']).resolve()
    if not path.is_relative_to(root.resolve()):raise ValueError('snapshot outside capture root')
    data=path.read_bytes();w,h=map(int,snapshot['extent']);bpp=int(snapshot['bytes_per_pixel'])
    if int(snapshot['row_pitch'])!=w*bpp:raise ValueError('snapshot file row pitch is not packed')
    if len(data)!=w*h*bpp:raise ValueError(f'packed size mismatch: {path.name}')
    return data,{'file':path.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}


def compare_draw(event,root):
    if not event.get('recorded') or not event.get('published'):raise ValueError('postprocess replay not recorded and published')
    if event.get('capture_failures'):raise ValueError('capture failures: '+str(event['capture_failures']))
    if int(event['submission_serial'])<=0:raise ValueError('no completed submission serial')
    if event['actual_topology']!='TRIANGLE_LIST' or int(event['index_count'])!=6:
        raise ValueError('unhandled actual topology')
    if not event['indexed']:raise ValueError('not indexed geometry')
    expected_vs={'7c260eacff1d681d':'2f6bbed8149a7804','53dd5d081c7945cf':'2f6bbed8149a7804','ee90000c755c0472':'d1b241c74103b6bf','b4b4d54a7a2d6b96':'9b81c55ca39bb529'}
    if expected_vs.get(event['ps_hash'])!=event['vs_hash']:raise ValueError('unaudited VS/PS pair')
    snapshots={s['label']:s for s in event['snapshots']}
    raw={};hashes=[]
    for label,s in snapshots.items():
        raw[label],entry=load_snapshot(root,s);hashes.append(entry)
    def plane(label,bpp):
        s=snapshots[label]
        if s['bytes_per_pixel']!=bpp:raise ValueError(f'unexpected pixel size: {label}')
        w,h=s['extent']
        return np.frombuffer(raw[label],dtype=np.uint8 if bpp==1 else '<f4').reshape(h,w)
    preservation={}
    for role in ('original-color','original-depth'):
        before,after=role+'-before',role+'-after'
        if before in raw or after in raw:
            if before not in raw or after not in raw:raise ValueError('incomplete preservation pair')
            if snapshots[before]['extent']!=snapshots[after]['extent'] or snapshots[before]['format']!=snapshots[after]['format']:
                raise ValueError('preservation pair shape/format mismatch')
            a=np.frombuffer(raw[before],np.uint8);b=np.frombuffer(raw[after],np.uint8)
            if a.shape!=b.shape:raise ValueError('preservation pair byte length mismatch')
            preservation[role]={'bytes':int(a.size),'mismatched_bytes':int(np.count_nonzero(a!=b))}
    if 'original-color' not in preservation:raise ValueError('missing original color preservation pair')
    vertices=event['vertices_in_index_order']
    if len(vertices)!=6:raise ValueError('missing actual six vertices')
    position=np.stack([floats(v['clip_position_bits']) for v in vertices])
    base_uv=np.stack([floats(v['base_uv_bits']) for v in vertices])
    vc=floats(event['vs_c0_c7_u32x4']).reshape(8,4)
    pc=np.zeros((256,4),np.float32);pc[:16]=floats(event['ps_c0_c15_u32x4']).reshape(16,4)
    pc[255]=floats(event['ps_c255_u32x4'])
    shared={'flags':event['shared_flags'],'vtx_fmt':event['vtx_fmt'],
        'ndc_scale':floats(event['ndc_scale_u32']), 'ndc_offset':floats(event['ndc_offset_u32']),
        'half_pixel':floats(event['half_pixel_u32'])}
    clip,attrs=vertex_outputs(event['vs_hash'],position,base_uv,vc,shared)
    l,t,r,b=event['scissor'];extent=event['output_extent']
    interp,covered,edge,overlap=raster_interpolants(clip,attrs,[[0,1,2],[3,4,5]],event['viewport'],extent,[l,t,r-l,b-t])
    if overlap>1e-5:raise ValueError(f'inconsistent overlapping interpolants: {overlap}')
    inputs={int(v['slot']):v for v in event['inputs']}
    masks={};rects={};filters={}
    for slot,item in inputs.items():
        filters[slot]=sampler(item)
        if slot==1:continue
        point=bool(int(event['point_slots'])&(1<<slot))
        if point==filters[slot]:raise ValueError('replay point-slot flag differs from actual sampler')
        label=f'input-t{slot}-mask';masks[slot]=plane(label,1)
        if list(reversed(masks[slot].shape))!=item['crop']:raise ValueError('input snapshot crop mismatch')
        if not item['actual_color_image'] or not item['mask_image']:raise ValueError('missing actual binding image')
        rects[slot]=item['valid_rect']
    if event['ps_hash']=='b4b4d54a7a2d6b96':
        depth=plane('input-t1-depth',4)
        # Scalar R32 textures read (R,0,0,1); qualify original XeTextureResult.x.
        info=int(inputs[1]['texture_info']);component=(info>>8)&7
        if info&(1<<20):raise ValueError('depth BGRA decode outside current scalar oracle')
        if component!=0:raise ValueError('depth result.x is not R32 R component')
        sign=info&3
        if sign==2:depth=depth*2-1
        elif sign==3:raise ValueError('depth gamma decode outside current oracle')
        reference,valid,diag=tonemap_reference(interp,pc,depth,masks,rects,filters,filters[1])
        branch_pixels=int(np.count_nonzero(diag['branch']&covered))
    else:
        reference,valid=blur_reference(event['ps_hash'],interp,pc,masks[0],rects[0],filters[0])
        branch_pixels=None
    actual=plane('output-mask',1)
    if actual.shape!=reference.shape:raise ValueError('output extent mismatch')
    vx,vy,vw,vh=map(int,event['published_valid_rect'])
    yy,xx=np.indices(actual.shape)
    declared=(xx>=vx)&(xx<vx+vw)&(yy>=vy)&(yy<vy+vh)
    interior=declared&covered&~edge
    unknown=interior&~valid
    compared=interior&valid
    mismatch=(actual!=reference)&compared
    edge_mismatch=(actual!=reference)&declared&covered&edge&valid
    bad_coords=np.argwhere(mismatch)[:16]
    return {'frame':event['render_frame'],'submission_serial':event['submission_serial'],
        'draw':event['draw_ordinal'],'ps':event['ps_hash'],'output_extent':extent,
        'source_revision':event['source_revision'],'source_stage':event['source_stage'],
        'output_mask_image':event['output_mask_image'],'published_valid_rect':event['published_valid_rect'],
        'interior_compared_pixels':int(compared.sum()),'unknown_input_pixels':int(unknown.sum()),
        'uncovered_declared_pixels':int((declared&~covered).sum()),
        'interior_mismatch_pixels':int(mismatch.sum()),'edge_pixels':int((declared&covered&edge).sum()),
        'edge_mismatch_pixels':int(edge_mismatch.sum()),'nonzero_output_pixels':int(np.count_nonzero(actual[compared])),
        'sample_mismatches':[{'xy':[int(x),int(y)],'actual':int(actual[y,x]),'reference':int(reference[y,x])} for y,x in bad_coords],
        'tone_branch_pixels':branch_pixels,'preservation':preservation,'snapshots':hashes,
        'limits':['Raster edge/top-left/cull/clip not certified by this oracle','Cross-stage source version identity audited separately']}


def main():
    p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    events=[json.loads(line) for line in a.trace.read_text().splitlines() if line.strip()]
    rows=[];errors=[]
    for e in events:
        if e.get('kind')!='postprocess_draw':continue
        if e.get('status')=='unavailable':
            errors.append({'draw':e.get('draw_ordinal'),'ps':e.get('ps_hash'),'error':'runtime_unavailable','reason':e.get('reason'),'metadata':{k:v for k,v in e.items() if k not in ('snapshots','inputs')}})
            continue
        try:rows.append(compare_draw(e,a.trace.parent))
        except (ValueError,KeyError,OSError) as exc:errors.append({'draw':e.get('draw_ordinal'),'error':str(exc)})
    required={'7c260eacff1d681d','53dd5d081c7945cf','ee90000c755c0472','b4b4d54a7a2d6b96'}
    seen={r['ps'] for r in rows}
    missing=sorted(required-seen)
    bad=bool(missing) or any(not r['interior_compared_pixels'] or r['interior_mismatch_pixels'] or r['unknown_input_pixels'] or r['uncovered_declared_pixels'] or any(p['mismatched_bytes'] for p in r['preservation'].values()) for r in rows)
    result={'status':'bounded_interior_checks_passed' if rows and not errors and not bad else 'incomplete_or_failed',
        'draws':rows,'errors':errors,'required_ps_seen':sorted(seen),'required_ps_missing':missing,
        'limits':['Host prefilter, crop/version chain, final scene-copy and raster edge adjudication remain separate checks','Only captured preservation pairs are checked; absent depth pairs do not prove depth invariance','No full P2/quality/performance acceptance']}
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='draws'},indent=2))
    raise SystemExit(0 if result['status']=='bounded_interior_checks_passed' else 1)

if __name__=='__main__':main()
