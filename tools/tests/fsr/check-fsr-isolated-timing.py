"""Inspect completed FSR timestamp diagnostics; not a performance A/B verdict."""
import argparse,json,math,re
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser()
p.add_argument('log',type=Path)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--exclude-frame',type=int,action='append',default=[])
p.add_argument('--first-frame',type=int,default=0,help='Explicit warmup cutoff; does not infer steady state')
a=p.parse_args();rows=[];errors=[];scope='prepare_sdk_encode_copy_sync'
for line_no,line in enumerate(a.log.read_text(errors='replace').splitlines(),1):
 if 'FSR GPU timing:' not in line:continue
 fields=dict(re.findall(r'(\w+)=([^\s]+)',line.split('FSR GPU timing:',1)[1]))
 required=['provider','frame','use','submission_serial','request','geometry_epoch','device_epoch','input','output','quality','sdk_reset','fsr_capture','status','sr_isolated_elapsed_ms','scope']
 if any(k not in fields for k in required):errors.append({'line':line_no,'reason':'missing fields'});continue
 try:
  row={k:int(fields[k]) for k in ['frame','use','submission_serial','geometry_epoch','device_epoch','quality','sdk_reset','fsr_capture']}
  row.update({k:fields[k] for k in ['provider','request','input','output','status','scope']})
  row['elapsed_ms']=float(fields['sr_isolated_elapsed_ms']);row['line']=line_no
 except ValueError:errors.append({'line':line_no,'reason':'invalid numeric field'});continue
 if row['provider']!='fsr' or row['scope']!=scope or row['submission_serial']<=0:
  errors.append({'line':line_no,'reason':'identity/scope invalid'})
 if row['status']=='complete' and (not math.isfinite(row['elapsed_ms']) or row['elapsed_ms']<=0):
  errors.append({'line':line_no,'reason':'complete query has nonpositive/nonfinite elapsed'})
 if row['status']=='unavailable' and row['elapsed_ms']!=-1:errors.append({'line':line_no,'reason':'unavailable query not explicit -1'})
 row['included']=(row['status']=='complete' and not row['sdk_reset'] and not row['fsr_capture'] and row['frame']>=a.first_frame and row['frame'] not in a.exclude_frame)
 rows.append(row)
keys=[(r['device_epoch'],r['request'],r['geometry_epoch'],r['use'],r['submission_serial']) for r in rows]
if len(set(keys))!=len(keys):errors.append({'reason':'duplicate completed-use identity'})
groups={}
for row in rows:
 if not row['included']:continue
 key=f"{row['input']}->{row['output']}/quality{row['quality']}/device{row['device_epoch']}"
 groups.setdefault(key,[]).append(row['elapsed_ms'])
summary={key:{'samples':len(v),'median_ms':float(np.median(v)),'p95_ms':float(np.percentile(v,95))} for key,v in groups.items()}
result={'status':'diagnostic_rows_valid' if rows and not errors else 'missing_or_invalid_rows','source':str(a.log.resolve()),'rows':len(rows),'errors':errors,'groups':summary,'first_frame':a.first_frame,'excluded_frames':a.exclude_frame,'limits':['Not SDK-only or whole-frame cost','No performance comparison or gain claim','Caller must exclude every other capture/transition frame via case metadata','Warmup cutoff does not prove steady-state scene matching']}
a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if rows and not errors else 1)
