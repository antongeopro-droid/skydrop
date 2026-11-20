#include "SDBuildPiece.h"
#include "SDAssets.h"
#include "SDGameMode.h"
#include "SDWorld.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float T = SDGrid::Tile;
	constexpr float H = SDGrid::Level;
	constexpr float Thick = 22.f;

	float MatMaxHP(ESDMat M)
	{
		switch (M)
		{
		case ESDMat::Stone: return 300.f;
		case ESDMat::Metal: return 500.f;
		default: return 150.f;
		}
	}
	float MatBuildTime(ESDMat M)
	{
		switch (M)
		{
		case ESDMat::Stone: return 7.f;
		case ESDMat::Metal: return 11.f;
		default: return 4.f;
		}
	}
}

ASDBuildPiece::ASDBuildPiece()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

FString ASDBuildPiece::MakeKey(const FSDBuildSlot& S)
{
	return FString::Printf(TEXT("%d_%d_%d_%d_%d"), (int32)S.Type, S.I, S.J, S.L, S.Type == ESDBuildType::Wall ? S.Axis : 0);
}

FTransform ASDBuildPiece::SlotTransform(const FSDBuildSlot& S)
{
	FVector Loc;
	float Yaw = 0.f;
	switch (S.Type)
	{
	case ESDBuildType::Wall:
		if (S.Axis == 0) { Loc = FVector(S.I * T, S.J * T + T * 0.5f, S.L * H); Yaw = 0.f; }
		else             { Loc = FVector(S.I * T + T * 0.5f, S.J * T, S.L * H); Yaw = 90.f; }
		break;
	case ESDBuildType::Ramp:
		Loc = FVector(S.I * T + T * 0.5f, S.J * T + T * 0.5f, S.L * H);
		Yaw = S.Dir * 90.f;
		break;
	default:
		Loc = FVector(S.I * T + T * 0.5f, S.J * T + T * 0.5f, S.L * H);
		break;
	}
	return FTransform(FRotator(0.f, Yaw, 0.f), Loc);
}

void ASDBuildPiece::Setup(const FSDBuildSlot& InSlot, ESDMat InMat, bool bInGhost, bool bInWorld, bool bInstant)
{
	Slot = InSlot;
	Key = MakeKey(Slot);
	Mat = InMat;
	bGhost = bInGhost;
	bWorld = bInWorld;
	MaxHP = MatMaxHP(Mat) * (bWorld ? 1.6f : 1.f);
	BuildTotal = MatBuildTime(Mat);
	if (bInstant || bGhost)
	{
		HP = MaxHP;
		BuildLeft = 0.f;
	}
	else
	{
		HP = MaxHP * 0.1f; // pieces start weak and gain health while building
		BuildLeft = BuildTotal;
	}
	SetActorTransform(SlotTransform(Slot));
	SetActorTickEnabled(!bGhost && !bInstant);
	Rebuild();
}

void ASDBuildPiece::AddPart(const FVector& Center, const FVector& Size, const FRotator& Rot)
{
	UStaticMeshComponent* C = SDAssets::AddBox(this, Root, Center, Size, Rot, nullptr, !bGhost);
	if (bGhost)
	{
		C->SetCastShadow(false);
	}
	else
	{
		C->SetCullDistance(bWorld ? 90000.f : 40000.f);
	}
	Parts.Add(C);
}

void ASDBuildPiece::AddCone()
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(Root);
	const float R = T * 0.5f;
	const float Top = H * 0.5f;
	const FVector A(0, 0, Top);
	const FVector C[4] = { FVector(-R, -R, 0), FVector(R, -R, 0), FVector(R, R, 0), FVector(-R, R, 0) };
	TArray<FVector> V; TArray<int32> Tri; TArray<FVector> N; TArray<FVector2D> UV;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector B0 = C[i], B1 = C[(i + 1) % 4];
		const FVector Nrm = FVector::CrossProduct(B1 - B0, A - B0).GetSafeNormal() * -1.f;
		const int32 Base = V.Num();
		V.Add(B0); V.Add(B1); V.Add(A);
		N.Add(Nrm); N.Add(Nrm); N.Add(Nrm);
		UV.Add(FVector2D(0, 1)); UV.Add(FVector2D(1, 1)); UV.Add(FVector2D(0.5f, 0));
		// both windings so it renders and collides from either side
		Tri.Add(Base); Tri.Add(Base + 2); Tri.Add(Base + 1);
		Tri.Add(Base); Tri.Add(Base + 1); Tri.Add(Base + 2);
	}
	P->bUseComplexAsSimpleCollision = true;
	P->CreateMeshSection(0, V, Tri, N, UV, {}, {}, !bGhost);
	if (bGhost)
	{
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
	}
	else
	{
		P->SetCollisionProfileName(TEXT("BlockAll"));
		P->SetCullDistance(bWorld ? 90000.f : 40000.f);
	}
	P->RegisterComponent();
	AddInstanceComponent(P);
	Parts.Add(P);
}

// ------------------------------------------------------------------ edit shapes (ported from Stormforge)

namespace
{
	inline bool Bit(int32 E, int32 I) { return ((E >> I) & 1) != 0; }

