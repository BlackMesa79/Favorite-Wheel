"""按同一 SWF 舞台/zoom 导出独立 FFDec 参照及诊断清单。"""
import argparse,csv,hashlib,json,math,os,random,subprocess
from pathlib import Path
from reference import prepare

def quoted(s):return '"'+str(s).replace('\\','\\\\').replace('"','\\"')+'"'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('previous',type=Path);ap.add_argument('output',type=Path)
    ap.add_argument('--java',type=Path,required=True);ap.add_argument('--ffdec',type=Path,required=True)
    args=ap.parse_args();out=args.output.resolve();assert Path('build').resolve() in out.parents
    out.mkdir(parents=True,exist_ok=True)
    rows=list(csv.DictReader((args.previous/'renders.csv').open(encoding='utf-8')))
    items=[];groups={}
    for i,r in enumerate(rows):
        data,_,rect,zoom,invalid=prepare(Path(r['source']),r['label'])
        group='z'+format(zoom,'.12g');folder=out/'copies'/group;folder.mkdir(parents=True,exist_ok=True)
        copy=folder/f'{i}.swf';copy.write_bytes(data)
        item={'index':i,'source':r['source'],'label':r['label'],'copy':str(copy),'bounds':rect,'zoom':zoom,
              'width':math.ceil((rect[1]-rect[0])*zoom/20),'height':math.ceil((rect[3]-rect[2])*zoom/20),
              'reference':str(out/'reference'/group/f'{i}.swf'/'1.png'),
              'old_original':r['original'],'old_status':r['status'],'ignored_invalid_shape_bounds':invalid}
        items.append(item);groups[group]=zoom
    # Select another nonempty label deterministically; source content is rendered
    # through the target viewport, never normalized or transformed as an image.
    rng=random.Random(0x4924);nonempty=[r for r in items if r['old_status']=='ok']
    for r in items:
        pool=[x for x in nonempty if x['index']!=r['index']];other=rng.choice(pool)
        r['other_index']=other['index'];r['other_source']=other['source'];r['other_label']=other['label']
    (out/'grid-references.json').write_text(json.dumps(items,ensure_ascii=False,indent=2),encoding='utf-8')
    with (out/'grid-manifest.txt').open('w',encoding='utf-8',newline='\n') as f:
        for r in items:
            f.write(f"{quoted(r['copy'])} {r['bounds'][0]} {r['bounds'][2]} {r['zoom']/20:.17g} {r['width']} {r['height']} {quoted(items[r['other_index']]['copy'])}\n")
    env=os.environ.copy();home=out/'java-home';home.mkdir(exist_ok=True)
    for key in ['APPDATA','LOCALAPPDATA','HOME','USERPROFILE','TEMP','TMP']:env[key]=str(home)
    commands=[]
    for group,zoom in groups.items():
        dest=out/'reference'/group;dest.mkdir(parents=True,exist_ok=True)
        cmd=[str(args.java.resolve()),'-Xmx512m','-XX:ActiveProcessorCount=2','-Djava.awt.headless=true','-Duser.home='+str(home),'-Djava.io.tmpdir='+str(home),'-jar',str(args.ffdec.resolve()),'-config','parallelSpeedUp=0,useMsaaForExport=false','-ignorebackground','-format','frame:png','-zoom',str(zoom),'-export','frame',str(dest),str(out/'copies'/group)]
        digest=hashlib.sha256(json.dumps(cmd).encode())
        for copy in sorted((out/'copies'/group).glob('*.swf')):
            digest.update(copy.name.encode());digest.update(copy.read_bytes())
        fingerprint=digest.hexdigest();marker=dest/'complete.txt'
        expected=[Path(r['reference']) for r in items if r['zoom']==zoom]
        cached=marker.exists() and marker.read_text()==fingerprint and all(p.exists() for p in expected)
        if not cached:
            with (dest/'ffdec.log').open('wb') as log:
                run=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1200,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            commands.append({'command':cmd,'exit':run.returncode})
            if run.returncode:raise RuntimeError(f'FFDec failed: {dest}')
            marker.write_text(fingerprint)
        else:commands.append({'command':cmd,'exit':0,'reused':True,'fingerprint':fingerprint})
        print(group,'complete',flush=True)
        (out/'reference-commands.json').write_text(json.dumps(commands,ensure_ascii=False,indent=2),encoding='utf-8')
    missing=[r['index'] for r in items if not Path(r['reference']).exists()]
    print('labels',len(items),'missing',missing)
    if missing:raise RuntimeError('missing reference PNGs')
if __name__=='__main__':main()
