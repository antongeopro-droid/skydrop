#include "SDWorld.h"
#include "SDInterior.h"
#include "SDAssets.h"
#include "SDGameMode.h"
#include "SDBuildPiece.h"
#include "SDLoot.h"
#include "ProceduralMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr float T = SDGrid::Tile;
	constexpr float H = SDGrid::Level;

	float SmoothStep01(float A, float B, float X)
	{
		const float t = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}

	float SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float L2 = AB.SizeSquared();
		const float t = L2 > 0.f ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * t);
	}

	FVector2D Dir2(float A) { return FVector2D(FMath::Cos(A), FMath::Sin(A)); }

	const TCHAR* NamesFor(ESDPoiType PT, int32 Idx)
	{
		static const TCHAR* Suburb[] = { TEXT("Maple Grove"), TEXT("Sunny Court"), TEXT("Cedar Lanes"), TEXT("Bluebell Row"), TEXT("Willow Bend") };
		static const TCHAR* City[] = { TEXT("Summit City"), TEXT("Clocktower Square") };
		static const TCHAR* Farm[] = { TEXT("Haystack Farms"), TEXT("Golden Acres") };
		static const TCHAR* Lodge[] = { TEXT("Timber Hollow"), TEXT("Bear Creek Lodge") };
		static const TCHAR* Ind[] = { TEXT("Rustworks"), TEXT("Foundry Yard") };
		static const TCHAR* Desert[] = { TEXT("Sunbaked Springs"), TEXT("Dustbowl") };
		static const TCHAR* Harbor[] = { TEXT("Gull Harbor"), TEXT("Anchor Bay") };
		switch (PT)
		{
		case ESDPoiType::City: return City[Idx % 2];
		case ESDPoiType::Farm: return Farm[Idx % 2];
		case ESDPoiType::Lodge: return Lodge[Idx % 2];
		case ESDPoiType::Industrial: return Ind[Idx % 2];
		case ESDPoiType::Desert: return Desert[Idx % 2];
		case ESDPoiType::Harbor: return Harbor[Idx % 2];
		default: return Suburb[Idx % 5];
		}
	}

	float PoiRadius(ESDPoiType PT)
	{
		switch (PT)
		{
		case ESDPoiType::City: return 5600.f;
		case ESDPoiType::Farm: return 5200.f;
		case ESDPoiType::Lodge: return 4000.f;
		default: return 4700.f;
		}
	}
}

// ====================================================================== setup / sky

ASDWorld::ASDWorld()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->SetupAttachment(Root);
	Terrain->bUseComplexAsSimpleCollision = true;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-38.f, 40.f, 0.f));
	Sun->SetIntensity(10.f);
	SDAssets::SetBoolProp(Sun, TEXT("bAtmosphereSunLight"), true);

	Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SDAssets::SetBoolProp(SkyLight, TEXT("bRealTimeCapture"), true);
	SkyLight->SetIntensity(1.2f);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;
	Post->Settings.bOverride_VignetteIntensity = true;
	Post->Settings.VignetteIntensity = 0.25f;
}

void ASDWorld::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"), nullptr, LOAD_NoWarn))
	{
		Clouds->SetMaterial(CloudMat);
	}
	int32 Preset = FMath::RandRange(0, 2);
	FParse::Value(FCommandLine::Get(), TEXT("sdsky="), Preset);
	ApplySkyPreset(Preset);
}

void ASDWorld::ApplySkyPreset(int32 Preset)
{
	using namespace SDAssets;
	SkyPreset = FMath::Clamp(Preset, 0, 2);

	struct FSkyLook
	{
		FRotator SunRot; FLinearColor SunColor; float SunLux;
		float Mie; float MieAniso; float Rayleigh; FLinearColor SkyTint; float Aerial;
		FLinearColor FogColor; float FogDensity;
		FVector4 GainShadows; FVector4 GainHighlights; float Saturation; float Contrast; float Exposure;
	};
	const FSkyLook Looks[3] = {
		// 0: golden hour - warm sun, pink/orange sky, violet distance haze
		{ FRotator(-16.f, 205.f, 0.f), FLinearColor(1.f, 0.74f, 0.52f), 9.f,
		  1.5f, 0.85f, 1.0f, FLinearColor(1.15f, 0.90f, 1.05f), 1.8f,
		  FLinearColor(0.55f, 0.42f, 0.78f), 0.0045f,
		  FVector4(0.88f, 0.92f, 1.18f, 1.f), FVector4(1.10f, 1.00f, 0.88f, 1.f), 1.25f, 1.08f, 0.15f },
		// 1: bright day - clean blue sky, teal-blue distance
		{ FRotator(-48.f, 35.f, 0.f), FLinearColor(1.f, 0.96f, 0.90f), 10.f,
		  1.2f, 0.8f, 1.0f, FLinearColor(0.95f, 1.0f, 1.12f), 2.0f,
		  FLinearColor(0.42f, 0.60f, 0.95f), 0.0040f,
		  FVector4(0.90f, 0.95f, 1.14f, 1.f), FVector4(1.06f, 1.02f, 0.95f, 1.f), 1.12f, 1.05f, 0.3f },
		// 2: dusk - purple sky, cool teal ground
		{ FRotator(-9.f, 140.f, 0.f), FLinearColor(0.88f, 0.66f, 1.f), 7.f,
		  1.4f, 0.84f, 1.1f, FLinearColor(0.95f, 0.75f, 1.22f), 2.2f,
		  FLinearColor(0.42f, 0.32f, 0.85f), 0.0055f,
		  FVector4(0.82f, 0.90f, 1.25f, 1.f), FVector4(1.02f, 0.94f, 1.08f, 1.f), 1.22f, 1.08f, 0.35f },
	};
	const FSkyLook& L = Looks[SkyPreset];

	Sun->SetWorldRotation(L.SunRot);
	Sun->SetLightColor(L.SunColor);
	Sun->SetIntensity(L.SunLux);

	SetFloatProp(Sky, TEXT("MieScatteringScale"), 0.003996f * L.Mie);
	SetFloatProp(Sky, TEXT("MieAnisotropy"), L.MieAniso);
	SetFloatProp(Sky, TEXT("RayleighScatteringScale"), 0.0331f * L.Rayleigh);
	SetColorProp(Sky, TEXT("SkyLuminanceFactor"), L.SkyTint);
	SetFloatProp(Sky, TEXT("AerialPespectiveViewDistanceScale"), L.Aerial);
	Sky->MarkRenderStateDirty();

	Fog->SetFogInscatteringColor(L.FogColor);
	Fog->SetFogDensity(L.FogDensity);
	Fog->SetFogHeightFalloff(0.05f);
	Fog->SetStartDistance(6000.f);

	FPostProcessSettings& S = Post->Settings;
	S.bOverride_ColorGainShadows = true;      S.ColorGainShadows = L.GainShadows;
	S.bOverride_ColorGainHighlights = true;   S.ColorGainHighlights = L.GainHighlights;
	S.bOverride_ColorSaturation = true;       S.ColorSaturation = FVector4(L.Saturation, L.Saturation, L.Saturation, 1.f);
	S.bOverride_ColorContrast = true;         S.ColorContrast = FVector4(L.Contrast, L.Contrast, L.Contrast, 1.f);
	S.bOverride_AutoExposureBias = true;      S.AutoExposureBias = L.Exposure;
	S.bOverride_BloomIntensity = true;        S.BloomIntensity = 0.8f;

	SkyLight->RecaptureSky();
}

// ====================================================================== terrain fields

float ASDWorld::Fbm(float X, float Y, int32 Oct) const
{
	float Sum = 0.f, Amp = 1.f, Freq = 1.f, Norm = 0.f;
	for (int32 o = 0; o < Oct; ++o)
	{
		Sum += Amp * FMath::PerlinNoise2D(FVector2D(X * Freq + NoiseOffset.X, Y * Freq + NoiseOffset.Y));
		Norm += Amp;
		Amp *= 0.5f;
		Freq *= 2.03f;
	}
	return Sum / Norm;
}

float ASDWorld::DesertW(float X, float Y) const
{
	const FVector2D P(X, Y);
	const float L = P.Size();
	if (L < 1.f) return 0.f;
	const float D = FVector2D::DotProduct(P / L, DesertDir) + Fbm(X * 0.00002f - 30.f, Y * 0.00002f + 12.f, 2) * 0.3f;
	return SmoothStep01(0.55f, 0.72f, D) * SmoothStep01(IslandRadius * 0.28f, IslandRadius * 0.45f, L);
}

float ASDWorld::ForestW(float X, float Y) const
{
	const FVector2D P(X, Y);
	const float L = P.Size();
	if (L < 1.f) return 0.f;
	const float D = FVector2D::DotProduct(P / L, ForestDir) + Fbm(X * 0.000025f + 70.f, Y * 0.000025f - 5.f, 2) * 0.3f;
	return SmoothStep01(0.55f, 0.75f, D) * SmoothStep01(IslandRadius * 0.22f, IslandRadius * 0.4f, L);
}

float ASDWorld::RiverDist(float X, float Y) const
{
	float Best = 1e9f;
	const FVector2D P(X, Y);
	for (int32 i = 0; i + 1 < River.Num(); ++i) Best = FMath::Min(Best, SegDist(P, River[i], River[i + 1]));
	return Best;
}

float ASDWorld::RoadDist(float X, float Y) const
{
	float Best = 1e9f;
	const FVector2D P(X, Y);
	for (const TPair<FVector2D, FVector2D>& Rd : Roads) Best = FMath::Min(Best, SegDist(P, Rd.Key, Rd.Value));
	return Best;
}

