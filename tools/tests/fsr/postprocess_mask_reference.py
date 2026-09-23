"""Independent numerical reference for captured FSR material-mask propagation.
Derived from the captured original guest VS/PS, not production replay shaders.
Capture loading/version validation is separate; these functions prove no runtime result.
"""
import numpy as np

F = np.float32


def sample_footprint(image, uv, linear=True):
    """LOD0 normalized clamp sampling: maximum over strictly positive weights."""
    image = np.asarray(image)
    uv = np.asarray(uv, dtype=np.float32)
    if image.ndim != 2 or uv.shape[-1] != 2 or not np.isfinite(uv).all():
        raise ValueError('expected finite normalized UVs and one-channel image')
    h, w = image.shape
    size = np.array([w, h], dtype=np.float32)
    if not linear:
        xy = np.floor(uv * size).astype(np.int64)
        return image[np.clip(xy[..., 1], 0, h-1), np.clip(xy[..., 0], 0, w-1)]
    xy = uv * size - F(.5)
    lower = np.floor(xy).astype(np.int64)
    frac = xy - lower.astype(np.float32)
    result = np.zeros(uv.shape[:-1], dtype=image.dtype)
    for dy in (0, 1):
        for dx in (0, 1):
            wx = frac[..., 0] if dx else F(1)-frac[..., 0]
            wy = frac[..., 1] if dy else F(1)-frac[..., 1]
            x = np.clip(lower[..., 0]+dx, 0, w-1)
            y = np.clip(lower[..., 1]+dy, 0, h-1)
            result = np.maximum(result, np.where((wx > 0) & (wy > 0), image[y, x], 0))
    return result


def footprint_valid(extent, valid_rect, uv, linear=True):
    """True only if every participating input texel has proven provenance."""
    w,h=extent
    x,y,rw,rh=valid_rect
    if min(w,h,rw,rh)<=0 or min(x,y)<0 or x+rw>w or y+rh>h:
        raise ValueError('invalid coverage rectangle')
    unknown=np.ones((h,w),dtype=np.uint8)
    unknown[y:y+rh,x:x+rw]=0
    return sample_footprint(unknown,uv,linear)==0


def sample_depth(image, uv, linear=True):
    """Actual numeric depth sample, never MAX like the material mask."""
    image = np.asarray(image, dtype=np.float32)
    uv = np.asarray(uv, dtype=np.float32)
    h, w = image.shape
    size = np.array([w,h], dtype=np.float32)
    if not linear:
        xy = np.floor(uv*size).astype(np.int64)
        return image[np.clip(xy[...,1],0,h-1), np.clip(xy[...,0],0,w-1)]
    xy = uv*size-F(.5)
    lower = np.floor(xy).astype(np.int64)
    frac = xy-lower.astype(np.float32)
    rows=[]
    for dy in (0,1):
        y=np.clip(lower[...,1]+dy,0,h-1)
        a=image[y,np.clip(lower[...,0],0,w-1)]
        b=image[y,np.clip(lower[...,0]+1,0,w-1)]
        rows.append(a+(b-a)*frac[...,0])
    return rows[0]+(rows[1]-rows[0])*frac[...,1]


def vertex_outputs(vs, position, base_uv, constants, shared):
    """Raw slot95 position/UV and original guest constants -> clip/interpolants."""
    pos=np.asarray(position,dtype=np.float32).copy()
    uv=np.asarray(base_uv,dtype=np.float32)
    c=np.asarray(constants,dtype=np.float32)
    if vs in ('2f6bbed8149a7804','d1b241c74103b6bf'):
        count=5 if vs.startswith('2f6') else 8
        interpolants=uv[:,[0,1,1,0]][:,None,:]+c[None,:count,:]
    elif vs=='9b81c55ca39bb529':
        interpolants=np.zeros((len(pos),2,4),dtype=np.float32)
        interpolants[:,0,:2]=uv
        reciprocal=np.clip(F(1)/pos[:,3],F(np.finfo(F).tiny),F(np.finfo(F).max))
        interpolants[:,1,:2]=(pos[:,:2]*reciprocal[:,None])*c[0,:2]+c[0,[3,2]]
    else:
        raise ValueError('unaudited VS')
    flags=int(shared['flags']); fmt=int(shared['vtx_fmt'])
    if flags & 4:
        raise ValueError('debug vertex replacement')
    if not flags & 8:
        if fmt & 1: pos[:,:2]*=pos[:,3,None]
        if fmt & 2: pos[:,2]*=pos[:,3]
        if not fmt & 4: pos[:,3]=F(1)/pos[:,3]
        pos[:,:3]=pos[:,:3]*np.asarray(shared['ndc_scale'],dtype=F)[:3]+pos[:,3,None]*np.asarray(shared['ndc_offset'],dtype=F)[:3]
        pos[:,:2]+=pos[:,3,None]*np.asarray(shared['half_pixel'],dtype=F)
    if not np.isfinite(pos).all() or np.any(pos[:,3]<=0):
        raise ValueError('clipping/nonpositive W not implemented in offline oracle')
    return pos,interpolants


