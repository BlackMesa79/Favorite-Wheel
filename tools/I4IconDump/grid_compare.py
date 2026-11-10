"""同坐标网格验收；旧指标仅从第二轮记录中读取。"""
import argparse,csv,json
# AC-4: same-grid soft IoU at zero shift; the ±1 px column is reference only.
METRIC='soft_iou_unshifted';THRESHOLD=.98
from collections import Counter
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compare import load_rgba,alpha,scores,soft_iou

CONTROLS=('random_other','mirror','drop_paths','scale_09','offset_4px')
def write_rows(path,rows):
    path.with_suffix('.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
    if rows:
        keys=list(dict.fromkeys(k for r in rows for k in r))
        with path.with_suffix('.csv').open('w',encoding='utf-8-sig',newline='') as f:
            w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)

def mosaic(rows,path):
    # Only display thumbnails share a scale; scores always use native pixels.
    canvas=Image.new('RGB',(1200,180*((len(rows)+3)//4)),(24,27,32));d=ImageDraw.Draw(canvas)
    for i,r in enumerate(rows):
        x=(i%4)*300;y=(i//4)*180
        for j,key in enumerate(('reference','original')):
            im=load_rgba(r[key])
            if im:
                im.thumbnail((142,142),Image.Resampling.LANCZOS);bg=Image.new('RGBA',im.size,(46,50,56,255));bg.alpha_composite(im)
                canvas.paste(bg.convert('RGB'),(x+j*150+(148-im.width)//2,y+(144-im.height)//2))
        name=r['label'].replace('\n','\\n').encode('ascii','replace').decode()[:35]
        d.text((x+4,y+145),f"{r['soft_iou']:.5f} {name}",fill='white')
        d.text((x+4,y+161),f"#{r['index']} FFDec | SwfRaster",fill=(160,170,185))
    canvas.save(path,quality=82,optimize=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);ap.add_argument('--previous',type=Path,required=True);ap.add_argument('--renders',default='renders');ap.add_argument('--prefix',default='grid-');args=ap.parse_args()
    out=args.output;items=json.loads((out/'grid-references.json').read_text(encoding='utf-8'))
    old=json.loads((args.previous/'expanded-comparisons.json').read_text(encoding='utf-8'))
    if isinstance(old,dict):old=old['items']
    old={(r['source'],r['label']):r for r in old}
    results=[];negative=[]
    for r in items:
        r=r.copy();i=r['index'];r['original']=str((out/args.renders/f'{i}-original.png').resolve())
        a,b=load_rgba(r['original']),load_rgba(r['reference']);prior=old.get((r['source'],r['label']),{})
        r.update(iou=prior.get('iou'),round2_soft_iou=prior.get('soft_iou'))
        if b is None:r.update(status='reference_failed',soft_iou=0.,soft_iou_unshifted=0.)
        elif a is None:r.update(status='render_failed',soft_iou=0.,soft_iou_unshifted=0.)
        elif a.size!=b.size or b.size!=(r['width'],r['height']):r.update(status='grid_mismatch',soft_iou=0.,soft_iou_unshifted=0.)
        elif not b.getchannel('A').getbbox():r.update(status='empty_ok' if not a.getchannel('A').getbbox() else 'empty_disagreement',soft_iou=0.,soft_iou_unshifted=0.)
        else:
            r.update(status='ok',**scores(a,b))
            for control in CONTROLS:
                path=out/args.renders/f'{i}-{control}.png';bad=load_rgba(path)
                n={k:r[k] for k in ('index','source','label','other_index','other_source','other_label')}
                n.update(control=control,baseline_soft_iou=r['soft_iou'],image=str(path.resolve()))
                if bad is None or bad.size!=b.size:n.update(status='render_failed',soft_iou=None,self_difference=None)
                else:n.update(status='ok',**scores(bad,b),self_difference=1-soft_iou(alpha(a),alpha(bad)))
                negative.append(n)
        results.append(r)
    nonempty=[r for r in results if load_rgba(r['reference']) is not None and load_rgba(r['reference']).getchannel('A').getbbox()]
    controls={}
    for control in CONTROLS:
        group=[n for n in negative if n['control']==control];valid=[n for n in group if n['status']=='ok'];visible=[n for n in valid if n['self_difference']>.02]
        accepted=[n for n in valid if n[METRIC]>=THRESHOLD];visible_accepted=[n for n in visible if n[METRIC]>=THRESHOLD]
        controls[control]={'count':len(group),'failed':len(group)-len(valid),'accepted':len(accepted),'fraction':len(accepted)/len(group) if group else None,
            'visible_count':len(visible),'visible_accepted':len(visible_accepted),'visible_fraction':len(visible_accepted)/len(visible) if visible else None,
            'invisible_count':len(valid)-len(visible),'unchanged_count':sum(n['self_difference']==0 for n in valid)}
    count=len(nonempty);passed=sum(r[METRIC]>=THRESHOLD for r in nonempty);shifted=sum(r['soft_iou']>=THRESHOLD for r in nonempty)
    calibrated=all(g['failed']==0 and g['visible_fraction'] is not None and g['visible_fraction']<=.01 for g in controls.values())
    summary={'labels':len(items),'nonempty':count,'empty':sum(r['status']=='empty_ok' for r in results),'status':dict(Counter(r['status'] for r in results)),
        'metric':METRIC,'threshold':THRESHOLD,'pass_count':passed,'pass_fraction':passed/count,'shifted_reference_pass_count':shifted,
        'best_shifts':dict(Counter(f"{r.get('shift_x')},{r.get('shift_y')}" for r in nonempty)),
        'controls':controls,'negative_controls_calibrated':calibrated,
        'ac4_accepted':passed/count>=.98 and calibrated and all(r['status'] in ('ok','empty_ok') for r in results)}
    prefix=args.prefix;write_rows(out/(prefix+'comparisons'),results);write_rows(out/(prefix+'negative-controls'),negative)
    write_rows(out/(prefix+'accepted-negatives'),[n for n in negative if n[METRIC] is not None and n[METRIC]>=THRESHOLD])
    (out/(prefix+'summary.json')).write_text(json.dumps(summary,indent=2),encoding='utf-8')
    folder=out/(prefix+'comparisons');folder.mkdir(exist_ok=True);ordered=sorted(nonempty,key=lambda r:r['soft_iou'])
    for i in range(0,len(ordered),20):mosaic(ordered[i:i+20],folder/f'{i//20:03}.jpg')
    print(json.dumps(summary))
if __name__=='__main__':main()