float ASDWorld::RawHeight(float X, float Y) const
{
	const FVector2D P(X, Y);
	const float R01 = P.Size() / IslandRadius;
	const float Edge = Fbm(X * 0.000022f + 11.3f, Y * 0.000022f - 4.1f, 4) * 0.28f;
	const float Mask = SmoothStep01(1.0f, 0.86f, R01 + Edge);

	// highlands: big terraced hills with cliff faces; lowlands: gentle rolling meadows
	const float High = SmoothStep01(0.50f, 0.76f, Fbm(X * 0.000014f + 3.f, Y * 0.000014f - 9.f, 2) * 0.5f + 0.5f);
	float Hills = Fbm(X * 0.00003f, Y * 0.00003f, 4) * 0.5f + 0.5f;
	Hills *= Hills;
	float Land = 170.f + Hills * (1500.f + High * 4400.f);
	// rolling bumps everywhere so open ground is never flat
	Land += (Fbm(X * 0.00009f - 13.f, Y * 0.00009f + 21.f, 3) * 0.5f + 0.5f) * 420.f;
	const float Step = 640.f;
	const float Lv = (Land - 170.f) / Step;
	const float F = FMath::FloorToFloat(Lv);
	const float Terr = 170.f + (F + SmoothStep01(0.55f, 0.85f, Lv - F)) * Step;
	Land = FMath::Lerp(Land, Terr, 0.3f + 0.6f * High);

	// desert: flat sand with tall flat-topped mesas
	const float Dw = DesertW(X, Y);
	if (Dw > 0.f)
	{
		const float M = Fbm(X * 0.00007f + 40.f, Y * 0.00007f, 3) * 0.5f + 0.5f;
		const float Mesa = 240.f + SmoothStep01(0.60f, 0.645f, M) * 1500.f + SmoothStep01(0.73f, 0.77f, M) * 900.f;
		Land = FMath::Lerp(Land, Mesa, Dw);
	}

	// one big mountain
	const float Md = FVector2D::Distance(P, MountainPos);
	const float Ridge = 1.f - FMath::Abs(FMath::PerlinNoise2D(FVector2D(X * 0.00008f + 5.f, Y * 0.00008f)));
	Land += 8500.f * FMath::Exp(-FMath::Square(Md / 15000.f)) * (0.75f + 0.25f * Ridge);

	Land += Fbm(X * 0.00025f, Y * 0.00025f, 2) * 60.f;

	// rock buttes: sheer cliffs, flat grassy tops, optional upper tier and a scree apron at the foot
	for (const FButte& B : Buttes)
	{
		const FVector2D D = P - B.Pos;
		const float Dist = D.Size();
		if (Dist > B.Rad * 1.7f) continue;
		const float Ang = FMath::Atan2(D.Y, D.X);
		const float Wob = 1.f + 0.32f * FMath::PerlinNoise2D(FVector2D(FMath::Cos(Ang) * 1.7f + B.Seed, FMath::Sin(Ang) * 1.7f - B.Seed));
		const float d = Dist / (B.Rad * Wob);
		const float Tier1 = SmoothStep01(1.0f, 0.9f, d) * B.Hgt;
		float Tier2 = 0.f;
		if (B.Hgt2 > 0.f)
		{
			const float d2 = FVector2D::Distance(P, B.Pos + B.Off2) / (B.Rad2 * Wob);
			Tier2 = SmoothStep01(1.0f, 0.88f, d2) * B.Hgt2;
		}
		const float Apron = SmoothStep01(1.6f, 1.0f, d) * B.Hgt * 0.14f;
		const float Rough = d > 0.85f && d < 1.05f ? Fbm(X * 0.0012f, Y * 0.0012f, 2) * 90.f : 0.f; // broken cliff face
		Land = FMath::Max(Land, B.BaseZ + Tier1 + Tier2 + Apron + Rough);
	}

	if (LakeRadius > 0.f)
	{
		const float LD = FVector2D::Distance(P, LakePos);
		Land = FMath::Lerp(Land, -130.f, 1.f - SmoothStep01(LakeRadius * 0.6f, LakeRadius * 1.25f, LD));
	}
	if (River.Num() > 1)
	{
		const float Rd = RiverDist(X, Y);
		Land = FMath::Lerp(Land, -95.f, 1.f - SmoothStep01(650.f, 1700.f, Rd));
	}
	return FMath::Lerp(-900.f, Land, Mask);
}

float ASDWorld::HeightAt(float X, float Y) const
{
	float Z = RawHeight(X, Y);
	for (const FSDPoi& P : Pois)
	{
		const float D = FVector2D::Distance(P.Pos, FVector2D(X, Y));
		const float W = 1.f - SmoothStep01(P.Radius * 0.8f, P.Radius * 1.35f, D);
		if (W > 0.f) Z = FMath::Lerp(Z, P.Z, W);
	}
	return Z;
}

FLinearColor ASDWorld::GroundColor(float X, float Y, float Z, float Nz) const
{
	if (Z < 25.f) return FLinearColor(0.36f, 0.31f, 0.20f); // pebbly river bed / wet sand, seen through the water
	const float Var = Fbm(X * 0.0004f + 50.f, Y * 0.0004f, 2) * 0.5f + 0.5f;
	const float Dw = DesertW(X, Y);
	const float Fw = ForestW(X, Y);

	const float Patch = SmoothStep01(0.35f, 0.65f, Fbm(X * 0.00012f - 20.f, Y * 0.00012f + 7.f, 3) * 0.5f + 0.5f);
	const FLinearColor Deep(0.05f, 0.21f, 0.035f), Mid(0.11f, 0.32f, 0.035f), Bright(0.21f, 0.44f, 0.05f);
	FLinearColor Grass = Deep * (1.f - Patch) + (Mid * (1.f - Var) + Bright * Var) * Patch;
	Grass = Grass * (1.f - Fw * 0.55f) + FLinearColor(0.025f, 0.14f, 0.05f) * (Fw * 0.55f);
	if (Nz < 0.85f) Grass = Grass * 0.8f + FLinearColor(0.30f, 0.22f, 0.12f) * 0.2f;

	const float G = 0.85f + Var * 0.3f;
	// grey stone cliffs, a little warmer low down where dirt collects
	FLinearColor Cliff(0.40f * G, 0.40f * G, 0.42f * G);
	if (Z < 900.f) Cliff = FMath::Lerp(FLinearColor(0.34f * G, 0.29f * G, 0.24f * G), Cliff, SmoothStep01(300.f, 900.f, Z));

	FLinearColor C = Nz < 0.72f ? Cliff : Grass;
	const FLinearColor Sand = FLinearColor(0.62f, 0.38f, 0.15f) * (0.92f + Var * 0.16f);
	const FLinearColor Red = FLinearColor(0.46f, 0.19f, 0.07f) * G;
	C = FMath::Lerp(C, Nz < 0.72f ? Red : Sand, Dw);

	if (Z < 170.f && FVector2D(X, Y).Size() > IslandRadius * 0.6f) C = FLinearColor(0.66f, 0.56f, 0.34f); // beach
	const float Rv = River.Num() > 1 ? RiverDist(X, Y) : 1e9f;
	if (Rv < 2300.f && Z < 600.f) C = FMath::Lerp(FLinearColor(0.24f, 0.19f, 0.11f), C, SmoothStep01(1500.f, 2300.f, Rv));

	const float Rd = RoadDist(X, Y);
	if (Rd < 520.f)
	{
		const FLinearColor Dirt(0.30f, 0.23f, 0.14f);
		C = FMath::Lerp(Dirt, C, SmoothStep01(300.f, 520.f, Rd));
	}
	FLinearColor Tc; float Paved = 0.f;
	const float Tw = TownGround(X, Y, Tc, Paved);
	if (Tw > 0.f && Nz > 0.7f) C = FMath::Lerp(C, Tc, Tw);
	return C;
}

bool ASDWorld::NearPoi(const FVector2D& P, float Extra) const
{
	for (const FSDPoi& Poi : Pois)
	{
		if (FVector2D::Distance(Poi.Pos, P) < Poi.Radius + Extra) return true;
	}
	return false;
}

FVector ASDWorld::RandomLandPoint(FRandomStream& R, float MinH, float MaxFrac) const
{
	for (int32 Try = 0; Try < 80; ++Try)
	{
		const float A = R.FRand() * 2.f * PI;
		const float Rad = FMath::Sqrt(R.FRand()) * IslandRadius * MaxFrac;
		const FVector2D P(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad);
		const float Z = HeightAt(P.X, P.Y);
		if (Z > MinH && Z < 5000.f) return FVector(P.X, P.Y, Z);
	}
	return FVector(0, 0, HeightAt(0, 0));
}

// ====================================================================== terrain mesh + map

void ASDWorld::BuildTerrain()
{
	const int32 N = 512;
	const int32 Sec = 8;
	const int32 Per = N / Sec;
	const float Ext = MapExtent();
	const float Step = Ext * 2.f / N;
	const int32 Row = N + 1;

	TArray<float> Hs;
	Hs.SetNumUninitialized(Row * Row);
	ParallelFor(Row, [&](int32 y)
	{
		for (int32 x = 0; x < Row; ++x)
		{
			Hs[y * Row + x] = HeightAt(-Ext + x * Step, -Ext + y * Step);
		}
	});
	auto Hg = [&](int32 x, int32 y) { return Hs[FMath::Clamp(y, 0, N) * Row + FMath::Clamp(x, 0, N)]; };

	TArray<FVector> Ns;
	TArray<FLinearColor> Cs;
	Ns.SetNumUninitialized(Row * Row);
	Cs.SetNumUninitialized(Row * Row);
	ParallelFor(Row, [&](int32 y)
	{
		for (int32 x = 0; x < Row; ++x)
		{
			const FVector Nn = FVector(Hg(x - 1, y) - Hg(x + 1, y), Hg(x, y - 1) - Hg(x, y + 1), 2.f * Step).GetSafeNormal();
			Ns[y * Row + x] = Nn;
			Cs[y * Row + x] = GroundColor(-Ext + x * Step, -Ext + y * Step, Hg(x, y), Nn.Z);
		}
	});

	int32 SectionIdx = 0;
	UMaterialInterface* Mat = SDAssets::VertexColor();
	for (int32 sy = 0; sy < Sec; ++sy)
	{
		for (int32 sx = 0; sx < Sec; ++sx)
		{
			float MaxH = -1e9f;
			for (int32 y = sy * Per; y <= (sy + 1) * Per; ++y)
				for (int32 x = sx * Per; x <= (sx + 1) * Per; ++x) MaxH = FMath::Max(MaxH, Hg(x, y));
			if (MaxH < -60.f) continue; // fully under the sea
			TArray<FVector> V; TArray<int32> Tri; TArray<FVector> Nrm; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<FProcMeshTangent> Tan;
			const int32 SR = Per + 1;
			V.Reserve(SR * SR); Nrm.Reserve(SR * SR); UV.Reserve(SR * SR); Col.Reserve(SR * SR);
			for (int32 y = 0; y <= Per; ++y)
			{
				for (int32 x = 0; x <= Per; ++x)
				{
					const int32 gx = sx * Per + x, gy = sy * Per + y;
					const float X = -Ext + gx * Step, Y = -Ext + gy * Step;
					V.Add(FVector(X, Y, Hg(gx, gy)));
					Nrm.Add(Ns[gy * Row + gx]);
					UV.Add(FVector2D(X / 1000.f, Y / 1000.f));
					Col.Add(Cs[gy * Row + gx]);
				}
			}
			for (int32 y = 0; y < Per; ++y)
			{
				for (int32 x = 0; x < Per; ++x)
				{
					const int32 A = y * SR + x, B = A + 1, C = A + SR, D = C + 1;
					Tri.Add(A); Tri.Add(C); Tri.Add(B);
					Tri.Add(B); Tri.Add(C); Tri.Add(D);
				}
			}
			Terrain->CreateMeshSection_LinearColor(SectionIdx, V, Tri, Nrm, UV, Col, Tan, true);
			Terrain->SetMaterial(SectionIdx, Mat);
			++SectionIdx;
		}
	}
	Terrain->SetCollisionProfileName(TEXT("BlockAll"));

	WaterMesh = SDAssets::AddMesh(this, Root, SDAssets::Plane(), FVector(0, 0, 0), FVector(6000.f, 6000.f, 1.f), FRotator::ZeroRotator, SDAssets::Water(), false);
	WaterMesh->SetCastShadow(false);
	// hidden floor so open water is wadeable instead of bottomless
	if (UStaticMeshComponent* Floor = SDAssets::AddBox(this, Root, FVector(0, 0, -175.f), FVector(600000.f, 600000.f, 100.f), FRotator::ZeroRotator, nullptr, true))
	{
		Floor->SetHiddenInGame(true);
		Floor->SetVisibility(false);
		Floor->SetCastShadow(false);
	}

	// --- map image (screen x = world +Y, screen up = world +X), shaded relief
	const int32 Res = N;
	TArray<FColor> Px;
	Px.SetNumUninitialized(Res * Res);
	const FVector Light = FVector(1.f, -1.f, 1.6f).GetSafeNormal();
	ParallelFor(Res, [&](int32 py)
	{
		for (int32 px = 0; px < Res; ++px)
		{
			const int32 gx = N - py; // world X index
			const int32 gy = px;     // world Y index
			const float Z = Hg(gx, gy);
			FLinearColor C;
			if (Z < 0.f)
			{
				const float Depth = FMath::Clamp(-Z / 900.f, 0.f, 1.f);
				C = FMath::Lerp(FLinearColor(0.25f, 0.62f, 0.72f), FLinearColor(0.05f, 0.28f, 0.45f), Depth);
			}
			else
			{
				const FVector Nn = Ns[FMath::Clamp(gy, 0, N) * Row + FMath::Clamp(gx, 0, N)];
				const float Shade = 0.7f + 0.45f * FVector::DotProduct(Nn, Light);
				C = Cs[FMath::Clamp(gy, 0, N) * Row + FMath::Clamp(gx, 0, N)] * Shade * 1.6f;
			}
			Px[py * Res + px] = C.ToFColor(true);
		}
	});
	MapTexture = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
	if (MapTexture)
	{
		MapTexture->SRGB = true;
		MapTexture->AddressX = TA_Clamp;
		MapTexture->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = MapTexture->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		MapTexture->UpdateResource();
	}
}