	// Fortnite wall edit chart. Patterns are written top row first, 'x' = removed tile.
	int32 Pat(const TCHAR* S)
	{
		int32 M = 0;
		const FString Str(S);
		TArray<FString> Rows;
		Str.ParseIntoArray(Rows, TEXT("/"));
		for (int32 Rt = 0; Rt < Rows.Num(); ++Rt)
			for (int32 C = 0; C < 3 && C < Rows[Rt].Len(); ++C)
				if (Rows[Rt][C] == TEXT('x')) M |= 1 << ((2 - Rt) * 3 + C);
		return M;
	}
	enum class EWallKind : uint8 { Plain, Triangle, Arch, HalfArch };
	struct FWallEdit { EWallKind Kind; int32 N; };
	const TMap<int32, FWallEdit>& WallChart()
	{
		static TMap<int32, FWallEdit> M;
		if (M.Num()) return M;
		auto Add = [](EWallKind K, std::initializer_list<const TCHAR*> L)
		{
			int32 N = 0;
			for (const TCHAR* S : L) M.Add(Pat(S), { K, N++ });
		};
		Add(EWallKind::Plain, { TEXT(".../.x./..."), TEXT(".../x../..."), TEXT(".../..x/...") });            // windows
		Add(EWallKind::Plain, { TEXT(".../.x./.x."), TEXT(".../x../x.."), TEXT(".../..x/..x") });            // doors
		Add(EWallKind::Plain, { TEXT("xxx/.../...") });                                                       // half wall
		Add(EWallKind::Plain, { TEXT(".../x.x/...") });                                                       // double window
		Add(EWallKind::Plain, { TEXT("xxx/..x/..x"), TEXT("xxx/x../x.."), TEXT("xxx/.x./.x.") });            // half wall door
		Add(EWallKind::Plain, { TEXT("xxx/xxx/...") });                                                       // low wall
		Add(EWallKind::Plain, { TEXT("xxx/xxx/..x"), TEXT("xxx/xxx/x..") });                                  // small low wall
		Add(EWallKind::Plain, { TEXT(".../x.x/..x"), TEXT(".../x.x/x..") });                                  // door + window
		Add(EWallKind::Plain, { TEXT(".xx/.xx/.xx"), TEXT("xx./xx./xx.") });                                  // side wall
		Add(EWallKind::Plain, { TEXT("xxx/.xx/.xx"), TEXT("xxx/xx./xx.") });                                  // small side wall
		Add(EWallKind::Triangle, { TEXT(".xx/..x/..."), TEXT("xx./x../..."), TEXT(".../..x/.xx"), TEXT(".../x../xx.") });
		Add(EWallKind::Arch, { TEXT(".../.x./xxx") });
		Add(EWallKind::HalfArch, { TEXT(".../.xx/.xx"), TEXT(".../xx./xx.") });
		return M;
	}

	// ---- ramps / stairs: 3x3 ring flat on the floor of the cell (4 = hole), t = r*3 + c, c along the rise
	FIntPoint RingXZ(int32 Tl) { return FIntPoint(Tl % 3 - 1, Tl / 3 - 1); }
	int32 RingT(int32 X, int32 Z) { return (Z + 1) * 3 + (X + 1); }
	int32 RotRing(int32 Tl, int32 R)
	{
		FIntPoint P = RingXZ(Tl);
		for (int32 k = 0; k < R; ++k) P = FIntPoint(P.Y, -P.X);
		return RingT(P.X, P.Y);
	}
	// path in a frame where the first step goes +u: returns the stair code, or -1 when not allowed
	int32 StairCanon(const TArray<int32>& P)
	{
		const int32 N = P.Num();
		if (N == 2) return (P[0] == 3 && P[1] == 5) ? 0 : -1;
		if (N < 3) return -1;
		TArray<FIntPoint> XZ;
		for (int32 Tl : P) XZ.Add(RingXZ(Tl));
		for (int32 k = 1; k < N; ++k)
			if (FMath::Abs(XZ[k].X - XZ[k - 1].X) + FMath::Abs(XZ[k].Y - XZ[k - 1].Y) != 1) return -1;
		if (XZ[1].X - XZ[0].X != 1 || XZ[1].Y != XZ[0].Y) return -1;
		if (N == 8) return 0;
		const int32 S = XZ[0].Y;
		if (S == 0 || XZ[0].X != -1 || XZ[2].X != 1 || XZ[2].Y != S) return -1;
		if (N == 3) return S > 0 ? 3 : 12;
		auto Turn = [&](int32 k, int32 X, int32 Z) { return XZ.IsValidIndex(k) && XZ[k].X == X && XZ[k].Y == Z; };
		if (N == 5 && Turn(3, 1, 0) && Turn(4, 1, -S)) return S > 0 ? 1 : 4;
		if (N == 7 && Turn(3, 1, 0) && Turn(4, 1, -S) && Turn(5, 0, -S) && Turn(6, -1, -S)) return S > 0 ? 16 : 32;
		return -1;
	}

