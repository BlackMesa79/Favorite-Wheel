"""同网格指标与已知 SWF 矩形的离线回归。"""
import json,math,struct,unittest
from pathlib import Path
import numpy as np
from PIL import Image
from compare import scores,soft_iou,alpha
from reference import rect_bytes,tag_bytes

class Bits:
    def __init__(self):self.bits=''
    def u(self,v,n):self.bits+=f'{v&((1<<n)-1):0{n}b}'
    def data(self):
        s=self.bits+'0'*((-len(self.bits))%8)
        return bytes(int(s[i:i+8],2) for i in range(0,len(s),8))

def rectangle_swf(stage,box):
    x0,x1,y0,y1=box;b=Bits();b.u(1,4);b.u(0,4)
    b.u(0,1);b.u(5,5);b.u(14,5);b.u(x0,14);b.u(y0,14);b.u(1,1)
    for dx,dy in ((x1-x0,0),(0,y1-y0),(x0-x1,0),(0,y0-y1)):
        b.u(1,1);b.u(1,1);b.u(12,4);b.u(1,1);b.u(dx,14);b.u(dy,14)
    b.u(0,6)
    shape=struct.pack('<H',1)+rect_bytes(box)+b'\x01\x00\xff\xff\xff\xff\x00'+b.data()
    # PlaceObject2, identity matrix; native twips are unchanged.
    place=b'\x06\x01\x00\x01\x00\x00'
    body=rect_bytes(stage)+struct.pack('<HH',24*256,1)+tag_bytes(32,shape)+tag_bytes(26,place)+tag_bytes(43,b'grid\0')+tag_bytes(1,b'')+tag_bytes(0,b'')
    return b'FWS\x0a'+struct.pack('<I',len(body)+8)+body

def fixtures(out):
    out.mkdir(parents=True,exist_ok=True);stage=[-203,1917,-57,1223];scale=.125
    rows=[]
    for i,box in enumerate(([117,1757,263,1063],[120,1755,266,1060])):
        p=(out/f'{i}.swf').resolve();p.write_bytes(rectangle_swf(stage,box))
        rows.append({'index':i,'stage':stage,'box':box,'scale':scale,'width':265,'height':160,'copy':str(p),
                     'expected_edges':[(box[0]-stage[0])*scale,(box[1]-stage[0])*scale,(box[2]-stage[2])*scale,(box[3]-stage[2])*scale]})
    (out/'manifest.txt').write_text(''.join(f'"{r["copy"].replace(chr(92),"/")}" -203 -57 .125 265 160 "{r["copy"].replace(chr(92),"/")}"\n' for r in rows),encoding='utf-8')
    (out/'fixtures.json').write_text(json.dumps(rows,indent=2))