void ASDWorld::BuildMapTexture()
{
	// built together with the terrain
}

// ====================================================================== generation

void ASDWorld::Generate(int32 Seed, ASDGameMode* GM)
{
	FRandomStream R(Seed);
	NoiseOffset = FVector2D(R.FRandRange(-500.f, 500.f), R.FRandRange(-500.f, 500.f));
	const float BaseA = R.FRand() * 2.f * PI;
	DesertDir = Dir2(BaseA);
	ForestDir = Dir2(BaseA + PI * R.FRandRange(0.7f, 0.95f));
	MountainPos = Dir2(BaseA - PI * R.FRandRange(0.45f, 0.65f)) * IslandRadius * 0.6f;
	LakePos = Dir2(R.FRand() * 2.f * PI) * IslandRadius * R.FRandRange(0.1f, 0.3f);
	LakeRadius = R.FRandRange(5000.f, 6500.f);

	// river: coast -> lake -> opposite coast, meandering
	{
		River.Reset();
		const float A = R.FRand() * 2.f * PI;
		const FVector2D From = Dir2(A) * IslandRadius * 1.08f;
		const FVector2D To = -Dir2(A + R.FRandRange(-0.35f, 0.35f)) * IslandRadius * 1.08f;
		auto Leg = [&](const FVector2D& S, const FVector2D& E, bool bLast)
		{
			const int32 Steps = 7;
			const FVector2D Side = FVector2D(-(E - S).Y, (E - S).X).GetSafeNormal();
			for (int32 i = 0; i < Steps; ++i)
			{
				const float t = (float)i / Steps;
				const float Wig = (i == 0) ? 0.f : R.FRandRange(-1.f, 1.f) * IslandRadius * 0.05f;
				River.Add(FMath::Lerp(S, E, t) + Side * Wig);
			}
			if (bLast) River.Add(E);
		};
		Leg(From, LakePos, false);
		Leg(LakePos, To, true);
	}

	// --- rock buttes scattered across the open country
	Buttes.Reset();
	for (int32 Try = 0; Try < 600 && Buttes.Num() < 46; ++Try)
	{
		const FVector2D P = Dir2(R.FRand() * 2.f * PI) * FMath::Sqrt(R.FRand()) * IslandRadius * 0.8f;
		const float Rad = R.FRandRange(1800.f, 5200.f);
		if (FVector2D::Distance(P, LakePos) < LakeRadius + Rad + 1500.f) continue;
		if (RiverDist(P.X, P.Y) < Rad * 1.6f + 1500.f) continue;
		if (FVector2D::Distance(P, MountainPos) < 16000.f) continue;
		bool bFar = true;
		for (const FButte& O : Buttes) { if (FVector2D::Distance(O.Pos, P) < (O.Rad + Rad) * 1.5f) { bFar = false; break; } }
		if (!bFar) continue;
		const float Base = RawHeight(P.X, P.Y);
		if (Base < 150.f) continue;
		FButte B;
		B.Pos = P;
		B.Rad = Rad;
		B.Hgt = R.FRandRange(900.f, 1500.f) + Rad * R.FRandRange(0.15f, 0.55f);
		B.BaseZ = Base - 60.f;
		B.Seed = R.FRandRange(0.f, 100.f);
		if (R.FRand() < 0.45f)
		{
			B.Rad2 = Rad * R.FRandRange(0.4f, 0.65f);
			B.Hgt2 = R.FRandRange(500.f, 1600.f);
			B.Off2 = Dir2(R.FRand() * 2.f * PI) * (Rad - B.Rad2) * R.FRandRange(0.f, 0.6f);
		}
		else
		{
			B.Rad2 = 1.f; B.Hgt2 = 0.f; B.Off2 = FVector2D::ZeroVector;
		}
		Buttes.Add(B);
	}

	// --- towns, placed by type
	Pois.Reset();
	Roads.Reset();
	Used.Reset();
	const ESDPoiType Plan[] = {
		ESDPoiType::City, ESDPoiType::Desert, ESDPoiType::Lodge, ESDPoiType::Harbor, ESDPoiType::Farm, ESDPoiType::Industrial,
		ESDPoiType::Suburb, ESDPoiType::Suburb, ESDPoiType::Farm, ESDPoiType::Lodge, ESDPoiType::Industrial, ESDPoiType::Suburb,
		ESDPoiType::Suburb, ESDPoiType::Harbor
	};
	TMap<int32, int32> TypeCount;
	for (ESDPoiType Type : Plan)
	{
		for (int32 Try = 0; Try < 700; ++Try)
		{
			const float A = R.FRand() * 2.f * PI;
			float Rmin = 0.15f, Rmax = 0.74f;
			if (Type == ESDPoiType::City) { Rmin = 0.08f; Rmax = 0.45f; }
			if (Type == ESDPoiType::Harbor) { Rmin = 0.70f; Rmax = 0.80f; }
			const FVector2D P = Dir2(A) * IslandRadius * FMath::Lerp(Rmin, Rmax, R.FRand());
			const float Dw = DesertW(P.X, P.Y), Fw = ForestW(P.X, P.Y);
			if (Type == ESDPoiType::Desert && Dw < 0.8f) continue;
			if (Type == ESDPoiType::Lodge && Fw < 0.7f) continue;
			if (Type != ESDPoiType::Desert && Dw > 0.3f) continue;
			if (Type != ESDPoiType::Lodge && Fw > 0.5f && Try < 400) continue;
			const float Z = RawHeight(P.X, P.Y);
			if (Z < 150.f || Z > 2800.f) continue;
			const float Rad = PoiRadius(Type);
			float Lo = Z, Hi = Z;
			for (int32 k = 0; k < 8; ++k)
			{
				const FVector2D Q = P + Dir2(k * PI / 4.f) * Rad * 0.7f;
				const float Zq = RawHeight(Q.X, Q.Y);
				Lo = FMath::Min(Lo, Zq); Hi = FMath::Max(Hi, Zq);
			}
			if (Hi - Lo > 1500.f || Lo < 60.f) continue;
			bool bOk = FVector2D::Distance(P, LakePos) > LakeRadius + Rad + 2000.f
				&& FVector2D::Distance(P, MountainPos) > 20000.f
				&& RiverDist(P.X, P.Y) > Rad + 1500.f;
			for (const FSDPoi& O : Pois)
			{
				if (FVector2D::Distance(O.Pos, P) < 15500.f) { bOk = false; break; }
			}
			for (const FButte& B : Buttes)
			{
				if (FVector2D::Distance(B.Pos, P) < B.Rad * 1.7f + Rad) { bOk = false; break; }
			}
			if (!bOk) continue;
			FSDPoi Poi;
			Poi.Type = Type;
			Poi.Name = NamesFor(Type, TypeCount.FindOrAdd((int32)Type)++);
			Poi.Pos = P;
			Poi.Radius = Rad;
			Poi.Z = FMath::CeilToFloat(Z / H) * H - 30.f;
			Pois.Add(Poi);
			break;
		}
	}

	// --- roads: each town links to its two nearest neighbours
	for (int32 i = 0; i < Pois.Num(); ++i)
	{
		TArray<TPair<float, int32>> D;
		for (int32 j = 0; j < Pois.Num(); ++j)
		{
			if (j != i) D.Add({ FVector2D::Distance(Pois[i].Pos, Pois[j].Pos), j });
		}
		D.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		for (int32 k = 0; k < FMath::Min(2, D.Num()); ++k)
		{
			const int32 j = D[k].Value;
			bool bDup = false;
			for (const TPair<FVector2D, FVector2D>& Rd : Roads)
			{
				if ((Rd.Key == Pois[j].Pos && Rd.Value == Pois[i].Pos) || (Rd.Key == Pois[i].Pos && Rd.Value == Pois[j].Pos)) bDup = true;
			}
			if (!bDup)
			{
				// slight bend through a midpoint
				const FVector2D Mid = (Pois[i].Pos + Pois[j].Pos) * 0.5f + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * 2500.f;
				Roads.Add({ Pois[i].Pos, Mid });
				Roads.Add({ Mid, Pois[j].Pos });
			}
		}
	}

	BuildTerrain();

	for (const FSDPoi& Poi : Pois) BuildTown(R, GM, Poi);

	UWorld* W = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto TileOf = [](const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / T), FMath::FloorToInt(P.Y / T)); };

	// --- lone houses and cabins between towns
	for (int32 i = 0, Made = 0; i < 300 && Made < 34; ++i)
	{
		const FVector P = RandomLandPoint(R, 200.f, 0.85f);
		if (NearPoi(FVector2D(P), 3000.f) || RiverDist(P.X, P.Y) < 2500.f || DesertW(P.X, P.Y) > 0.6f) continue;
		float Lo = P.Z, Hi = P.Z;
		for (int32 k = 0; k < 4; ++k)
		{
			const float Zq = HeightAt(P.X + (k < 2 ? -900.f : 900.f), P.Y + (k % 2 ? -900.f : 900.f));
			Lo = FMath::Min(Lo, Zq); Hi = FMath::Max(Hi, Zq);
		}
		if (Hi - Lo > 500.f) continue;
		const FIntPoint Tl = TileOf(P);
		const int32 Wd = R.RandRange(1, 2), Dp = 2;
		if (!Reserve(Tl.X, Tl.Y, Wd, Dp, 1)) continue;
		const int32 L0 = FMath::CeilToInt((Hi - 20.f) / H);
		// foundation so it never floats
		Slab(FVector(Tl.X * T + Wd * T * 0.5f, Tl.Y * T + Dp * T * 0.5f, (L0 * H + Lo - 200.f) * 0.5f - 22.f),
			FVector(Wd * T + 40.f, Dp * T + 40.f, FMath::Max(60.f, L0 * H - Lo + 200.f)), FLinearColor(0.22f, 0.21f, 0.2f));
		BuildHouse(R, GM, Tl.X, Tl.Y, Wd, Dp, R.FRand() < 0.3f ? 2 : 1, L0, R.FRand() < 0.7f ? ESDMat::Wood : ESDMat::Stone, false);
		++Made;
	}

	// --- vegetation by biome
	for (int32 i = 0; i < 9000; ++i)
	{
		const float A = R.FRand() * 2.f * PI;
		const FVector2D P = Dir2(A) * FMath::Sqrt(R.FRand()) * IslandRadius * 0.95f;
		const float Z = HeightAt(P.X, P.Y);
		if (Z < 190.f || Z > 5200.f) continue;
		if (Used.Contains(TileOf(FVector(P, 0)))) continue;
		if (RoadDist(P.X, P.Y) < 650.f || RiverDist(P.X, P.Y) < 1700.f) continue;
		const float Dw = DesertW(P.X, P.Y), Fw = ForestW(P.X, P.Y);
		bool bInTown = false;
		for (const FSDPoi& Poi : Pois)
		{
			if (FVector2D::Distance(Poi.Pos, P) < Poi.Radius * 0.9f) { bInTown = Poi.Type != ESDPoiType::Lodge || R.FRand() < 0.7f; break; }
		}
		if (bInTown) continue;
		const FVector At(P.X, P.Y, Z - 10.f);
		if (Dw > 0.5f)
		{
			if (R.FRand() < 0.08f) SpawnResource(R, (int32)ESDResource::Palm, At, R.FRandRange(0.85f, 1.2f));
			continue;
		}
		const float Density = 0.32f + 0.65f * Fw;
		if (R.FRand() < Density) SpawnResource(R, (int32)ESDResource::Tree, At, R.FRandRange(0.85f, 1.5f));
	}
	for (int32 i = 0; i < 700; ++i)
	{
		const FVector P = RandomLandPoint(R, 160.f, 0.93f);
		if (Used.Contains(TileOf(P)) || NearPoi(FVector2D(P), -500.f) || RoadDist(P.X, P.Y) < 600.f) continue;
		if (R.FRand() < 0.45f) SpawnResource(R, (int32)ESDResource::Rock, P - FVector(0, 0, 25), R.FRandRange(0.9f, 2.0f));
	}

	// --- loot in the open
	for (int32 i = 0; i < 420; ++i)
	{
		const FVector P = RandomLandPoint(R, 200.f, 0.88f);
		if (Used.Contains(TileOf(P))) continue;
		SDLoot::SpawnFloorLoot(W, P + FVector(0, 0, 30));
	}
	for (int32 i = 0; i < 75; ++i)
	{
		const FVector P = RandomLandPoint(R, 200.f, 0.85f);
		if (Used.Contains(TileOf(P))) continue;
		W->SpawnActor<ASDChest>(ASDChest::StaticClass(), FTransform(FRotator(0, R.FRand() * 360.f, 0), P), SP);
	}
	UE_LOG(LogTemp, Display, TEXT("[skydrop] island: %d towns, %d resources, %d build pieces"), Pois.Num(), Spawned, GM->Pieces.Num());
}