	struct FStairQuad { float U0, U1, V0, V1, Y0, Y1; int32 Along; }; // Along: 0 +u, 1 -u, 2 +v, 3 -v, 4 flat
	TArray<FStairQuad> StairQuads(int32 E)
	{
		TArray<FStairQuad> Q;
		const float h = T * 0.5f;
		auto R = [&](float U0, float U1, float V0, float V1, float Y0, float Y1, int32 A) { Q.Add({ U0, U1, V0, V1, Y0, Y1, A }); };
		if (E == 3) R(0, T, h, T, 0, H, 0);
		else if (E == 12) R(0, T, 0, h, 0, H, 0);
		else if (E == 1) { R(0, h, h, T, 0, H / 2, 0); R(h, T, h, T, H / 2, H / 2, 4); R(h, T, 0, h, H / 2, H, 3); }
		else if (E == 4) { R(0, h, 0, h, 0, H / 2, 0); R(h, T, 0, h, H / 2, H / 2, 4); R(h, T, h, T, H / 2, H, 2); }
		else if (E == 16) { R(0, h, h, T, 0, H / 2, 0); R(h, T, 0, T, H / 2, H / 2, 4); R(0, h, 0, h, H / 2, H, 1); }
		else if (E == 32) { R(0, h, 0, h, 0, H / 2, 0); R(h, T, 0, T, H / 2, H / 2, 4); R(0, h, h, T, H / 2, H, 1); }
		else R(0, T, 0, T, 0, H, 0);
		return Q;
	}

	// pyramid heights on the 3x3 vertex grid for each cone edit (removed tiles)
	float RoofHeight(int32 E, int32 Pp, int32 Q)
	{
		TArray<int32> Rem;
		for (int32 i = 0; i < 4; ++i) if (Bit(E, i)) Rem.Add(i);
		auto Corner = [](int32 i) { return FIntPoint((i % 2) * 2, (i >> 1) * 2); };
		const float EH = H;
		if (Rem.Num() == 0) return (Pp == 1 && Q == 1) ? H * 0.5f : 0.f;
		if (Rem.Num() == 1)
		{
			const FIntPoint A = Corner(Rem[0]);
			return EH * (1.f - FMath::Max(FMath::Abs(Pp - A.X), FMath::Abs(Q - A.Y)) / 2.f);
		}
		if (Rem.Num() == 3)
		{
			int32 Keep = 0;
			for (int32 i = 0; i < 4; ++i) if (!Bit(E, i)) Keep = i;
			const FIntPoint K = Corner(Keep);
			return EH * FMath::Max(FMath::Abs(Pp - K.X), FMath::Abs(Q - K.Y)) / 2.f;
		}
		if (Rem.Num() == 2)
		{
			if (E == 3) return EH * (2 - Q) / 2.f;
			if (E == 12) return EH * Q / 2.f;
			if (E == 5) return EH * (2 - Pp) / 2.f;
			if (E == 10) return EH * Pp / 2.f;
			if (E == 6) return EH * FMath::Abs(Pp - Q) / 2.f;
			return EH * FMath::Abs(Pp + Q - 2) / 2.f;
		}
		return 0.f;
	}

	struct FMeshBuf
	{
		TArray<FVector> V; TArray<int32> Tri; TArray<FVector> N; TArray<FVector2D> UV;
		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD)
		{
			FVector Nrm = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
			if (Nrm.IsNearlyZero()) Nrm = FVector::CrossProduct(C - B, A - B).GetSafeNormal();
			const int32 S = V.Num();
			V.Append({ A, B, C, D });
			N.Append({ Nrm, Nrm, Nrm, Nrm });
			UV.Append({ UA, UB, UC, UD });
			// both windings: renders and collides from either side
			Tri.Append({ S, S + 1, S + 2, S, S + 2, S + 3, S, S + 2, S + 1, S, S + 3, S + 2 });
		}
		void Triangle(const FVector& A, const FVector& B, const FVector& C, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC)
		{
			FVector Nrm = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			if (Nrm.Z < 0.f) Nrm = -Nrm; // roof faces always look up at the sky
			const int32 S = V.Num();
			V.Append({ A, B, C });
			N.Append({ Nrm, Nrm, Nrm });
			UV.Append({ UA, UB, UC });
			Tri.Append({ S, S + 1, S + 2, S, S + 2, S + 1 });
		}
	};
}

void ASDBuildPiece::AddMeshPart(UProceduralMeshComponent* P)
{
	P->bUseComplexAsSimpleCollision = true;
	if (bGhost)
	{
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
	}
	else
	{
		P->SetCollisionProfileName(TEXT("BlockAll"));
		P->SetCullDistance(bWorld ? 90000.f : 40000.f);
	}
	P->RegisterComponent();
	AddInstanceComponent(P);
	Parts.Add(P);
}

