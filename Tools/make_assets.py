"""Creates Skydrop's base materials and the empty island map.
Run: UnrealEditor-Cmd.exe Skydrop.uproject -run=pythonscript -script=<this file>
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty
BASE = '/Game/Skydrop/Materials'


def new_mat(name):
    path = f'{BASE}/{name}'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, BASE, unreal.Material, unreal.MaterialFactoryNew()), path


def node(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def vparam(m, name, rgba, x, y):
    n = node(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', unreal.LinearColor(*rgba))
    return n


def sparam(m, name, value, x, y):
    n = node(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n


def const(m, value, x, y):
    n = node(m, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property('r', value)
    return n


def link(a, b, pin):
    MEL.connect_material_expressions(a, '', b, pin)


def out(n, prop):
    MEL.connect_material_property(n, '', prop)


def finish(m, path):
    MEL.recompile_material(m)
    EAL.save_asset(path)
    unreal.log(f'[skydrop] saved {path}')


def translucent_unlit(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)


# --- M_Solid: Color / Emissive / Roughness / Metallic
m, p = new_mat('M_Solid')
c = vparam(m, 'Color', (0.5, 0.5, 0.5, 1), -700, 0)
e = sparam(m, 'Emissive', 0.0, -700, 220)
r = sparam(m, 'Roughness', 0.65, -700, 320)
mt = sparam(m, 'Metallic', 0.0, -700, 420)
mul = node(m, unreal.MaterialExpressionMultiply, -350, 200)
link(c, mul, 'A')
link(e, mul, 'B')
out(c, MP.MP_BASE_COLOR)
out(mul, MP.MP_EMISSIVE_COLOR)
out(r, MP.MP_ROUGHNESS)
out(mt, MP.MP_METALLIC)
finish(m, p)

# --- M_VertexColor: terrain
m, p = new_mat('M_VertexColor')
vc = node(m, unreal.MaterialExpressionVertexColor, -500, 0)
out(vc, MP.MP_BASE_COLOR)
out(const(m, 0.92, -500, 200), MP.MP_ROUGHNESS)
out(const(m, 0.3, -500, 300), MP.MP_SPECULAR)
finish(m, p)

# --- M_Ghost: build preview
m, p = new_mat('M_Ghost')
translucent_unlit(m)
c = vparam(m, 'Color', (0.25, 0.6, 1.0, 1), -600, 0)
mul = node(m, unreal.MaterialExpressionMultiply, -300, 0)
link(c, mul, 'A')
link(const(m, 1.6, -600, 200), mul, 'B')
out(mul, MP.MP_EMISSIVE_COLOR)
out(const(m, 0.18, -300, 200), MP.MP_OPACITY)
finish(m, p)

# --- M_Storm: swirling purple cloud wall (built further down, after the noise helpers)
STORM_PENDING = True

# --- M_Water is built further down (needs the noise helpers)

# =====================================================================
# Procedural pattern surfaces (HLSL in a Custom node, no texture files)
# Inputs: P = world position, N = world normal, C = colour (param or vertex colour)
# =====================================================================
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

CODE_TERRAIN = NOISE + r"""
float steep = saturate((0.80 - Nw.z) * 5.0);
float big = n.fbm(Pw.xy * 0.0012);
float fine = n.fbm(Pw.xy * 0.03);
float blades = n.v(Pw.xy * 0.22);
float3 grass = C * (0.80 + 0.40 * big) * (0.90 + 0.18 * fine) * (0.94 + 0.10 * blades);
float horiz = abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x;
float strata = n.fbm(float2(horiz * 0.004, Pw.z * 0.0011));
float cracks = smoothstep(0.42, 0.47, n.fbm(float2(horiz * 0.012, Pw.z * 0.0016)));
float3 rock = C * (0.68 + 0.62 * strata) * (0.75 + 0.25 * cracks);
return lerp(grass, rock, steep);
"""

CODE_WOOD = NOISE + r"""
bool flat = abs(Nw.z) > 0.5;
float along = flat ? Pw.y : (abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x);
float across = flat ? Pw.x : Pw.z;
float v = across / 30.0;
float row = floor(v);
float seam = smoothstep(0.0, 0.07, frac(v)) * smoothstep(1.0, 0.93, frac(v));
float grain = n.fbm(float2(along * 0.012 + row * 13.1, across * 0.25));
float tone = 0.85 + 0.3 * n.h(float2(row, floor(along / 160.0)));
float joint = smoothstep(0.0, 0.015, frac(along / 160.0 + n.h(float2(row, 3.0))));
return C * tone * (0.78 + 0.32 * grain) * (0.45 + 0.55 * seam) * (0.6 + 0.4 * joint);
"""

CODE_BRICK = NOISE + r"""
bool flat = abs(Nw.z) > 0.5;
float u = flat ? Pw.x : (abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x);
float w = flat ? Pw.y : Pw.z;
float bh = flat ? 48.0 : 22.0;
float bl = flat ? 48.0 : 52.0;
float r = floor(w / bh);
float uu = u + fmod(abs(r), 2.0) * bl * 0.5;
float mortar = (frac(w / bh) < 0.12 || frac(uu / bl) < 0.06) ? 1.0 : 0.0;
float id = n.h(float2(floor(uu / bl), r));
float3 brick = C * (0.75 + 0.45 * id) * (0.85 + 0.25 * n.fbm(Pw.xy * 0.05 + Pw.z * 0.05));
float3 mort = float3(0.32, 0.30, 0.28);
return lerp(brick, mort, mortar);
"""

CODE_METAL = NOISE + r"""
float u = abs(Nw.z) > 0.5 ? Pw.x : (abs(Nw.x) > abs(Nw.y) ? Pw.y : Pw.x);
float w = abs(Nw.z) > 0.5 ? Pw.y : Pw.z;
float2 q = float2(u, w) / 128.0;
float2 f = frac(q);
float seam = (f.x < 0.02 || f.y < 0.02) ? 0.55 : 1.0;
float2 b = abs(f - float2(0.06, 0.06));
float bolt = (length(b * 128.0) < 3.0) ? 1.35 : 1.0;
float streak = 0.85 + 0.25 * n.fbm(float2(u * 0.02, w * 0.002));
float rib = 0.9 + 0.1 * sin(u * 0.4);
return C * seam * bolt * streak * rib;
"""

CODE_ROCK = NOISE + r"""
float horiz = Pw.x + Pw.y;
float strata = n.fbm(float2(horiz * 0.006, Pw.z * 0.004));
float spots = n.fbm(Pw.xy * 0.02 + Pw.z * 0.01);
float top = saturate(Nw.z * 2.0 - 1.0);
float3 rock = C * (0.65 + 0.6 * strata) * (0.85 + 0.25 * spots);
return lerp(rock, rock * float3(0.75, 1.15, 0.7), top * 0.35);
"""

CODE_LEAF = NOISE + r"""
float clump = n.fbm(float2(Pw.x + Pw.z * 0.7, Pw.y - Pw.z * 0.7) * 0.018);
float leaf = n.v(float2(Pw.x + Pw.z, Pw.y) * 0.12);
float up = saturate(Nw.z * 0.5 + 0.5);
float3 c = C * (0.55 + 0.75 * clump) * (0.88 + 0.2 * leaf);
return c * (0.7 + 0.45 * up) + float3(0.02, 0.03, 0.0) * up;
"""

CODE_GROUND = NOISE + r"""
float2 q = Pw.xy / 256.0;
float2 f = frac(q);
float joint = (f.x < 0.015 || f.y < 0.015) ? 0.7 : 1.0;
float tile = 0.92 + 0.12 * n.h(floor(q));
float grime = 0.8 + 0.3 * n.fbm(Pw.xy * 0.004);
return C * joint * tile * grime;
"""


def custom_surface(name, code, use_vertex_color=False, default_color=(0.5, 0.5, 0.5, 1)):
    m, p = new_mat(name)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -900, -100)
    nw = node(m, unreal.MaterialExpressionVertexNormalWS, -900, 50)
    if use_vertex_color:
        col = node(m, unreal.MaterialExpressionVertexColor, -900, 200)
    else:
        col = vparam(m, 'Color', default_color, -900, 200)
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
    out(cu, MP.MP_BASE_COLOR)
    if use_vertex_color:
        out(const(m, 0.92, -500, 250), MP.MP_ROUGHNESS)
        out(const(m, 0.3, -500, 350), MP.MP_SPECULAR)
    else:
        e = sparam(m, 'Emissive', 0.0, -900, 400)
        r = sparam(m, 'Roughness', 0.7, -900, 500)
        mt = sparam(m, 'Metallic', 0.0, -900, 600)
        mul = node(m, unreal.MaterialExpressionMultiply, -500, 400)
        link(col, mul, 'A')
        link(e, mul, 'B')
        out(mul, MP.MP_EMISSIVE_COLOR)
        out(r, MP.MP_ROUGHNESS)
        out(mt, MP.MP_METALLIC)
    finish(m, p)


STORM_BODY = r"""
float ang = atan2(Pw.y, Pw.x);
float2 q = float2(ang * 900.0 + Tm * 60.0, Pw.z * 0.25 - Tm * 25.0) * 0.004;
float c1 = n.fbm(q);
float c2 = n.fbm(q * 2.3 + float2(Tm * 0.05, 0.0));
float cloud = saturate(c1 * 1.2 + c2 * 0.5 - 0.25);
"""
CODE_STORM_COLOR = NOISE + STORM_BODY + r"""
float bolt = smoothstep(0.86, 0.9, n.v(q * 6.0 + Tm)) * step(0.97, frac(Tm * 0.37 + n.h(floor(q * 3.0))));
return C * (0.6 + 1.8 * cloud) + float3(0.9, 0.7, 1.0) * bolt * 6.0;
"""
CODE_STORM_ALPHA = NOISE + STORM_BODY + r"""
float dist = length(Pw.xy - Cam.xy);
float fade = lerp(1.0, 0.18, saturate((dist - 12000.0) / 45000.0));
return (0.28 + 0.45 * cloud) * fade;
"""


def storm_material():
    m, p = new_mat('M_Storm')
    translucent_unlit(m)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -900, -100)
    nw = node(m, unreal.MaterialExpressionVertexNormalWS, -900, 50)
    col = vparam(m, 'Color', (0.45, 0.10, 0.90, 1), -900, 200)
    tm = node(m, unreal.MaterialExpressionTime, -900, 350)
    cam = node(m, unreal.MaterialExpressionCameraPositionWS, -900, 500)

    def make(code, y, otype):
        cu = node(m, unreal.MaterialExpressionCustom, -500, y)
        ins = []
        for nm in ('P', 'N', 'C', 'Tm', 'Cam'):
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', nm)
            ins.append(ci)
        cu.set_editor_property('inputs', ins)
        cu.set_editor_property('output_type', otype)
        cu.set_editor_property('code', code)
        MEL.connect_material_expressions(wp, '', cu, 'P')
        MEL.connect_material_expressions(nw, '', cu, 'N')
        MEL.connect_material_expressions(col, '', cu, 'C')
        MEL.connect_material_expressions(tm, '', cu, 'Tm')
        MEL.connect_material_expressions(cam, '', cu, 'Cam')
        return cu

    out(make(CODE_STORM_COLOR, 0, unreal.CustomMaterialOutputType.CMOT_FLOAT3), MP.MP_EMISSIVE_COLOR)
    out(make(CODE_STORM_ALPHA, 300, unreal.CustomMaterialOutputType.CMOT_FLOAT1), MP.MP_OPACITY)
    finish(m, p)


storm_material()

# --- M_Water: see-through stylised water, colour and opacity from depth, animated ripples, shoreline foam
WATER_NOISE = NOISE + r"""
float2 w1 = Pw.xy * 0.0025 + float2(Tm * 0.035, Tm * 0.02);
float2 w2 = Pw.xy * 0.006 - float2(Tm * 0.05, -Tm * 0.03);
"""
CODE_WATER_COLOR = WATER_NOISE + r"""
float d = max(D, 0.0);
float3 shallow = float3(0.03, 0.42, 0.46);
float3 mid = float3(0.012, 0.18, 0.36);
float3 deep = float3(0.004, 0.06, 0.20);
float3 c = lerp(shallow, mid, saturate(d / 70.0));
c = lerp(c, deep, saturate((d - 70.0) / 300.0));
float sparkle = n.fbm(w1 * 3.0) * n.fbm(w2 * 2.0);
c += float3(0.03, 0.06, 0.07) * sparkle;
float foamEdge = 1.0 - saturate(d / 22.0);
float foamNoise = n.fbm(Pw.xy * 0.02 + Tm * 0.15);
float foam = saturate(foamEdge * 1.6 - foamNoise * 0.8);
return lerp(c, float3(0.85, 0.95, 0.95), foam);
"""
CODE_WATER_ALPHA = WATER_NOISE + r"""
float d = max(D, 0.0);
float a = lerp(0.50, 0.97, saturate(d / 160.0));
float foamEdge = 1.0 - saturate(d / 22.0);
float foamNoise = n.fbm(Pw.xy * 0.02 + Tm * 0.15);
float foam = saturate(foamEdge * 1.6 - foamNoise * 0.8);
return saturate(max(a, foam * 0.9)) * saturate(d / 4.0);
"""
CODE_WATER_NORMAL = WATER_NOISE + r"""
float e = 14.0;
float h0 = n.fbm(w1) + 0.5 * n.fbm(w2);
float hx = n.fbm(w1 + float2(e * 0.0025, 0)) + 0.5 * n.fbm(w2 + float2(e * 0.006, 0));
float hy = n.fbm(w1 + float2(0, e * 0.0025)) + 0.5 * n.fbm(w2 + float2(0, e * 0.006));
float k = 9.0;
return normalize(float3((h0 - hx) * k, (h0 - hy) * k, 1.0));
"""


def water_material():
    m, p = new_mat('M_Water')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as ex:
        unreal.log_warning(f'[skydrop] lighting mode: {ex}')
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1100, -100)
    tm = node(m, unreal.MaterialExpressionTime, -1100, 50)
    sd = node(m, unreal.MaterialExpressionSceneDepth, -1300, 200)
    pd = node(m, unreal.MaterialExpressionPixelDepth, -1300, 350)
    sub = node(m, unreal.MaterialExpressionSubtract, -1100, 250)
    MEL.connect_material_expressions(sd, '', sub, 'A')
    MEL.connect_material_expressions(pd, '', sub, 'B')

    def make(code, y, otype):
        cu = node(m, unreal.MaterialExpressionCustom, -600, y)
        ins = []
        for nm in ('P', 'N', 'D', 'Tm'):
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', nm)
            ins.append(ci)
        cu.set_editor_property('inputs', ins)
        cu.set_editor_property('output_type', otype)
        cu.set_editor_property('code', code)
        MEL.connect_material_expressions(wp, '', cu, 'P')
        MEL.connect_material_expressions(wp, '', cu, 'N')  # placeholder for the shared preamble
        MEL.connect_material_expressions(sub, '', cu, 'D')
        MEL.connect_material_expressions(tm, '', cu, 'Tm')
        return cu

    out(make(CODE_WATER_COLOR, -100, unreal.CustomMaterialOutputType.CMOT_FLOAT3), MP.MP_BASE_COLOR)
    out(make(CODE_WATER_ALPHA, 200, unreal.CustomMaterialOutputType.CMOT_FLOAT1), MP.MP_OPACITY)
    out(make(CODE_WATER_NORMAL, 500, unreal.CustomMaterialOutputType.CMOT_FLOAT3), MP.MP_NORMAL)
    out(const(m, 0.04, -600, 800), MP.MP_ROUGHNESS)
    out(const(m, 0.6, -600, 900), MP.MP_SPECULAR)
    finish(m, p)


water_material()

# --- M_GrassBlades: alpha-tested blade cards with root-to-tip gradient and wind sway
GRASS_PRE = r"""
struct FSDN {
  float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
  float v(float2 p) {
    float2 i = floor(p); float2 f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), u.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), u.x), u.y);
  }
  float fbm(float2 p) { float t = 0; float a = 0.5; for (int o = 0; o < 4; o++) { t += a * v(p); p *= 2.03; a *= 0.5; } return t; }
};
FSDN n;
float up = 1.0 - UV.y;
"""
CODE_GRASS_MASK = GRASS_PRE + r"""
float best = 0.0;
for (int layer = 0; layer < 4; layer++) {
  float count = 7.0 + layer * 5.0;
  float u = UV.x * count + layer * 0.37 + n.h(float2(layer, 1.0));
  float i = floor(u);
  float f = frac(u);
  float r1 = n.h(float2(i, layer * 7.0));
  float r2 = n.h(float2(i + 11.3, layer * 3.0));
  float r3 = n.h(float2(i + 3.1, layer));
  float hgt = 0.35 + 0.65 * r1;
  float wid = 0.20 + 0.22 * r2;
  float lean = (r3 - 0.5) * 1.3;
  float t = up / hgt;
  float width = wid * pow(saturate(1.0 - t), 0.8);
  float centre = 0.5 + lean * t * t;
  float inside = (t < 1.0 && abs(f - centre) < width) ? 1.0 : 0.0;
  best = max(best, inside);
}
return best;
"""
CODE_GRASS_COLOR = GRASS_PRE + r"""
float rnd = n.h(floor(P.xy / 70.0));
float sun = n.fbm(P.xy * 0.003);
float3 root = G * 0.9;
float3 tip = G * (1.30 + 0.25 * rnd + 0.2 * sun) + float3(0.035, 0.05, 0.0);
return lerp(root, tip, smoothstep(0.0, 1.0, up));
"""
CODE_GRASS_WPO = GRASS_PRE + r"""
float sway = sin(Tm * 1.7 + P.x * 0.006 + P.y * 0.004) * 0.6 + sin(Tm * 3.1 + P.x * 0.02) * 0.25;
float gust = n.v(P.xy * 0.0008 + float2(Tm * 0.15, 0.0));
float k = up * up * (7.0 + 12.0 * gust);
float dist = length(P.xy - Cam.xy);
float sink = saturate((dist - 3000.0) / 1800.0);
return float3(sway * k, sway * k * 0.6, -up * 80.0 * sink);
"""


def grass_material():
    m, p = new_mat('M_GrassBlades')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('tangent_space_normal', False)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, -100)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1300, 50)
    tm = node(m, unreal.MaterialExpressionTime, -1300, 200)
    cam = node(m, unreal.MaterialExpressionCameraPositionWS, -1300, 300)
    # ground colour under each tuft comes from per-instance custom data (r, g, b)
    cd = []
    for k in range(3):
        c = node(m, unreal.MaterialExpressionPerInstanceCustomData, -1500, 450 + k * 100)
        c.set_editor_property('data_index', k)
        cd.append(c)
    rg = node(m, unreal.MaterialExpressionAppendVector, -1300, 500)
    link(cd[0], rg, 'A'); link(cd[1], rg, 'B')
    rgb = node(m, unreal.MaterialExpressionAppendVector, -1100, 550)
    link(rg, rgb, 'A'); link(cd[2], rgb, 'B')

    def make(code, y, otype):
        cu = node(m, unreal.MaterialExpressionCustom, -600, y)
        ins = []
        for nm in ('P', 'UV', 'Tm', 'Cam', 'G'):
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', nm)
            ins.append(ci)
        cu.set_editor_property('inputs', ins)
        cu.set_editor_property('output_type', otype)
        cu.set_editor_property('code', code)
        MEL.connect_material_expressions(wp, '', cu, 'P')
        MEL.connect_material_expressions(uv, '', cu, 'UV')
        MEL.connect_material_expressions(tm, '', cu, 'Tm')
        MEL.connect_material_expressions(cam, '', cu, 'Cam')
        MEL.connect_material_expressions(rgb, '', cu, 'G')
        return cu

    out(make(CODE_GRASS_COLOR, -100, unreal.CustomMaterialOutputType.CMOT_FLOAT3), MP.MP_BASE_COLOR)
    out(make(CODE_GRASS_MASK, 200, unreal.CustomMaterialOutputType.CMOT_FLOAT1), MP.MP_OPACITY_MASK)
    out(make(CODE_GRASS_WPO, 500, unreal.CustomMaterialOutputType.CMOT_FLOAT3), MP.MP_WORLD_POSITION_OFFSET)
    # light the blades like the ground beneath them: world-space normal pointing up
    upn = node(m, unreal.MaterialExpressionConstant3Vector, -600, 800)
    upn.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    out(upn, MP.MP_NORMAL)
    out(const(m, 0.9, -600, 900), MP.MP_ROUGHNESS)
    out(const(m, 0.2, -600, 1000), MP.MP_SPECULAR)
    finish(m, p)

grass_material()
custom_surface('M_Terrain', CODE_TERRAIN, use_vertex_color=True)
custom_surface('M_Wood', CODE_WOOD)
custom_surface('M_Brick', CODE_BRICK)
custom_surface('M_Metal', CODE_METAL)
custom_surface('M_Rock', CODE_ROCK)
custom_surface('M_Leaf', CODE_LEAF)
custom_surface('M_Ground', CODE_GROUND)

# --- empty map (the game generates the island at runtime)
MAP = '/Game/Skydrop/Maps/L_Island'
if not EAL.does_asset_exist(MAP):
    AT.create_asset('L_Island', '/Game/Skydrop/Maps', unreal.World, unreal.WorldFactory())
    EAL.save_asset(MAP)
    unreal.log(f'[skydrop] saved {MAP}')

unreal.log('[skydrop] assets done')