// ====================================================================== helpers

bool ASDWorld::Reserve(int32 I0, int32 J0, int32 W, int32 D, int32 Margin)
{
	for (int32 i = I0 - Margin; i < I0 + W + Margin; ++i)
		for (int32 j = J0 - Margin; j < J0 + D + Margin; ++j)
			if (Used.Contains(FIntPoint(i, j))) return false;
	for (int32 i = I0; i < I0 + W; ++i)
		for (int32 j = J0; j < J0 + D; ++j)
			Used.Add(FIntPoint(i, j));
	return true;
}

void ASDWorld::Slab(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollide, ESDSurf Surf)
{
	UStaticMeshComponent* C = SDAssets::AddBox(this, Root, Center, Size, FRotator::ZeroRotator, SDAssets::Surface(Surf, Color, 0.f, 0.9f), bCollide);
	C->SetCullDistance(90000.f);
}

void ASDWorld::SpawnResource(FRandomStream& R, int32 Type, const FVector& At, float Size)
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASDResource* Res = GetWorld()->SpawnActor<ASDResource>(ASDResource::StaticClass(), FTransform(FRotator(0, R.FRand() * 360.f, 0), At), SP))
	{
		Res->Setup((ESDResource)Type, Size, R.RandHelper(1 << 30));
		++Spawned;
	}
}

// ====================================================================== towns

