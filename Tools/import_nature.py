"""Imports Tools/Final/*.glb (textured Sketchfab pieces) into /Game/Skydrop/Nature.
Run: UnrealEditor-Cmd.exe Skydrop.uproject -run=pythonscript -script=<this file>
Credits: Handpainted Tree Pack by rudolfs, Oak Tree | Game Ready by mapletree,
Stone Pack by ashkan.fancy, Stylized Palm tree set by parelaxel (all CC-BY 4.0, sketchfab.com).
"""
import os
import unreal

SRC = os.environ.get('SKY_SRC') or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Final')
DEST = os.environ.get('SKY_DEST') or '/Game/Skydrop/Nature'
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

tasks = []
for fn in sorted(os.listdir(SRC)):
    if not fn.lower().endswith('.glb'):
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(SRC, fn))
    t.set_editor_property('destination_path', DEST + '/' + os.path.splitext(fn)[0])
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', True)
    tasks.append(t)
AT.import_asset_tasks(tasks)

for t in tasks:
    for path in t.get_editor_property('imported_object_paths'):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            body = a.get_editor_property('body_setup')
            if body:
                body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            try:
                ns = a.get_editor_property('nanite_settings')
                ns.set_editor_property('enabled', True)
                a.set_editor_property('nanite_settings', ns)
            except Exception as ex:
                unreal.log_warning(f'[nature] nanite {path}: {ex}')
            b = a.get_bounding_box()
            sz = b.max - b.min
            mats = [str(m.get_editor_property('material_slot_name')) for m in a.get_editor_property('static_materials')]
            EAL.save_asset(path)
            unreal.log(f'[nature] MESH {path} size={sz.x:.0f}x{sz.y:.0f}x{sz.z:.0f} mats={mats}')
        else:
            unreal.log(f'[nature] {type(a).__name__} {path}')
unreal.log('[nature] done')
