"""Read-only inventory of a MO2 profile's loose I4 rules and SWF frame labels.
Does not rasterize/copy artwork, evaluate live game keywords, or inspect BSA archives.
"""
import argparse,json,re,struct,zlib
from pathlib import Path

def parse_swf(path):
    data=path.read_bytes()
    if data[:3]==b'CWS': data=data[:8]+zlib.decompress(data[8:])
    elif data[:3]!=b'FWS': raise ValueError('unsupported SWF compression')
    rect_bits=data[8]>>3
    start=8+(5+4*rect_bits+7)//8+4
    labels=[]
    def tags(body,pos,scope):
        frame=1
        while pos+2<=len(body):
            packed=struct.unpack_from('<H',body,pos)[0];pos+=2
            kind,length=packed>>6,packed&63
            if length==63:
                length=struct.unpack_from('<I',body,pos)[0];pos+=4
            payload=body[pos:pos+length];pos+=length
            if len(payload)!=length: raise ValueError('truncated tag')
            if kind==0:break
            if kind==1:frame+=1
            elif kind==43:labels.append({'label':payload.split(b'\0')[0].decode('utf-8'),'frame':frame,'sprite':scope})
            elif kind==39 and len(payload)>=4: tags(payload,4,struct.unpack_from('<H',payload)[0])
    tags(data,start,0)
    return labels

def read_json(path):
    text=path.read_text(encoding='utf-8-sig')
    # JsonCpp accepts comments. Preserve strings including URL slashes.
    text=re.sub(r'("(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/)',lambda m:m[0] if m[0].startswith('"') else '',text)
    return json.loads(text)

def audit(mods,profile,out):
    # MO2 writes highest priority first. First loose path wins.
    roots=[mods/line[1:] for line in (profile/'modlist.txt').read_text(encoding='utf-8-sig').splitlines() if line.startswith('+')]
    plugins={Path(line[1:]).stem.casefold() for line in (profile/'plugins.txt').read_text(encoding='utf-8-sig').splitlines() if line.startswith('*')}
    plugins.update(['skyrim','update','dawnguard','hearthfires','dragonborn'])
    configs={};movies={};providers={}
    for root in roots:
        configdir=root/'SKSE'/'Plugins'/'InventoryInjector'
        if configdir.is_dir():
            for file in configdir.glob('*.json'): configs.setdefault(file.name.casefold(),file)
        interface=root/'Interface'
        if interface.is_dir():
            for file in interface.rglob('*.swf'): movies.setdefault(file.relative_to(interface).as_posix().casefold(),file)
        if configdir.is_dir() or any(key in root.name.casefold() for key in ['boobies','b.o.o','handy icon','rotols']):
            providers[root.name]={'configs':len(list(configdir.glob('*.json'))) if configdir.is_dir() else 0,'swf':len(list(interface.rglob('*.swf'))) if interface.is_dir() else 0}
    result={'note':'Loose files only; no BSA scan or live I4 rule evaluation. No artwork copied.','providers':providers,'configs':[],'sources':{},'errors':[]}
    for name,file in sorted(configs.items()):
        if file.stem.casefold() not in plugins:continue
        try:
            rules=read_json(file).get('rules',[])
            result['configs'].append({'file':str(file),'rules':len(rules)})
            for rule in rules:
                assign=rule.get('assign',{})
                source,label=assign.get('iconSource'),assign.get('iconLabel')
                if not isinstance(source,str):continue
                entry=result['sources'].setdefault(source.replace('\\','/').casefold(),{'file':str(movies.get(source.replace('\\','/').casefold(),'')),'references':[]})
                entry['references'].append({'config':name,'label':label,'color':assign.get('iconColor'),'match':rule.get('match')})
        except Exception as e:result['errors'].append({'file':str(file),'error':str(e)})
    for source,entry in result['sources'].items():
        if not entry['file']:continue
        try:
            entry['frames']=parse_swf(Path(entry['file']))
            labels={x['label'] for x in entry['frames']}
            entry['unmatched_labels']=sorted({x['label'] for x in entry['references'] if isinstance(x['label'],str) and x['label'] not in labels})
        except Exception as e:entry['swf_error']=str(e)
    result['summary']={'active_loose_configs':len(result['configs']),'rules':sum(x['rules'] for x in result['configs']),
        'referenced_sources':len(result['sources']),'located_sources':sum(bool(x['file']) for x in result['sources'].values()),
        'frame_labels':sum(len(x.get('frames',[])) for x in result['sources'].values()),'parse_errors':len(result['errors'])}
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result['summary'],ensure_ascii=False));print('Audit:',out)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--mods',type=Path,required=True);parser.add_argument('--profile',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();audit(args.mods,args.profile,args.out)
