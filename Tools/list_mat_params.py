import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Characters'], True)
for p in ['/Game/Characters/Mannequins/Materials/M_Mannequin',
          '/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New',
          '/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New',
          '/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01']:
    m = EAL.load_asset(p)
    if not m:
        continue
    try:
        vec = MEL.get_vector_parameter_names(m)
        sca = MEL.get_scalar_parameter_names(m)
        tex = MEL.get_texture_parameter_names(m)
        unreal.log_warning(f'[mat] {p}\n  vec={list(vec)}\n  scalar={list(sca)}\n  tex={list(tex)}')
        if isinstance(m, unreal.MaterialInstance):
            for n in vec:
                unreal.log_warning(f'[mat]   {n} = {MEL.get_material_instance_vector_parameter_value(m, n)}')
    except Exception as ex:
        unreal.log_warning(f'[mat] {p} err {ex}')
mesh = EAL.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
unreal.log_warning(f'[mat] manny slots: {[str(s.get_editor_property("material_slot_name")) for s in mesh.get_editor_property("materials")]}')