def measure(out):
    results=[]
    for row in json.loads((out/'fixtures.json').read_text()):
        i=row['index'];a=Image.open(out/'renders'/f'{i}-original.png').convert('RGBA');b=Image.open(out/'reference'/f'{i}.swf'/'1.png').convert('RGBA')
        def edges(im):
            ar=alpha(im);h,w=ar.shape;horizontal=ar[h//2];vertical=ar[:,w//2]
            def pair(line):
                xs=np.flatnonzero(line);lo,hi=int(xs[0]),int(xs[-1]);return [lo+1-float(line[lo]),hi+float(line[hi])]
            return pair(horizontal)+pair(vertical)
        ea,eb=edges(a),edges(b)
        err=max(abs(x-y) for x,y in zip(ea,eb));expected_err=max(abs(x-y) for x,y in zip(ea,row['expected_edges']))
        results.append({**row,'swfraster_edges':ea,'ffdec_edges':eb,'maximum_edge_difference':err,'expected_error':expected_err,**scores(a,b)})
        assert a.size==b.size==(265,160) and err<=.5 and expected_err<=.5
    (out/'alignment.json').write_text(json.dumps(results,indent=2));print('synthetic grid alignment PASS',len(results))

def stroke_swf(width,scale=1):
    b=Bits();b.u(0,4);b.u(1,4)
    b.u(0,1);b.u(9,5);b.u(14,5);b.u(200,14);b.u(400,14);b.u(1,1)
    b.u(1,1);b.u(1,1);b.u(12,4);b.u(1,1);b.u(1600,14);b.u(0,14);b.u(0,6)
    shape=struct.pack('<H',1)+rect_bytes([190,1810,390,410])+b'\x00\x01'+struct.pack('<H',width)+b'\xff\xff\xff\xff'+b.data()
    matrix=Bits();matrix.u(1,1);matrix.u(18,5);matrix.u(round(65536*scale),18);matrix.u(round(65536*scale),18);matrix.u(0,1);matrix.u(0,5)
    place=b'\x06\x01\x00\x01\x00'+matrix.data()
    stage=[0,2000,0,1000]
    body=rect_bytes(stage)+struct.pack('<HH',6144,1)+tag_bytes(32,shape)+tag_bytes(26,place)+tag_bytes(43,b'grid\0')+tag_bytes(1,b'')+tag_bytes(0,b'')
    return b'FWS\x0a'+struct.pack('<I',len(body)+8)+body

def stroke_fixtures(out):
    out.mkdir(parents=True,exist_ok=True);rows=[]
    for i,(width,scale) in enumerate(((1,1),(1,.25),(0,.25),(20,1))):
        p=(out/f'{i}.swf').resolve();p.write_bytes(stroke_swf(width,scale))
        rows.append({'index':i,'width_twips':width,'instance_scale':scale,'copy':str(p)})
    (out/'fixtures.json').write_text(json.dumps(rows,indent=2))
    (out/'manifest.txt').write_text(''.join(f'"{r["copy"].replace(chr(92),"/")}" 0 0 .125 250 125 "{r["copy"].replace(chr(92),"/")}"\n' for r in rows),encoding='utf-8')

def measure_strokes(out,folder):
    results=[]
    for r in json.loads((out/'fixtures.json').read_text()):
        a=Image.open(out/folder/f'{r["index"]}-original.png').convert('RGBA');ar=alpha(a)
        x=round(1000*r['instance_scale']*.125);coverage=float(ar[:,x].sum());expected=max(1,r['width_twips']*r['instance_scale']*.125)
        ref=Image.open(out/'reference'/f'{r["index"]}.swf'/'1.png').convert('RGBA')
        results.append({**r,'column_alpha_sum':coverage,'expected_width':expected,'width_ok':abs(coverage-expected)<.02,**scores(a,ref)})
    (out/(folder+'-results.json')).write_text(json.dumps(results,indent=2));print([(r['index'],r['column_alpha_sum'],r['width_ok']) for r in results])
    return all(r['width_ok'] for r in results)

class GridTests(unittest.TestCase):
    def image(self,array):
        a=np.zeros((*array.shape,4),np.uint8);a[:,:,3]=array;return Image.fromarray(a)
    def test_direct_grid_preserves_offset_and_scale(self):
        from compare import shifted
        a=np.zeros((80,80),np.uint8);a[15:65,25:55]=255
        b=shifted(a,4,0)
        self.assertLess(scores(self.image(a),self.image(b))['soft_iou'],.97)
        b=np.zeros_like(a);b[18:63,27:54]=255
        self.assertLess(scores(self.image(a),self.image(b))['soft_iou'],.97)
    def test_one_pixel_search_and_zero_tie(self):
        from compare import shifted
        a=np.zeros((40,40),np.uint8);a[10:30,10:30]=200
        s=scores(self.image(a),self.image(shifted(a,1,0)))
        self.assertEqual(s['soft_iou'],1);self.assertEqual(s['shift_x'],1)
        self.assertEqual(scores(self.image(a),self.image(a))['shift_x'],0)
    def test_subthreshold_alpha_is_not_empty(self):
        a=np.full((10,10),100,np.uint8);b=np.full((10,10),50,np.uint8)
        self.assertAlmostEqual(scores(self.image(a),self.image(b))['soft_iou'],.5)
    def test_duplicate_label_uses_first_frame(self):
        import tempfile
        from reference import prepare
        raw=rectangle_swf([-200,2000,-100,1200],[100,1700,200,1000])
        # Append a second same-named empty frame. First-label semantics must
        # preserve the first rectangle, matching Movie::main.labels.emplace.
        body=raw[8:-6]+tag_bytes(28,struct.pack('<H',1))+tag_bytes(43,b'grid\0')+tag_bytes(1,b'')+tag_bytes(0,b'')
        duplicate=raw[:4]+struct.pack('<I',len(body)+8)+body
        with tempfile.TemporaryDirectory(dir='build/i4-phase1/round3') as d:
            a=Path(d)/'one.swf';b=Path(d)/'two.swf';a.write_bytes(raw);b.write_bytes(duplicate)
            self.assertEqual(prepare(a,'grid'),prepare(b,'grid'))

    def test_swf_fixture_and_frame_copy(self):
        import tempfile
        from reference import prepare
        with tempfile.TemporaryDirectory(dir='build/i4-phase1/round3') as d:
            p=Path(d)/'rect.swf';p.write_bytes(rectangle_swf([-200,2000,-100,1200],[100,1700,200,1000]))
            file,labels,rect,zoom,invalid=prepare(p,'grid')
            self.assertEqual(labels,{'grid':1});self.assertEqual(invalid,[])
            self.assertLess(rect[0],100);self.assertGreater(rect[1],1700)
            self.assertGreater(zoom,0);self.assertTrue(file.startswith(b'FWS'))
if __name__=='__main__':
    import sys
    if len(sys.argv)>1 and sys.argv[1]=='--fixtures':fixtures(Path(sys.argv[2]))
    elif len(sys.argv)>1 and sys.argv[1]=='--measure':measure(Path(sys.argv[2]))
    elif len(sys.argv)>1 and sys.argv[1]=='--stroke-fixtures':stroke_fixtures(Path(sys.argv[2]))
    elif len(sys.argv)>1 and sys.argv[1]=='--measure-strokes':
        ok=measure_strokes(Path(sys.argv[2]),sys.argv[3])
        if '--require-pass' in sys.argv:assert ok
    else:unittest.main()