// wall with a solid band between Bottom(u) and Top(u), u = 0..T across the wall
void ASDBuildPiece::AddProfileWall(TFunctionRef<float(float)> Bottom, TFunctionRef<float(float)> Top)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(Root);
	FMeshBuf M;
	const int32 Steps = 32;
	const float X = Thick * 0.5f;
	auto Y = [](float U) { return U - T * 0.5f; };
	auto UVf = [](float U, float Z) { return FVector2D(U / T, 1.f - Z / H); };
	for (int32 s = 0; s < Steps; ++s)
	{
		const float U0 = T * s / Steps, U1 = T * (s + 1) / Steps;
		const float B0 = Bottom(U0), B1 = Bottom(U1), T0 = Top(U0), T1 = Top(U1);
		if (T0 - B0 < 0.5f && T1 - B1 < 0.5f) continue;
		// faces
		M.Quad(FVector(X, Y(U0), B0), FVector(X, Y(U1), B1), FVector(X, Y(U1), T1), FVector(X, Y(U0), T0), UVf(U0, B0), UVf(U1, B1), UVf(U1, T1), UVf(U0, T0));
		M.Quad(FVector(-X, Y(U1), B1), FVector(-X, Y(U0), B0), FVector(-X, Y(U0), T0), FVector(-X, Y(U1), T1), UVf(U1, B1), UVf(U0, B0), UVf(U0, T0), UVf(U1, T1));
		// top and bottom edges
		M.Quad(FVector(X, Y(U0), T0), FVector(X, Y(U1), T1), FVector(-X, Y(U1), T1), FVector(-X, Y(U0), T0), FVector2D(0, 0), FVector2D(0.05f, 0), FVector2D(0.05f, 0.05f), FVector2D(0, 0.05f));
		if (B0 > 0.5f || B1 > 0.5f)
			M.Quad(FVector(-X, Y(U0), B0), FVector(-X, Y(U1), B1), FVector(X, Y(U1), B1), FVector(X, Y(U0), B0), FVector2D(0, 0), FVector2D(0.05f, 0), FVector2D(0.05f, 0.05f), FVector2D(0, 0.05f));
	}
	// end caps
	for (const float U : { 0.f, T })
	{
		const float B = Bottom(U), Tp = Top(U);
		if (Tp - B < 0.5f) continue;
		M.Quad(FVector(-X, Y(U), B), FVector(X, Y(U), B), FVector(X, Y(U), Tp), FVector(-X, Y(U), Tp), FVector2D(0, 1), FVector2D(0.05f, 1), FVector2D(0.05f, 0), FVector2D(0, 0));
	}
	P->CreateMeshSection(0, M.V, M.Tri, M.N, M.UV, {}, {}, !bGhost);
	AddMeshPart(P);
}

void ASDBuildPiece::AddRoof(int32 E)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(Root);
	FMeshBuf M;
	auto Pt = [E](int32 Pp, int32 Q) { return FVector(Pp * T / 2 - T / 2, Q * T / 2 - T / 2, RoofHeight(E, Pp, Q)); };
	auto UVf = [](const FVector& V) { return FVector2D(V.X / T + 0.5f, V.Y / T + 0.5f); };
	for (int32 a = 0; a < 2; ++a)
		for (int32 b = 0; b < 2; ++b)
		{
			const FVector P00 = Pt(a, b), P10 = Pt(a + 1, b), P01 = Pt(a, b + 1), P11 = Pt(a + 1, b + 1);
			// split along the diagonal through the centre so the peak stays sharp
			if ((a == 0) == (b == 0))
			{
				M.Triangle(P00, P01, P11, UVf(P00), UVf(P01), UVf(P11));
				M.Triangle(P00, P11, P10, UVf(P00), UVf(P11), UVf(P10));
			}
			else
			{
				M.Triangle(P10, P00, P01, UVf(P10), UVf(P00), UVf(P01));
				M.Triangle(P10, P01, P11, UVf(P10), UVf(P01), UVf(P11));
			}
		}
	P->CreateMeshSection(0, M.V, M.Tri, M.N, M.UV, {}, {}, !bGhost);
	AddMeshPart(P);
}

void ASDBuildPiece::AddStairs(int32 E)
{
	for (const FStairQuad& Q : StairQuads(E))
	{
		const float Du = Q.U1 - Q.U0, Dv = Q.V1 - Q.V0, Dz = Q.Y1 - Q.Y0;
		const FVector C((Q.U0 + Q.U1) * 0.5f - T * 0.5f, (Q.V0 + Q.V1) * 0.5f - T * 0.5f, (Q.Y0 + Q.Y1) * 0.5f);
		if (Q.Along == 4)
		{
			AddPart(C - FVector(0, 0, Thick * 0.5f), FVector(Du, Dv, Thick));
			continue;
		}
		const bool bAlongU = Q.Along <= 1;
		const float Run = bAlongU ? Du : Dv, Across = bAlongU ? Dv : Du;
		const float Len = FMath::Sqrt(Run * Run + Dz * Dz);
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Dz, Run));
		const float Yaw = Q.Along == 0 ? 0.f : Q.Along == 1 ? 180.f : Q.Along == 2 ? 90.f : -90.f;
		AddPart(C, FVector(Len, Across, Thick), FRotator(Pitch, Yaw, 0.f));
	}
}

// ------------------------------------------------------------------ tile editing

int32 ASDBuildPiece::NumTiles() const
{
	switch (Slot.Type)
	{
	case ESDBuildType::Wall: return 9;
	case ESDBuildType::Ramp: return 9;
	default: return 4;
	}
}

