"""Town surfaces: clapboard siding, roof shingles, plaster, asphalt and concrete paving.
Original procedural patterns in the spirit of OG-style towns (no external assets).
Run: UnrealEditor-Cmd.exe Skydrop.uproject -run=pythonscript -script=<this file>
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty
BASE = '/Game/Skydrop/Materials'

NOISE = r"""
struct FSDN {
  float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
  float v(float2 p) {
    float2 i = floor(p); float2 f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), u.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), u.x), u.y);
  }
  float fbm(float2 p) { float t = 0; float a = 0.5; for (int o = 0; o < 4; o++) { t += a * v(p); p *= 2.03; a *= 0.5; } return t; }
};
FSDN n;
float3 Pw = P;
float3 Nw = normalize(N);
"""

# painted lap siding: boards 18 units tall, each lip throws a soft shadow on the board below
CODE_SIDING = NOISE + r"""
bool flat = abs(Nw.z) > 0.5;
float along = flat ? Pw.y : (abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x);
float hgt = flat ? Pw.x : Pw.z;
float v = hgt / 18.0;
float f = frac(v);
float lap = 0.58 + 0.42 * smoothstep(0.0, 0.16, f);
float lip = 1.0 + 0.10 * smoothstep(0.82, 1.0, f);
float grain = n.fbm(float2(along * 0.012, floor(v) * 7.3 + f));
float paint = 0.93 + 0.09 * grain;
float storey = frac(Pw.z / 384.0);
float dirt = 1.0 - 0.14 * saturate(1.0 - storey * 4.0);
float weather = 0.94 + 0.08 * n.fbm(float2(along, hgt) * 0.004);
return C * paint * lap * lip * dirt * weather;
"""

# roof shingles: horizontal rows (by height on sloped faces), offset tabs with dark gaps
CODE_SHINGLE = NOISE + r"""
bool flat = Nw.z > 0.97;
float u = abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x;
float w = flat ? Pw.x : Pw.z * 1.7;
float r = floor(w / 20.0);
float uu = u + n.h(float2(r, 1.0)) * 40.0 + fmod(abs(r), 2.0) * 22.0;
float gap = step(frac(uu / 44.0), 0.05);
float lip = smoothstep(0.0, 0.3, frac(w / 20.0));
float id = n.h(float2(floor(uu / 44.0), r));
float moss = smoothstep(0.62, 0.8, n.fbm(Pw.xy * 0.004));
float3 c = C * (0.72 + 0.42 * id) * (0.5 + 0.5 * lip);
c = lerp(c, float3(0.09, 0.12, 0.05), moss * 0.35);
return lerp(c, C * 0.3, gap);
"""

# smooth stucco with soft streaks
CODE_PLASTER = NOISE + r"""
float hz = Pw.x + Pw.y;
float big = n.fbm(float2(hz, Pw.z) * 0.006);
float fine = n.v(float2(hz, Pw.z) * 0.35);
float streak = smoothstep(0.55, 0.8, n.fbm(float2(hz * 0.02, Pw.z * 0.002)));
float storey = frac(Pw.z / 384.0);
float base = 1.0 - 0.12 * saturate(1.0 - storey * 5.0);
return C * (0.9 + 0.12 * big) * (0.96 + 0.06 * fine) * (1.0 - 0.12 * streak) * base;
"""

# asphalt: fine grain, larger tone patches, thin cracks
CODE_ASPHALT = NOISE + r"""
float fine = n.v(Pw.xy * 0.45);
float big = n.fbm(Pw.xy * 0.002);
float cr = n.fbm(Pw.xy * 0.008);
float crack = smoothstep(0.012, 0.0, abs(cr - 0.5));
return C * (0.8 + 0.28 * big) * (0.84 + 0.32 * fine) * (1.0 - 0.45 * crack);
"""

# concrete paving slabs with joints (sidewalks, foundations, plazas)
CODE_CONCRETE = NOISE + r"""
bool flat = abs(Nw.z) > 0.5;
float2 q = flat ? Pw.xy : float2(abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x, Pw.z);
q /= 128.0;
float2 f = frac(q);
float joint = (f.x < 0.018 || f.y < 0.018) ? 0.72 : 1.0;
float tone = 0.92 + 0.12 * n.h(floor(q));
float fine = 0.92 + 0.1 * n.v(q * 40.0);
float stain = 1.0 - 0.12 * smoothstep(0.6, 0.85, n.fbm(q * 0.7));
return C * joint * tone * fine * stain;
"""

# striped wallpaper: wide/narrow two-tone stripes, slight paper texture, darker toward the floor
CODE_STRIPES = NOISE + r"""
float u = abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x;
float s = frac(u / 64.0);
float band = s < 0.42 ? 0.78 : (s < 0.5 ? 0.92 : 1.0);
float paper = 0.95 + 0.06 * n.v(float2(u, Pw.z) * 0.15);
float blotch = 0.94 + 0.08 * n.fbm(float2(u, Pw.z) * 0.006);
return C * band * paper * blotch;
"""

# damask: soft repeating ornament (diamond lattice with a rounded motif), lighter print on the base colour
CODE_DAMASK = NOISE + r"""
float u = abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x;
float2 q = float2(u, Pw.z) / 56.0;
float2 cell = floor(q);
float2 f = frac(q + float2(fmod(abs(cell.y), 2.0) * 0.5, 0.0)) - 0.5;
float d = abs(f.x) * 1.3 + abs(f.y);
float motif = smoothstep(0.34, 0.3, d) * (1.0 - smoothstep(0.16, 0.12, length(f * float2(1.6, 1.0))));
float leaf = smoothstep(0.08, 0.04, abs(length(f) - 0.22)) * 0.6;
float paper = 0.95 + 0.06 * n.v(q * 9.0);
return C * paper * (1.0 + 0.32 * saturate(motif + leaf));
"""


def new_mat(name):
    path = f'{BASE}/{name}'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, BASE, unreal.Material, unreal.MaterialFactoryNew()), path


def node(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def surface(name, code):
    m, p = new_mat(name)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -900, -100)
    nw = node(m, unreal.MaterialExpressionVertexNormalWS, -900, 50)
    col = node(m, unreal.MaterialExpressionVectorParameter, -900, 200)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('default_value', unreal.LinearColor(0.5, 0.5, 0.5, 1))
    cu = node(m, unreal.MaterialExpressionCustom, -500, 0)
    ins = []
    for nm in ('P', 'N', 'C'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', nm)
        ins.append(ci)
    cu.set_editor_property('inputs', ins)
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property('code', code)
    MEL.connect_material_expressions(wp, '', cu, 'P')
    MEL.connect_material_expressions(nw, '', cu, 'N')
    MEL.connect_material_expressions(col, '', cu, 'C')
    MEL.connect_material_property(cu, '', MP.MP_BASE_COLOR)

    def sp(nm, v, y):
        s = node(m, unreal.MaterialExpressionScalarParameter, -900, y)
        s.set_editor_property('parameter_name', nm)
        s.set_editor_property('default_value', v)
        return s
    e = sp('Emissive', 0.0, 400)
    mul = node(m, unreal.MaterialExpressionMultiply, -500, 400)
    MEL.connect_material_expressions(col, '', mul, 'A')
    MEL.connect_material_expressions(e, '', mul, 'B')
    MEL.connect_material_property(mul, '', MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(sp('Roughness', 0.8, 500), '', MP.MP_ROUGHNESS)
    MEL.connect_material_property(sp('Metallic', 0.0, 600), '', MP.MP_METALLIC)
    MEL.recompile_material(m)
    EAL.save_asset(p)
    unreal.log_warning(f'[skydrop] saved {p}')


surface('M_Siding', CODE_SIDING)
surface('M_Shingle', CODE_SHINGLE)
surface('M_Plaster', CODE_PLASTER)
surface('M_Asphalt', CODE_ASPHALT)
surface('M_Concrete', CODE_CONCRETE)
surface('M_Stripes', CODE_STRIPES)
surface('M_Damask', CODE_DAMASK)
