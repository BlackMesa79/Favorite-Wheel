"""Synthetic comparison checks; no third-party artwork or exports."""
import unittest
import numpy as np
from PIL import Image, ImageDraw
from compare import aligned, alpha, registration, scores, soft_iou, solid_box, shifted, warp, pair_score
from reference import prepare, rect_bytes
import tempfile, struct
from pathlib import Path

class ComparisonTests(unittest.TestCase):
    def icon(self):
        image=Image.new('RGBA',(256,256))
        draw=ImageDraw.Draw(image);draw.rectangle((40,50,90,210),fill='white');draw.rectangle((90,160,190,210),fill='white')
        return image

    def test_continuous_alpha(self):
        self.assertAlmostEqual(soft_iou(np.array([[0,.5,1]]),np.array([[0,1,1]])),.75)

    def test_solid_bounds_ignore_faint_edge(self):
        a=self.icon();b=a.copy();b.putpixel((1,1),(255,255,255,127))
        self.assertEqual(solid_box(a),solid_box(b))
        b.putpixel((1,1),(255,255,255,128));self.assertNotEqual(solid_box(a),solid_box(b))

    def test_identity(self):
        a,b=aligned(self.icon(),self.icon());self.assertEqual(scores(a,b)['soft_iou'],1.)

    def test_shift_does_not_wrap(self):
        a=np.zeros((256,256),dtype=np.float32);a[:,255]=1
        self.assertEqual(shifted(a,1,0).sum(),0)

    def test_one_pixel_tolerance(self):
        b=self.icon();a=b.transform(b.size,Image.Transform.AFFINE,(1,0,-1,0,1,0))
        result=scores(a,b);self.assertEqual(result['soft_iou'],1.);self.assertEqual(result['shift_x'],-1)

    def test_controls_and_registration_blind_spot(self):
        a=self.icon();ta,tb=registration(a,a);ref=warp(a,tb)
        moved=a.transform(a.size,Image.Transform.AFFINE,(1,0,-4,0,1,0))
        self.assertLess(scores(warp(moved,ta),ref)['soft_iou'],.97)
        # Do not claim that bbox registration can detect global translation.
        self.assertEqual(pair_score(moved,a),1.)
        mirror=a.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        self.assertLess(pair_score(mirror,a),.97)
        scaled=a.transform(a.size,Image.Transform.AFFINE,(1/.9,0,-256*.05/.9,0,1/.9,-256*.05/.9),Image.Resampling.BILINEAR)
        self.assertLess(scores(warp(scaled,ta),ref)['soft_iou'],.97)

    def test_empty_shape_sentinel_does_not_expand_stage(self):
        def tag(kind,payload=b''):return struct.pack('<HI',kind*64+63,len(payload))+payload
        def movie(sentinel):
            body=rect_bytes([0,2000,0,2000])+struct.pack('<HH',6144,1)
            body+=tag(2,struct.pack('<H',1)+rect_bytes([0,2000,0,2000]))
            # Identity MATRIX is a single zero byte; reference reads only bounds.
            body+=tag(4,struct.pack('<HH',1,1)+b'\x00')+tag(43,b'icon\0')+tag(1)
            if sentinel:
                body+=tag(2,struct.pack('<H',2)+rect_bytes([1073741823,0,1073741823,0]))
                body+=tag(4,struct.pack('<HH',2,2)+b'\x00')+tag(43,b'empty\0')+tag(1)
            body+=tag(0);return b'FWS'+bytes([10])+struct.pack('<I',len(body)+8)+body
        Path('build/i4-phase1').mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir='build/i4-phase1') as folder:
            a=Path(folder)/'a.swf';b=Path(folder)/'b.swf';a.write_bytes(movie(False));b.write_bytes(movie(True))
            pa,pb=prepare(a),prepare(b)
            self.assertEqual(pa[2:4],pb[2:4]);self.assertEqual(pb[4],[{'id':2,'rect':[1073741823,0,1073741823,0]}])

if __name__=='__main__':unittest.main()
