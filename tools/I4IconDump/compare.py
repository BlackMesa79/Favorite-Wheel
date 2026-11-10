"""Local FFDec reference export and alpha comparison; all artwork stays under build/.
Run I4IconDump first, then this script for its output folder. Requires Pillow and numpy.
"""
import argparse, csv, json, os, struct, subprocess, time, zlib
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps
import numpy as np


def tags(data, pos):
    while pos + 2 <= len(data):
        h = struct.unpack_from('<H', data, pos)[0]; pos += 2
        kind, size = h >> 6, h & 63
        if size == 63:
            size = struct.unpack_from('<I', data, pos)[0]; pos += 4
        payload = data[pos:pos+size]; pos += size
        if len(payload) != size: raise ValueError('truncated tag')
        yield kind, payload
        if kind == 0: break


def mapping(file):
    d = file.read_bytes()
    if d[:3] == b'CWS': d = d[:8] + zlib.decompress(d[8:])
    start = 8 + (5 + 4 * (d[8] >> 3) + 7) // 8 + 4
    records = list(tags(d, start)); sprites = {struct.unpack_from('<H', b)[0] for k,b in records if k == 39}
    display, labels, result = {}, [], {}
    for kind, b in records:
        if kind == 43: labels.append(b.split(b'\0')[0].decode('utf-8'))
        elif kind == 4:
            ident, depth = struct.unpack_from('<HH', b); display[depth] = ident
        elif kind in (26, 70):
            flags = b[0]; p = 2 if kind == 70 else 1
            depth = struct.unpack_from('<H', b, p)[0]; p += 2
            if kind == 70 and (b[1]&8 or b[1]&16 and flags&2): p = b.index(0,p)+1
            if flags&2: display[depth] = struct.unpack_from('<H', b, p)[0]
        elif kind in (5,28): display.pop(struct.unpack_from('<H', b, 2 if kind==5 else 0)[0], None)
        elif kind == 1:
            for label in labels: result.setdefault(label, [v for v in display.values() if v in sprites])
            labels = []
    # Some libraries omit the final ShowFrame. FFDec still exposes the label.
    for label in labels: result.setdefault(label, [v for v in display.values() if v in sprites])
    return result