void ASDBuildPiece::GetEditTiles(const FVector& ViewPos, TArray<FSDEditTile>& Out) const
{
	Out.Reset();
	const FTransform& Xf = GetActorTransform();
	const FVector LocalView = Xf.InverseTransformPosition(ViewPos);
	auto Add = [&](int32 Idx, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		FSDEditTile Tl; Tl.Idx = Idx;
		Tl.C[0] = Xf.TransformPosition(A); Tl.C[1] = Xf.TransformPosition(B); Tl.C[2] = Xf.TransformPosition(C); Tl.C[3] = Xf.TransformPosition(D);
		Out.Add(Tl);
	};
	switch (Slot.Type)
	{
	case ESDBuildType::Wall:
	{
		const float X = (LocalView.X >= 0.f ? 1.f : -1.f) * (Thick * 0.5f + 3.f);
		for (int32 r = 0; r < 3; ++r)
			for (int32 c = 0; c < 3; ++c)
			{
				const float Y0 = c * T / 3 - T / 2, Y1 = (c + 1) * T / 3 - T / 2, Z0 = r * H / 3, Z1 = (r + 1) * H / 3;
				Add(r * 3 + c, FVector(X, Y0, Z0), FVector(X, Y1, Z0), FVector(X, Y1, Z1), FVector(X, Y0, Z1));
			}
		break;
	}
	case ESDBuildType::Ramp:
	{
		const float Z = LocalView.Z < 0.f ? -3.f : 3.f;
		for (int32 Tl : { 0, 1, 2, 3, 5, 6, 7, 8 })
		{
			const int32 c = Tl % 3, r = Tl / 3;
			const float X0 = c * T / 3 - T / 2, X1 = (c + 1) * T / 3 - T / 2, Y0 = r * T / 3 - T / 2, Y1 = (r + 1) * T / 3 - T / 2;
			Add(Tl, FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z));
		}
		break;
	}
	default:
	{
		const float Base = Slot.Type == ESDBuildType::Floor ? -Thick : 0.f;
		const float Z = LocalView.Z < Base ? Base - 3.f : 3.f;
		for (int32 b = 0; b < 2; ++b)
			for (int32 a = 0; a < 2; ++a)
			{
				const float X0 = a * T / 2 - T / 2, X1 = (a + 1) * T / 2 - T / 2, Y0 = b * T / 2 - T / 2, Y1 = (b + 1) * T / 2 - T / 2;
				Add(b * 2 + a, FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z));
			}
		break;
	}
	}
}

TArray<int32> ASDBuildPiece::CleanStairPath(const TArray<int32>& Path)
{
	TArray<int32> P;
	for (int32 Tl : Path)
	{
		if (Tl == 4 || P.Contains(Tl)) continue;
		if (P.Num())
		{
			const FIntPoint A = RingXZ(P.Last()), B = RingXZ(Tl);
			if ((A.X == B.X && FMath::Abs(A.Y - B.Y) == 2) || (A.Y == B.Y && FMath::Abs(A.X - B.X) == 2))
			{
				const int32 Mid = RingT((A.X + B.X) / 2, (A.Y + B.Y) / 2);
				if (Mid != 4 && !P.Contains(Mid)) P.Add(Mid);
			}
		}
		P.Add(Tl);
	}
	return P;
}

bool ASDBuildPiece::StairFromPath(const TArray<int32>& Path, int32& OutEdit, int32& OutRot)
{
	const TArray<int32> P = CleanStairPath(Path);
	for (int32 R = 0; R < 4; ++R)
	{
		TArray<int32> Rp;
		for (int32 Tl : P) Rp.Add(RotRing(Tl, R));
		const int32 E = StairCanon(Rp);
		if (E >= 0) { OutEdit = E; OutRot = R; return true; }
	}
	return false;
}

bool ASDBuildPiece::IsValidEdit(int32 Sel, const TArray<int32>& Path) const
{
	if (Sel == 0) return true;
	switch (Slot.Type)
	{
	case ESDBuildType::Wall: return WallChart().Contains(Sel);
	case ESDBuildType::Floor:
	{
		TArray<int32> R;
		for (int32 i = 0; i < 4; ++i) if (Bit(Sel, i)) R.Add(i);
		if (R.Num() == 1 || R.Num() == 3) return true;
		if (R.Num() == 2) return FMath::Abs(R[0] % 2 - R[1] % 2) + FMath::Abs((R[0] >> 1) - (R[1] >> 1)) == 1;
		return false;
	}
	case ESDBuildType::Cone:
	{
		int32 N = 0;
		for (int32 i = 0; i < 4; ++i) N += Bit(Sel, i) ? 1 : 0;
		return N >= 1 && N <= 3;
	}
	case ESDBuildType::Ramp:
	{
		int32 E, R;
		return StairFromPath(Path, E, R);
	}
	}
	return false;
}

bool ASDBuildPiece::ApplyEdit(int32 Sel, const TArray<int32>& Path)
{
	if (bWorld || bGhost) return false;
	if (Slot.Type == ESDBuildType::Ramp)
	{
		if (Sel == 0) return false;
		int32 E, R;
		if (!StairFromPath(Path, E, R)) return false;
		if (R == 0 && E == Edit) return false;
		Slot.Dir = (Slot.Dir + R) % 4;
		Edit = E;
		Rebuild();
		return true;
	}
	if (Sel == Edit || !IsValidEdit(Sel, Path)) return false;
	SetEdit(Sel);
	return true;
}

