"""Splits a downloaded GLB asset pack into one GLB per piece (origin at base centre, metres).
Run: blender.exe -b --factory-startup --python split_pack.py -- <in.glb> <out_dir> <prefix> [preview.png]
"""
import bpy, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, PREFIX = argv[0], argv[1], argv[2]
PREVIEW = argv[3] if len(argv) > 3 and argv[3] != '-' else None
MERGE_ALL = len(argv) > 4 and argv[4] == 'merge'
os.makedirs(OUT, exist_ok=True)

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.import_scene.gltf(filepath=SRC)

meshes = [o for o in bpy.data.objects if o.type == 'MESH']


def chain(o):
    c = []
    while o:
        c.append(o)
        o = o.parent
    return list(reversed(c))


# grouping level: the first depth where the meshes' ancestors split into several branches
chains = [chain(m) for m in meshes]
depth = 0
while True:
    keys = set(c[depth].name if len(c) > depth else c[-1].name for c in chains)
    if len(keys) > 1 or all(len(c) <= depth + 1 for c in chains):
        break
    depth += 1
groups = {}
for m, c in zip(meshes, chains):
    k = c[depth].name if len(c) > depth else c[-1].name
    groups.setdefault(k, []).append(m)

# a pack with a single root containing everything as one mesh-group gets split per mesh instead
if len(groups) == 1 and len(meshes) > 1:
    groups = {m.name: [m] for m in meshes}
if MERGE_ALL:
    groups = {'all': meshes}

pieces = []
for gi, (k, ms) in enumerate(sorted(groups.items())):
    bpy.ops.object.select_all(action='DESELECT')
    for m in ms:
        m.select_set(True)
    bpy.context.view_layer.objects.active = ms[0]
    # bake parent transforms into the meshes
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(ms) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = f'{PREFIX}_{gi:02d}'
    # origin at bottom centre
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    base = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
    ob.data.transform(__import__('mathutils').Matrix.Translation(-base))
    ob.location = (0, 0, 0)
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    dims = mx - mn
    mats = [m.name for m in ob.data.materials if m]
    pieces.append(ob)
    print(f'[split] {ob.name} src={k} size={dims.x:.2f}x{dims.y:.2f}x{dims.z:.2f}m tris={tris} mats={mats}')

# delete leftovers (empties)
for o in list(bpy.data.objects):
    if o not in pieces:
        bpy.data.objects.remove(o, do_unlink=True)

for ob in pieces:
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, ob.name + '.glb'), use_selection=True, export_format='GLB')

if PREVIEW:
    x = 0.0
    scn = bpy.context.scene
    tallest = max(p.dimensions.z for p in pieces)
    for i, ob in enumerate(pieces):
        w = max(ob.dimensions.x, 1.0)
        ob.location = (x + w / 2, 0, 0)
        cu = bpy.data.curves.new(f'L{i}', 'FONT')
        cu.body = str(i)
        cu.size = max(tallest * 0.08, 0.5)
        t = bpy.data.objects.new(f'L{i}', cu)
        t.location = (x + w / 2, -1, -cu.size * 1.4)
        t.rotation_euler = (math.radians(90), 0, 0)
        scn.collection.objects.link(t)
        x += w + max(1.0, tallest * 0.05)
    scn.render.engine = 'BLENDER_EEVEE_NEXT'
    scn.render.resolution_x = 2000
    scn.render.resolution_y = 800
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = max(x, tallest * 2.6) * 1.02
    cam.location = (x / 2, -200, tallest * 0.4)
    cam.rotation_euler = (math.radians(90), 0, 0)
    cam.data.clip_end = 1000
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(55), 0, math.radians(35))
    scn.collection.objects.link(sun)
    w = bpy.data.worlds.new('W')
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.5, 0.62, 0.8, 1)
    w.node_tree.nodes['Background'].inputs[1].default_value = 1.2
    scn.world = w
    scn.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
print('[split] done', len(pieces))