def normalized(file, size=256):
    if not file or not Path(file).exists(): return None
    image = Image.open(file).convert('RGBA'); bbox = image.getchannel('A').getbbox()
    if bbox is None: return None
    image = image.crop(bbox); w,h = image.size; scale=size/max(w,h)
    # Bilinear avoids ringing that would create nonzero alpha outside the silhouette.
    image=image.resize((max(1,round(w*scale)),max(1,round(h*scale))),Image.Resampling.BILINEAR)
    out=Image.new('RGBA',(size,size));out.paste(image,((size-image.width)//2,(size-image.height)//2))
    return out


# Primary comparison keeps continuous coverage; legacy normalized() stays intact.
def load_rgba(file):
    if not file or not Path(file).exists(): return None
    with Image.open(file) as image: return image.convert('RGBA')


def solid_box(image):
    if image is None: return None
    return image.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox()


def registration(a, b, size=256, reference_box=None):
    ba, bb = solid_box(a), reference_box or solid_box(b)
    if ba is None or bb is None: return None
    wa, ha = ba[2]-ba[0], ba[3]-ba[1]
    wb, hb = bb[2]-bb[0], bb[3]-bb[1]
    # Map both solid boxes into reference coordinates, then apply ONE reference
    # scale and translation. Two pixels of margin retain antialiased fringes.
    scale = (size-4)/max(wb, hb)
    x, y = (size-wb*scale)/2, (size-hb*scale)/2
    def transform(box, w, h):
        sx, sy = wb*scale/w, hb*scale/h
        return (1/sx, 0, box[0]-x/sx, 0, 1/sy, box[1]-y/sy)
    return transform(ba, wa, ha), transform(bb, wb, hb)


def warp(image, transform, size=256):
    sx,_,x,_,sy,y=transform
    box=(x,y,x+size*sx,y+size*sy)
    # resize(BILINEAR) integrates the triangular filter over the source footprint
    # when reducing large FFDec PNGs. AFFINE samples only a 2x2 neighbourhood and
    # aliases fine lines at these reduction ratios. Preserve outside-box alpha.
    pad=(max(0,int(np.ceil(-box[0]))),max(0,int(np.ceil(-box[1]))),
         max(0,int(np.ceil(box[2]-image.width))),max(0,int(np.ceil(box[3]-image.height))))
    padded=ImageOps.expand(image,pad)
    region=(box[0]+pad[0],box[1]+pad[1],box[2]+pad[0],box[3]+pad[1])
    return padded.resize((size,size),Image.Resampling.BILINEAR,box=region)


def aligned(a, b):
    transforms = registration(a,b)
    return (warp(a,transforms[0]), warp(b,transforms[1])) if transforms else (None,None)


def alpha(image):
    return np.asarray(image.getchannel('A'), dtype=np.float32)/255


def shifted(a, dx, dy):
    out = np.zeros_like(a); h,w = a.shape
    x0,x1=max(0,dx),min(w,w+dx);y0,y1=max(0,dy),min(h,h+dy)
    out[y0:y1,x0:x1]=a[y0-dy:y1-dy,x0-dx:x1-dx]
    return out


def soft_iou(a, b):
    union=np.maximum(a,b).sum(dtype=np.float64)
    return float(np.minimum(a,b).sum(dtype=np.float64)/union) if union else 1.


def scores(a, b):
    aa,bb=alpha(a),alpha(b);best=(-1.,0,0)
    # Stable tie order prefers no movement. Zero padding never wraps pixels.
    for dy in (0,-1,1):
        for dx in (0,-1,1):
            value=soft_iou(shifted(aa,dx,dy),bb)
            if value>best[0]:best=(value,dx,dy)
    ax=shifted(aa,best[1],best[2])>=128/255;bx=bb>=128/255
    union=np.logical_or(ax,bx).sum()
    return {'soft_iou':best[0],'binary128_iou':float(np.logical_and(ax,bx).sum()/union) if union else 1.,
            'shift_x':best[1],'shift_y':best[2],'soft_iou_unshifted':soft_iou(aa,bb)}


def pair_score(a,b):
    x,y=aligned(a,b)
    return scores(x,y)['soft_iou'] if x is not None else 0.


def negative_controls(items, out, prefix):
    rng=np.random.default_rng(0x4924);results=[]
    for index,r in enumerate(items):
        a,b=load_rgba(r["original"]),load_rgba(r["reference"])
        other=int(rng.integers(len(items)-1));other+=other>=index
        rr=items[other];rb=load_rgba(rr["reference"])
        box_b=solid_box(b);transforms=registration(a,b,reference_box=box_b);aa,bb=warp(a,transforms[0]),warp(b,transforms[1])
        # Inject in the 256px output domain, before alignment. The frozen column
        # isolates sensitivity from registration; it never replaces the primary.
        defects={'mirror':a.transpose(Image.Transpose.FLIP_LEFT_RIGHT),
                 'scale_0.9':a.transform(a.size,Image.Transform.AFFINE,(1/.9,0,-a.width*.05/.9,0,1/.9,-a.height*.05/.9),Image.Resampling.BILINEAR),
                 'offset_4px':a.transform(a.size,Image.Transform.AFFINE,(1,0,-4,0,1,0),Image.Resampling.BILINEAR)}
        if r.get('drop_paths'):defects['drop_20pct_paths']=load_rgba(r['drop_paths'])
        results.append({'source':r['source'],'label':r['label'],'control':'random_other',
                        'other_source':rr['source'],'other_label':rr['label'],'soft_iou':pair_score(a,rb),'frozen_soft_iou':None,
                        'baseline_soft_iou':r['soft_iou'],'self_difference':None})
        for name,damaged in defects.items():
            fixed=warp(damaged,transforms[0]);new_transforms=registration(damaged,b,reference_box=box_b)
            realigned=scores(warp(damaged,new_transforms[0]),bb)['soft_iou'] if new_transforms else 0.
            results.append({'source':r['source'],'label':r['label'],'control':name,'other_source':'','other_label':'',
                            'soft_iou':realigned,'frozen_soft_iou':scores(fixed,bb)['soft_iou'],
                            'baseline_soft_iou':r['soft_iou'],'self_difference':1-soft_iou(alpha(aa),alpha(fixed))})
    with (out/(prefix+'negative-controls.csv')).open('w',newline='',encoding='utf-8-sig') as stream:
        writer=csv.DictWriter(stream,fieldnames=results[0]);writer.writeheader();writer.writerows(results)
    summary={}
    for name in dict.fromkeys(r['control'] for r in results):
        group=[r for r in results if r['control']==name];passed=[r for r in group if r['soft_iou']>=.97]
        frozen=[r for r in group if r['frozen_soft_iou'] is not None]
        summary[name]={'count':len(group),'pass_count':len(passed),'pass_fraction':len(passed)/len(group),
                       'frozen_pass_count':sum(r['frozen_soft_iou']>=.97 for r in frozen),
                       'frozen_pass_fraction':sum(r['frozen_soft_iou']>=.97 for r in frozen)/len(frozen) if frozen else None,
                       'unchanged_count':sum(r['self_difference'] is not None and r['self_difference']<1e-6 for r in group),
                       'baseline_pass_count':sum(r['baseline_soft_iou']>=.97 for r in group),
                       'accepted_from_passing_baseline':sum(r['baseline_soft_iou']>=.97 for r in passed)}
    (out/(prefix+'negative-summary.json')).write_text(json.dumps(summary,indent=2),encoding='utf8')
    return summary


def main():
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path)
    parser.add_argument('--report-prefix',default='');parser.add_argument('--expanded',action='store_true');parser.add_argument('--existing-frames',type=Path);parser.add_argument('--export',action='store_true')
    parser.add_argument('--references-from',type=Path);parser.add_argument('--negative-controls',action='store_true')
    parser.add_argument('--java',type=Path);parser.add_argument('--ffdec',type=Path)
    args=parser.parse_args();out=args.output.resolve();build=Path('build').resolve()
    if build not in out.parents: raise ValueError('output must be under build/')
    rows=list(csv.DictReader((out/'renders.csv').open(encoding='utf-8')))
    existing={}
    if args.existing_frames:
        for row in json.loads(args.existing_frames.read_text(encoding='utf-8-sig')):
            existing[(row['source'],row['label'])]=row
    reference_root=(args.references_from or out).resolve()
    if args.export and args.references_from:raise ValueError('--references-from is read-only; export into output instead')
    sources=list(dict.fromkeys(r['source'] for r in rows));refs={};commands=[]
    env=os.environ.copy();home=out/'java-home';home.mkdir(exist_ok=True)
    for key in ['APPDATA','LOCALAPPDATA','HOME','USERPROFILE','TEMP','TMP']:env[key]=str(home)
    for index,source in enumerate(sources):
        file=Path(source);folder=reference_root/'reference'/str(index);maps=mapping(file)
        if args.export and not (folder/'complete.txt').exists():
            folder.mkdir(parents=True,exist_ok=True)
            cmd=[str(args.java.resolve()),'-Xmx512m','-XX:ActiveProcessorCount=2','-Djava.awt.headless=true','-Duser.home='+str(home),'-Djava.io.tmpdir='+str(home),'-jar',str(args.ffdec.resolve()),'-config','parallelSpeedUp=0','-ignorebackground','-format','sprite:png','-zoom','4','-export','sprite',str(folder),str(file)]
            selected=sorted({sid for ids in maps.values() for sid in ids})
            if selected:cmd[cmd.index('-export'):cmd.index('-export')]=['-selectid',','.join(map(str,selected))]
            for zoom in ['4','1','0.25','0.0625']:
                cmd[cmd.index('-zoom')+1]=zoom;start=time.perf_counter()
                with (folder/('ffdec-'+zoom+'.log')).open('wb') as log:
                    run=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                commands.append({'command':cmd.copy(),'exit':run.returncode,'seconds':time.perf_counter()-start})
                (out/'export-commands.json').write_text(json.dumps(commands,ensure_ascii=False,indent=2),encoding='utf-8')
                if not run.returncode:break
            if not run.returncode:(folder/'complete.txt').write_text('FFDec 26.4.0 sprite:png zoom='+zoom+'\n')
            print('reference',index+1,'/',len(sources),file.name,flush=True)
        for label,ids in maps.items():
            old=existing.get((file.stem,label))
            if old is not None:ref=old.get('export_file','')
            elif len(ids)==1:ref=str(folder/('DefineSprite_'+str(ids[0]))/'1.png')
            elif not ids:ref=''
            else:ref=None
            refs[(source,label)]=(ref,ids)
    if args.expanded:
        expanded_path=out/'expanded-references.json'
        if not expanded_path.exists():expanded_path=reference_root/'expanded-references.json'
        expanded=json.loads(expanded_path.read_text(encoding='utf-8'))
        for row in rows:
            refs[(row['source'],row['label'])]=(expanded.get(row['source'],{}).get('labels',{}).get(row['label']),[])
    comparisons=[];montages=[];control_items=[]
    for row in rows:
        ref,ids=refs.get((row['source'],row['label']),(None,[]))
        raw_a,raw_b=load_rgba(row['original']),load_rgba(ref)
        old_a,old_b=normalized(row['original']),normalized(ref)
        a,b=aligned(raw_a,raw_b);status='ok';iou=None;reason=''
        metric={'soft_iou':None,'binary128_iou':None,'shift_x':0,'shift_y':0,'soft_iou_unshifted':None}
        if ref is None:status='reference_failed';reason='multiple or unmapped display sprites'
        elif ref and not Path(ref).exists():status='reference_failed';reason='FFDec PNG missing'
        elif old_b is None:
            status='empty_ok' if old_a is None else 'unexpected_nonempty';reason='' if old_a is None else 'reference is empty'
        elif old_a is None:status='render_failed';iou=0.;metric['soft_iou']=0.;reason=row['warnings']
        else:
            ax=np.asarray(old_a.getchannel('A'))>0;bx=np.asarray(old_b.getchannel('A'))>0
            iou=float(np.logical_and(ax,bx).sum()/np.logical_or(ax,bx).sum())
            if a is None:status='no_solid_bbox';metric['soft_iou']=0.;reason='nonempty alpha but no alpha >= 128 bbox'
            else:
                metric=scores(a,b)
                if metric['soft_iou']<.97:status='low_iou';reason='continuous alpha mismatch (inspect comparison)'
        result={**row,'reference':ref,'sprites':ids,'iou':iou,**metric,'comparison':status,'reason':reason};comparisons.append(result)
        if old_b is not None:montages.append((result,a or old_a,b or old_b))
        if a is not None:control_items.append(result)
    # Sorted worst-first, every nonempty reference appears in a contact sheet.
    montages.sort(key=lambda v:v[0]['soft_iou'] if v[0]['soft_iou'] is not None else -1)
    sheets=out/(args.report_prefix+'comparisons');sheets.mkdir(exist_ok=True)
    for page in range(0,len(montages),20):
        sheet=Image.new('RGB',(1000,5*148),(35,38,44));draw=ImageDraw.Draw(sheet)
        for n,(r,a,b) in enumerate(montages[page:page+20]):
            x=(n%4)*250;y=(n//4)*148
            for dx,im in [(0,b),(124,a)]:
                if im:im=im.resize((120,120),Image.Resampling.LANCZOS);sheet.paste(im,(x+dx,y),im)
            text=Path(r['source']).stem+':'+r['label'].replace('\n','\\n')
            draw.text((x,y+120),text[:32],fill='white');draw.text((x,y+134),f"FFDec | SwfRaster  soft={r['soft_iou'] if r['soft_iou'] is not None else 0:.4f}",fill='white')
        sheet.save(sheets/f'{page//20:03}.jpg',quality=85)
    nonempty=[r for r in comparisons if r['iou'] is not None];passed=sum(r['soft_iou']>=.97 for r in nonempty)
    timings=[float(r['parse_render_ms']) for r in rows if r['status']=='ok'];parses=list(csv.DictReader((out/'parses.csv').open()))
    summary={'files':len(sources),'labels':len(rows),'nonempty_reference':len(nonempty),'empty_ok':sum(r['comparison']=='empty_ok' for r in comparisons),'pass_count':passed,'pass_fraction':passed/len(nonempty) if nonempty else 0,'unexpected_nonempty':sum(r['comparison']=='unexpected_nonempty' for r in comparisons),'reference_failures':sum(r['comparison']=='reference_failed' for r in comparisons),
        'legacy_pass_count':sum(r['iou']>=.97 for r in nonempty),
        'legacy_pass_fraction':sum(r['iou']>=.97 for r in nonempty)/len(nonempty) if nonempty else 0,
        'normalization':'alpha >= 128 bbox; candidate solid bbox mapped to reference solid bbox, common reference scale to 252px in 256px canvas; bilinear; continuous soft IoU, integer dx/dy in [-1,1]; binary128 at same winning shift',
        'legacy_normalization':'independent alpha > 0 bbox, aspect-preserving bilinear resize to longest side 256, centered; binary alpha > 0 IoU; no search',
        'parse_render_ms_mean':sum(timings)/len(timings) if timings else 0,'parse_render_ms_max':max(timings,default=0),'parse_ms_mean':sum(float(r['parse_ms']) for r in parses)/len(parses),'parse_ms_max':max(float(r['parse_ms']) for r in parses),
        'worst20':sorted(nonempty,key=lambda r:r['soft_iou'])[:20],'contact_sheets':str(sheets)}
    if args.negative_controls:
        summary['negative_controls']=negative_controls(control_items,out,args.report_prefix)
        summary['negative_controls_calibrated']=all(v['pass_fraction']<=.01 for v in summary['negative_controls'].values())
    summary['ac4_numeric_pass']=summary['pass_fraction']>=.98
    summary['ac4_accepted']=summary['ac4_numeric_pass'] and summary.get('negative_controls_calibrated',False) and not summary['unexpected_nonempty'] and not summary['reference_failures']
    for name,content in [('summary.json',summary),('comparisons.json',comparisons),('failures.json',[r for r in comparisons if r['comparison'] not in ('ok','empty_ok')])]:
        (out/(args.report_prefix+name)).write_text(json.dumps(content,ensure_ascii=False,indent=2),encoding='utf-8')
    with (out/(args.report_prefix+'comparisons.csv')).open('w',newline='',encoding='utf-8-sig') as stream:
        writer=csv.DictWriter(stream,fieldnames=comparisons[0]);writer.writeheader();writer.writerows(comparisons)
    print(json.dumps({k:v for k,v in summary.items() if k!='worst20'},ensure_ascii=False),flush=True)

if __name__=='__main__':main()
