"""Post-import pass over /Game/Skydrop/Nature: complex collision, Nanite, report sizes and material slots."""
import unreal

EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Skydrop/Nature'], True)
for path in EAL.list_assets('/Game/Skydrop/Nature', recursive=True, include_folder=False):
    a = EAL.load_asset(path)
    if not isinstance(a, unreal.StaticMesh):
        continue
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
    EAL.save_loaded_asset(a)
    unreal.log_warning(f'[nature] MESH {a.get_path_name()} size={sz.x:.0f}x{sz.y:.0f}x{sz.z:.0f} min_z={b.min.z:.0f} mats={mats}')
unreal.log('[nature] done')