void ASDBuildPiece::Rebuild()
{
	for (UPrimitiveComponent* P : Parts)
	{
		if (P) P->DestroyComponent();
	}
	Parts.Reset();
	for (UPrimitiveComponent* P : DecoParts) if (P) P->DestroyComponent();
	DecoParts.Reset();
	SetActorTransform(SlotTransform(Slot));

	switch (Slot.Type)
	{
	case ESDBuildType::Wall:
	{
		const FWallEdit* WE = WallChart().Find(Edit);
		if (WE && WE->Kind == EWallKind::Triangle)
		{
			const int32 N = WE->N;
			AddProfileWall(
				[N](float U) { return N == 2 ? H * U / T : N == 3 ? H * (1.f - U / T) : 0.f; },
				[N](float U) { return N == 0 ? H * (1.f - U / T) : N == 1 ? H * U / T : H; });
		}
		else if (WE && WE->Kind == EWallKind::Arch)
		{
			const float LegA = T * 0.14f, Hs = H * 0.36f, H3 = H * 2.f / 3.f, Rx = T * 0.5f - LegA, Ry = H3 - Hs;
			AddProfileWall(
				[=](float U) { if (U <= LegA || U >= T - LegA) return 0.f; const float K = (U - T * 0.5f) / Rx; return Hs + Ry * FMath::Sqrt(FMath::Max(0.f, 1.f - K * K)); },
				[](float) { return H; });
		}
		else if (WE && WE->Kind == EWallKind::HalfArch)
		{
			const bool bMirror = WE->N == 1;
			const float LegH = T * 0.2f, Hs = H * 0.3f, H3 = H * 2.f / 3.f, Rx = T - LegH, Ry = H3 - Hs;
			AddProfileWall(
				[=](float U0) { const float U = bMirror ? T - U0 : U0; if (U <= LegH) return 0.f; const float K = (U - T) / Rx; return Hs + Ry * FMath::Sqrt(FMath::Max(0.f, 1.f - K * K)); },
				[](float) { return H; });
		}
		else
		{
			// kept tiles as row runs, then runs with the same span merged vertically
			struct FRun { int32 C0, C1, R0, R1; };
			TArray<FRun> Runs;
			for (int32 r = 0; r < 3; ++r)
			{
				int32 c = 0;
				while (c < 3)
				{
					if (Bit(Edit, r * 3 + c)) { ++c; continue; }
					int32 c1 = c;
					while (c1 + 1 < 3 && !Bit(Edit, r * 3 + c1 + 1)) ++c1;
					bool bMerged = false;
					for (FRun& Rn : Runs) if (Rn.C0 == c && Rn.C1 == c1 && Rn.R1 == r) { Rn.R1 = r + 1; bMerged = true; break; }
					if (!bMerged) Runs.Add({ c, c1, r, r + 1 });
					c = c1 + 1;
				}
			}
			for (const FRun& Rn : Runs)
			{
				const float Y0 = Rn.C0 * T / 3 - T / 2, Y1 = (Rn.C1 + 1) * T / 3 - T / 2, Z0 = Rn.R0 * H / 3, Z1 = Rn.R1 * H / 3;
				AddPart(FVector(0, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f), FVector(Thick, Y1 - Y0, Z1 - Z0));
			}
		}
		break;
	}
	case ESDBuildType::Floor:
		for (int32 b = 0; b < 2; ++b)
			for (int32 a = 0; a < 2; ++a)
			{
				if (Bit(Edit, b * 2 + a)) continue;
				// whole floor stays one slab when nothing is cut
				if (Edit == 0) { AddPart(FVector(0, 0, -Thick * 0.5f), FVector(T, T, Thick)); a = b = 2; break; }
				AddPart(FVector((a + 0.5f) * T / 2 - T / 2, (b + 0.5f) * T / 2 - T / 2, -Thick * 0.5f), FVector(T / 2, T / 2, Thick));
			}
		break;
	case ESDBuildType::Ramp:
		if (Edit == 0)
		{
			const float Len = FMath::Sqrt(T * T + H * H);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(H, T));
			AddPart(FVector(0, 0, H * 0.5f), FVector(Len, T, Thick), FRotator(Pitch, 0, 0));
		}
		else AddStairs(Edit);
		break;
	case ESDBuildType::Cone:
		AddRoof(Edit);
		break;
	}
	if (bWorld && bStyled && Slot.Type == ESDBuildType::Wall && !bGhost) AddWallDecor();
	if (bWorld && bCeiling && Slot.Type == ESDBuildType::Floor && !bGhost)
	{
		UStaticMeshComponent* Ceil = SDAssets::AddBox(this, Root, FVector(0, 0, -Thick - 1.5f), FVector(T, T, 3.f), FRotator::ZeroRotator,
			SDAssets::Surface(ESDSurf::Plaster, FLinearColor(0.82f, 0.81f, 0.78f), 0.f, 0.9f), false);
		Ceil->SetCullDistance(30000.f);
		Ceil->SetCastShadow(false);
		DecoParts.Add(Ceil);
	}
	LastVisualStep = -1;
	ApplyMaterial();
}