def raster_interpolants(clip, attributes, triangles, viewport, output_extent, scissor):
    """Perspective interpolation on original triangles at pixel centers.
    Uses captured signed Vulkan viewport. Does not certify clipping or fill rules.
    Reports overlap inconsistency and proximity to triangle edges for diagnosis.
    """
    clip=np.asarray(clip,dtype=np.float64)
    attr=np.asarray(attributes,dtype=np.float64)
    width,height=output_extent
    vx,vy,vw,vh=viewport[:4]
    screen=clip[:,:2]/clip[:,3,None]
    screen=screen*np.array([vw,vh])/2+np.array([vx+vw/2,vy+vh/2])
    yy,xx=np.mgrid[:height,:width]
    xy=np.stack([xx+.5,yy+.5],axis=-1)
    output=np.zeros((height,width)+attr.shape[1:],dtype=np.float64)
    covered=np.zeros((height,width),bool)
    edge=np.zeros((height,width),bool)
    max_overlap_error=0.
    sx,sy,sw,sh=scissor
    eligible=(xx>=sx)&(xx<sx+sw)&(yy>=sy)&(yy<sy+sh)
    for tri in triangles:
        ids=np.asarray(tri,dtype=int)
        a,b,c=screen[ids]
        matrix=np.stack([a-c,b-c],axis=1)
        if abs(np.linalg.det(matrix))<1e-12: continue
        bary2=(xy-c)@np.linalg.inv(matrix).T
        bary=np.concatenate([bary2,1-bary2.sum(axis=-1,keepdims=True)],axis=-1)
        inside=(bary.min(axis=-1)>=0)&eligible
        invw=bary/clip[ids,3]
        weights=invw/invw.sum(axis=-1,keepdims=True)
        values=np.einsum('...v,vij->...ij',weights,attr[ids])
        overlap=inside&covered
        if overlap.any():
            max_overlap_error=max(max_overlap_error,float(np.max(np.abs(output[overlap]-values[overlap]))))
        output[inside]=values[inside]
        covered|=inside
        edge|=inside&(np.min(np.abs(bary),axis=-1)<1e-7)
    return output.astype(np.float32),covered,edge,max_overlap_error


def blur_reference(ps, interpolants, constants, mask, valid_rect, linear=True):
    """Original blur taps; all taps retained for a conservative upper bound."""
    i=np.asarray(interpolants,dtype=np.float32)
    c=np.asarray(constants,dtype=np.float32)
    if ps in ('7c260eacff1d681d','53dd5d081c7945cf'):
        taps=[i[...,j,sw] for j in range(4) for sw in ([0,1],[3,2])]+[i[...,4,[0,1]]]
        if ps=='53dd5d081c7945cf':
            taps=[np.minimum(np.maximum(t,c[10,:2]),c[10,2:4]) for t in taps]
            if c[9,0]<=0: return np.zeros(i.shape[:-2],np.uint8), np.ones(i.shape[:-2],bool)
    elif ps=='ee90000c755c0472':
        taps=[i[...,j,sw] for j in range(8) for sw in ([0,1],[3,2])]
    else:
        raise ValueError('unaudited blur PS')
    result=np.zeros(i.shape[:-2],dtype=np.uint8)
    valid=np.ones(i.shape[:-2],dtype=bool)
    for uv in taps:
        result=np.maximum(result,sample_footprint(mask,uv,linear))
        valid &= footprint_valid([mask.shape[1],mask.shape[0]],valid_rect,uv,linear)
    return result,valid


