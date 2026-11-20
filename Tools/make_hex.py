"""M_Hex: unlit glow for the lobby honeycomb. Each instance carries its own timing in
per-instance custom data: [0] phase, [1] speed, [2] brightness (edge fade).
Run: UnrealEditor-Cmd.exe Skydrop.uproject -run=pythonscript -script=<this file>
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
BASE = '/Game/Skydrop/Materials'
NAME = 'M_Hex'
path = f'{BASE}/{NAME}'
if EAL.does_asset_exist(path):
    EAL.delete_asset(path)
m = AT.create_asset(NAME, BASE, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('used_with_instanced_static_meshes', True)


def node(cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def sparam(name, value, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n


col = node(unreal.MaterialExpressionVectorParameter, -700, -200)
col.set_editor_property('parameter_name', 'Color')
col.set_editor_property('default_value', unreal.LinearColor(0.25, 0.55, 1.0, 1.0))
base = sparam('Base', 0.25, -700, -60)
peak = sparam('Peak', 9.0, -700, 20)
tm = node(unreal.MaterialExpressionTime, -700, 100)
cd = []
for i in range(3):
    n = node(unreal.MaterialExpressionPerInstanceCustomData, -700, 180 + i * 80)
    n.set_editor_property('data_index', i)
    cd.append(n)

cu = node(unreal.MaterialExpressionCustom, -350, 0)
ins = []
for nm in ('C', 'B', 'K', 'T', 'Ph', 'Sp', 'F'):
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', nm)
    ins.append(ci)
cu.set_editor_property('inputs', ins)
cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
cu.set_editor_property('code', '''
float sp = max(Sp, 0.05);
float s = max(sin(T * sp + Ph), 0.0);
float s2 = s * s;
float g = B + K * s2 * s2 * s2;
return C.rgb * g * F;
''')
for nm, src in (('C', col), ('B', base), ('K', peak), ('T', tm), ('Ph', cd[0]), ('Sp', cd[1]), ('F', cd[2])):
    MEL.connect_material_expressions(src, '', cu, nm)
MEL.connect_material_property(cu, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m)
EAL.save_asset(path)
unreal.log_warning('[skydrop] saved M_Hex')
