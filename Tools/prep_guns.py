"""Normalises the downloaded gun models: one mesh each, muzzle along +X, origin at the pistol grip,
real-world length, recolours (red tactical) and an added suppressor on the SMG. Exports GLB.
Credits: all guns are TastyTony's Low-Poly series on Sketchfab (CC-BY 4.0).
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
ROOT, OUT, PREVIEW = argv[0], argv[1], argv[2]
os.makedirs(OUT, exist_ok=True)

# name, source, length (m), grip position from the rear (fraction), grip height from the bottom (fraction)
GUNS = [
    ('SM_Gun_AR', 'm16', 0.95, 0.36, 0.55),
    ('SM_Gun_SCAR', 'scar', 0.98, 0.30, 0.58),
    ('SM_Gun_Pump', 'pump', 0.95, 0.30, 0.55),
    ('SM_Gun_Tactical', 'tactical', 0.95, 0.30, 0.62),
    ('SM_Gun_SMG', 'mp5', 0.62, 0.40, 0.55),
    ('SM_Gun_Pistol', 'g17', 0.21, 0.30, 0.70),
    ('SM_Gun_Sniper', 'sniper', 1.22, 0.27, 0.55),
]


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def bbox(objs):
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def flat_mat(name, rgb, rough=0.6, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    return m


done = []
for out_name, src, length, grip_x, grip_z in GUNS:
    clear()
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, src, 'scene.gltf'))
    bpy.context.view_layer.update()
    meshes = [o for o in bpy.data.objects if o.type == 'MESH' and not o.hide_render and not o.hide_get()
              and not o.name.startswith('Icosphere')]
    depsgraph = bpy.context.evaluated_depsgraph_get()
    # bake every part (with its rig deformation) into world space, then merge with bmesh
    merged = bmesh.new()
    mats = []
    for o in meshes:
        me = o.data
        eo = o.evaluated_get(depsgraph)
        emesh = eo.to_mesh()
        tmp = bmesh.new()
        tmp.from_mesh(emesh)
        eo.to_mesh_clear()
        tmp.transform(o.matrix_world)
        slot_map = []
        for m in me.materials:
            if m not in mats:
                mats.append(m)
            slot_map.append(mats.index(m))
        for f in tmp.faces:
            f.material_index = slot_map[f.material_index] if slot_map else 0
        tmp_me = bpy.data.meshes.new('tmp')
        tmp.to_mesh(tmp_me)
        tmp.free()
        merged.from_mesh(tmp_me)
        bpy.data.meshes.remove(tmp_me)
    new_me = bpy.data.meshes.new(out_name)
    merged.to_mesh(new_me)
    merged.free()
    for m in mats:
        new_me.materials.append(m)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    ob = bpy.data.objects.new(out_name, new_me)
    bpy.context.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    print(f'[gun] {src}: merged {len(meshes)} parts, {len(new_me.polygons)} faces')
    mn, mx = bbox([ob])
    s = length / (mx.x - mn.x)
    pivot = Vector((mn.x + (mx.x - mn.x) * grip_x, (mn.y + mx.y) / 2, mn.z + (mx.z - mn.z) * grip_z))
    ob.data.transform(Matrix.Diagonal((s, s, s, 1)) @ Matrix.Translation(-pivot))
    ob.location = (0, 0, 0)

    if src == 'tactical':
        # red furniture, dark metal: recolour the darkest (polymer) materials
        red = flat_mat('Tactical_Red', (0.42, 0.05, 0.04), 0.55)
        for i, m in enumerate(ob.data.materials):
            b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m and m.use_nodes else None
            if b and max(b.inputs['Base Color'].default_value[:3]) <= 0.015:
                ob.data.materials[i] = red

    if src == 'mp5':
        # suppressor on the muzzle
        mn2, mx2 = bbox([ob])
        muzzle_pts = [ob.matrix_world @ v.co for v in ob.data.vertices if (ob.matrix_world @ v.co).x > mx2.x - 0.02]
        mz = sum(p.z for p in muzzle_pts) / max(len(muzzle_pts), 1)
        my = sum(p.y for p in muzzle_pts) / max(len(muzzle_pts), 1)
        bm = bmesh.new()
        L, R = 0.22, 0.024
        bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=R, radius2=R, depth=L,
                              matrix=Matrix.Translation((mx2.x + L / 2 - 0.03, my, mz)) @ Matrix.Rotation(math.radians(90), 4, 'Y'))
        sup = bpy.data.meshes.new('Supp')
        bm.to_mesh(sup)
        bm.free()
        so = bpy.data.objects.new('Supp', sup)
        bpy.context.collection.objects.link(so)
        sup.materials.append(flat_mat('Suppressor', (0.03, 0.03, 0.035), 0.4, 0.3))
        for p in sup.polygons:
            p.use_smooth = True
        bpy.ops.object.select_all(action='DESELECT')
        so.select_set(True)
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.join()
        ob = bpy.context.view_layer.objects.active

    ob.name = out_name
    mn, mx = bbox([ob])
    print(f'[gun] {out_name} len={mx.x - mn.x:.3f} x[{mn.x:.3f},{mx.x:.3f}] z[{mn.z:.3f},{mx.z:.3f}] tris={sum(len(p.vertices) - 2 for p in ob.data.polygons)}')
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, out_name + '.glb'), use_selection=True, export_format='GLB')
    done.append(out_name)

# preview: re-import all exported guns stacked, same scale, grip marked by a small sphere
clear()
z = 0.0
for n in done:
    bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, n + '.glb'))
    for o in bpy.context.selected_objects:
        o.location.z += z
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.012, location=(0, 0, z))
    z += 0.32
scn = bpy.context.scene
scn.render.engine = 'BLENDER_EEVEE_NEXT'
scn.render.resolution_x = 1200
scn.render.resolution_y = 1400
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
scn.collection.objects.link(cam)
scn.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = z + 0.2
cam.location = (0.25, -5, z / 2 - 0.1)
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
