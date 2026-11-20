"""Imports Tools/Art/*.fbx as static meshes into /Game/Skydrop/Art.
Run: UnrealEditor-Cmd.exe Skydrop.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Art')
DEST = '/Game/Skydrop/Art'
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

tasks = []
for fn in sorted(os.listdir(ART)):
    if not fn.lower().endswith('.fbx'):
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(ART, fn))
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', True)
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    sm = ui.get_editor_property('static_mesh_import_data')
    sm.set_editor_property('combine_meshes', True)
    sm.set_editor_property('auto_generate_collision', False)
    sm.set_editor_property('generate_lightmap_u_vs', False)
    t.set_editor_property('options', ui)
    tasks.append(t)

AT.import_asset_tasks(tasks)

for t in tasks:
    for path in t.get_editor_property('imported_object_paths'):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        body = mesh.get_editor_property('body_setup')
        if body:
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        name = mesh.get_name()
        # Nanite for the static nature pieces; small props keep regular rendering
        if name.startswith(('SM_Tree', 'SM_Pine', 'SM_Rock', 'SM_Bush')):
            ns = mesh.get_editor_property('nanite_settings')
            ns.set_editor_property('enabled', True)
            mesh.set_editor_property('nanite_settings', ns)
        slots = [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('static_materials')]
        b = mesh.get_bounding_box()
        size = b.max - b.min
        EAL.save_asset(path)
        unreal.log(f'[art] {name} slots={slots} size={size.x:.0f}x{size.y:.0f}x{size.z:.0f}')
unreal.log('[art] import done')