void ASDWorld::BuildTown(FRandomStream& R, ASDGameMode* GM, const FSDPoi& Poi)
{
	CurrentPoiType = (int32)Poi.Type;
	const int32 CI = FMath::FloorToInt(Poi.Pos.X / T);
	const int32 CJ = FMath::FloorToInt(Poi.Pos.Y / T);
	const int32 L0 = FMath::RoundToInt((Poi.Z + 30.f) / H);
	const float FloorZ = L0 * H;
	const int32 Half = FMath::FloorToInt(Poi.Radius * 0.8f / T);
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto Street = [&](bool bAlongX, int32 Line)
	{
		// reserve a one-tile street and pave it
		for (int32 k = -Half; k <= Half; ++k) Used.Add(bAlongX ? FIntPoint(CI + k, Line) : FIntPoint(Line, CJ + k));
		const FVector C = bAlongX ? FVector(CI * T + T * 0.5f, Line * T + T * 0.5f, FloorZ - 32.f) : FVector(Line * T + T * 0.5f, CJ * T + T * 0.5f, FloorZ - 32.f);
		const float Len = (Half * 2 + 1) * T;
		const FVector Ax = bAlongX ? FVector(1, 0, 0) : FVector(0, 1, 0), Ac = bAlongX ? FVector(0, 1, 0) : FVector(1, 0, 0);
		const FVector Ground = FVector(C.X, C.Y, FloorZ - 30.f);
		// asphalt road a little above the ground, raised concrete sidewalks with curbs either side
		Slab(Ground, Ax * Len + Ac * (T * 0.64f) + FVector(0, 0, 10.f), FLinearColor(0.085f, 0.085f, 0.09f), true, ESDSurf::Asphalt);
		for (const float Side : { -1.f, 1.f })
		{
			Slab(Ground + Ac * (Side * T * 0.41f) + FVector(0, 0, 6.f), Ax * Len + Ac * (T * 0.18f) + FVector(0, 0, 22.f), FLinearColor(0.5f, 0.5f, 0.48f), true, ESDSurf::Concrete);
		}
		// dashed centre line
		for (float d = -Len * 0.5f + 80.f; d < Len * 0.5f - 80.f; d += 260.f)
		{
			Slab(Ground + Ax * d + FVector(0, 0, 5.6f), Ax * 130.f + Ac * 12.f + FVector(0, 0, 1.f), FLinearColor(0.85f, 0.75f, 0.4f), false, ESDSurf::Plain);
		}
		// street lamps on alternating sides
		int32 n = 0;
		for (float d = -Len * 0.5f + T; d < Len * 0.5f - T * 0.5f; d += T * 2.5f, ++n)
		{
			const float Side = (n % 2) ? 1.f : -1.f;
			const FVector Foot = Ground + Ax * d + Ac * (Side * T * 0.46f) + FVector(0, 0, 17.f);
			StreetLamp(Foot, FMath::RadiansToDegrees(FMath::Atan2(-Side * Ac.Y, -Side * Ac.X)));
		}
	};
	auto TryBuild = [&](int32 W, int32 D, int32 Margin, TFunctionRef<void(int32, int32)> Fn, int32 Tries = 40)
	{
		for (int32 t = 0; t < Tries; ++t)
		{
			const int32 I0 = CI + R.RandRange(-Half, Half - W + 1);
			const int32 J0 = CJ + R.RandRange(-Half, Half - D + 1);
			if (Reserve(I0, J0, W, D, Margin)) { Fn(I0, J0); return true; }
		}
		return false;
	};
	auto RandMat = [&](float PWood, float PStone) { const float M = R.FRand(); return M < PWood ? ESDMat::Wood : M < PWood + PStone ? ESDMat::Stone : ESDMat::Metal; };

	switch (Poi.Type)
	{
	case ESDPoiType::City:
	{
		Street(true, CJ);
		Street(false, CI);
		for (int32 b = 0; b < 9; ++b)
		{
			const int32 W = R.RandRange(2, 4), D = R.RandRange(3, 4);
			TryBuild(W, D, 1, [&](int32 I0, int32 J0) { BuildTower(R, GM, I0, J0, W, D, R.RandRange(3, 6), L0, R.FRand() < 0.6f ? ESDMat::Stone : ESDMat::Metal); });
		}
		for (int32 b = 0; b < 3; ++b)
		{
			TryBuild(2, 2, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, 2, 2, 2, L0, ESDMat::Stone, true); });
		}
		// clock tower centrepiece
		TryBuild(1, 1, 1, [&](int32 I0, int32 J0)
		{
			for (int32 f = 0; f < 6; ++f)
			{
				for (int32 a = 0; a < 2; ++a)
				{
					FSDBuildSlot S; S.Type = ESDBuildType::Wall; S.L = L0 + f; S.Axis = a;
					S.I = I0; S.J = J0; GM->TryPlace(S, ESDMat::Stone, nullptr, true, true);
					S.I = a == 0 ? I0 + 1 : I0; S.J = a == 0 ? J0 : J0 + 1; GM->TryPlace(S, ESDMat::Stone, nullptr, true, true);
				}
			}
			FSDBuildSlot Roof; Roof.Type = ESDBuildType::Cone; Roof.I = I0; Roof.J = J0; Roof.L = L0 + 6;
			GM->TryPlace(Roof, ESDMat::Metal, nullptr, true, true);
			SDAssets::AddMesh(this, Root, SDAssets::Cylinder(), FVector(I0 * T + T * 0.5f, J0 * T - 14.f, (L0 + 5) * H + H * 0.5f),
				FVector(2.4f, 2.4f, 0.1f), FRotator(0, 0, 90), SDAssets::Solid(FLinearColor(0.9f, 0.88f, 0.8f), 0.3f), true);
		});
		break;
	}
	case ESDPoiType::Suburb:
	case ESDPoiType::Harbor:
	{
		Street(true, CJ);
		const int32 N = Poi.Type == ESDPoiType::Harbor ? 7 : 10;
		for (int32 b = 0; b < N; ++b)
		{
			const int32 W = R.RandRange(2, 3), D = R.RandRange(2, 3);
			TryBuild(W, D, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, W, D, R.FRand() < 0.5f ? 2 : 1, L0, RandMat(0.55f, 0.35f), R.FRand() < 0.2f); });
		}
		if (Poi.Type == ESDPoiType::Harbor)
		{
			// wooden pier heading out to sea
			const FVector2D Out = Poi.Pos.GetSafeNormal();
			const int32 Axis = FMath::Abs(Out.X) > FMath::Abs(Out.Y) ? 0 : 1;
			const int32 Sgn = (Axis == 0 ? Out.X : Out.Y) > 0 ? 1 : -1;
			for (int32 k = Half - 1; k < Half + 14; ++k)
			{
				FSDBuildSlot S; S.Type = ESDBuildType::Floor; S.L = L0;
				S.I = Axis == 0 ? CI + Sgn * k : CI; S.J = Axis == 0 ? CJ : CJ + Sgn * k;
				Used.Add(FIntPoint(S.I, S.J));
				GM->TryPlace(S, ESDMat::Wood, nullptr, true, true);
			}
		}
		break;
	}
	case ESDPoiType::Farm:
	{
		TryBuild(3, 4, 1, [&](int32 I0, int32 J0) { BuildBarn(R, GM, I0, J0, L0); });
		for (int32 b = 0; b < 2; ++b)
		{
			TryBuild(2, 2, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, 2, 2, 2, L0, ESDMat::Wood, false); });
		}
		// crop fields
		for (int32 f = 0; f < 4; ++f)
		{
			TryBuild(3, 4, 0, [&](int32 I0, int32 J0)
			{
				const bool bWheat = R.FRand() < 0.5f;
				const FLinearColor Crop = bWheat ? FLinearColor(0.55f, 0.40f, 0.10f) : FLinearColor(0.10f, 0.32f, 0.04f);
				Slab(FVector(I0 * T + 1.5f * T, J0 * T + 2.f * T, FloorZ - 40.f), FVector(3 * T, 4 * T, 30.f), FLinearColor(0.20f, 0.13f, 0.07f), false);
				for (int32 row = 0; row < 10; ++row)
				{
					Slab(FVector(I0 * T + 1.5f * T, J0 * T + (row + 0.5f) * (4 * T / 10.f), FloorZ - 5.f), FVector(3 * T - 60.f, 90.f, 60.f), Crop, false, ESDSurf::Leaf);
				}
			});
		}
		break;
	}
	case ESDPoiType::Lodge:
	{
		TryBuild(3, 3, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, 3, 3, 2, L0, ESDMat::Wood, false); });
		for (int32 b = 0; b < 6; ++b)
		{
			const int32 W = R.RandRange(1, 2);
			TryBuild(W, 2, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, W, 2, 1, L0, ESDMat::Wood, false); });
		}
		break;
	}
	case ESDPoiType::Industrial:
	{
		for (int32 b = 0; b < 3; ++b)
		{
			const int32 W = R.RandRange(3, 4), D = R.RandRange(3, 4);
			TryBuild(W, D, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, W, D, R.FRand() < 0.5f ? 2 : 1, L0, ESDMat::Metal, true); });
		}
		for (int32 c = 0; c < 10; ++c)
		{
			TryBuild(1, 1, 0, [&](int32 I0, int32 J0)
			{
				SpawnResource(R, (int32)ESDResource::Container, FVector(I0 * T + T * 0.5f, J0 * T + T * 0.5f, FloorZ - 22.f), 1.f);
			});
		}
		break;
	}
	case ESDPoiType::Desert:
	{
		for (int32 b = 0; b < 7; ++b)
		{
			const int32 W = R.RandRange(2, 3), D = 2;
			TryBuild(W, D, 1, [&](int32 I0, int32 J0) { BuildHouse(R, GM, I0, J0, W, D, R.FRand() < 0.4f ? 2 : 1, L0, ESDMat::Stone, true); });
		}
		for (int32 c = 0; c < 6; ++c)
		{
			TryBuild(1, 1, 0, [&](int32 I0, int32 J0) { SpawnResource(R, (int32)ESDResource::Palm, FVector(I0 * T + T * 0.5f, J0 * T + T * 0.5f, FloorZ - 22.f), 1.f); });
		}
		break;
	}
	}

	// cars on free tiles, loot on the ground
	for (int32 c = 0; c < 4; ++c)
	{
		TryBuild(1, 1, 0, [&](int32 I0, int32 J0)
		{
			const FVector P(I0 * T + T * 0.5f, J0 * T + T * 0.5f, FloorZ - 22.f);
			if (ASDResource* Car = World->SpawnActor<ASDResource>(ASDResource::StaticClass(), FTransform(FRotator(0, R.RandRange(0, 3) * 90.f + 8.f, 0), P), SP))
			{
				Car->Setup(ESDResource::Car, 1.f, R.RandHelper(1 << 30));
			}
		});
	}
	for (int32 i = 0; i < 18; ++i)
	{
		const FVector2D P = Poi.Pos + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * Poi.Radius * 0.7f;
		const FIntPoint Tile(FMath::FloorToInt(P.X / T), FMath::FloorToInt(P.Y / T));
		if (Used.Contains(Tile)) continue;
		SDLoot::SpawnFloorLoot(World, FVector(P.X, P.Y, HeightAt(P.X, P.Y) + 40.f));
	}
}

// ====================================================================== buildings

void ASDWorld::BuildHouse(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 Floors, int32 L0, ESDMat Mat, bool bFlatRoof)
{
	const FSDTownStyle Sty = PickStyle(R, Mat, 0);
	const int32 StyTop = L0 + (W < 2 ? 1 : Floors);
	const FVector2D StyCenter((I0 + W * 0.5f) * T, (J0 + D * 0.5f) * T);
	const bool StyInterior = true;
	Foundation(I0, J0, W, D, L0);
	const FIntPoint Stairs(I0, J0 + D - 1);
	if (W < 2) Floors = 1; // stairs need room to land

	auto Place = [&](ESDBuildType Type, int32 I, int32 J, int32 L, int32 Axis, int32 Dir, ESDMat PM) -> ASDBuildPiece*
	{
		FSDBuildSlot S;
		S.Type = Type; S.I = I; S.J = J; S.L = L; S.Axis = Axis; S.Dir = Dir;
		ASDBuildPiece* Pc = GM->TryPlace(S, PM, nullptr, true, true);
		if (Pc) StylePiece(Pc, Sty, L0, StyTop, StyCenter, StyInterior);
		return Pc;
	};

	for (int32 f = 0; f < Floors; ++f)
	{
		const int32 L = L0 + f;
		for (int32 i = I0; i < I0 + W; ++i)
		{
			for (int32 j = J0; j < J0 + D; ++j)
			{
				if (f > 0 && i == Stairs.X && j == Stairs.Y) continue; // stairwell
				Place(ESDBuildType::Floor, i, j, L, 0, 0, f == 0 ? ESDMat::Stone : Mat);
			}
		}
		const int32 DoorI = I0 + R.RandRange(0, W - 1);
		for (int32 i = I0; i < I0 + W; ++i)
		{
			ASDBuildPiece* S = Place(ESDBuildType::Wall, i, J0, L, 1, 0, Mat);
			ASDBuildPiece* N = Place(ESDBuildType::Wall, i, J0 + D, L, 1, 0, Mat);
			if (S) S->SetEdit(f == 0 && i == DoorI ? SDEditMask::Door : R.FRand() < 0.45f ? SDEditMask::Window : 0);
			if (N) N->SetEdit(R.FRand() < 0.45f ? SDEditMask::Window : 0);
		}
		for (int32 j = J0; j < J0 + D; ++j)
		{
			ASDBuildPiece* Wst = Place(ESDBuildType::Wall, I0, j, L, 0, 0, Mat);
			ASDBuildPiece* E = Place(ESDBuildType::Wall, I0 + W, j, L, 0, 0, Mat);
			if (Wst) Wst->SetEdit(R.FRand() < 0.35f ? SDEditMask::Window : 0);
			if (E) E->SetEdit(f == 0 && R.FRand() < 0.3f ? SDEditMask::Door : R.FRand() < 0.4f ? SDEditMask::Window : 0);
		}
	}
	if (Floors > 1)
	{
		Place(ESDBuildType::Ramp, Stairs.X, Stairs.Y, L0, 0, 0, ESDMat::Wood); // rises toward +X onto the upper floor
	}
	const int32 Top = L0 + Floors;
	if (bFlatRoof)
	{
		for (int32 i = I0; i < I0 + W; ++i)
			for (int32 j = J0; j < J0 + D; ++j)
				Place(ESDBuildType::Floor, i, j, Top, 0, 0, Mat);
	}
	else
	{
		for (int32 i = I0; i < I0 + W; ++i)
			for (int32 j = J0; j < J0 + D; ++j)
				Place(ESDBuildType::Floor, i, j, Top, 0, 0, Mat); // ceiling of the top room, hidden under the roof
		GableRoof(I0, J0, W, D, Top, Mat, Place);
	}
	// rooms: living room or kitchen downstairs, bedrooms upstairs
	{
		TArray<FIntPoint> Avoid;
		if (Floors > 1) { Avoid.Add(Stairs); Avoid.Add(Stairs + FIntPoint(1, 0)); Avoid.Add(Stairs - FIntPoint(0, 1)); }
		Avoid.Add(FIntPoint(I0 + W - 1, J0)); // chest corner
		for (int32 f = 0; f < Floors; ++f)
		{
			const int32 Kind = f > 0 ? 1 : (Floors == 1 ? (R.FRand() < 0.5f ? 0 : 1) : (R.FRand() < 0.65f ? 0 : 2));
			FurnishRoom(R, GM, I0, J0, W, D, L0 + f, Kind, Avoid);
		}
	}

	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float FloorZ = L0 * H;
	if (R.FRand() < 0.6f)
	{
		const FVector P((I0 + W - 1) * T + T * 0.5f + (W > 1 ? 120.f : 0.f), J0 * T + T * 0.5f, FloorZ);
		World->SpawnActor<ASDChest>(ASDChest::StaticClass(), FTransform(FRotator(0, 180.f, 0), P), SP);
	}
	SDLoot::SpawnFloorLoot(World, FVector((I0 + W - 1) * T + T * 0.5f, (J0 + D - 1) * T + T * 0.5f, FloorZ + 30.f));
	if (Floors > 1)
	{
		const FVector Up((I0 + W - 1) * T + T * 0.5f, J0 * T + T * 0.5f, FloorZ + H + 30.f);
		if (R.FRand() < 0.5f) World->SpawnActor<ASDChest>(ASDChest::StaticClass(), FTransform(FRotator(0, 90.f, 0), Up - FVector(0, 0, 30)), SP);
		else SDLoot::SpawnFloorLoot(World, Up);
	}
}

