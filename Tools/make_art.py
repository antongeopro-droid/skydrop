"""Builds Skydrop's stylised nature + props in Blender and exports one FBX per asset.
Run: blender.exe -b --factory-startup --python make_art.py -- <out_dir> [preview.png]
Material slot names are what the game binds its own materials to.
"""
import bpy, bmesh, math, random, sys, os
from mathutils import Vector, Matrix, noise

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = argv[0] if argv else os.path.join(os.path.dirname(__file__), 'Art')
PREVIEW = argv[1] if len(argv) > 1 else None
os.makedirs(OUT, exist_ok=True)

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

MATS = {}


def mat(name, rgb):
    if name not in MATS:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        bsdf.inputs['Base Color'].default_value = (*rgb, 1)
        bsdf.inputs['Roughness'].default_value = 0.8
        MATS[name] = m
    return MATS[name]


class Builder:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.slots = []

    def slot(self, name, rgb):
        if name not in [s[0] for s in self.slots]:
            self.slots.append((name, rgb))
        return [s[0] for s in self.slots].index(name)

    def _new_faces(self, before):
        return [f for f in self.bm.faces if f not in before]

    def add(self, maker, slot, matrix=Matrix(), smooth=True, deform=None):
        before = set(self.bm.faces)
        res = maker(self.bm)
        verts = res['verts']
        bmesh.ops.transform(self.bm, matrix=matrix, verts=verts)
        if deform:
            for v in verts:
                deform(v)
        faces = self._new_faces(before)
        for f in faces:
            f.material_index = slot
            f.smooth = smooth
        return verts, faces

    def finish(self, loc=(0, 0, 0)):
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        self.bm.free()
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.collection.objects.link(ob)
        for name, rgb in self.slots:
            me.materials.append(mat(name, rgb))
        ob.location = loc
        return ob


def cone(r1, r2, depth, seg=10):
    return lambda bm: bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=depth)


def ico(r, sub=2):
    return lambda bm: bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)


def cube(s=1.0):
    return lambda bm: bmesh.ops.create_cube(bm, size=s)


def T(x=0, y=0, z=0):
    return Matrix.Translation((x, y, z))


def S(x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    return Matrix.Diagonal((x, y, z, 1))


def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 4, axis)


BARK = (0.25, 0.14, 0.07)
LEAF = (0.15, 0.45, 0.08)

# ---------------------------------------------------------------- trees


def broadleaf(name, seed, height, canopy, blobs):
    rnd = random.Random(seed)
    b = Builder(name)
    bark = b.slot('Bark', BARK)
    leaves = b.slot('Leaves', LEAF)
    trunk_h = height * 0.55

    def flare(v):
        if v.co.z < 0.6:
            k = 1.0 + (0.6 - v.co.z) * 1.1
            v.co.x *= k
            v.co.y *= k
        v.co.x += math.sin(v.co.z * 0.7) * 0.12

    b.add(cone(0.42, 0.26, trunk_h, 12), bark, T(0, 0, trunk_h / 2), deform=flare)
    # branches reaching into the canopy
    for i in range(3):
        a = rnd.uniform(0, 360)
        m = T(0, 0, trunk_h * 0.8) @ R('Z', a) @ R('Y', 40) @ T(0, 0, 1.1)
        b.add(cone(0.17, 0.08, 2.2, 8), bark, m)
    top = Vector((0, 0, trunk_h + canopy * 0.55))
    off = Vector((rnd.uniform(-9, 9), rnd.uniform(-9, 9), rnd.uniform(-9, 9)))
    for i in range(blobs):
        if i == 0:
            c, r = top, canopy
        else:
            a = (i / (blobs - 1)) * math.tau + rnd.uniform(-0.3, 0.3)
            d = canopy * rnd.uniform(0.55, 0.85)
            c = top + Vector((math.cos(a) * d, math.sin(a) * d, rnd.uniform(-0.5, 0.6) * canopy))
            r = canopy * rnd.uniform(0.55, 0.75)

        def lump(v, c=c, r=r):
            n = (v.co - c).normalized()
            big = noise.noise(v.co * 0.45 + off) * 0.28
            small = noise.noise(v.co * 1.6 + off) * 0.08
            v.co += n * (big + small) * r
            if v.co.z < c.z - r * 0.35:  # flatter underside, like a real crown
                v.co.z = c.z - r * 0.35 + (v.co.z - (c.z - r * 0.35)) * 0.45

        b.add(ico(r, 3), leaves, T(*c), deform=lump)
    return b.finish()


