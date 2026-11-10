"""FFDec reference with an expanded stage, derived only from SWF declared bounds.
Never edits installed SWFs. Uses FFDec for display-list replay, geometry and rendering.
"""
import argparse,csv,json,math,os,struct,subprocess,zlib
from pathlib import Path
from compare import tags

class Bits:
    def __init__(self,data,pos=0):self.data=data;self.bit=pos*8
    def u(self,n):
        v=0
        for _ in range(n):v=v*2+((self.data[self.bit//8]>>(7-self.bit%8))&1);self.bit+=1
        return v
    def s(self,n):
        v=self.u(n);return v-(1<<n) if n and v&(1<<(n-1)) else v
    def align(self):self.bit=(self.bit+7)//8*8
    def rect(self):
        self.align();n=self.u(5);v=[self.s(n) for _ in range(4)];self.align();return v
    def matrix(self):
        self.align();a=d=1.;b=c=0.
        if self.u(1):n=self.u(5);a=self.s(n)/65536;d=self.s(n)/65536
        if self.u(1):n=self.u(5);b=self.s(n)/65536;c=self.s(n)/65536
        n=self.u(5);x=self.s(n);y=self.s(n);self.align();return (a,b,c,d,x,y)

def mul(m,n):
    a,b,c,d,x,y=m;e,f,g,h,z,w=n
    return a*e+c*f,b*e+d*f,a*g+c*h,b*g+d*h,a*z+c*w+x,b*z+d*w+y
IDENT=(1,0,0,1,0,0)
def rect_bytes(bounds):
    vals=list(map(int,bounds));n=max(abs(x).bit_length()+1 for x in vals)
    bits=f'{n:05b}'+''.join(f'{x&((1<<n)-1):0{n}b}' for x in vals);bits+='0'*((-len(bits))%8)
    return bytes(int(bits[i:i+8],2) for i in range(0,len(bits),8))

def tag_bytes(kind,payload):
    return struct.pack('<HI',(kind<<6)|63,len(payload))+payload

def prepare(source, frame_label=None):
    file=source.read_bytes();body=zlib.decompress(file[8:]) if file[:3]==b'CWS' else file[8:]
    r=Bits(body);r.rect();old_rect_end=r.bit//8;records=list(tags(body,old_rect_end+4));chars={};labels={};invalid_bounds=[]
    def definitions(records):
        for kind,b in records:
            if kind in (2,22,32,83):
                ident=struct.unpack_from('<H',b)[0];box=Bits(b,2).rect()
                # Empty authoring shapes use inverted sentinel RECTs (2^30-1,0).
                # They have no geometric extent; treating their corners as points
                # reduced whole libraries to subpixel dots in the old reference.
                if box[0]>=box[1] or box[2]>=box[3]:invalid_bounds.append({'id':ident,'rect':box});continue
                chars[ident]=('shape',box)
            elif kind==39:
                sub=list(tags(b,4));chars[struct.unpack_from('<H',b)[0]]=('sprite',sub);definitions(sub)
    definitions(records)
    def frames(records):
        display={};frame=1;pending=[]
        for kind,b in records:
            if kind==43:pending.append(b.split(b'\0')[0].decode('utf-8'))
            elif kind in (4,26,70):
                if kind==4:ident,depth=struct.unpack_from('<HH',b);mat=Bits(b,4).matrix();display[depth]=(ident,mat)
                else:
                    flags=b[0];p=2 if kind==70 else 1;depth=struct.unpack_from('<H',b,p)[0];p+=2
                    if kind==70 and (b[1]&8 or b[1]&16 and flags&2):p=b.index(0,p)+1
                    ident,mat=display.get(depth,(0,IDENT)) if flags&1 else (0,IDENT)
                    if flags&2:ident=struct.unpack_from('<H',b,p)[0];p+=2
                    if flags&4:mat=Bits(b,p).matrix()
                    display[depth]=(ident,mat)
            elif kind in (5,28):display.pop(struct.unpack_from('<H',b,2 if kind==5 else 0)[0],None)
            elif kind==1:
                yield frame,pending,display.copy();frame+=1;pending=[]
        if pending:yield frame,pending,display.copy()
    points=[]
    def bounds(display,parent=IDENT,depth=0):
        if depth>32:return
        for ident,m in display.values():
            m=mul(parent,m);entry=chars.get(ident)
            if not entry:continue
            kind,v=entry
            if kind=='sprite':
                first=next(frames(v),None)
                if first:bounds(first[2],m,depth+1)
            else:
                x0,x1,y0,y1=v
                for x in (x0,x1):
                    for y in (y0,y1):points.append((m[0]*x+m[2]*y+m[4],m[1]*x+m[3]*y+m[5]))
    selected_frame=None
    for frame,names,display in frames(records):
        for name in names:labels.setdefault(name,frame)
        if names and (frame_label is None or frame_label in names and selected_frame is None):
            bounds(display)
            if frame_label is not None:selected_frame=frame
    if frame_label is not None and selected_frame is None:raise ValueError('frame label not found')
    if not points:points=[(0,0),(640,640)]
    x0=min(x for x,y in points);x1=max(x for x,y in points);y0=min(y for x,y in points);y1=max(y for x,y in points)
    if frame_label is not None:
        span=max(1,x1-x0,y1-y0)
        # Quantized zoom batches keep content's long side in [328,464] pixels.
        zoom=2**(math.floor(2*math.log2(464*20/span))/2)
        pad=12*20/zoom
        bounds4=[math.floor(x0-pad),math.ceil(x1+pad),math.floor(y0-pad),math.ceil(y1+pad)]
        # Replay all main controls up to this frame without earlier ShowFrames.
        # The resulting single frame has exactly the original display state.
        controls={4,5,26,28,70};defs=[];ops=[];current=1
        for kind,payload in records:
            if kind==1:current+=1
            elif kind in controls:
                if current<=selected_frame:ops.append(tag_bytes(kind,payload))
            elif kind not in (0,43,12,59,82):defs.append(tag_bytes(kind,payload))
        body=rect_bytes(bounds4)+body[old_rect_end:old_rect_end+2]+struct.pack('<H',1)+b''.join(defs+ops)+tag_bytes(43,b'grid\0')+tag_bytes(1,b'')+tag_bytes(0,b'')
        result=b'FWS'+file[3:4]+struct.pack('<I',len(body)+8)+body
        return result,{'grid':1},bounds4,zoom,invalid_bounds
    # Generous margin protects against inaccurate authoring bounds and strokes.
    pad=max(x1-x0,y1-y0)*.5+100
    bounds4=[math.floor(x0-pad),math.ceil(x1+pad),math.floor(y0-pad),math.ceil(y1+pad)]
    rect=rect_bytes(bounds4);body=rect+body[old_rect_end:]
    result=b'FWS'+file[3:4]+struct.pack('<I',len(body)+8)+body
    # Independent reference geometry uses >= 1024 pixels across typical content.
    zoom=min(32.,1024*20/max(1,x1-x0,y1-y0))
    return result,labels,bounds4,zoom,invalid_bounds

def main():
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);ap.add_argument('--java',type=Path,required=True);ap.add_argument('--ffdec',type=Path,required=True);ap.add_argument('--reuse-from',type=Path);args=ap.parse_args()
    out=args.output.resolve();assert Path('build').resolve() in out.parents
    reuse=json.loads((args.reuse_from/'expanded-references.json').read_text(encoding='utf-8')) if args.reuse_from else {}
    rows=list(csv.DictReader((out/'renders.csv').open(encoding='utf-8')));sources=list(dict.fromkeys(r['source'] for r in rows));metadata={};commands=[]
    env=os.environ.copy();home=out/'java-home';home.mkdir(exist_ok=True)
    for key in ['APPDATA','LOCALAPPDATA','HOME','USERPROFILE','TEMP','TMP']:env[key]=str(home)
    for i,source in enumerate(sources):
        folder=out/'reference-expanded'/str(i);folder.mkdir(parents=True,exist_ok=True)
        try:data,labels,bounds,zoom,invalid_bounds=prepare(Path(source))
        except Exception as e:metadata[source]={'error':str(e)};continue
        old=reuse.get(source,{})
        if old.get('bounds')==bounds and old.get('zoom')==zoom and old.get('labels') and all(Path(f).exists() for f in old['labels'].values()):
            metadata[source]={**old,'ignored_invalid_shape_bounds':invalid_bounds,'reused':True}
            (out/'expanded-references.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
            continue
        copy=folder/'expanded.swf';copy.write_bytes(data)
        if not (folder/'complete.txt').exists():
            cmd=[str(args.java.resolve()),'-Xmx512m','-XX:ActiveProcessorCount=2','-Djava.awt.headless=true','-Duser.home='+str(home),'-Djava.io.tmpdir='+str(home),'-jar',str(args.ffdec.resolve()),'-config','parallelSpeedUp=0','-ignorebackground','-format','frame:png','-zoom',str(zoom),'-export','frame',str(folder),str(copy)]
            with (folder/'ffdec.log').open('wb') as log:run=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            commands.append({'command':cmd,'exit':run.returncode});(out/'expanded-commands.json').write_text(json.dumps(commands,ensure_ascii=False,indent=2),encoding='utf-8')
            if run.returncode:metadata[source]={'error':'FFDec export failed'};continue
            (folder/'complete.txt').write_text('FFDec 26.4.0; expanded stage only\n')
        metadata[source]={'bounds':bounds,'zoom':zoom,'ignored_invalid_shape_bounds':invalid_bounds,'labels':{name:str(folder/(str(frame)+'.png')) for name,frame in labels.items()}}
        print('expanded',i+1,'/',len(sources),Path(source).name,flush=True)
        (out/'expanded-references.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
if __name__=='__main__':main()