void ASDWorld::BuildTower(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 Floors, int32 L0, ESDMat Mat)
{
	const FSDTownStyle Sty = PickStyle(R, Mat, 1);
	const int32 StyTop = L0 + Floors;
	const FVector2D StyCenter((I0 + W * 0.5f) * T, (J0 + D * 0.5f) * T);
	const bool StyInterior = true;
	Foundation(I0, J0, W, D, L0);
	// stair core in tiles (I0, J0+1) and (I0+1, J0+1), switchback between landings on rows J0 and J0+2
	const bool bStairs = W >= 2 && D >= 3;
	auto IsCore = [&](int32 i, int32 j) { return bStairs && j == J0 + 1 && (i == I0 || i == I0 + 1); };
	auto Place = [&](ESDBuildType Type, int32 I, int32 J, int32 L, int32 Axis, int32 Dir, ESDMat PM) -> ASDBuildPiece*
	{
		FSDBuildSlot S;
		S.Type = Type; S.I = I; S.J = J; S.L = L; S.Axis = Axis; S.Dir = Dir;
		ASDBuildPiece* Pc = GM->TryPlace(S, PM, nullptr, true, true);
		if (Pc) StylePiece(Pc, Sty, L0, StyTop, StyCenter, StyInterior);
		return Pc;
	};
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 f = 0; f < Floors; ++f)
	{
		const int32 L = L0 + f;
		for (int32 i = I0; i < I0 + W; ++i)
		{
			for (int32 j = J0; j < J0 + D; ++j)
			{
				if (f > 0 && IsCore(i, j)) continue;
				Place(ESDBuildType::Floor, i, j, L, 0, 0, f == 0 ? ESDMat::Stone : Mat);
			}
		}
		for (int32 i = I0; i < I0 + W; ++i)
		{
			ASDBuildPiece* S = Place(ESDBuildType::Wall, i, J0, L, 1, 0, Mat);
			ASDBuildPiece* N = Place(ESDBuildType::Wall, i, J0 + D, L, 1, 0, Mat);
			if (S) S->SetEdit(f == 0 && i == I0 + W - 1 ? SDEditMask::Door : R.FRand() < 0.75f ? SDEditMask::Window : 0);
			if (N) N->SetEdit(f == 0 && i == I0 + W - 1 ? SDEditMask::Door : R.FRand() < 0.75f ? SDEditMask::Window : 0);
		}
		for (int32 j = J0; j < J0 + D; ++j)
		{
			ASDBuildPiece* Wst = Place(ESDBuildType::Wall, I0, j, L, 0, 0, Mat);
			ASDBuildPiece* E = Place(ESDBuildType::Wall, I0 + W, j, L, 0, 0, Mat);
			if (Wst) Wst->SetEdit(R.FRand() < 0.7f ? SDEditMask::Window : 0);
			if (E) E->SetEdit(R.FRand() < 0.7f ? SDEditMask::Window : 0);
		}
		if (bStairs)
		{
			if (f % 2 == 0) Place(ESDBuildType::Ramp, I0, J0 + 1, L, 0, 1, ESDMat::Wood);
			else Place(ESDBuildType::Ramp, I0 + 1, J0 + 1, L, 0, 3, ESDMat::Wood);
		}
		// loot on each floor
		const FVector Corner((I0 + W - 1) * T + T * 0.5f, (J0 + D - 1) * T + T * 0.5f, L * H);
		if (f % 2 == 1 && R.FRand() < 0.7f) World->SpawnActor<ASDChest>(ASDChest::StaticClass(), FTransform(FRotator(0, 180.f, 0), Corner), SP);
		else SDLoot::SpawnFloorLoot(World, Corner + FVector(0, 0, 30));
	}
	// flat roof; leave the opening above the last ramp
	const int32 Top = L0 + Floors;
	const FIntPoint LastRamp = (Floors - 1) % 2 == 0 ? FIntPoint(I0, J0 + 1) : FIntPoint(I0 + 1, J0 + 1);
	for (int32 i = I0; i < I0 + W; ++i)
	{
		for (int32 j = J0; j < J0 + D; ++j)
		{
			if (bStairs && i == LastRamp.X && j == LastRamp.Y) continue;
			Place(ESDBuildType::Floor, i, j, Top, 0, 0, Mat);
		}
	}
	// low parapet so the roof reads as a rooftop
	for (int32 i = I0; i < I0 + W; ++i)
	{
		if (ASDBuildPiece* P = Place(ESDBuildType::Wall, i, J0, Top, 1, 0, Mat)) P->SetEdit(SDEditMask::HalfWall);
		if (ASDBuildPiece* P = Place(ESDBuildType::Wall, i, J0 + D, Top, 1, 0, Mat)) P->SetEdit(SDEditMask::HalfWall);
	}
	for (int32 j = J0; j < J0 + D; ++j)
	{
		if (ASDBuildPiece* P = Place(ESDBuildType::Wall, I0, j, Top, 0, 0, Mat)) P->SetEdit(SDEditMask::HalfWall);
		if (ASDBuildPiece* P = Place(ESDBuildType::Wall, I0 + W, j, Top, 0, 0, Mat)) P->SetEdit(SDEditMask::HalfWall);
	}
	// offices on every floor, clear of the stair core
	{
		TArray<FIntPoint> Avoid;
		if (bStairs) for (int32 i = I0; i <= I0 + 1; ++i) for (int32 j = J0; j <= J0 + 2; ++j) Avoid.Add(FIntPoint(i, j));
		for (int32 f = 0; f < Floors; ++f) FurnishRoom(R, GM, I0, J0, W, D, L0 + f, 3, Avoid);
	}
}

void ASDWorld::BuildBarn(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 L0)
{
	const FSDTownStyle Sty = PickStyle(R, ESDMat::Wood, 2);
	const int32 StyTop = L0 + 2;
	const FVector2D StyCenter((I0 + 1.5f) * T, (J0 + 2.f) * T);
	const bool StyInterior = false;
	Foundation(I0, J0, 3, 4, L0);
	// tall open barn: two storeys of walls, hay loft on half the floor, peaked roof
	auto Place = [&](ESDBuildType Type, int32 I, int32 J, int32 L, int32 Axis, int32 Dir, ESDMat PM) -> ASDBuildPiece*
	{
		FSDBuildSlot S;
		S.Type = Type; S.I = I; S.J = J; S.L = L; S.Axis = Axis; S.Dir = Dir;
		ASDBuildPiece* Pc = GM->TryPlace(S, PM, nullptr, true, true);
		if (Pc) StylePiece(Pc, Sty, L0, StyTop, StyCenter, StyInterior);
		return Pc;
	};
	const int32 W = 3, D = 4;
	for (int32 f = 0; f < 2; ++f)
	{
		const int32 L = L0 + f;
		for (int32 i = I0; i < I0 + W; ++i)
		{
			for (int32 j = J0; j < J0 + D; ++j)
			{
				if (f == 0 || j >= J0 + 2) Place(ESDBuildType::Floor, i, j, L, 0, 0, ESDMat::Wood);
			}
		}
		for (int32 i = I0; i < I0 + W; ++i)
		{
			if (ASDBuildPiece* S = Place(ESDBuildType::Wall, i, J0, L, 1, 0, ESDMat::Wood)) S->SetEdit(f == 0 && i == I0 + 1 ? SDEditMask::Door : 0);
			if (ASDBuildPiece* N = Place(ESDBuildType::Wall, i, J0 + D, L, 1, 0, ESDMat::Wood)) N->SetEdit(f == 1 && i == I0 + 1 ? SDEditMask::Window : 0);
		}
		for (int32 j = J0; j < J0 + D; ++j)
		{
			Place(ESDBuildType::Wall, I0, j, L, 0, 0, ESDMat::Wood);
			Place(ESDBuildType::Wall, I0 + W, j, L, 0, 0, ESDMat::Wood);
		}
	}
	Place(ESDBuildType::Ramp, I0 + W - 1, J0 + 1, L0, 0, 1, ESDMat::Wood); // up to the loft
	GableRoof(I0, J0, W, D, L0 + 2, ESDMat::Wood, Place);

	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ASDChest>(ASDChest::StaticClass(), FTransform(FRotator(0, 0.f, 0), FVector(I0 * T + T * 0.5f, (J0 + 3) * T + T * 0.5f, (L0 + 1) * H)), SP);
	SDLoot::SpawnFloorLoot(World, FVector(I0 * T + T * 0.5f, J0 * T + T * 0.5f, L0 * H + 30.f));
}

// ====================================================================== town looks