void ASDBuildPiece::ApplyMaterial()
{
	UMaterialInterface* M = nullptr;
	if (bGhost)
	{
		M = SDAssets::Ghost(bGhostValid ? FLinearColor(0.25f, 0.6f, 1.f) : FLinearColor(1.f, 0.2f, 0.2f));
	}
	else
	{
		const float Built = BuildTotal > 0 ? 1.f - BuildLeft / BuildTotal : 1.f;
		const int32 Step = FMath::Clamp(FMath::FloorToInt(Built * 4.f), 0, 4);
		const bool bFlash = HitFlash > 0.f;
		const int32 VisualKey = Step + (bFlash ? 10 : 0);
		if (VisualKey == LastVisualStep) return;
		LastVisualStep = VisualKey;
		const FLinearColor Base = bStyled ? StyleColor : SDData::MatColor(Mat);
		const FLinearColor Blue(0.25f, 0.55f, 1.f);
		const float A = Step / 4.f;
		FLinearColor C = Blue * (1.f - A) + Base * A;
		float Em = (1.f - A) * 0.6f;
		if (bFlash) { C = C * 1.5f; Em += 0.35f; }
		const float Rough = Mat == ESDMat::Metal ? 0.35f : 0.85f;
		const float Metal = Mat == ESDMat::Metal ? 0.7f : 0.f;
		const ESDSurf Surf = bStyled ? StyleSurf : Mat == ESDMat::Wood ? ESDSurf::Wood : Mat == ESDMat::Stone ? ESDSurf::Brick : ESDSurf::Metal;
		M = SDAssets::Surface(Surf, C, Em, Rough, Metal);
	}
	for (UPrimitiveComponent* P : Parts)
	{
		if (P) P->SetMaterial(0, M);
	}
}

void ASDBuildPiece::SetGhostValid(bool bValid)
{
	if (bValid == bGhostValid) return;
	bGhostValid = bValid;
	ApplyMaterial();
}

bool ASDBuildPiece::CycleEdit()
{
	if (bWorld) return false;
	if (Slot.Type == ESDBuildType::Wall)
	{
		static const int32 Presets[4] = { 0, SDEditMask::Door, SDEditMask::Window, SDEditMask::HalfWall };
		int32 Cur = 0;
		for (int32 i = 0; i < 4; ++i) if (Presets[i] == Edit) Cur = i;
		SetEdit(Presets[(Cur + 1) % 4]);
		return true;
	}
	if (Slot.Type == ESDBuildType::Ramp)
	{
		Slot.Dir = (Slot.Dir + 1) % 4;
		Rebuild();
		return true;
	}
	return false;
}

void ASDBuildPiece::SetEdit(int32 NewEdit)
{
	Edit = NewEdit;
	Rebuild();
}

void ASDBuildPiece::Tick(float Dt)
{
	Super::Tick(Dt);
	bool bChanged = false;
	if (BuildLeft > 0.f)
	{
		const float Step = FMath::Min(BuildLeft, Dt);
		BuildLeft -= Step;
		HP = FMath::Min(MaxHP, HP + MaxHP * 0.9f * Step / BuildTotal);
		bChanged = true;
	}
	if (HitFlash > 0.f)
	{
		HitFlash -= Dt;
		bChanged = true;
	}
	if (bChanged) ApplyMaterial();
	if (BuildLeft <= 0.f && HitFlash <= 0.f) SetActorTickEnabled(false);
}

float ASDBuildPiece::TakeDamage(float Amount, FDamageEvent const& Ev, AController* Inst, AActor* Causer)
{
	if (bGhost || bCollapsing) return 0.f;
	HP -= Amount;
	HitFlash = 0.08f;
	LastVisualStep = -1;
	SetActorTickEnabled(true);
	ApplyMaterial();
	if (HP <= 0.f)
	{
		Collapse();
	}
	return Amount;
}

void ASDBuildPiece::Collapse()
{
	if (bCollapsing) return;
	bCollapsing = true;
	Destroy();
}

bool ASDBuildPiece::IsGrounded() const
{
	if (bWorld) return true;
	const ASDGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASDGameMode>() : nullptr;
	if (!GM || !GM->Island) return true;
	const FVector C = GetActorLocation();
	float Bottom = C.Z;
	TArray<FVector> Probe;
	switch (Slot.Type)
	{
	case ESDBuildType::Wall:
	{
		const FVector Side = GetActorRightVector() * (T * 0.5f - 20.f);
		Probe = { C, C + Side, C - Side };
		break;
	}
	case ESDBuildType::Ramp:
	{
		const FVector Back = -GetActorForwardVector() * (T * 0.5f - 20.f);
		Probe = { C + Back, C };
		break;
	}
	default:
	{
		const float R = T * 0.5f - 20.f;
		Probe = { C, C + FVector(R, R, 0), C + FVector(-R, R, 0), C + FVector(R, -R, 0), C + FVector(-R, -R, 0) };
		break;
	}
	}
	for (const FVector& P : Probe)
	{
		const float G = FMath::Max(GM->Island->HeightAt(P.X, P.Y), 0.f);
		if (Bottom <= G + 40.f) return true;
	}
	return false;
}

void ASDBuildPiece::EndPlay(const EEndPlayReason::Type Reason)
{
	if (!bGhost && Reason == EEndPlayReason::Destroyed)
	{
		if (ASDGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASDGameMode>() : nullptr)
		{
			GM->OnPieceRemoved(this);
		}
	}
	Super::EndPlay(Reason);
}

// ------------------------------------------------------------------ town style

void ASDBuildPiece::SetWorldStyle(ESDSurf Surf, const FLinearColor& Color, const FLinearColor& Trim)
{
	bStyled = true;
	StyleSurf = Surf;
	StyleColor = Color;
	TrimColor = Trim;
	LastVisualStep = -1;
	if (Slot.Type == ESDBuildType::Wall || bCeiling) Rebuild(); // trim, windows, interior skins and ceilings depend on the style
	else ApplyMaterial();
}