def pine(name, seed, height, width, tiers):
    rnd = random.Random(seed)
    b = Builder(name)
    bark = b.slot('Bark', BARK)
    leaves = b.slot('Leaves', (0.05, 0.25, 0.10))
    b.add(cone(0.3, 0.12, height * 0.9, 10), bark, T(0, 0, height * 0.45))
    base = height * 0.18
    span = height - base
    for t in range(tiers):
        f = t / tiers
        r = width * (1.0 - f * 0.78)
        h = span / tiers * 1.75
        z = base + span * f
        seg = 14

        def jag(v, z=z, r=r, h=h):
            # bottom ring: alternating long drooping points and short notches
            if v.co.z < z - h * 0.45:
                ang = math.atan2(v.co.y, v.co.x)
                k = int(round((ang + math.pi) / math.tau * seg)) % 2
                s = 1.18 if k == 0 else 0.78
                v.co.x *= s
                v.co.y *= s
                v.co.z -= (0.35 if k == 0 else -0.05) * h * 0.4
            v.co += Vector((noise.noise(v.co * 0.8), noise.noise(v.co * 0.8 + Vector((5, 5, 5))), 0)) * 0.12

        b.add(cone(r, 0.05, h, seg), leaves, T(0, 0, z + h / 2) @ R('Z', rnd.uniform(0, 30)), deform=jag)
    return b.finish()


def bush(name, seed, size):
    rnd = random.Random(seed)
    b = Builder(name)
    leaves = b.slot('Leaves', LEAF)
    off = Vector((seed * 3.1, seed * 1.7, 0))
    for i in range(5):
        a = i / 5 * math.tau + rnd.uniform(-0.4, 0.4)
        d = 0 if i == 0 else size * 0.5
        r = size * (0.75 if i == 0 else rnd.uniform(0.45, 0.6))
        c = Vector((math.cos(a) * d, math.sin(a) * d, r * 0.75))

        def lump(v, c=c, r=r):
            n = (v.co - c).normalized()
            v.co += n * noise.noise(v.co * 1.2 + off) * 0.25 * r
            v.co.z = max(v.co.z, 0.0)

        b.add(ico(r, 2), leaves, T(*c), deform=lump)
    return b.finish()


def rock(name, seed, sx, sy, sz):
    b = Builder(name)
    st = b.slot('Rock', (0.45, 0.45, 0.47))
    off = Vector((seed * 2.3, seed * 0.7, seed * 1.9))

    def chunk(v):
        p = v.co.copy()
        v.co += p.normalized() * (noise.noise(p * 1.3 + off) * 0.35 + noise.noise(p * 3.0 + off) * 0.08)
        v.co.x *= sx
        v.co.y *= sy
        v.co.z = (v.co.z + 0.55) * sz
        if v.co.z < 0.0:
            v.co.z = 0.0

    b.add(ico(1.0, 2), st, Matrix(), smooth=False, deform=chunk)
    return b.finish()

# ---------------------------------------------------------------- props


def bevel_box(b, slot, size, loc, bevel=0.08, segs=2, taper_top=None):
    verts, faces = b.add(cube(1.0), slot, T(*loc) @ S(*size))
    if taper_top:
        zmax = max(v.co.z for v in verts)
        for v in verts:
            if abs(v.co.z - zmax) < 1e-4:
                v.co.x = loc[0] + (v.co.x - loc[0]) * taper_top[0]
                v.co.y = loc[1] + (v.co.y - loc[1]) * taper_top[1]
    edges = list({e for f in faces for e in f.edges})
    bmesh.ops.bevel(b.bm, geom=edges, offset=bevel, segments=segs, profile=0.5, affect='EDGES')
    for f in b.bm.faces:
        if f.material_index == slot:
            f.smooth = True
    return verts


def car(name):
    b = Builder(name)
    paint = b.slot('Paint', (0.7, 0.1, 0.08))
    glass = b.slot('Glass', (0.08, 0.12, 0.16))
    tire = b.slot('Tire', (0.03, 0.03, 0.03))
    chrome = b.slot('Chrome', (0.8, 0.8, 0.8))
    bevel_box(b, paint, (4.3, 1.85, 0.62), (0, 0, 0.62), bevel=0.22, segs=3)
    bevel_box(b, glass, (2.3, 1.7, 0.6), (-0.25, 0, 1.2), bevel=0.18, segs=3, taper_top=(0.82, 0.9))
    bevel_box(b, paint, (1.9, 1.55, 0.08), (-0.3, 0, 1.53), bevel=0.04, segs=2)
    for sx in (1.35, -1.35):
        for sy in (0.86, -0.86):
            b.add(cone(0.4, 0.4, 0.3, 16), tire, T(sx, sy, 0.4) @ R('X', 90))
            b.add(cone(0.22, 0.22, 0.32, 12), chrome, T(sx, sy, 0.4) @ R('X', 90))
    for sy in (0.6, -0.6):
        b.add(cube(1.0), chrome, T(2.16, sy, 0.72) @ S(0.06, 0.32, 0.16))
        b.add(cube(1.0), glass, T(-2.16, sy, 0.72) @ S(0.06, 0.3, 0.14))
    b.add(cube(1.0), chrome, T(2.2, 0, 0.42) @ S(0.12, 1.8, 0.16))
    b.add(cube(1.0), chrome, T(-2.2, 0, 0.42) @ S(0.12, 1.8, 0.16))
    return b.finish()