float ASDWorld::TownGround(float X, float Y, FLinearColor& OutColor, float& OutPaved) const
{
	OutPaved = 0.f;
	float Best = 0.f;
	for (const FSDPoi& P : Pois)
	{
		const float D = FVector2D::Distance(P.Pos, FVector2D(X, Y)) / FMath::Max(P.Radius, 1.f);
		if (D > 1.2f) continue;
		// ragged edge so towns fade into the land instead of ending on a circle
		const float Edge = Fbm(X * 0.0007f + 31.f, Y * 0.0007f - 17.f, 2) * 0.2f;
		float Wt = 1.f - SmoothStep01(0.66f, 0.98f, D + Edge);
		if (Wt <= Best) continue;
		const float Var = Fbm(X * 0.002f, Y * 0.002f, 2) * 0.5f + 0.5f;
		FLinearColor C;
		float Paved = 0.f;
		switch (P.Type)
		{
		case ESDPoiType::City:       C = FLinearColor(0.17f, 0.17f, 0.175f) * (0.9f + Var * 0.2f); Paved = 1.f; break;
		case ESDPoiType::Industrial: C = FMath::Lerp(FLinearColor(0.21f, 0.2f, 0.19f), FLinearColor(0.25f, 0.2f, 0.14f), Var); Paved = 1.f; break;
		case ESDPoiType::Desert:     C = FLinearColor(0.6f, 0.43f, 0.24f) * (0.92f + Var * 0.16f); Paved = 0.6f; break;
		case ESDPoiType::Farm:       C = FMath::Lerp(FLinearColor(0.27f, 0.19f, 0.11f), FLinearColor(0.18f, 0.32f, 0.07f), SmoothStep01(0.45f, 0.6f, Var)); Wt *= 0.85f; Paved = Var < 0.5f ? 0.8f : 0.f; break;
		case ESDPoiType::Lodge:      C = FLinearColor(0.2f, 0.15f, 0.09f); Wt *= 0.6f; Paved = 0.f; break;
		default:                     C = FMath::Lerp(FLinearColor(0.14f, 0.36f, 0.05f), FLinearColor(0.2f, 0.44f, 0.08f), Var); Wt *= 0.8f; Paved = 0.f; break; // neat lawns
		}
		Best = Wt;
		OutColor = C;
		OutPaved = Paved * Wt;
	}
	return Best;
}

ASDWorld::FSDTownStyle ASDWorld::PickStyle(FRandomStream& R, ESDMat Mat, int32 Kind) const
{
	auto Pick = [&R](std::initializer_list<FLinearColor> L) { const int32 i = R.RandRange(0, (int32)L.size() - 1); return *(L.begin() + i); };
	const FLinearColor White(0.86f, 0.85f, 0.8f), Cream(0.78f, 0.72f, 0.58f), DarkTrim(0.12f, 0.12f, 0.13f);
	const FLinearColor RoofRed(0.3f, 0.09f, 0.06f), Slate(0.12f, 0.13f, 0.16f), RoofBrown(0.26f, 0.15f, 0.08f), RoofTeal(0.1f, 0.19f, 0.21f);
	FSDTownStyle S;
	const ESDPoiType Town = (ESDPoiType)CurrentPoiType;
	{
		// inside: painted walls, striped wallpaper or damask, over wooden plank floors
		const float Kind0 = R.FRand();
		if (Kind0 < 0.35f) { S.InSurf = ESDSurf::Plaster; S.InC = Pick({ FLinearColor(0.08f, 0.16f, 0.55f), FLinearColor(0.42f, 0.55f, 0.4f), FLinearColor(0.72f, 0.5f, 0.42f), FLinearColor(0.75f, 0.7f, 0.58f), FLinearColor(0.35f, 0.5f, 0.62f) }); }
		else if (Kind0 < 0.7f) { S.InSurf = ESDSurf::Stripes; S.InC = Pick({ FLinearColor(0.1f, 0.32f, 0.6f), FLinearColor(0.12f, 0.42f, 0.45f), FLinearColor(0.55f, 0.18f, 0.2f), FLinearColor(0.62f, 0.56f, 0.4f) }); }
		else { S.InSurf = ESDSurf::Damask; S.InC = Pick({ FLinearColor(0.55f, 0.6f, 0.72f), FLinearColor(0.62f, 0.58f, 0.45f), FLinearColor(0.42f, 0.5f, 0.42f) }); }
		S.FloorSurf = ESDSurf::Wood;
		S.FloorC = Pick({ FLinearColor(0.45f, 0.3f, 0.17f), FLinearColor(0.32f, 0.2f, 0.12f), FLinearColor(0.55f, 0.42f, 0.28f) });
		if (Kind == 1) { S.FloorSurf = ESDSurf::Ground; S.FloorC = FLinearColor(0.55f, 0.55f, 0.53f); S.InSurf = ESDSurf::Plaster; S.InC = Pick({ FLinearColor(0.7f, 0.7f, 0.68f), FLinearColor(0.35f, 0.45f, 0.55f), FLinearColor(0.62f, 0.55f, 0.45f) }); }
	}	if (Kind == 2)
	{
		S.Wall = ESDSurf::Siding; S.WallC = FLinearColor(0.5f, 0.11f, 0.07f); S.Trim = White; S.Roof = ESDSurf::Shingle; S.RoofC = Slate;
		return S;
	}
	if (Town == ESDPoiType::Desert)
	{
		S.Wall = ESDSurf::Plaster; S.WallC = Pick({ FLinearColor(0.78f, 0.62f, 0.42f), FLinearColor(0.72f, 0.45f, 0.28f), FLinearColor(0.82f, 0.74f, 0.6f) });
		S.Trim = White; S.Roof = ESDSurf::Shingle; S.RoofC = FLinearColor(0.45f, 0.2f, 0.1f);
		return S;
	}
	switch (Mat)
	{
	case ESDMat::Wood:
		S.Wall = ESDSurf::Siding;
		S.WallC = Pick({ FLinearColor(0.78f, 0.7f, 0.52f), FLinearColor(0.42f, 0.58f, 0.72f), FLinearColor(0.48f, 0.58f, 0.42f), FLinearColor(0.78f, 0.45f, 0.36f),
			FLinearColor(0.82f, 0.8f, 0.74f), FLinearColor(0.8f, 0.62f, 0.25f), FLinearColor(0.5f, 0.7f, 0.6f), FLinearColor(0.6f, 0.5f, 0.68f) });
		S.Trim = White;
		S.Roof = ESDSurf::Shingle; S.RoofC = Pick({ RoofRed, Slate, RoofBrown, RoofTeal });
		break;
	case ESDMat::Stone:
		if (Kind == 1 && R.FRand() < 0.5f)
		{
			S.Wall = ESDSurf::Plaster; S.WallC = Pick({ Cream, FLinearColor(0.45f, 0.5f, 0.55f), FLinearColor(0.62f, 0.55f, 0.48f) });
		}
		else
		{
			S.Wall = ESDSurf::Brick; S.WallC = Pick({ FLinearColor(0.5f, 0.18f, 0.11f), FLinearColor(0.36f, 0.2f, 0.13f), FLinearColor(0.6f, 0.46f, 0.32f) });
		}
		S.Trim = Cream;
		S.Roof = ESDSurf::Shingle; S.RoofC = Pick({ Slate, RoofRed });
		break;
	default:
		if (Kind == 1)
		{
			S.Wall = ESDSurf::Plaster; S.WallC = Pick({ FLinearColor(0.7f, 0.72f, 0.75f), FLinearColor(0.55f, 0.6f, 0.66f) }); S.Trim = DarkTrim;
		}
		else
		{
			S.Wall = ESDSurf::Metal; S.WallC = Pick({ FLinearColor(0.38f, 0.42f, 0.44f), FLinearColor(0.45f, 0.28f, 0.18f), FLinearColor(0.2f, 0.36f, 0.4f) }); S.Trim = DarkTrim;
		}
		S.Roof = ESDSurf::Asphalt; S.RoofC = FLinearColor(0.1f, 0.1f, 0.1f);
		break;
	}
	return S;
}

void ASDWorld::StylePiece(ASDBuildPiece* P, const FSDTownStyle& S, int32 L0, int32 Top, const FVector2D& Center, bool bInterior) const
{
	switch (P->Slot.Type)
	{
	case ESDBuildType::Wall:
		if (bInterior && P->Slot.L < Top)
		{
			// which face looks into the building: local +X or -X
			const FVector2D ToIn = Center - FVector2D(P->GetActorLocation());
			P->InteriorSide = FVector2D::DotProduct(ToIn, FVector2D(P->GetActorForwardVector())) >= 0.f ? 1 : -1;
			P->InteriorSurf = S.InSurf;
			P->InteriorColor = S.InC;
		}
		P->SetWorldStyle(S.Wall, S.WallC, S.Trim);
		break;
	case ESDBuildType::Cone: P->SetWorldStyle(S.Roof, S.RoofC, S.Trim); break;
	case ESDBuildType::Ramp: P->SetWorldStyle(ESDSurf::Wood, FLinearColor(0.42f, 0.28f, 0.16f), S.Trim); break;
	case ESDBuildType::Floor:
		P->bCeiling = bInterior && P->Slot.L > L0 && P->Slot.L <= Top;
		if (P->Slot.L > Top) P->SetWorldStyle(S.Roof, S.RoofC, S.Trim);                                     // gable ridge cap
		else if (P->Slot.L == Top) P->SetWorldStyle(ESDSurf::Asphalt, FLinearColor(0.11f, 0.11f, 0.11f), S.Trim); // flat roof / attic
		else P->SetWorldStyle(S.FloorSurf, S.FloorC, S.Trim);                                                // room floors
		break;
	}
}
void ASDWorld::Foundation(int32 I0, int32 J0, int32 W, int32 D, int32 L0)
{
	// concrete plinth under the building, deep enough to hide any slope at the town edge
	const float FloorZ = L0 * H, Top = FloorZ - 21.f, Bottom = FloorZ - 420.f;
	Slab(FVector((I0 + W * 0.5f) * T, (J0 + D * 0.5f) * T, (Top + Bottom) * 0.5f), FVector(W * T + 28.f, D * T + 28.f, Top - Bottom),
		FLinearColor(0.44f, 0.43f, 0.41f), true, ESDSurf::Concrete);
}

void ASDWorld::StreetLamp(const FVector& Base, float Yaw)
{
	UMaterialInterface* Pole = SDAssets::Solid(FLinearColor(0.04f, 0.07f, 0.06f), 0.f, 0.45f, 0.6f);
	UMaterialInterface* Glow = SDAssets::Solid(FLinearColor(1.f, 0.85f, 0.55f), 3.f);
	const FRotator Rot(0.f, Yaw, 0.f);
	const FVector Fwd = Rot.Vector();
	UStaticMeshComponent* P = SDAssets::AddMesh(this, Root, SDAssets::Cylinder(), Base + FVector(0, 0, 260.f), FVector(0.1f, 0.1f, 5.2f), FRotator::ZeroRotator, Pole, true);
	P->SetCullDistance(60000.f);
	UStaticMeshComponent* B = SDAssets::AddMesh(this, Root, SDAssets::Cylinder(), Base + FVector(0, 0, 8.f), FVector(0.22f, 0.22f, 0.16f), FRotator::ZeroRotator, Pole, false);
	B->SetCullDistance(60000.f);
	UStaticMeshComponent* Arm = SDAssets::AddBox(this, Root, Base + FVector(0, 0, 515.f) + Fwd * 60.f, FVector(130.f, 8.f, 8.f), Rot, Pole, false);
	Arm->SetCullDistance(60000.f);
	UStaticMeshComponent* Head = SDAssets::AddBox(this, Root, Base + FVector(0, 0, 505.f) + Fwd * 118.f, FVector(46.f, 24.f, 10.f), Rot, Glow, false);
	Head->SetCullDistance(60000.f);
	Head->SetCastShadow(false);
}

