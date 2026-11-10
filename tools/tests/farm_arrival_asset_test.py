"""Bounds, topology, texture availability and audio checks for the staged opening."""
import json
import math
import sys
import unittest
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'project/Resources/farm/arrival'
sys.path.insert(0,str(ROOT/'tools/title'))
import generate_farm_arrival as arrival
import generate_title_grass as grass
import generate_title_landscape as landscape


class ArrivalAssets(unittest.TestCase):
    def test_rooted_leaf_no_backward_hook(self):
        vertices=[]
        for line in (OUT/'leaf.obj').read_text().splitlines():
            if line.startswith('v '):
                x,y,z=map(float,line.split()[1:])
                vertices.append((-x,y,z)) # Match runtime handedness.
        self.assertGreaterEqual(min(p[1] for p in vertices),0)
        self.assertLessEqual(max(p[1] for p in vertices),.912)
        self.assertTrue(any(math.dist(p,(.64,.8,0))<.001 for p in vertices))
        previous=-1.0
        for section in range(16):
            segment=vertices[section*6:(section+1)*6]
            lower=(segment[0][0]+segment[4][0])*.5
            self.assertGreaterEqual(lower,previous-.00001)
            previous=(segment[1][0]+segment[2][0])*.5
        normals=[tuple(map(float,line.split()[1:])) for line in (OUT/'leaf.obj').read_text().splitlines() if line.startswith('vn ')]
        self.assertTrue(all(abs(n[2])>.99 for n in normals[:32]))
        self.assertTrue(all(n[2]*normals[0][2]>.99 for n in normals[:32]))
        # Each of six blades reaches the same ground plane; none is detached.
        for blade in range(6):
            base=vertices[blade*96:blade*96+6]
            self.assertTrue(any(abs(p[1])<.00001 for p in base))

    def test_meshes_are_bounded_finite_triangles(self):
        manifest = json.loads((OUT/'manifest.json').read_text())
        for name, count in manifest['triangles'].items():
            vertices, faces = [], []
            for line in (OUT/(name+'.obj')).read_text().splitlines():
                if line.startswith('v '):
                    v=tuple(map(float,line.split()[1:]))
                    self.assertTrue(all(math.isfinite(x) and abs(x)<200 for x in v))
                    vertices.append(v)
                elif line.startswith('f '):
                    faces.append([int(x.split('/')[0])-1 for x in line.split()[1:]])
            self.assertEqual(count,len(faces))
            self.assertLess(count,10000)
            for face in faces:
                self.assertEqual(len(face),3)
                self.assertTrue(all(0<=i<len(vertices) for i in face))
                a,b,c=(vertices[i] for i in face)
                u,v=tuple(b[i]-a[i] for i in range(3)),tuple(c[i]-a[i] for i in range(3))
                cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                self.assertGreater(sum(x*x for x in cross),1e-14)

    def test_grass_excludes_plot_road_and_props(self):
        self.assertEqual(tuple(json.loads((OUT/'manifest.json').read_text())['plotKeepout']), arrival.PLOT_KEEPOUT)
        a,b,c,d=arrival.PLOT_KEEPOUT
        self.assertLess(a,-5*1.43/2)
        self.assertGreater(b,5*1.43/2)
        self.assertLess(c,8-6*1.43/2)
        self.assertGreater(d,8+6*1.43/2)
        layout=json.loads((OUT/'grass_layout.json').read_text())
        generated=arrival.build_grass_layout()
        self.assertEqual(layout,generated)
        self.assertEqual(generated,arrival.build_grass_layout())
        self.assertEqual(len(layout['patches']),json.loads((OUT/'manifest.json').read_text())['grassInstances'])
        self.assertGreater(len(layout['patches']),400)
        self.assertLessEqual(len(layout['patches']),1024)
        self.assertEqual(layout['shadowCount'],60)
        previous_distance = -1
        for patch in layout['patches']:
            x,y,z=patch['position']
            scale=patch['scale']
            self.assertTrue(all(math.isfinite(v) for v in (*patch['position'],scale,*patch['tint'])))
            self.assertTrue(.95<=scale<=1.05)
            self.assertEqual(patch['yaw'],0)
            self.assertAlmostEqual(y,grass.terrain_height(x,z))
            extent=scale*(grass.PATCH_RADIUS+grass.MAX_LEAN+grass.MAX_WIDTH/2)+grass.MAX_WIND*(grass.MAX_HEIGHT*scale)**2/grass.BEND_HEIGHT
            self.assertTrue(-50<x-extent<x+extent<50 and -74<z-extent<z+extent<86)
            for a,b,c,d in arrival.GRASS_KEEPOUTS:
                self.assertFalse(x+extent>=a and x-extent<=b and z+extent>=c and z-extent<=d)
            distance=math.hypot(x,z-8)
            self.assertGreaterEqual(distance,previous_distance)
            previous_distance=distance

    def test_grass_contiguous_background_rows(self):
        patches=arrival.build_grass_layout()['patches']
        # Away from the authored road/props, no missing cells or random square rotations.
        for zi in range(25):
            z=-14+zi*2.5
            row=sorted(p['position'][0] for p in patches if abs(p['position'][2]-z)<.061 and p['position'][0]<-10)
            self.assertGreaterEqual(len(row),8)
            for a,b in zip(row,row[1:]):
                self.assertLessEqual(b-a,2.62)

    def test_ground_shared_edges_sample_identical_uvs(self):
        mesh=landscape.Mesh()
        mesh.ground()
        self.assertEqual(len(mesh.triangles),1280)
        edges={}
        for points,uv,normal,material,section in mesh.triangles:
            self.assertEqual((material,section),(landscape.GRASS,'ground'))
            self.assertGreater(normal[1],.99)
            for point in points:
                self.assertEqual(point[1],landscape.ground_height(point[0],point[2]))
            for i,j in ((0,1),(1,2),(2,0)):
                key=tuple(sorted((points[i],points[j])))
                samples=dict(zip((points[i],points[j]),(uv[i],uv[j])))
                if key in edges:
                    self.assertEqual(edges[key],samples)
                else:
                    edges[key]=samples
        self.assertEqual(mesh.triangles,[t for t in arrival.build_scenery().triangles if t[4]=='ground'])
        # Check exported UVs too, including rounding and the Model loader's winding conversion.
        for path,digits in ((OUT/'scenery.obj',6),(ROOT/'project/Resources/title/countryside.obj',7)):
            exported=[tuple(map(float,line.split()[1:])) for line in path.read_text().splitlines() if line.startswith('vt ')]
            for i,(_,uv,_,_,_) in enumerate(mesh.triangles):
                for j,k in enumerate((0,2,1)):
                    expected=((.025+uv[k][0]*.95)/3,1-(.025+uv[k][1]*.95)/2)
                    for a,b in zip(exported[i*3+j],expected):
                        self.assertAlmostEqual(a,b,places=digits-1)

    def test_private_audio_is_short_pcm(self):
        for name,maximum in [('dew',.3),('morning',16.01)]:
            with wave.open(str(OUT/(name+'.wav'))) as audio:
                self.assertEqual(audio.getsampwidth(),2)
                self.assertGreater(audio.getnframes(),0)
                self.assertLessEqual(audio.getnframes()/audio.getframerate(),maximum)

    def test_shared_textures_exist(self):
        for path in ['title/countryside_atlas.png','title/sky.png','ui/farm_seed_packets.png','ui/farm_seed_packets_formal.png']:
            self.assertTrue((ROOT/'project/Resources'/path).is_file())


if __name__=='__main__':
    unittest.main()