def tonemap_weights(depth, constants):
    """Original b4 depth arithmetic through conditional DOF composition."""
    c=np.asarray(constants,dtype=np.float32)
    d=np.asarray(depth,dtype=np.float32)
    low,high=F(np.finfo(F).tiny),F(np.finfo(F).max)
    with np.errstate(all='ignore'):
        distance=np.clip(F(1)/(d*c[0,2]-c[0,3]),low,high)
        delta=distance-c[1,0]
        near=-delta>0
        ramp_near=np.clip((-delta-c[2,0])*np.clip(F(1)/c[3,0],low,high),0,1)
        ramp_far=np.clip((delta-c[2,1])*np.clip(F(1)/c[3,1],low,high),0,1)
        ramp=np.where(near,ramp_near,ramp_far)
        exponent=np.where(near,c[4,0],c[4,1])
        scale=np.where(near,c[5,0],c[5,1])
        weight=np.exp2(np.clip(np.log2(np.abs(ramp)),low,high)*exponent)*scale
    branch=weight>c[255,0]
    scene=np.where(branch,c[255,1]-weight,F(1))
    dof=np.where(branch[...,None],weight[...,None]*c[6,:3],F(0))
    return scene,dof,F(c[7,0]),branch,weight


def tonemap_reference(interpolants, constants, depth, masks, valid_rects,
                      linear_by_slot, depth_linear=True):
    """Returns upper-bound mask plus per-pixel provenance validity.
    masks/valid_rects map color slots 0,2,3. Missing inactive inputs are legal.
    Caller must qualify actual depth channel/swizzle and final image identities.
    """
    i=np.asarray(interpolants,dtype=np.float32)
    depth_uv=i[...,1,:2]
    filter_uv=i[...,0,:2]
    sampled_depth=sample_depth(depth,depth_uv,depth_linear)
    scene,dof,bloom,branch,weight=tonemap_weights(sampled_depth,constants)
    needs={0:scene!=0,2:np.any(dof!=0,axis=-1),3:np.full(scene.shape,bloom!=0)}
    result=np.zeros(scene.shape,dtype=np.uint8)
    valid=np.isfinite(weight)
    for slot in (0,2,3):
        needed=needs[slot]
        if not np.any(needed): continue
        source=masks.get(slot)
        if source is None:
            valid=valid & ~needed
            continue
        source=np.asarray(source,dtype=np.uint8)
        uv=depth_uv if slot==0 else filter_uv
        linear=linear_by_slot[slot]
        value=sample_footprint(source,uv,linear)
        source_valid=footprint_valid([source.shape[1],source.shape[0]],valid_rects[slot],uv,linear)
        result=np.maximum(result,np.where(needed,value,0))
        valid=valid & (~needed | source_valid)
    return result,valid,{'branch':branch,'weight':weight,'needed_slots':needs}


def area_max_reference(mask, output_extent, valid_rect):
    """Separable positive-overlap area footprint of original bloom_prefilter.h.
    Caller must match its real fixed 1280x720 target; small extents aid math tests.
    """
    mask=np.asarray(mask,dtype=np.uint8)
    h,w=mask.shape; ow,oh=output_extent
    x,y,rw,rh=valid_rect
    if min(w,h,ow,oh,rw,rh)<=0 or min(x,y)<0 or x+rw>w or y+rh>h:
        raise ValueError('invalid source extent/coverage')
    known=np.zeros((h,w),bool);known[y:y+rh,x:x+rw]=True
    def participating(length,out_length):
        scale=F(length)/F(out_length)
        result=[]
        for dst in range(out_length):
            lower=F(dst)*scale;upper=lower+scale
            indices=[]
            for src in range(int(np.floor(lower)),int(np.ceil(upper))):
                overlap=max(F(0),min(upper,F(src+1))-max(lower,F(src)))
                if overlap>0:indices.append(min(length-1,max(0,src)))
            if not indices:raise ValueError('empty area footprint')
            result.append(indices)
        return result
    xx=participating(w,ow);yy=participating(h,oh)
    col=np.stack([mask[:,idx].max(axis=1) for idx in xx],axis=1)
    col_valid=np.stack([known[:,idx].all(axis=1) for idx in xx],axis=1)
    output=np.stack([col[idx,:].max(axis=0) for idx in yy],axis=0)
    valid=np.stack([col_valid[idx,:].all(axis=0) for idx in yy],axis=0)
    return output,valid


