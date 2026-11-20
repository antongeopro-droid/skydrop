import unreal
EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Characters'], True)
for p in EAL.list_assets('/Game/Characters/Mannequins/Anims', recursive=True, include_folder=False):
    a = EAL.load_asset(p)
    extra = ''
    if isinstance(a, unreal.AnimSequence):
        extra = f'len={a.get_play_length():.2f} additive={a.get_editor_property("additive_anim_type")}'
    unreal.log_warning(f'[anim] {type(a).__name__} {p.split(".")[0]} {extra}')
sk = EAL.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
mesh = EAL.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
names = []
try:
    ref = mesh.get_editor_property('skeleton')
    lib = unreal.SkeletalMeshEditorSubsystem if hasattr(unreal, 'SkeletalMeshEditorSubsystem') else None
except Exception as ex:
    unreal.log_warning(f'[anim] skel err {ex}')
for bn in ['weapon_r', 'weapon_l', 'hand_r', 'ik_hand_gun', 'ik_hand_r', 'spine_01', 'spine_03', 'spine_05', 'neck_01', 'head']:
    try:
        idx = unreal.SkeletalMeshLibrary if False else None
    except Exception:
        pass
unreal.log_warning('[anim] done')