def crate(name):
    b = Builder(name)
    wood = b.slot('Wood', (0.45, 0.28, 0.12))
    bevel_box(b, wood, (1.2, 1.2, 1.2), (0, 0, 0.6), bevel=0.05)
    for sgn in (1, -1):
        for ax in ('x', 'y'):
            for z in (0.12, 1.08):
                if ax == 'x':
                    b.add(cube(1.0), wood, T(sgn * 0.62, 0, z) @ S(0.06, 1.25, 0.16))
                else:
                    b.add(cube(1.0), wood, T(0, sgn * 0.62, z) @ S(1.25, 0.06, 0.16))
            if ax == 'x':
                b.add(cube(1.0), wood, T(sgn * 0.62, 0, 0.6) @ R('X', 45) @ S(0.05, 1.5, 0.14))
    return b.finish()


def barrel(name):
    b = Builder(name)
    body = b.slot('Paint', (0.15, 0.35, 0.6))
    band = b.slot('Chrome', (0.3, 0.3, 0.3))

    def bulge(v):
        k = 1.0 + 0.08 * math.sin(max(0.0, min(1.0, v.co.z / 1.3)) * math.pi)
        v.co.x *= k
        v.co.y *= k

    b.add(cone(0.42, 0.42, 1.3, 20), body, T(0, 0, 0.65), deform=bulge)
    for z in (0.2, 0.65, 1.1):
        b.add(cone(0.47, 0.47, 0.06, 20), band, T(0, 0, z))
    return b.finish()


def fence(name):
    b = Builder(name)
    wood = b.slot('Wood', (0.5, 0.35, 0.2))
    for x in (-2.0, 0.0, 2.0):
        bevel_box(b, wood, (0.14, 0.14, 1.2), (x, 0, 0.6), bevel=0.02)
    for z in (0.45, 0.95):
        bevel_box(b, wood, (4.2, 0.06, 0.16), (0, 0.08, z), bevel=0.015)
    return b.finish()


def lamp(name):
    b = Builder(name)
    metal = b.slot('Metal', (0.15, 0.17, 0.2))
    light = b.slot('Light', (1.0, 0.9, 0.6))
    b.add(cone(0.12, 0.08, 5.0, 10), metal, T(0, 0, 2.5))
    b.add(cone(0.22, 0.2, 0.4, 10), metal, T(0, 0, 0.2))
    b.add(cone(0.05, 0.05, 1.3, 8), metal, T(0.6, 0, 4.9) @ R('Y', 90))
    bevel_box(b, metal, (0.7, 0.35, 0.16), (1.2, 0, 4.85), bevel=0.04)
    b.add(cube(1.0), light, T(1.2, 0, 4.76) @ S(0.55, 0.26, 0.04))
    return b.finish()


def haybale(name):
    b = Builder(name)
    hay = b.slot('Hay', (0.75, 0.6, 0.25))

    def round_edge(v):
        r = math.hypot(v.co.y, v.co.z - 0.7)
        if abs(v.co.x) > 0.75 and r > 0.5:
            s = 0.92
            v.co.y *= s
            v.co.z = 0.7 + (v.co.z - 0.7) * s

    b.add(cone(0.7, 0.7, 1.6, 24), hay, T(0, 0, 0.7) @ R('Y', 90), deform=round_edge)
    return b.finish()


ASSETS = [
    broadleaf('SM_TreeA', 1, 9.0, 3.2, 6),
    broadleaf('SM_TreeB', 2, 7.5, 2.7, 5),
    broadleaf('SM_TreeC', 3, 11.0, 3.6, 7),
    pine('SM_PineA', 4, 12.0, 3.0, 5),
    pine('SM_PineB', 5, 9.0, 2.4, 4),
    bush('SM_BushA', 6, 1.4),
    bush('SM_BushB', 7, 1.0),
    rock('SM_RockA', 8, 2.0, 1.5, 1.1),
    rock('SM_RockB', 9, 1.4, 1.3, 1.4),
    rock('SM_RockC', 10, 3.0, 2.0, 1.4),
    car('SM_Car'),
    crate('SM_Crate'),
    barrel('SM_Barrel'),
    fence('SM_Fence'),
    lamp('SM_Lamp'),
    haybale('SM_HayBale'),
]

for ob in ASSETS:
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUT, ob.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, mesh_smooth_type='FACE', apply_unit_scale=True,
                             add_leaf_bones=False, bake_anim=False, object_types={'MESH'})
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    print(f'[art] {ob.name}: {tris} tris -> {path}')

if PREVIEW:
    # lay everything out in a row and render a contact sheet
    x = 0.0
    for ob in ASSETS:
        w = ob.dimensions.x
        ob.location = (x + w / 2, 0, 0)
        x += w + 1.5
    scn = bpy.context.scene
    scn.render.engine = 'BLENDER_EEVEE_NEXT'
    scn.render.resolution_x = 1800
    scn.render.resolution_y = 700
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = x + 2
    cam.location = (x / 2, -60, 6)
    cam.rotation_euler = (math.radians(86), 0, 0)
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 4
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    scn.collection.objects.link(sun)
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.7, 0.9, 1)
    scn.world = world
    scn.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
    print('[art] preview', PREVIEW)