void ASDBuildPiece::AddWallDecor()
{
	// frames and glass only make sense on the plain tile shapes; gable triangles just get their colour
	if (const FWallEdit* WE = WallChart().Find(Edit)) if (WE->Kind != EWallKind::Plain) return;
	UMaterialInterface* TrimM = SDAssets::Surface(ESDSurf::Plaster, TrimColor, 0.f, 0.6f);
	UMaterialInterface* GlassM = SDAssets::Ghost(FLinearColor(0.55f, 0.75f, 0.9f));
	const float TW = T / 3.f, TH = H / 3.f;
	auto Kept = [this](int32 r, int32 c) { return r >= 0 && r < 3 && c >= 0 && c < 3 && !((Edit >> (r * 3 + c)) & 1); };
	auto Deco = [&](const FVector& C, const FVector& S, UMaterialInterface* M, bool bShadow = true)
	{
		UStaticMeshComponent* P = SDAssets::AddBox(this, Root, C, S, FRotator::ZeroRotator, M, false);
		P->SetCastShadow(bShadow);
		P->SetCullDistance(60000.f);
		DecoParts.Add(P);
	};
	const float X = Thick + 10.f;
	// exterior-only trim (corner boards) sits on the outside face when we know which side the room is on
	const float OutX = InteriorSide == 0 ? 0.f : -InteriorSide * (Thick * 0.5f + 4.f);
	const float OutW = InteriorSide == 0 ? X : 8.f;
	if (InteriorSide != 0)
	{
		UMaterialInterface* Skin = SDAssets::Surface(InteriorSurf, InteriorColor, 0.f, 0.85f);
		UMaterialInterface* Base = SDAssets::Surface(ESDSurf::Wood, FLinearColor(0.22f, 0.13f, 0.07f), 0.f, 0.6f);
		UMaterialInterface* Crown = SDAssets::Surface(ESDSurf::Plaster, FLinearColor(0.85f, 0.84f, 0.8f), 0.f, 0.6f);
		const float In = InteriorSide * (Thick * 0.5f);
		for (int32 r = 0; r < 3; ++r)
			for (int32 c = 0; c < 3; ++c)
			{
				if (!Kept(r, c)) continue;
				const float Yc = c * TW - T / 2 + TW * 0.5f;
				Deco(FVector(In + InteriorSide * 1.5f, Yc, r * TH + TH * 0.5f), FVector(3.f, TW, TH), Skin);
				if (r == 0) Deco(FVector(In + InteriorSide * 4.f, Yc, 9.f), FVector(6.f, TW, 18.f), Base);
				if (r == 2) Deco(FVector(In + InteriorSide * 5.f, Yc, H - 6.f), FVector(8.f, TW, 12.f), Crown);
			}
	}
	for (int32 r = 0; r < 3; ++r)
	{
		for (int32 c = 0; c < 3; ++c)
		{
			const float Y0 = c * TW - T / 2, Z0 = r * TH;
			const float Yc = Y0 + TW * 0.5f, Zc = Z0 + TH * 0.5f;
			if (Kept(r, c))
			{
				// cap on top of the wall (cornice), and corner boards at the wall ends
				if (!Kept(r + 1, c)) Deco(FVector(0, Yc, Z0 + TH - 5.f), FVector(X + 8.f, TW + (c == 0 || c == 2 ? 10.f : 0.f), 12.f), TrimM);
				if (c == 0) Deco(FVector(OutX, -T / 2 + 5.f, Zc), FVector(OutW, 12.f, TH), TrimM);
				if (c == 2) Deco(FVector(OutX, T / 2 - 5.f, Zc), FVector(OutW, 12.f, TH), TrimM);
				continue;
			}
			// opening: frame every side that meets the wall, a sill and glass under windows
			const bool bDoor = r == 0 || !Kept(r - 1, c) && r == 1 && !Kept(0, c);
			if (Kept(r + 1, c)) Deco(FVector(0, Yc, Z0 + TH - 5.f), FVector(X, TW, 10.f), TrimM);
			if (Kept(r, c - 1)) Deco(FVector(0, Y0 + 5.f, Zc), FVector(X, 10.f, TH), TrimM);
			if (Kept(r, c + 1)) Deco(FVector(0, Y0 + TW - 5.f, Zc), FVector(X, 10.f, TH), TrimM);
			if (r > 0 && Kept(r - 1, c) && Kept(r + 1, c)) // a real window: wall below and above it
			{
				Deco(FVector(0, Yc, Z0 + 4.f), FVector(X + 18.f, TW + 14.f, 8.f), TrimM); // sill
				if (!bDoor) Deco(FVector(0, Yc, Zc), FVector(3.f, TW - 14.f, TH - 14.f), GlassM, false);
			}
		}
	}
}
void ASDBuildPiece::SetInterior(int32 Side, ESDSurf Surf, const FLinearColor& Color, bool bCeilingBelow)
{
	InteriorSide = Side;
	InteriorSurf = Surf;
	InteriorColor = Color;
	bCeiling = bCeilingBelow;
	Rebuild();
}