void ASDWorld::GableRoof(int32 I0, int32 J0, int32 W, int32 D, int32 Top, ESDMat Mat, TFunctionRef<ASDBuildPiece*(ESDBuildType, int32, int32, int32, int32, int32, ESDMat)> Place)
{
	// gable roof: the ridge runs along the long side, each half is a ramp-shaped cone rising to it,
	// a flat cap covers the middle of wide houses, and triangle walls close the gable ends
	const bool bRidgeAlongY = D >= W;
	const int32 Across = bRidgeAlongY ? W : D;
	constexpr int32 RiseX = 10, FallX = 5, RiseY = 12, FallY = 3;              // cone edits (ramp pyramids)
	constexpr int32 TriHighStart = 416, TriHighEnd = 200;                       // wall triangle edits
	for (int32 i = I0; i < I0 + W; ++i)
	{
		for (int32 j = J0; j < J0 + D; ++j)
		{
			const int32 A = bRidgeAlongY ? i - I0 : j - J0;
			if (Across >= 2 && A > 0 && A < Across - 1)
			{
				Place(ESDBuildType::Floor, i, j, Top + 1, 0, 0, Mat); // flat ridge cap
				continue;
			}
			ASDBuildPiece* Rf = Place(ESDBuildType::Cone, i, j, Top, 0, 0, ESDMat::Stone);
			if (Rf && Across >= 2) Rf->SetEdit(A == 0 ? (bRidgeAlongY ? RiseX : RiseY) : (bRidgeAlongY ? FallX : FallY));
		}
	}
	if (Across >= 2)
	{
		for (int32 A = 0; A < Across; ++A)
		{
			const bool bFirst = A == 0, bLast = A == Across - 1;
			for (const int32 End : { 0, 1 })
			{
				ASDBuildPiece* Gw = bRidgeAlongY
					? Place(ESDBuildType::Wall, I0 + A, End == 0 ? J0 : J0 + D, Top, 1, 0, Mat)   // spans X, local +Y = world -X
					: Place(ESDBuildType::Wall, End == 0 ? I0 : I0 + W, J0 + A, Top, 0, 0, Mat);  // spans Y, local +Y = world +Y
				if (!Gw || (!bFirst && !bLast)) continue;
				if (bRidgeAlongY) Gw->SetEdit(bFirst ? TriHighStart : TriHighEnd);
				else Gw->SetEdit(bFirst ? TriHighEnd : TriHighStart);
			}
		}
	}
}
void ASDWorld::FurnishRoom(FRandomStream& R, ASDGameMode* GM, int32 I0, int32 J0, int32 W, int32 D, int32 L, int32 RoomKind, const TArray<FIntPoint>& Avoid)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float Z = L * H;
	const float Inset = 26.f; // wall half thickness + interior skin + a little air
	auto WallAt = [&](int32 I, int32 J, int32 Axis) -> ASDBuildPiece*
	{
		FSDBuildSlot S; S.Type = ESDBuildType::Wall; S.I = I; S.J = J; S.L = L; S.Axis = Axis;
		const TWeakObjectPtr<ASDBuildPiece>* F = GM->Pieces.Find(ASDBuildPiece::MakeKey(S));
		return F ? F->Get() : nullptr;
	};
	struct FSpot { FVector Pos; FVector In; };
	TArray<FSpot> Spots;
	TArray<FIntPoint> Free;
	for (int32 i = I0; i < I0 + W; ++i)
	{
		for (int32 j = J0; j < J0 + D; ++j)
		{
			if (Avoid.Contains(FIntPoint(i, j))) continue;
			Free.Add(FIntPoint(i, j));
			TArray<FSpot> Sides;
			auto Try = [&](ASDBuildPiece* Wl, const FVector& P, const FVector& In)
			{
				if (Wl && ((Wl->Edit >> 1) & 1)) return; // never block a door
				Sides.Add({ P, In });
			};
			const float Cx = (i + 0.5f) * T, Cy = (j + 0.5f) * T;
			if (i == I0) Try(WallAt(I0, j, 0), FVector(I0 * T + Inset, Cy, Z), FVector(1, 0, 0));
			if (i == I0 + W - 1) Try(WallAt(I0 + W, j, 0), FVector((I0 + W) * T - Inset, Cy, Z), FVector(-1, 0, 0));
			if (j == J0) Try(WallAt(i, J0, 1), FVector(Cx, J0 * T + Inset, Z), FVector(0, 1, 0));
			if (j == J0 + D - 1) Try(WallAt(i, J0 + D, 1), FVector(Cx, (J0 + D) * T - Inset, Z), FVector(0, -1, 0));
			if (Sides.Num()) Spots.Add(Sides[R.RandRange(0, Sides.Num() - 1)]);
		}
	}
	// shuffle
	for (int32 k = Spots.Num() - 1; k > 0; --k) Spots.Swap(k, R.RandRange(0, k));

	static const ESDFurn Living[] = { ESDFurn::Couch, ESDFurn::TVStand, ESDFurn::Bookshelf, ESDFurn::Armchair, ESDFurn::Plant, ESDFurn::FloorLamp, ESDFurn::Dresser, ESDFurn::Crate };
	static const ESDFurn Bedroom[] = { ESDFurn::Bed, ESDFurn::Nightstand, ESDFurn::Desk, ESDFurn::Dresser, ESDFurn::Bookshelf, ESDFurn::Crate, ESDFurn::Plant, ESDFurn::FloorLamp };
	static const ESDFurn Kitchen[] = { ESDFurn::Fridge, ESDFurn::Counter, ESDFurn::Counter, ESDFurn::Plant, ESDFurn::Crate, ESDFurn::Bookshelf };
	static const ESDFurn Office[] = { ESDFurn::Desk, ESDFurn::FilingCabinet, ESDFurn::Desk, ESDFurn::FilingCabinet, ESDFurn::Plant, ESDFurn::Bookshelf, ESDFurn::Crate };
	TArray<ESDFurn> List;
	switch (RoomKind)
	{
	case 0: List.Append(Living, UE_ARRAY_COUNT(Living)); break;
	case 1: List.Append(Bedroom, UE_ARRAY_COUNT(Bedroom)); break;
	case 2: List.Append(Kitchen, UE_ARRAY_COUNT(Kitchen)); break;
	default: List.Append(Office, UE_ARRAY_COUNT(Office)); break;
	}
	const FLinearColor Accent = FLinearColor::MakeFromHSV8((uint8)R.RandRange(0, 255), (uint8)R.RandRange(110, 190), (uint8)R.RandRange(120, 190));
	auto Spawn = [&](ESDFurn K, const FVector& Pos, float Yaw)
	{
		if (ASDFurniture* F = World->SpawnActor<ASDFurniture>(ASDFurniture::StaticClass(), FTransform(FRotator(0, Yaw, 0), Pos), SP))
		{
			F->Build(K, R.RandHelper(1 << 30), Accent);
		}
	};
	int32 Placed = 0;
	for (const ESDFurn K : List)
	{
		if (Placed >= Spots.Num()) break;
		const FSpot& S = Spots[Placed++];
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(S.In.Y, S.In.X));
		const FVector Pos = S.Pos + S.In * (ASDFurniture::Size(K).X * 0.5f);
		Spawn(K, Pos, Yaw);
		if (K == ESDFurn::Desk) Spawn(ESDFurn::OfficeChair, S.Pos + S.In * 110.f, Yaw + 180.f);
		if (K == ESDFurn::Couch) Spawn(ESDFurn::CoffeeTable, S.Pos + S.In * 150.f, Yaw);
	}

	// a rug in the middle of the room (and the kitchen table on it)
	if (Free.Num())
	{
		const FVector2D Mid((I0 + W * 0.5f) * T, (J0 + D * 0.5f) * T);
		FIntPoint Best = Free[0];
		for (const FIntPoint& Tl : Free) if (FVector2D::Distance(FVector2D((Tl.X + 0.5f) * T, (Tl.Y + 0.5f) * T), Mid) < FVector2D::Distance(FVector2D((Best.X + 0.5f) * T, (Best.Y + 0.5f) * T), Mid)) Best = Tl;
		const FVector C((Best.X + 0.5f) * T, (Best.Y + 0.5f) * T, Z);
		if (RoomKind != 3 || R.FRand() < 0.5f)
		{
			const bool bTurn = R.FRand() < 0.5f;
			const FVector Sz = bTurn ? FVector(300, 220, 1) : FVector(220, 300, 1);
			const FLinearColor Border = FLinearColor::MakeFromHSV8((uint8)R.RandRange(0, 255), 160, 120);
			const FLinearColor Field = FLinearColor::MakeFromHSV8((uint8)R.RandRange(0, 255), 120, 90);
			Slab(C + FVector(0, 0, 1.f), Sz, Border, false, ESDSurf::Damask);
			Slab(C + FVector(0, 0, 1.6f), Sz - FVector(40, 40, 0), Field, false, ESDSurf::Damask);
			Slab(C + FVector(0, 0, 2.2f), FVector(70, 70, 1), FLinearColor(0.75f, 0.68f, 0.55f), false, ESDSurf::Damask);
		}
		if (RoomKind == 2) Spawn(ESDFurn::DiningTable, C, R.FRand() < 0.5f ? 0.f : 90.f);
	}

	// pictures on the free walls
	for (int32 k = Placed; k < Spots.Num() && k < Placed + 2; ++k)
	{
		const FSpot& S = Spots[k];
		const FRotator Rot(0, FMath::RadiansToDegrees(FMath::Atan2(S.In.Y, S.In.X)), 0);
		const FVector P = S.Pos - S.In * 9.f + FVector(0, 0, 170.f);
		auto Part = [&](const FVector& Off, const FVector& Sz, const FLinearColor& Col)
		{
			UStaticMeshComponent* C = SDAssets::AddBox(this, Root, P + Rot.RotateVector(Off), Sz, Rot, SDAssets::Solid(Col, 0.f, 0.7f), false);
			C->SetCullDistance(8000.f);
			C->SetCastShadow(false);
		};
		Part(FVector(0, 0, 0), FVector(4, 84, 62), FLinearColor(0.18f, 0.12f, 0.08f));
		Part(FVector(2.5f, 0, 0), FVector(1, 72, 50), FLinearColor(0.85f, 0.83f, 0.78f));
		Part(FVector(3.2f, 0, 8), FVector(1, 60, 18), FLinearColor(0.35f, 0.55f, 0.75f));
		Part(FVector(3.2f, 0, -8), FVector(1, 60, 16), FLinearColor(0.2f, 0.42f, 0.18f));
	}
}