def self_check():
    src=np.array([[10,250],[100,20]],np.uint8)
    assert sample_footprint(src,np.array([.25,.25],F))==10
    assert sample_footprint(src,np.array([.5,.25],F))==250
    assert sample_footprint(src,np.array([-1.,-1.],F))==10
    assert sample_footprint(src,np.array([1.,1.],F))==20
    assert sample_footprint(src,np.array([.25,.5],F))==100
    assert sample_footprint(src,np.array([.49,.49],F),False)==10
    assert sample_depth(src,np.array([.5,.5],F))==95
    assert footprint_valid([2,2],[0,0,1,1],np.array([.25,.25],F))
    assert not footprint_valid([2,2],[0,0,1,1],np.array([.5,.25],F))
    clip=np.array([[-1,-1,0,1],[1,-1,0,1],[-1,1,0,1],[1,1,0,1]],F)
    attr=np.zeros((4,1,4),F); attr[:,0,:2]=np.array([[0,0],[1,0],[0,1],[1,1]],F)
    vals,cov,_,err=raster_interpolants(clip,attr,[[0,1,2],[2,1,3]],[0,0,2,2],[2,2],[0,0,2,2])
    assert cov.all() and err==0 and np.allclose(vals[0,0,0,:2],[.25,.25])
    c=np.zeros((256,4),F); c[0,2]=1; c[3,:2]=1; c[4,:2]=1; c[5,:2]=.5; c[255,0]=1; c[255,1]=.5
    scene,dof,_,branch,w=tonemap_weights(np.array([1],F),c)
    assert not branch[0] and scene[0]==1 and not dof.any() and w[0]==.5
    uv=np.zeros((1,1,2,4),F); uv[...,:2]=.5
    c[7,0]=0
    out,valid,_=tonemap_reference(uv,c,np.ones((1,1),F),{0:np.array([[42]],np.uint8)},{0:[0,0,1,1]},{0:True})
    assert out[0,0]==42 and valid[0,0]
    c[255,0]=0; c[6,:3]=1
    _,valid,_=tonemap_reference(uv,c,np.ones((1,1),F),{}, {}, {})
    assert not valid[0,0]
    taps=np.full((1,1,5,4),.25,F); taps[0,0,4,0]=.5
    _,valid=blur_reference('7c260eacff1d681d',taps,np.zeros((16,4),F),src,[0,0,1,1])
    assert not valid[0,0]
    perspective_clip=np.array([[-1,-1,0,1],[2,-2,0,2],[-4,4,0,4]],F)
    vals,cov,_,_=raster_interpolants(perspective_clip,attr[:3],[[0,1,2]],[0,0,2,2],[2,2],[0,0,2,2])
    assert cov[0,0] and np.allclose(vals[0,0,0,:2],[2/11,1/11])
    clamp_taps=np.broadcast_to(np.array([-1,2,2,-1],F),(1,1,5,4)).copy()
    clamp_c=np.zeros((16,4),F); clamp_c[9,0]=1; clamp_c[10]=[.5,.2,.8,.6]
    out,valid=blur_reference('53dd5d081c7945cf',clamp_taps,clamp_c,np.arange(10,100,10,dtype=np.uint8).reshape(3,3),[0,0,3,3])
    assert out[0,0]==80 and valid[0,0]
    out,valid,diag=tonemap_reference(uv,c,np.ones((1,1),F),{2:np.array([[77]],np.uint8)},{2:[0,0,1,1]},{2:True})
    assert diag['branch'][0,0] and out[0,0]==77 and valid[0,0]
    out,valid=area_max_reference(np.array([[10,20,30],[40,50,60]],np.uint8),[2,1],[0,0,3,2])
    assert np.array_equal(out,[[50,60]]) and valid.all()
    _,valid=area_max_reference(np.array([[10,20,30],[40,50,60]],np.uint8),[2,1],[0,0,2,2])
    assert valid[0,0] and not valid[0,1]
    print('PASS independent numerical reference self-checks; no GPU acceptance claimed')

if __name__=='__main__': self_check()
