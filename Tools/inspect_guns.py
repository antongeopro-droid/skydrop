import bpy, sys, os, math
from mathutils import Vector
argv = sys.argv[sys.argv.index('--') + 1:]
ROOT, PREVIEW = argv[0], argv[1]
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)
x = 0.0
for name in ['m16', 'scar', 'pump', 'tactical', 'mp5', 'g17', 'sniper']:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, name, 'scene.gltf'))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == 'MESH']
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    d = mx - mn
    mats = {}
    for o in meshes:
        for m in o.data.materials:
            if m and m.use_nodes:
                b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
                if b:
                    c = b.inputs['Base Color'].default_value
                    mats[m.name] = tuple(round(v, 2) for v in c[:3])
    print(f'[gun] {name} size={d.x:.3f}x{d.y:.3f}x{d.z:.3f} min={tuple(round(v,3) for v in mn)} meshes={len(meshes)} mats={mats}')
    # lay them out along Y for the preview
    roots = [o for o in new if o.parent is None]
    for r in roots:
        r.location.y += x - mn.y if False else 0
    for r in roots:
        r.location += Vector((0, 0, -mn.z + x))
    x += d.z + 0.15
scn = bpy.context.scene
scn.render.engine = 'BLENDER_EEVEE_NEXT'
scn.render.resolution_x = 1400
scn.render.resolution_y = 1400
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
scn.collection.objects.link(cam)
scn.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = max(x, 1.5) * 1.1
# look along -Y so X runs left-right and Z up
cam.location = (0, -10, x / 2)
cam.rotation_euler = (math.radians(90), 0, 0)
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
sun.data.energy = 3
sun.rotation_euler = (math.radians(60), 0, math.radians(20))
scn.collection.objects.link(sun)
w = bpy.data.worlds.new('W'); w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.7, 0.75, 0.8, 1)
scn.world = w
scn.render.filepath = PREVIEW
bpy.ops.render.render(write_still=True)
