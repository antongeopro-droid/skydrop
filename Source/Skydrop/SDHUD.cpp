#include "SDHUD.h"
#include "SDCharacter.h"
#include "SDGameMode.h"
#include "SDPlayerController.h"
#include "SDWorld.h"
#include "SDMatchActors.h"
#include "SDBuildPiece.h"
#include "SDLobby.h"
#include "SDSave.h"
#include "SDCosmetics.h"
#include "SDSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	FString Clock(float S)
	{
		const int32 T = FMath::Max(0, FMath::CeilToInt(S));
		return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60);
	}
	const FLinearColor White(1, 1, 1, 1);
	const FLinearColor Dim(0, 0, 0, 0.45f);
}

void ASDHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale, bool bCenter)
{
	const bool bSmall = F == GEngine->GetSmallFont() || F == GEngine->GetTinyFont();
	const float Pt = (F && F->LegacyFontSize > 0 ? F->LegacyFontSize : 12.f) * Scale;
	Txt(S, X, Y, Pt, bSmall ? TEXT("Medium") : TEXT("Bold"), C, bCenter ? 1 : 0, 2.f);
}

void ASDHUD::Box(float X, float Y, float W, float H, const FLinearColor& C)
{
	DrawRect(C, X, Y, W, H);
}

void ASDHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float T)
{
	DrawLine(X, Y, X + W, Y, C, T);
	DrawLine(X + W, Y, X + W, Y + H, C, T);
	DrawLine(X + W, Y + H, X, Y + H, C, T);
	DrawLine(X, Y + H, X, Y, C, T);
}

void ASDHUD::Circle(const FVector2D& C, float R, const FLinearColor& Col, float T)
{
	const int32 N = 64;
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = i * 2.f * PI / N, A1 = (i + 1) * 2.f * PI / N;
		DrawLine(C.X + FMath::Cos(A0) * R, C.Y + FMath::Sin(A0) * R, C.X + FMath::Cos(A1) * R, C.Y + FMath::Sin(A1) * R, Col, T);
	}
}

void ASDHUD::DrawMap(ASDGameMode* GM, ASDCharacter* C, float X, float Y, float Size, float View, bool bLabels, const FVector2D* CenterIn, float HalfIn)
{
	ASDWorld* Isl = GM->Island;
	if (!Isl || !Isl->MapTexture) return;
	const float Ext = Isl->MapExtent();
	FVector2D Ctr(0.f, 0.f);
	float Half = Ext;
	if (View > 0.f && C) { Ctr = FVector2D(C->GetActorLocation()); Half = View; }
	if (CenterIn) { Ctr = *CenterIn; Half = FMath::Max(HalfIn, 100.f); }
	auto ToMap = [&](const FVector2D& P)
	{
		return FVector2D(X + (P.Y - (Ctr.Y - Half)) / (2.f * Half) * Size, Y + ((Ctr.X + Half) - P.X) / (2.f * Half) * Size);
	};
	auto Inside = [&](const FVector2D& P) { return P.X >= X && P.X <= X + Size && P.Y >= Y && P.Y <= Y + Size; };
	auto Line = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Col, float Th)
	{
		if (Inside(A) && Inside(B)) DrawLine(A.X, A.Y, B.X, B.Y, Col, Th);
	};
	auto Ring = [&](const FVector2D& WC, float WR, const FLinearColor& Col, float Th)
	{
		const int32 N = 96;
		for (int32 i = 0; i < N; ++i)
		{
			const float A0 = i * 2.f * PI / N, A1 = (i + 1) * 2.f * PI / N;
			Line(ToMap(WC + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * WR), ToMap(WC + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * WR), Col, Th);
		}
	};

	Box(X - 4, Y - 4, Size + 8, Size + 8, FLinearColor(0, 0, 0, 0.6f));
	const float U0 = (Ctr.Y - Half + Ext) / (2.f * Ext);
	const float V0 = (Ext - (Ctr.X + Half)) / (2.f * Ext);
	const float UL = Half / Ext;
	DrawTexture(Isl->MapTexture, X, Y, Size, Size, U0, V0, UL, UL, FLinearColor::White, BLEND_Opaque);
	if (bLabels)
	{
		// 10x10 grid with A-J / 1-10 like the OG map
		const float Cell = 2.f * Ext / 10.f;
		for (int32 i = 0; i <= 10; ++i)
		{
			const float WY = -Ext + i * Cell, WX = Ext - i * Cell;
			const FVector2D A = ToMap(FVector2D(Ext, WY)), B = ToMap(FVector2D(-Ext, WY));
			if (A.X >= X && A.X <= X + Size) DrawLine(A.X, FMath::Max(A.Y, Y), B.X, FMath::Min(B.Y, Y + Size), FLinearColor(1, 1, 1, 0.18f), 1.f);
			const FVector2D Cc = ToMap(FVector2D(WX, -Ext)), D = ToMap(FVector2D(WX, Ext));
			if (Cc.Y >= Y && Cc.Y <= Y + Size) DrawLine(FMath::Max(Cc.X, X), Cc.Y, FMath::Min(D.X, X + Size), D.Y, FLinearColor(1, 1, 1, 0.18f), 1.f);
			if (i < 10)
			{
				const FVector2D Lc = ToMap(FVector2D(Ext, WY + Cell * 0.5f));
				if (Lc.X > X + 8 && Lc.X < X + Size - 8) Txt(FString::Chr(TEXT('A') + i), Lc.X, Y + 4 * U, 14.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.75f), 1);
				const FVector2D Ln = ToMap(FVector2D(WX - Cell * 0.5f, -Ext));
				if (Ln.Y > Y + 8 && Ln.Y < Y + Size - 12) Txt(FString::FromInt(i + 1), X + 6 * U, Ln.Y - 9 * U, 14.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.75f));
			}
		}
	}

	if (bLabels)
	{
		UFont* F = GEngine->GetSmallFont();
		for (const FSDPoi& P : Isl->Pois)
		{
			const FVector2D M = ToMap(P.Pos);
			if (Inside(M)) Text(P.Name, M.X, M.Y - 8 * U, White, F, 1.15f, true);
		}
	}
	if (ASDStorm* S = GM->Storm)
	{
		Ring(S->Center, S->Radius, FLinearColor(0.7f, 0.25f, 1.f, 1.f), 2.5f);
		if (S->bActive) Ring(S->NextCenter, S->NextRadius, White, 1.5f);
	}
	if (GM->Bus && GM->Phase == ESDPhase::Bus)
	{
		const FVector2D A = ToMap(FVector2D(GM->Bus->StartPos)), B = ToMap(FVector2D(GM->Bus->EndPos));
		if (View <= 0.f)
		{
			// the airship route, in short pieces so it stays inside a zoomed map
			for (int32 k = 0; k < 64; ++k) Line(FMath::Lerp(A, B, k / 64.f), FMath::Lerp(A, B, (k + 1) / 64.f), FLinearColor(1.f, 0.6f, 0.1f), 2.f);
		}
		const FVector2D BP = ToMap(FVector2D(GM->Bus->GetActorLocation()));
		if (Inside(BP)) Box(BP.X - 4, BP.Y - 4, 8, 8, FLinearColor(1.f, 0.6f, 0.1f));
	}
	if (const ASDPlayerController* MPC = Cast<ASDPlayerController>(GetOwningPlayerController()))
	{
		if (MPC->bHasMarker)
		{
			FVector2D Mk = ToMap(MPC->MarkerPos);
			const float Pin = (bLabels ? 16.f : 11.f) * U;
			if (!Inside(Mk))
			{
				// off the minimap: pin it to the edge in its direction
				const FVector2D Cm(X + Size * 0.5f, Y + Size * 0.5f);
				const FVector2D D = Mk - Cm;
				const float K = (Size * 0.5f - Pin) / FMath::Max(FMath::Abs(D.X), FMath::Abs(D.Y));
				Mk = Cm + D * FMath::Min(K, 1.f);
			}
			MarkerPin(Mk.X, Mk.Y, Pin);
		}
	}
	if (C)
	{
		const FVector2D M = ToMap(FVector2D(C->GetActorLocation()));
		const float Yaw = FMath::DegreesToRadians(C->GetControlRotation().Yaw);
		const FVector2D Dir(FMath::Sin(Yaw), -FMath::Cos(Yaw));
		DrawLine(M.X, M.Y, M.X + Dir.X * 14 * U, M.Y + Dir.Y * 14 * U, White, 2.f);
		Box(M.X - 4 * U, M.Y - 4 * U, 8 * U, 8 * U, FLinearColor(1.f, 0.85f, 0.1f));
	}
	Frame(X - 4, Y - 4, Size + 8, Size + 8, FLinearColor(1, 1, 1, 0.5f), 1.5f);
}

void ASDHUD::DrawCompass(ASDCharacter* C)
{
	// heading strip: screen-up on the map is world +X, so heading 0 = +X ("N")
	UFont* F = GEngine->GetSmallFont();
	const float CX = Canvas->SizeX * 0.5f, Y = 14.f * U, Wd = 520.f * U;
	const float Yaw = FRotator::NormalizeAxis(C->GetControlRotation().Yaw);
	const float Heading = FMath::Fmod(Yaw + 360.f, 360.f);
	const float PxPerDeg = Wd / 120.f;
	for (int32 D = 0; D < 360; D += 15)
	{
		float Rel = FRotator::NormalizeAxis(D - Heading);
		if (FMath::Abs(Rel) > 60.f) continue;
		const float X = CX + Rel * PxPerDeg;
		const float A = 1.f - FMath::Abs(Rel) / 60.f;
		static const TCHAR* Card[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
		if (D % 45 == 0) Text(Card[D / 45], X, Y, FLinearColor(1, 1, 1, A), GEngine->GetMediumFont(), 1.f, true);
		else Text(FString::FromInt(D), X, Y + 4 * U, FLinearColor(1, 1, 1, A * 0.6f), F, 0.9f, true);
	}
	Text(FString::FromInt(FMath::RoundToInt(Heading) % 360), CX, Y + 26 * U, White, F, 1.f, true);
	DrawLine(CX, Y + 22 * U, CX, Y + 26 * U, White, 2.f);
	// marker on the compass, clamped to the strip ends when behind you
	if (const ASDPlayerController* MPC = Cast<ASDPlayerController>(GetOwningPlayerController()))
	{
		if (MPC->bHasMarker)
		{
			const FVector2D D = MPC->MarkerPos - FVector2D(C->GetActorLocation());
			const float MarkYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const float Rel = FMath::Clamp(FRotator::NormalizeAxis(MarkYaw - Heading), -60.f, 60.f);
			const float X = CX + Rel * PxPerDeg;
			MarkerPin(X, Y + 44 * U, 8.f * U);
			Txt(FString::Printf(TEXT("%dm"), FMath::RoundToInt(D.Size() / 100.f)), X, Y + 46 * U, 12.f, TEXT("Bold"), FLinearColor(0.6f, 0.85f, 1.f), 1);
		}
	}
}

void ASDHUD::DrawCrosshair(ASDCharacter* C)
{
	const float CX = Canvas->SizeX * 0.5f, CY = Canvas->SizeY * 0.5f;
	const float Now = GetWorld()->GetTimeSeconds();
	if (C->Mode == ESDMode::Build || C->Active < 0)
	{
		Box(CX - 2, CY - 2, 4, 4, White);
	}
	else
	{
		const float Fov = C->Camera ? C->Camera->FieldOfView : 80.f;
		const float Spread = C->CurrentSpread();
		const float Gap = FMath::Tan(FMath::DegreesToRadians(Spread)) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) * CX + 3.f * U;
		const float L = 9.f * U;
		DrawLine(CX - Gap - L, CY, CX - Gap, CY, White, 2.f);
		DrawLine(CX + Gap, CY, CX + Gap + L, CY, White, 2.f);
		DrawLine(CX, CY - Gap - L, CX, CY - Gap, White, 2.f);
		DrawLine(CX, CY + Gap, CX, CY + Gap + L, White, 2.f);
		Box(CX - 1, CY - 1, 2, 2, White);
	}
	if (Now - C->HitMarkerTime < 0.18f)
	{
		const FLinearColor HC = C->bHitMarkerHead ? FLinearColor(1.f, 0.85f, 0.1f) : White;
		const float A = 6 * U, B = 13 * U;
		DrawLine(CX - B, CY - B, CX - A, CY - A, HC, 2.5f);
		DrawLine(CX + A, CY - A, CX + B, CY - B, HC, 2.5f);
		DrawLine(CX - B, CY + B, CX - A, CY + A, HC, 2.5f);
		DrawLine(CX + A, CY + A, CX + B, CY + B, HC, 2.5f);
	}
}

void ASDHUD::DrawVitals(ASDCharacter* C)
{
	UFont* F = GEngine->GetMediumFont();
	const float X = 40 * U, W = 320 * U;
	const float Y = Canvas->SizeY - 110 * U;
	Box(X, Y, W, 18 * U, Dim);
	Box(X, Y, W * C->Shield / 100.f, 18 * U, FLinearColor(0.15f, 0.55f, 1.f, 0.95f));
	Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(C->Shield)), X + W + 10 * U, Y - 3 * U, FLinearColor(0.5f, 0.8f, 1.f), F);
	Box(X, Y + 26 * U, W, 22 * U, Dim);
	Box(X, Y + 26 * U, W * C->Health / 100.f, 22 * U, FLinearColor(0.25f, 0.85f, 0.25f, 0.95f));
	Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(C->Health)), X + W + 10 * U, Y + 24 * U, FLinearColor(0.6f, 1.f, 0.6f), F);
}

void ASDHUD::DrawHotbar(ASDCharacter* C)
{
	UFont* F = GEngine->GetSmallFont();
	UFont* M = GEngine->GetMediumFont();
	const float S = 70 * U, Pad = 8 * U;
	const float X0 = Canvas->SizeX - 6 * (S + Pad) - 30 * U;
	const float Y = Canvas->SizeY - S - 40 * U;
	for (int32 i = -1; i < 5; ++i)
	{
		const float X = X0 + (i + 1) * (S + Pad);
		FLinearColor Fill(0, 0, 0, 0.45f);
		FString Label, Count;
		if (i < 0)
		{
			Label = TEXT("Pickaxe");
		}
		else
		{
			const FSDItemStack& It = C->Slots[i];
			if (It.Item == ESDItem::Weapon)
			{
				Fill = SDData::RarityColor(It.Rarity) * 0.55f;
				Fill.A = 0.75f;
				Label = SDData::Weapon(It.Weapon).Name;
				Count = FString::FromInt(It.Count);
			}
			else if (!It.IsEmpty())
			{
				Fill = FLinearColor(0.2f, 0.35f, 0.6f, 0.7f);
				Label = SDData::ItemName(It);
				Count = FString::Printf(TEXT("x%d"), It.Count);
			}
		}
		Box(X, Y, S, S, Fill);
		const bool bSel = C->Mode == ESDMode::Combat && C->Active == i;
		Frame(X, Y, S, S, bSel ? White : FLinearColor(1, 1, 1, 0.25f), bSel ? 3.f : 1.f);
		Text(FString::FromInt(i + 2 == 1 ? 1 : i + 2), X + 4 * U, Y + 2 * U, FLinearColor(1, 1, 1, 0.6f), F, 0.9f);
		if (!Label.IsEmpty())
		{
			// wrap long names onto two short lines
			FString A = Label, B;
			int32 Space;
			if (Label.Len() > 10 && Label.FindChar(TEXT(' '), Space)) { A = Label.Left(Space); B = Label.Mid(Space + 1); }
			Text(A, X + S * 0.5f, Y + S * 0.38f, White, F, 0.85f, true);
			if (!B.IsEmpty()) Text(B, X + S * 0.5f, Y + S * 0.55f, White, F, 0.85f, true);
		}
		if (!Count.IsEmpty()) Text(Count, X + S - 6 * U, Y + S - 18 * U, White, F, 0.9f, true);
	}

	// ammo line
	if (const FSDItemStack* It = C->ActiveItem())
	{
		if (It->Item == ESDItem::Weapon && C->Mode == ESDMode::Combat)
		{
			const FSDWeaponDef& D = SDData::Weapon(It->Weapon);
			const FString A = C->ReloadLeft > 0.f ? TEXT("RELOADING") : FString::Printf(TEXT("%d / %d"), It->Count, C->Ammo[(int32)D.Ammo]);
			Text(A, X0 + 3 * (S + Pad), Y - 40 * U, White, M, 1.f, true);
			if (C->ReloadLeft > 0.f)
			{
				const float P = 1.f - C->ReloadLeft / D.Reload;
				Box(Canvas->SizeX * 0.5f - 60 * U, Canvas->SizeY * 0.5f + 40 * U, 120 * U * P, 5 * U, White);
			}
		}
	}

	// materials
	const float MX = Canvas->SizeX - 150 * U;
	const float MY = Y - 150 * U;
	for (int32 m = 0; m < 3; ++m)
	{
		const bool bSel = C->Mode == ESDMode::Build && (int32)C->BuildMat == m;
		const float RY = MY + m * 32 * U;
		Box(MX, RY, 22 * U, 22 * U, SDData::MatColor((ESDMat)m));
		if (bSel) Frame(MX - 2, RY - 2, 22 * U + 4, 22 * U + 4, White, 2.f);
		Text(FString::Printf(TEXT("%s  %d"), SDData::MatName((ESDMat)m), C->Mats[m]), MX + 30 * U, RY - 1 * U, bSel ? White : FLinearColor(1, 1, 1, 0.75f), M, 0.9f);
	}

	// build pieces bar
	if (C->Mode == ESDMode::Build)
	{
		static const TCHAR* Names[] = { TEXT("F1 Wall"), TEXT("F2 Floor"), TEXT("F3 Ramp"), TEXT("F4 Roof") };
		const float BW = 110 * U;
		const float BX = Canvas->SizeX * 0.5f - 2 * BW;
		const float BY = Canvas->SizeY - 80 * U;
		for (int32 b = 0; b < 4; ++b)
		{
			const bool bSel = (int32)C->BuildType == b;
			Box(BX + b * BW + 4, BY, BW - 8, 40 * U, bSel ? FLinearColor(0.15f, 0.45f, 0.9f, 0.85f) : Dim);
			Text(Names[b], BX + b * BW + BW * 0.5f, BY + 10 * U, White, F, 1.f, true);
		}
		Text(TEXT("Right click: change material    G: edit    Q: weapons"), Canvas->SizeX * 0.5f, BY - 24 * U, FLinearColor(1, 1, 1, 0.7f), F, 1.f, true);
	}
}

void ASDHUD::DrawEnd(ASDGameMode* GM)
{
	UFont* L = GEngine->GetLargeFont();
	UFont* M = GEngine->GetMediumFont();
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.32f;
	Box(0, CY - 40 * U, Canvas->SizeX, 200 * U + GM->RewardLines.Num() * 26 * U + 20 * U, FLinearColor(0, 0, 0, 0.55f));
	if (GM->bPlayerWon)
	{
		Text(TEXT("#1  SKYDROP CHAMPION"), CX, CY, FLinearColor(1.f, 0.8f, 0.15f), L, 2.6f, true);
	}
	else if (GM->bPlayerOut)
	{
		Text(FString::Printf(TEXT("ELIMINATED  -  #%d of %d"), GM->PlayerPlace, GM->TotalPlayers), CX, CY, FLinearColor(1.f, 0.35f, 0.3f), L, 2.f, true);
		if (GM->Phase == ESDPhase::Over) Text(FString::Printf(TEXT("Winner: %s"), *GM->WinnerName), CX, CY + 60 * U, White, M, 1.2f, true);
	}
	const int32 Kills = GM->PlayerChar ? GM->PlayerChar->Kills : 0;
	Text(FString::Printf(TEXT("Eliminations: %d"), Kills), CX, CY + 95 * U, White, M, 1.1f, true);
	float RY2 = CY + 160 * U;
	for (const FString& Line : GM->RewardLines)
	{
		Text(Line, CX, RY2, FLinearColor(1.f, 0.85f, 0.3f), M, 1.f, true);
		RY2 += 26 * U;
	}
	Text(TEXT("Press ENTER to return to the lobby"), CX, CY + 125 * U, FLinearColor(1, 1, 1, 0.8f), M, 1.f, true);
}

bool ASDHUD::ButtonCenter(FName Id, FVector2D& Out) const
{
	for (const FSDButton& B : Buttons) if (B.Id == Id) { Out = (B.Min + B.Max) * 0.5f; return true; }
	return false;
}

FName ASDHUD::LobbyHit(const FVector2D& P, FName Filter) const
{
	// last drawn is on top
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FSDButton& B = Buttons[i];
		if (!Filter.IsNone()) { if (B.Id != Filter) continue; }
		else if (!(P.X >= B.Min.X && P.X <= B.Max.X && P.Y >= B.Min.Y && P.Y <= B.Max.Y)) continue;
		if (B.bSlider)
		{
			const float F = FMath::Clamp((P.X - B.Min.X) / FMath::Max(B.Max.X - B.Min.X, 1.f), 0.f, 1.f);
			return FName(*FString::Printf(TEXT("%s:%.4f"), *B.Id.ToString(), F));
		}
		return B.Id;
	}
	return NAME_None;
}

bool ASDHUD::Button(FName Id, float X, float Y, float W, float H, const FString& Label, const FLinearColor& Fill, const FLinearColor& TextC, UFont* F, float Scale)
{
	float MX = -1.f, MY = -1.f;
	if (PlayerOwner) PlayerOwner->GetMousePosition(MX, MY);
	const bool bHover = MX >= X && MX <= X + W && MY >= Y && MY <= Y + H;
	FLinearColor C = Fill;
	if (bHover) C = FLinearColor(FMath::Min(Fill.R * 1.2f + 0.05f, 1.f), FMath::Min(Fill.G * 1.2f + 0.05f, 1.f), FMath::Min(Fill.B * 1.2f + 0.05f, 1.f), Fill.A);
	Box(X, Y, W, H, C);
	if (bHover) Frame(X, Y, W, H, FLinearColor(1, 1, 1, 0.9f), 2.f);
	const float TH = TxtSize(Label, (F && F->LegacyFontSize > 0 ? F->LegacyFontSize : 12.f) * Scale, TEXT("Bold")).Y;
	Text(Label, X + W * 0.5f, Y + (H - TH) * 0.5f, TextC, F, Scale, true);
	FSDButton B; B.Min = FVector2D(X, Y); B.Max = FVector2D(X + W, Y + H); B.Id = Id;
	Buttons.Add(B);
	return bHover;
}

void ASDHUD::CoinIcon(float X, float Y, float R)
{
	for (float r = R * 0.35f; r <= R; r += 1.5f) Circle(FVector2D(X, Y), r, FLinearColor(1.f, 0.8f, 0.15f), 2.f);
	Circle(FVector2D(X, Y), R * 0.55f, FLinearColor(0.75f, 0.5f, 0.05f), 2.f);
}

void ASDHUD::Card(FName ClickId, float X, float Y, float W, float H, const FSDCosmetic* C, bool bSel, const FString& Bottom, bool bDim, ASDGameMode* GM)
{
	const FLinearColor Rc = C ? SDData::RarityColor(C->Rarity) : FLinearColor(0.3f, 0.35f, 0.45f);
	Box(X, Y, W, H, Rc * 0.35f + FLinearColor(0, 0, 0, 1));
	Box(X, Y, W, H * 0.55f, Rc * 0.6f + FLinearColor(0, 0, 0, 1));
	UTextureRenderTarget2D* Tex = (C && GM && GM->Lobby) ? GM->Lobby->Thumb(C->Id) : nullptr;
	const float Bh = Bottom.IsEmpty() ? 0.f : FMath::Min(30.f * U, H * 0.28f);
	if (Tex)
	{
		// fill the card above the price strip, cropping the square thumbnail to fit
		const float Ha = H - Bh;
		float U0 = 0.f, V0 = 0.f, UL = 1.f, VL = 1.f;
		if (Ha > W) { UL = W / Ha; U0 = (1.f - UL) * 0.5f; }
		else { VL = Ha / W; V0 = (1.f - VL) * 0.25f; }
		DrawTexture(Tex, X, Y, W, Ha, U0, V0, UL, VL, FLinearColor::White, BLEND_Opaque);
	}
	else if (!C)
	{
		Text(TEXT("X"), X + W * 0.5f, Y + H * 0.25f, FLinearColor(1, 1, 1, 0.5f), GEngine->GetLargeFont(), 1.6f, true);
	}
	if (!Bottom.IsEmpty())
	{
		Box(X, Y + H - Bh, W, Bh, FLinearColor(0.02f, 0.04f, 0.1f, 0.9f));
		Text(Bottom, X + W * 0.5f, Y + H - Bh + (Bh - 18.f * U) * 0.5f, White, GEngine->GetSmallFont(), 1.f, true);
	}
	if (bDim) Box(X, Y, W, H, FLinearColor(0, 0, 0, 0.55f));
	Frame(X, Y, W, H, bSel ? White : Rc, bSel ? 3.f : 1.5f);
	if (!ClickId.IsNone())
	{
		float MX = -1.f, MY = -1.f;
		if (PlayerOwner) PlayerOwner->GetMousePosition(MX, MY);
		if (MX >= X && MX <= X + W && MY >= Y && MY <= Y + H) Frame(X - 2, Y - 2, W + 4, H + 4, FLinearColor(1.f, 0.9f, 0.3f), 2.f);
		FSDButton B; B.Min = FVector2D(X, Y); B.Max = FVector2D(X + W, Y + H); B.Id = ClickId;
		Buttons.Add(B);
	}
}

void ASDHUD::InfoBox(float X, float Y, float W, const FSDCosmetic* C)
{
	if (!C) return;
	UFont* L = GEngine->GetLargeFont();
	UFont* S = GEngine->GetSmallFont();
	const FLinearColor Rc = SDData::RarityColor(C->Rarity);
	Box(X, Y, W, 110 * U, FLinearColor(0.02f, 0.04f, 0.12f, 0.9f));
	Box(X, Y, 6 * U, 110 * U, Rc);
	Text(FString::Printf(TEXT("%s  |  %s"), SDData::RarityName(C->Rarity), SDCos::TypeName(C->Type)), X + 16 * U, Y + 10 * U, Rc, S, 1.f);
	Text(FString(C->Name).ToUpper(), X + 16 * U, Y + 32 * U, White, L, 1.4f);
	Text(C->Desc, X + 16 * U, Y + 80 * U, FLinearColor(1, 1, 1, 0.75f), S, 1.f);
}

// ------------------------------------------------------------------ styled drawing helpers

FSlateFontInfo ASDHUD::UIFontInfo(float Size, const TCHAR* Face)
{
	// canvas text needs a runtime UFont, so wrap the engine Roboto family (all weights) in one
	if (!UIFont)
	{
		UIFont = NewObject<UFont>(this);
		UIFont->FontCacheType = EFontCacheType::Runtime;
		UIFont->CompositeFont = *FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10).CompositeFont;
		UIFont->LegacyFontSize = 12;
	}
	FSlateFontInfo Fi(UIFont, FMath::Max(1, FMath::RoundToInt(Size * U)), FName(Face));
	return Fi;
}

FVector2D ASDHUD::TxtSize(const FString& S, float Size, const TCHAR* Face) const
{
	if (!FSlateApplication::IsInitialized()) return FVector2D(S.Len() * Size * 0.5f * U, Size * U);
	const FSlateFontInfo Fi = const_cast<ASDHUD*>(this)->UIFontInfo(Size, Face);
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi);
}

float ASDHUD::Txt(const FString& S, float X, float Y, float Size, const TCHAR* Face, const FLinearColor& C, int32 Align, float Shadow)
{
	const FSlateFontInfo Fi = UIFontInfo(Size, Face);
	const FVector2D Sz = TxtSize(S, Size, Face);
	const float DX = Align == 1 ? X - Sz.X * 0.5f : Align == 2 ? X - Sz.X : X;
	FCanvasTextItem T(FVector2D(DX, Y), FText::FromString(S), Fi, C);
	if (Shadow > 0.f) T.EnableShadow(FLinearColor(0.f, 0.f, 0.05f, C.A * 0.55f), FVector2D(Shadow * U, Shadow * U));
	Canvas->DrawItem(T);
	return Sz.X;
}

void ASDHUD::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
	const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD)
{
	// A top-left, B top-right, C bottom-right, D bottom-left
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T0; T0.V0_Pos = A; T0.V1_Pos = B; T0.V2_Pos = C; T0.V0_Color = CA; T0.V1_Color = CB; T0.V2_Color = CC;
	FCanvasUVTri T1; T1.V0_Pos = A; T1.V1_Pos = C; T1.V2_Pos = D; T1.V0_Color = CA; T1.V1_Color = CC; T1.V2_Color = CD;
	Tris.Add(T0); Tris.Add(T1);
	FCanvasTriangleItem It(Tris, GWhiteTexture);
	It.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(It);
}

void ASDHUD::Slant(float X, float Y, float W, float H, float Skew, const FLinearColor& C)
{
	Quad(FVector2D(X, Y), FVector2D(X + W + Skew, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H), C, C, C, C);
}

void ASDHUD::Grad(float X, float Y, float W, float H, const FLinearColor& Top, const FLinearColor& Bot)
{
	Quad(FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H), Top, Top, Bot, Bot);
}

void ASDHUD::Star(float X, float Y, float R, const FLinearColor& C)
{
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < 10; ++i)
	{
		const float A0 = -PI * 0.5f + i * PI / 5.f, A1 = -PI * 0.5f + (i + 1) * PI / 5.f;
		const float R0 = (i % 2 == 0) ? R : R * 0.45f, R1 = (i % 2 == 0) ? R * 0.45f : R;
		FCanvasUVTri T;
		T.V0_Pos = FVector2D(X, Y);
		T.V1_Pos = FVector2D(X + FMath::Cos(A0) * R0, Y + FMath::Sin(A0) * R0);
		T.V2_Pos = FVector2D(X + FMath::Cos(A1) * R1, Y + FMath::Sin(A1) * R1);
		T.V0_Color = T.V1_Color = T.V2_Color = C;
		Tris.Add(T);
	}
	FCanvasTriangleItem It(Tris, GWhiteTexture);
	It.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(It);
}

void ASDHUD::Shield(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Edge)
{
	const FVector2D P[] = {
		FVector2D(X + W * 0.5f, Y), FVector2D(X + W, Y + H * 0.12f), FVector2D(X + W, Y + H * 0.55f),
		FVector2D(X + W * 0.5f, Y + H), FVector2D(X, Y + H * 0.55f), FVector2D(X, Y + H * 0.12f) };
	const FVector2D Cn(X + W * 0.5f, Y + H * 0.45f);
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < 6; ++i)
	{
		FCanvasUVTri T;
		T.V0_Pos = Cn; T.V1_Pos = P[i]; T.V2_Pos = P[(i + 1) % 6];
		T.V0_Color = Fill * 1.25f; T.V0_Color.A = Fill.A;
		T.V1_Color = T.V2_Color = Fill;
		Tris.Add(T);
	}
	FCanvasTriangleItem It(Tris, GWhiteTexture);
	It.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(It);
	for (int32 i = 0; i < 6; ++i) DrawLine(P[i].X, P[i].Y, P[(i + 1) % 6].X, P[(i + 1) % 6].Y, Edge, 3.f * U);
}

bool ASDHUD::Hot(FName Id, float X, float Y, float W, float H)
{
	float MX = -1.f, MY = -1.f;
	if (PlayerOwner) PlayerOwner->GetMousePosition(MX, MY);
	FSDButton B; B.Min = FVector2D(X, Y); B.Max = FVector2D(X + W, Y + H); B.Id = Id;
	Buttons.Add(B);
	return MX >= X && MX <= X + W && MY >= Y && MY <= Y + H;
}

// ------------------------------------------------------------------ lobby: top bar

void ASDHUD::LobbyTopBar(ASDGameMode* GM)
{
	const float W = Canvas->SizeX;
	USDSave* Sv = GM->Save;
	const FLinearColor Navy(0.02f, 0.05f, 0.13f, 1.f);
	const FLinearColor Yellow(1.f, 0.9f, 0.16f, 1.f);
	const FLinearColor Ink(0.03f, 0.07f, 0.2f, 1.f);

	// bar + tab strip
	Grad(0, 0, W, 64 * U, FLinearColor(0.035f, 0.08f, 0.19f, 0.97f), FLinearColor(0.02f, 0.05f, 0.13f, 0.97f));
	Grad(0, 64 * U, W, 40 * U, FLinearColor(0.06f, 0.14f, 0.32f, 0.85f), FLinearColor(0.04f, 0.1f, 0.25f, 0.6f));
	Box(0, 64 * U, W, 1.5f * U, FLinearColor(0.3f, 0.5f, 0.85f, 0.6f));

	// logo plate
	Slant(0, 0, 300 * U, 64 * U, 26 * U, FLinearColor(0.01f, 0.02f, 0.06f, 1.f));
	Txt(TEXT("SKYDROP"), 30 * U, 8 * U, 30.f, TEXT("BlackItalic"), FLinearColor::White, 0, 3.f);

	// tabs
	static const TCHAR* Tabs[] = { TEXT("LOBBY"), TEXT("BATTLE PASS"), TEXT("CHALLENGES"), TEXT("LOCKER"), TEXT("ITEM SHOP"), TEXT("SETTINGS") };
	float Total = 0.f;
	float TW[6];
	for (int32 i = 0; i < 6; ++i) { TW[i] = TxtSize(Tabs[i], 15.f, TEXT("BoldCondensed")).X + 40 * U; Total += TW[i] + 6 * U; }
	float TX = W * 0.5f - Total * 0.5f;
	for (int32 i = 0; i < 6; ++i)
	{
		const bool bSel = GM->LobbyTab == i;
		const bool bHov = Hot(FName(*FString::Printf(TEXT("tab%d"), i)), TX, 70 * U, TW[i], 28 * U);
		const FLinearColor Fill = bSel ? Yellow : bHov ? FLinearColor(0.22f, 0.42f, 0.8f, 1.f) : FLinearColor(0.12f, 0.26f, 0.55f, 0.95f);
		Box(TX, 70 * U, TW[i], 28 * U, Fill);
		Txt(Tabs[i], TX + TW[i] * 0.5f, 74 * U, 15.f, TEXT("BoldCondensed"), bSel ? Ink : FLinearColor::White, 1, bSel ? 0.f : 1.5f);
		TX += TW[i] + 6 * U;
	}

	// coins and level
	const float CX = W - 360 * U;
	Box(CX, 13 * U, 170 * U, 38 * U, FLinearColor(0.01f, 0.02f, 0.06f, 0.95f));
	Frame(CX, 13 * U, 170 * U, 38 * U, FLinearColor(0.4f, 0.55f, 0.85f, 0.8f), 1.5f * U);
	CoinIcon(CX + 22 * U, 32 * U, 12 * U);
	Txt(FText::AsNumber(Sv->Coins).ToString(), CX + 44 * U, 18 * U, 18.f, TEXT("Bold"), FLinearColor::White);
	Box(CX + 134 * U, 18 * U, 28 * U, 28 * U, FLinearColor(0.12f, 0.26f, 0.55f, 1.f));
	Txt(TEXT("+"), CX + 148 * U, 17 * U, 18.f, TEXT("Bold"), FLinearColor::White, 1, 0.f);
	Hot(TEXT("tab4"), CX, 13 * U, 170 * U, 38 * U);

	const float LX = W - 176 * U;
	Box(LX, 13 * U, 146 * U, 38 * U, FLinearColor(0.01f, 0.02f, 0.06f, 0.95f));
	Frame(LX, 13 * U, 146 * U, 38 * U, FLinearColor(0.4f, 0.55f, 0.85f, 0.8f), 1.5f * U);
	Star(LX + 22 * U, 32 * U, 12 * U, Yellow);
	Txt(FString::Printf(TEXT("LVL %d"), Sv->Level()), LX + 42 * U, 18 * U, 18.f, TEXT("Bold"), FLinearColor::White);
}

// ------------------------------------------------------------------ lobby: home screen

void ASDHUD::LobbyHome(ASDGameMode* GM)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	USDSave* Sv = GM->Save;
	const bool bQueue = GM->Phase == ESDPhase::Matchmaking;
	const FLinearColor Panel(0.03f, 0.07f, 0.18f, 0.86f);
	const FLinearColor HeaderC(0.16f, 0.3f, 0.58f, 0.95f);
	const FLinearColor Yellow(1.f, 0.9f, 0.16f, 1.f);
	const FLinearColor Ink(0.03f, 0.07f, 0.2f, 1.f);
	const FLinearColor Soft(0.75f, 0.83f, 1.f, 0.9f);
	const FLinearColor XPGreen(0.42f, 0.85f, 0.2f, 1.f);

	// ---- name tag and invite slots in the 3D room
	if (GM->Lobby)
	{
		const FVector Head = Project(GM->Lobby->PadLocation(0) + FVector(0, 0, 222.f), true);
		const float NW = Txt(TEXT("You"), Head.X + 12 * U, Head.Y, 17.f, TEXT("Bold"), FLinearColor::White, 1);
		// little crown
		const float CrX = Head.X + 12 * U - NW * 0.5f - 16 * U, CrY = Head.Y + 6 * U;
		Quad(FVector2D(CrX - 8 * U, CrY), FVector2D(CrX + 8 * U, CrY), FVector2D(CrX + 8 * U, CrY + 12 * U), FVector2D(CrX - 8 * U, CrY + 12 * U),
			FLinearColor(0, 0, 0, 0), FLinearColor(0, 0, 0, 0), Yellow, Yellow);
		Star(CrX, CrY + 4 * U, 7 * U, Yellow);
		Box(CrX - 8 * U, CrY + 8 * U, 16 * U, 5 * U, Yellow);
		Txt(bQueue ? TEXT("Ready") : TEXT("Not ready"), Head.X, Head.Y + 24 * U, 14.f, TEXT("Bold"),
			bQueue ? FLinearColor(0.45f, 1.f, 0.5f) : FLinearColor(1.f, 0.38f, 0.38f), 1);
		for (int32 p = 1; p <= 2; ++p)
		{
			const FVector Pp = Project(GM->Lobby->PadLocation(p) + FVector(0, 0, 120.f), true);
			const float S = 36 * U;
			if (Pp.X < 470 * U) continue; // keep clear of the left panels
			// dashed square
			for (int32 d = 0; d < 4; ++d)
			{
				const float A = -S * 0.5f + d * S / 4.f, B = A + S / 8.f * 1.2f;
				const FLinearColor Lc(1, 1, 1, 0.75f);
				DrawLine(Pp.X + A, Pp.Y - S * 0.5f, Pp.X + B, Pp.Y - S * 0.5f, Lc, 2.f * U);
				DrawLine(Pp.X + A, Pp.Y + S * 0.5f, Pp.X + B, Pp.Y + S * 0.5f, Lc, 2.f * U);
				DrawLine(Pp.X - S * 0.5f, Pp.Y + A, Pp.X - S * 0.5f, Pp.Y + B, Lc, 2.f * U);
				DrawLine(Pp.X + S * 0.5f, Pp.Y + A, Pp.X + S * 0.5f, Pp.Y + B, Lc, 2.f * U);
			}
			Box(Pp.X - 9 * U, Pp.Y - 1.5f * U, 18 * U, 3 * U, FLinearColor::White);
			Box(Pp.X - 1.5f * U, Pp.Y - 9 * U, 3 * U, 18 * U, FLinearColor::White);
		}
	}

	// ---- left column
	const float PX = 28 * U, PW = 400 * U;
	float Y = 124 * U;

	// season header
	Slant(PX, Y, PW - 14 * U, 36 * U, 14 * U, HeaderC);
	Txt(TEXT("SEASON 1"), PX + 14 * U, Y + 6 * U, 18.f, TEXT("BlackItalic"), FLinearColor::White);
	Y += 36 * U;

	// level card
	Box(PX, Y, PW, 150 * U, Panel);
	Shield(PX + 22 * U, Y + 18 * U, 92 * U, 112 * U, FLinearColor(0.12f, 0.5f, 0.95f, 1.f), FLinearColor(0.85f, 0.95f, 1.f, 1.f));
	Star(PX + 68 * U, Y + 66 * U, 26 * U, FLinearColor::White);
	Txt(TEXT("LEVEL"), PX + 132 * U, Y + 26 * U, 22.f, TEXT("BlackItalic"), FLinearColor::White);
	Txt(FString::FromInt(Sv->Level()), PX + 132 * U + TxtSize(TEXT("LEVEL "), 22.f, TEXT("BlackItalic")).X, Y + 12 * U, 38.f, TEXT("BlackItalic"), FLinearColor::White);
	Txt(FString::Printf(TEXT("%s / %s"), *FText::AsNumber(Sv->XP % USDSave::XPPerLevel).ToString(), *FText::AsNumber(USDSave::XPPerLevel).ToString()),
		PX + 134 * U, Y + 70 * U, 13.f, TEXT("Bold"), Soft);
	Box(PX + 134 * U, Y + 92 * U, 190 * U, 10 * U, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Box(PX + 134 * U, Y + 92 * U, 190 * U * Sv->LevelFrac(), 10 * U, XPGreen);
	Star(PX + 344 * U, Y + 97 * U, 9 * U, Yellow);
	Txt(TEXT("1"), PX + 358 * U, Y + 88 * U, 13.f, TEXT("Bold"), FLinearColor::White);
	Txt(FString::Printf(TEXT("NEXT  LVL %d"), Sv->Level() + 1), PX + 134 * U, Y + 112 * U, 12.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.6f));
	Y += 158 * U;

	// battle pass block (click -> battle pass tab)
	{
		const bool bHov = Hot(TEXT("tab1"), PX, Y, PW, 112 * U);
		Grad(PX, Y, PW, 56 * U, FLinearColor(1.f, 0.78f, 0.25f, 0.97f), FLinearColor(0.85f, 0.55f, 0.1f, 0.97f));
		Star(PX + 30 * U, Y + 28 * U, 18 * U, FLinearColor(1.f, 0.97f, 0.8f));
		Txt(TEXT("BATTLE PASS"), PX + 58 * U, Y + 6 * U, 19.f, TEXT("BlackItalic"), FLinearColor::White);
		Txt(FString::Printf(TEXT("Tier %d"), Sv->Tier()), PX + 58 * U, Y + 32 * U, 13.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.9f));
		Star(PX + PW - 84 * U, Y + 28 * U, 11 * U, FLinearColor(1.f, 0.97f, 0.8f));
		Txt(FString::Printf(TEXT("%d / %d"), Sv->Tier(), SDCos::PassTiers), PX + PW - 66 * U, Y + 18 * U, 15.f, TEXT("Bold"), FLinearColor::White);
		// first challenge sits inside the gold block like the pass's weekly goal
		const SDProfile::FChallenge& C0 = SDProfile::Challenge(0);
		Box(PX, Y + 56 * U, PW, 56 * U, FLinearColor(0.62f, 0.42f, 0.1f, 0.92f));
		Txt(C0.Text, PX + 14 * U, Y + 62 * U, 14.f, TEXT("Bold"), FLinearColor::White);
		const float F0 = FMath::Clamp(Sv->Challenge[0] / float(C0.Goal), 0.f, 1.f);
		Box(PX + 14 * U, Y + 88 * U, 220 * U, 8 * U, FLinearColor(0, 0, 0, 0.45f));
		Box(PX + 14 * U, Y + 88 * U, 220 * U * F0, 8 * U, FLinearColor(1.f, 0.95f, 0.6f));
		Txt(FString::Printf(TEXT("%d / %d"), Sv->Challenge[0], C0.Goal), PX + 246 * U, Y + 81 * U, 13.f, TEXT("Bold"), FLinearColor::White);
		Circle(FVector2D(PX + 316 * U, Y + 89 * U), 10 * U, XPGreen, 7.f * U);
		Txt(TEXT("XP"), PX + 316 * U, Y + 82 * U, 9.f, TEXT("Black"), Ink, 1, 0.f);
		Txt(FString::FromInt(C0.Reward), PX + 332 * U, Y + 81 * U, 13.f, TEXT("Bold"), FLinearColor::White);
		if (bHov) Frame(PX, Y, PW, 112 * U, FLinearColor(1, 1, 1, 0.8f), 2.f * U);
		Y += 120 * U;
	}

	// daily challenges (click -> challenges tab)
	{
		const bool bHov = Hot(TEXT("tab2"), PX, Y, PW, 30 * U + 2 * 62 * U);
		Slant(PX, Y, PW - 14 * U, 30 * U, 14 * U, HeaderC);
		Txt(TEXT("DAILY CHALLENGES"), PX + 14 * U, Y + 5 * U, 15.f, TEXT("BlackItalic"), FLinearColor::White);
		Y += 30 * U;
		for (int32 i = 1; i <= 2; ++i)
		{
			const SDProfile::FChallenge& C = SDProfile::Challenge(i);
			const bool bDone = Sv->ChallengeDone[i];
			Box(PX, Y, PW, 60 * U, Panel);
			Box(PX, Y + 59 * U, PW, 1 * U, FLinearColor(0.3f, 0.45f, 0.75f, 0.4f));
			Txt(C.Text, PX + 14 * U, Y + 8 * U, 14.f, TEXT("Bold"), FLinearColor::White);
			const float Fr = FMath::Clamp(Sv->Challenge[i] / float(C.Goal), 0.f, 1.f);
			Box(PX + 14 * U, Y + 38 * U, 220 * U, 8 * U, FLinearColor(0, 0, 0, 0.5f));
			Box(PX + 14 * U, Y + 38 * U, 220 * U * Fr, 8 * U, bDone ? XPGreen : FLinearColor(0.35f, 0.75f, 1.f));
			Txt(FString::Printf(TEXT("%d / %d"), Sv->Challenge[i], C.Goal), PX + 246 * U, Y + 31 * U, 13.f, TEXT("Bold"), FLinearColor::White);
			if (bDone) Txt(TEXT("DONE"), PX + PW - 16 * U, Y + 31 * U, 13.f, TEXT("Black"), XPGreen, 2);
			else
			{
				Circle(FVector2D(PX + 316 * U, Y + 39 * U), 10 * U, XPGreen, 7.f * U);
				Txt(TEXT("XP"), PX + 316 * U, Y + 32 * U, 9.f, TEXT("Black"), Ink, 1, 0.f);
				Txt(FString::FromInt(C.Reward), PX + 332 * U, Y + 31 * U, 13.f, TEXT("Bold"), FLinearColor::White);
			}
			Y += 62 * U;
		}
		if (bHov) Frame(PX, Y - 2 * 62 * U - 30 * U, PW, 30 * U + 2 * 62 * U, FLinearColor(1, 1, 1, 0.6f), 2.f * U);
	}

	// ---- right column: mode + ready
	const SDProfile::FMode& Mo = SDProfile::Mode(Sv->Mode);
	const float RW = 360 * U, RX = W - RW - 28 * U;
	float RY = H - 48 * U - 330 * U;

	// banner
	Slant(RX - 10 * U, RY, RW + 4 * U, 32 * U, 12 * U, Yellow);
	Txt(TEXT("Season 1 is live!"), RX + RW * 0.5f, RY + 5 * U, 16.f, TEXT("BlackItalic"), Ink, 1, 0.f);
	RY += 32 * U;

	Box(RX, RY, RW, 188 * U, Panel);
	Txt(TEXT("GAME MODE"), RX + RW * 0.5f, RY + 8 * U, 13.f, TEXT("Bold"), Soft, 1);
	Txt(FString(Mo.Name).ToUpper(), RX + RW * 0.5f, RY + 26 * U, 30.f, TEXT("BlackItalic"), FLinearColor::White, 1, 3.f);
	if (!bQueue)
	{
		const bool bHov = Hot(TEXT("modenext"), RX + 14 * U, RY + 76 * U, RW - 28 * U, 36 * U);
		Box(RX + 14 * U, RY + 76 * U, RW - 28 * U, 36 * U, bHov ? FLinearColor(0.25f, 0.5f, 0.92f, 1.f) : FLinearColor(0.16f, 0.36f, 0.72f, 1.f));
		Frame(RX + 14 * U, RY + 76 * U, RW - 28 * U, 36 * U, FLinearColor(0.85f, 0.92f, 1.f, 0.9f), 1.5f * U);
		Txt(TEXT("Change mode"), RX + RW * 0.5f, RY + 82 * U, 16.f, TEXT("Bold"), FLinearColor::White, 1);
	}
	// players row
	Box(RX + 14 * U, RY + 126 * U, RW - 28 * U, 1 * U, FLinearColor(1, 1, 1, 0.15f));
	Txt(TEXT("Players"), RX + 18 * U, RY + 142 * U, 15.f, TEXT("Bold"), Soft);
	{
		const float AX = RX + RW - 170 * U, AY = RY + 138 * U;
		if (!bQueue)
		{
			const bool bL = Hot(TEXT("modeprev"), AX, AY, 30 * U, 30 * U);
			Box(AX, AY, 30 * U, 30 * U, bL ? FLinearColor(0.25f, 0.5f, 0.92f, 1.f) : FLinearColor(0.12f, 0.26f, 0.55f, 1.f));
			Txt(TEXT("<"), AX + 15 * U, AY + 3 * U, 16.f, TEXT("Black"), FLinearColor::White, 1, 0.f);
			const bool bR = Hot(TEXT("modenext"), AX + 126 * U, AY, 30 * U, 30 * U);
			Box(AX + 126 * U, AY, 30 * U, 30 * U, bR ? FLinearColor(0.25f, 0.5f, 0.92f, 1.f) : FLinearColor(0.12f, 0.26f, 0.55f, 1.f));
			Txt(TEXT(">"), AX + 141 * U, AY + 3 * U, 16.f, TEXT("Black"), FLinearColor::White, 1, 0.f);
		}
		Txt(FString::FromInt(Mo.Bots + 1), AX + 78 * U, AY + 3 * U, 17.f, TEXT("Bold"), FLinearColor::White, 1);
	}
	RY += 196 * U;

	// ready button
	const float BH = 96 * U;
	if (!bQueue)
	{
		const bool bHov = Hot(TEXT("ready"), RX, RY, RW, BH);
		Grad(RX, RY, RW, BH, bHov ? FLinearColor(0.9f, 1.f, 1.f, 1.f) : FLinearColor(0.8f, 0.97f, 1.f, 1.f),
			bHov ? FLinearColor(0.5f, 0.85f, 1.f, 1.f) : FLinearColor(0.38f, 0.74f, 1.f, 1.f));
		Frame(RX, RY, RW, BH, FLinearColor(1, 1, 1, 0.95f), 2.f * U);
		Txt(TEXT("Ready!"), RX + RW * 0.5f, RY + 12 * U, 46.f, TEXT("BlackItalic"), Ink, 1, 0.f);
	}
	else
	{
		Grad(RX, RY, RW, BH, FLinearColor(0.55f, 0.95f, 0.5f, 1.f), FLinearColor(0.25f, 0.7f, 0.3f, 1.f));
		Frame(RX, RY, RW, BH, FLinearColor(1, 1, 1, 0.95f), 2.f * U);
		const int32 Dots = int32(GetWorld()->GetTimeSeconds() * 3.f) % 4;
		Txt(FString(TEXT("Searching")) + FString::ChrN(Dots, TEXT('.')), RX + RW * 0.5f, RY + 10 * U, 30.f, TEXT("BlackItalic"), Ink, 1, 0.f);
		Txt(FString::Printf(TEXT("%d / %d players"), GM->FakeQueue, Mo.Bots + 1), RX + RW * 0.5f, RY + 56 * U, 16.f, TEXT("Bold"), Ink, 1, 0.f);
	}
}

// ------------------------------------------------------------------ lobby: bottom bar

void ASDHUD::LobbyBottomBar(ASDGameMode* GM)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float BY = H - 40 * U;
	Grad(0, BY, W, 40 * U, FLinearColor(0.02f, 0.04f, 0.1f, 0.92f), FLinearColor(0.01f, 0.02f, 0.06f, 0.97f));
	Box(0, BY, W, 1 * U, FLinearColor(0.3f, 0.5f, 0.85f, 0.4f));
	Txt(TEXT("General"), 22 * U, BY + 10 * U, 14.f, TEXT("Bold"), FLinearColor::White);
	Txt(TEXT("ENTER  Ready     1-6  Tabs     Q / E  Mode     ESC  Back"), 110 * U, BY + 11 * U, 13.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.5f));
	struct FB { const TCHAR* Label; const TCHAR* Id; };
	const FB Bs[] = { { TEXT("View challenges"), TEXT("tab2") }, { TEXT("Locker"), TEXT("tab3") }, { TEXT("Item shop"), TEXT("tab4") } };
	float X = W - 16 * U;
	for (int32 i = UE_ARRAY_COUNT(Bs) - 1; i >= 0; --i)
	{
		const float BW = TxtSize(Bs[i].Label, 14.f, TEXT("Regular")).X + 28 * U;
		X -= BW;
		const bool bHov = Hot(Bs[i].Id, X, BY + 6 * U, BW, 28 * U);
		if (bHov) Box(X, BY + 6 * U, BW, 28 * U, FLinearColor(0.2f, 0.4f, 0.8f, 0.6f));
		Frame(X, BY + 6 * U, BW, 28 * U, FLinearColor(1, 1, 1, 0.45f), 1.f * U);
		Txt(Bs[i].Label, X + BW * 0.5f, BY + 10 * U, 14.f, TEXT("Regular"), FLinearColor::White, 1);
		X -= 8 * U;
	}
}

void ASDHUD::DrawLobby(ASDGameMode* GM)
{
	UFont* L = GEngine->GetLargeFont();
	UFont* M = GEngine->GetMediumFont();
	UFont* S = GEngine->GetSmallFont();
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	USDSave* Sv = GM->Save;
	if (!Sv) return;
	const FLinearColor Panel(0.03f, 0.07f, 0.18f, 0.88f);
	const FLinearColor Header(0.07f, 0.16f, 0.36f, 0.95f);
	const FLinearColor Accent(1.f, 0.88f, 0.1f, 1.f);
	const FLinearColor Dark(0.03f, 0.04f, 0.08f, 1.f);
	const bool bQueue = GM->Phase == ESDPhase::Matchmaking;
	const int32 Tab = GM->LobbyTab;

	LobbyTopBar(GM);

	// ---------------- per tab
	if (Tab == 0) LobbyHome(GM);
	else if (Tab == 5) DrawSettings(GM, 60 * U, 124 * U, W - 120 * U, H - 124 * U - 60 * U, false);
	else if (Tab == 1) // ---------------- battle pass
	{
		const float GX = 70 * U, GY = 120 * U;
		const int32 Pages = FMath::DivideAndRoundUp(SDCos::PassTiers, 7);
		const int32 Page = FMath::Clamp(GM->PassPage, 0, Pages - 1);
		// tier box + progress
		Box(GX, GY, 120 * U, 120 * U, FLinearColor(0.08f, 0.12f, 0.22f, 0.95f));
		Frame(GX, GY, 120 * U, 120 * U, FLinearColor(1, 1, 1, 0.6f), 2.f);
		Text(TEXT("TIER"), GX + 60 * U, GY + 12 * U, FLinearColor(1, 1, 1, 0.8f), M, 1.f, true);
		Text(FString::FromInt(Sv->Tier()), GX + 60 * U, GY + 40 * U, White, L, 2.4f, true);
		const float BarX = GX + 140 * U;
		Text(FString::Printf(TEXT("%d / %d XP"), Sv->XP % USDSave::XPPerLevel, USDSave::XPPerLevel), BarX, GY + 8 * U, Accent, M, 1.f);
		for (int32 s = 0; s < 10; ++s)
		{
			const bool bOn = Sv->LevelFrac() * 10.f > s;
			Box(BarX + s * 26 * U, GY + 40 * U, 22 * U, 12 * U, bOn ? Accent : FLinearColor(0.1f, 0.15f, 0.3f, 0.9f));
		}
		Text(FString::Printf(TEXT("> %d"), FMath::Min(Sv->Tier() + 1, SDCos::PassTiers)), BarX + 268 * U, GY + 34 * U, White, M, 1.f);
		Text(TEXT("SEASON 1"), W * 0.33f, GY + 2 * U, White, L, 1.8f, true);
		Text(FString::Printf(TEXT("SEASON ENDS: %d Days"), SDProfile::SeasonDaysLeft()), W * 0.33f, GY + 56 * U, FLinearColor(1, 1, 1, 0.8f), M, 1.f, true);
		Text(FString::Printf(TEXT("PAGE %d / %d"), Page + 1, Pages), W * 0.57f, GY + 80 * U, White, M, 1.1f, true);
		// grid
		const float CellW = 118 * U, CellH = 150 * U, RowX = GX + 130 * U, RowY = GY + 140 * U;
		Button(TEXT("bpprev"), GX - 50 * U, RowY + CellH - 30 * U, 40 * U, 60 * U, TEXT("<"), Header, White, L, 1.3f);
		Button(TEXT("bpnext"), RowX + 7 * (CellW + 6 * U) + 10 * U, RowY + CellH - 30 * U, 40 * U, 60 * U, TEXT(">"), Header, White, L, 1.3f);
		// row headers
		Box(GX, RowY + 30 * U, 120 * U, CellH, FLinearColor(0.2f, 0.28f, 0.42f, 0.9f));
		Text(TEXT("FREE"), GX + 60 * U, RowY + 30 * U + CellH * 0.42f, White, M, 1.1f, true);
		Box(GX, RowY + 36 * U + CellH, 120 * U, CellH, FLinearColor(0.7f, 0.5f, 0.12f, 0.95f));
		Text(TEXT("BATTLE"), GX + 60 * U, RowY + 36 * U + CellH * 1.32f, White, M, 1.1f, true);
		Text(TEXT("PASS"), GX + 60 * U, RowY + 36 * U + CellH * 1.52f, White, M, 1.1f, true);
		for (int32 c = 0; c < 7; ++c)
		{
			const int32 TierN = Page * 7 + c + 1;
			if (TierN > SDCos::PassTiers) break;
			const float CX = RowX + c * (CellW + 6 * U);
			Box(CX, RowY, CellW, 26 * U, FLinearColor(0.1f, 0.18f, 0.35f, 0.95f));
			Text(FString::FromInt(TierN), CX + CellW * 0.5f, RowY + 3 * U, TierN <= Sv->Tier() ? Accent : White, M, 1.f, true);
			for (int32 Row = 0; Row < 2; ++Row)
			{
				const bool bPrem = Row == 1;
				const float CY = RowY + 30 * U + Row * (CellH + 6 * U);
				Box(CX, CY, CellW, CellH, bPrem ? FLinearColor(0.55f, 0.4f, 0.12f, 0.75f) : FLinearColor(0.2f, 0.28f, 0.42f, 0.75f));
				int32 Coins = 0;
				const FName Id = SDCos::PassReward(TierN, bPrem, Coins);
				const bool bUnlocked = TierN <= Sv->Tier() && (!bPrem || Sv->bPassOwned);
				const FName Click(*FString::Printf(TEXT("bpcell:%d:%d"), TierN, Row));
				const bool bSel = GM->PassSelTier == TierN && GM->PassSelRow == Row;
				if (Id != NAME_None)
				{
					Card(Click, CX + 6 * U, CY + 6 * U, CellW - 12 * U, CellH - 12 * U, SDCos::Find(Id), bSel, FString(), !bUnlocked, GM);
				}
				else if (Coins > 0)
				{
					Box(CX + 6 * U, CY + 6 * U, CellW - 12 * U, CellH - 12 * U, FLinearColor(0.85f, 0.55f, 0.1f, 0.9f));
					CoinIcon(CX + CellW * 0.5f, CY + CellH * 0.42f, 22 * U);
					Text(FString::Printf(TEXT("%d"), Coins), CX + CellW * 0.5f, CY + CellH * 0.66f, White, M, 1.2f, true);
					if (!bUnlocked) Box(CX + 6 * U, CY + 6 * U, CellW - 12 * U, CellH - 12 * U, FLinearColor(0, 0, 0, 0.55f));
				}
				if (bUnlocked && (Id != NAME_None || Coins > 0))
				{
					// check mark badge
					Box(CX + CellW - 30 * U, CY + 8 * U, 22 * U, 22 * U, FLinearColor(0.2f, 0.75f, 0.3f, 1.f));
					DrawLine(CX + CellW - 26 * U, CY + 19 * U, CX + CellW - 21 * U, CY + 25 * U, White, 2.5f);
					DrawLine(CX + CellW - 21 * U, CY + 25 * U, CX + CellW - 12 * U, CY + 12 * U, White, 2.5f);
				}
			}
		}
		// purchase
		const float PY2 = RowY + 30 * U + 2 * (CellH + 6 * U) + 14 * U;
		Box(GX, PY2, 7 * (CellW + 6 * U) + 130 * U, 60 * U, FLinearColor(0.03f, 0.06f, 0.16f, 0.92f));
		if (!Sv->bPassOwned)
		{
			Button(TEXT("buypass"), GX + 10 * U, PY2 + 8 * U, 330 * U, 44 * U, FString::Printf(TEXT("PURCHASE BATTLE PASS   %d"), SDCos::PassPrice), Accent, Dark, M, 1.1f);
			Text(TEXT("Unlock every premium reward you've already earned, and more."), GX + 360 * U, PY2 + 20 * U, FLinearColor(1, 1, 1, 0.8f), S, 1.05f);
		}
		else
		{
			Text(TEXT("Battle Pass owned. Keep playing to unlock new tiers!"), GX + 20 * U, PY2 + 18 * U, Accent, M, 1.f);
		}
		if (GM->PassSelTier > 0)
		{
			int32 Coins = 0;
			InfoBox(W - 470 * U, H - 170 * U, 440 * U, SDCos::Find(SDCos::PassReward(GM->PassSelTier, GM->PassSelRow == 1, Coins)));
		}
	}
	else if (Tab == 2) // ---------------- challenges
	{
		const float PX = 70 * U, PY = 120 * U, PW = W * 0.5f;
		Box(PX, PY, PW, 40 * U, Header);
		Text(TEXT("WEEK 1 CHALLENGES"), PX + 14 * U, PY + 8 * U, White, M, 1.15f);
		float Y = PY + 46 * U;
		for (int32 i = 0; i < SDProfile::NumChallenges; ++i)
		{
			const SDProfile::FChallenge& C = SDProfile::Challenge(i);
			const bool bDone = Sv->ChallengeDone[i];
			Box(PX, Y, PW, 74 * U, Panel);
			Box(PX, Y, 6 * U, 74 * U, bDone ? FLinearColor(0.3f, 0.9f, 0.4f) : Accent);
			Text(C.Text, PX + 20 * U, Y + 8 * U, White, M, 1.15f);
			const float Frac = FMath::Clamp(Sv->Challenge[i] / float(C.Goal), 0.f, 1.f);
			Box(PX + 20 * U, Y + 46 * U, PW * 0.55f, 12 * U, FLinearColor(0, 0, 0, 0.6f));
			Box(PX + 20 * U, Y + 46 * U, PW * 0.55f * Frac, 12 * U, bDone ? FLinearColor(0.3f, 0.9f, 0.4f) : FLinearColor(0.35f, 0.75f, 1.f));
			Text(FString::Printf(TEXT("%d / %d"), Sv->Challenge[i], C.Goal), PX + PW * 0.6f + 20 * U, Y + 40 * U, White, M, 1.f);
			Text(bDone ? FString(TEXT("COMPLETE")) : FString::Printf(TEXT("+%d XP"), C.Reward), PX + PW - 70 * U, Y + 26 * U, bDone ? FLinearColor(0.4f, 1.f, 0.5f) : Accent, M, 1.1f, true);
			Y += 80 * U;
		}
	}
	else if (Tab == 3) // ---------------- locker
	{
		const float PX = 70 * U, PY = 112 * U, PW = W * 0.42f, PH = H - 112 * U - 60 * U;
		Box(PX, PY, PW, PH, Panel);
		if (!GM->bSelecting)
		{
			Box(PX, PY, PW, 40 * U, Header);
			Text(TEXT("LOCKER"), PX + 16 * U, PY + 8 * U, White, M, 1.2f);
			const int32 Sel = FMath::Max(GM->LockerSlot, 0);
			const ESDCos SelType = Sel < 4 ? (ESDCos)Sel : ESDCos::Emote;
			const FSDCosmetic* Cur = SDCos::Find(Sv->Equipped(SelType, Sel - 4));
			// selected item
			Card(NAME_None, PX + 20 * U, PY + 56 * U, 150 * U, 170 * U, Cur, false, FString(), false, GM);
			Text(SDCos::TypeName(SelType), PX + 190 * U, PY + 66 * U, FLinearColor(1, 1, 1, 0.7f), M, 1.f);
			Text(Cur ? FString(Cur->Name).ToUpper() : FString(TEXT("NONE")), PX + 190 * U, PY + 94 * U, White, L, 1.6f);
			if (Cur) Text(Cur->Desc, PX + 190 * U, PY + 150 * U, FLinearColor(1, 1, 1, 0.8f), M, 1.f);
			// account slots
			float Y = PY + 250 * U;
			Text(TEXT("ACCOUNT"), PX + 20 * U, Y, FLinearColor(1, 1, 1, 0.75f), M, 1.f);
			DrawLine(PX + 130 * U, Y + 12 * U, PX + PW - 20 * U, Y + 12 * U, FLinearColor(1, 1, 1, 0.3f), 1.f);
			Y += 30 * U;
			const float CW = 100 * U, CH = 118 * U;
			for (int32 s = 0; s < 4; ++s)
			{
				Card(FName(*FString::Printf(TEXT("slot%d"), s)), PX + 20 * U + s * (CW + 12 * U), Y, CW, CH,
					SDCos::Find(Sv->Equipped((ESDCos)s)), GM->LockerSlot == s, FString(), false, GM);
			}
			Y += CH + 24 * U;
			Text(TEXT("EMOTES"), PX + 20 * U, Y, FLinearColor(1, 1, 1, 0.75f), M, 1.f);
			DrawLine(PX + 130 * U, Y + 12 * U, PX + PW - 20 * U, Y + 12 * U, FLinearColor(1, 1, 1, 0.3f), 1.f);
			Y += 30 * U;
			for (int32 e = 0; e < 6; ++e)
			{
				Card(FName(*FString::Printf(TEXT("slot%d"), 4 + e)), PX + 20 * U + e * (84 * U + 10 * U), Y, 84 * U, 100 * U,
					SDCos::Find(Sv->Equipped(ESDCos::Emote, e)), GM->LockerSlot == 4 + e, FString(), false, GM);
			}
			Text(TEXT("Click a slot to change it"), PX + 20 * U, PY + PH - 34 * U, FLinearColor(1, 1, 1, 0.55f), S, 1.f);
			if (Cur) InfoBox(W - 470 * U, H - 180 * U, 440 * U, Cur);
		}
		else
		{
			const int32 Slot = GM->LockerSlot;
			const ESDCos Type = Slot < 4 ? (ESDCos)Slot : ESDCos::Emote;
			Box(PX, PY, PW, 46 * U, Header);
			Text(TEXT("SELECTING"), PX + 16 * U, PY + 9 * U, White, L, 1.3f);
			Box(PX + PW - 230 * U, PY + 8 * U, 210 * U, 30 * U, FLinearColor(0.1f, 0.55f, 1.f, 1.f));
			Text(SDCos::TypeName(Type), PX + PW - 125 * U, PY + 11 * U, White, M, 1.f, true);
			TArray<const FSDCosmetic*> Items;
			if (Type == ESDCos::BackBling || Type == ESDCos::Emote) Items.Add(nullptr); // "none"
			for (const FSDCosmetic* C : SDCos::OfType(Type)) if (Sv->Owns(C->Id)) Items.Add(C);
			const float CW = 92 * U, CH = 92 * U, Gap = 10 * U;
			const int32 Cols = FMath::Max(1, FMath::FloorToInt((PW - 40 * U) / (CW + Gap)));
			const FName Cur = Sv->Equipped(Type, Slot - 4);
			for (int32 i = 0; i < Items.Num(); ++i)
			{
				const int32 Cx = i % Cols, Cy = i / Cols;
				const FSDCosmetic* C = Items[i];
				const FName Id = C ? C->Id : NAME_None;
				Card(FName(*FString::Printf(TEXT("pick:%s"), *Id.ToString())), PX + 20 * U + Cx * (CW + Gap), PY + 64 * U + Cy * (CH + Gap), CW, CH,
					C, Id == Cur, FString(), false, GM);
			}
			Text(FString::Printf(TEXT("%d owned  |  more in the Item Shop and Battle Pass"), Items.Num() - ((Type == ESDCos::BackBling || Type == ESDCos::Emote) ? 1 : 0)),
				PX + 20 * U, PY + PH - 100 * U, FLinearColor(1, 1, 1, 0.55f), S, 1.f);
			Button(TEXT("saveexit"), PX + PW * 0.5f - 130 * U, PY + PH - 64 * U, 260 * U, 44 * U, TEXT("SAVE AND EXIT"), FLinearColor(0.1f, 0.55f, 1.f, 1.f), White, M, 1.1f);
			if (const FSDCosmetic* C = SDCos::Find(Cur)) InfoBox(W - 470 * U, H - 180 * U, 440 * U, C);
		}
	}
	else if (Tab == 4 && !GM->ShopView.IsNone()) // ---------------- shop item page (3D preview on the right)
	{
		const FSDCosmetic* C = SDCos::Find(GM->ShopView);
		if (C)
		{
			const FLinearColor Rc = SDData::RarityColor(C->Rarity);
			const float PX = 60 * U, PY = 140 * U, PW = 600 * U;
			Grad(PX, PY, PW, 150 * U, Rc * 0.95f + FLinearColor(0, 0, 0, 1), Rc * 0.55f + FLinearColor(0, 0, 0, 1));
			Txt(FString::Printf(TEXT("%s  |  %s"), SDData::RarityName(C->Rarity), SDCos::TypeName(C->Type)), PX + 20 * U, PY + 14 * U, 17.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.92f));
			Txt(FString(C->Name).ToUpper(), PX + 20 * U, PY + 42 * U, 44.f, TEXT("BlackItalic"), FLinearColor::White, 0, 3.f);
			Txt(C->Desc, PX + 22 * U, PY + 112 * U, 16.f, TEXT("Medium"), FLinearColor(1, 1, 1, 0.9f));
			float Y = PY + 160 * U;
			Box(PX, Y, PW, 92 * U, FLinearColor(0.03f, 0.06f, 0.16f, 0.92f));
			const bool bOwned = Sv->Owns(C->Id);
			if (bOwned)
			{
				Txt(TEXT("OWNED"), PX + 24 * U, Y + 24 * U, 30.f, TEXT("BlackItalic"), FLinearColor(0.5f, 1.f, 0.6f));
				Txt(TEXT("Equip it from the Locker"), PX + PW - 24 * U, Y + 34 * U, 15.f, TEXT("Medium"), FLinearColor(1, 1, 1, 0.7f), 2);
			}
			else
			{
				CoinIcon(PX + 40 * U, Y + 46 * U, 16 * U);
				Txt(FText::AsNumber(C->Price).ToString(), PX + 66 * U, Y + 24 * U, 30.f, TEXT("BlackItalic"), FLinearColor::White);
				const bool bHov = Hot(TEXT("shopbuy"), PX + PW - 260 * U, Y + 18 * U, 236 * U, 56 * U);
				Box(PX + PW - 260 * U, Y + 18 * U, 236 * U, 56 * U, bHov ? FLinearColor(1.f, 0.96f, 0.5f, 1.f) : Accent);
				Txt(TEXT("PURCHASE"), PX + PW - 142 * U, Y + 30 * U, 22.f, TEXT("BlackItalic"), Dark, 1, 0.f);
			}
			Y += 108 * U;
			const bool bBack = Hot(TEXT("shopback"), PX, Y, 200 * U, 50 * U);
			Box(PX, Y, 200 * U, 50 * U, bBack ? FLinearColor(0.25f, 0.5f, 0.92f, 1.f) : FLinearColor(0.12f, 0.26f, 0.55f, 1.f));
			Frame(PX, Y, 200 * U, 50 * U, FLinearColor(1, 1, 1, 0.5f), 1.f * U);
			Txt(TEXT("Back"), PX + 100 * U, Y + 12 * U, 20.f, TEXT("BlackItalic"), FLinearColor::White, 1);
			const TCHAR* How = C->Type == ESDCos::BackBling ? TEXT("Shown on your outfit, turned to the back")
				: C->Type == ESDCos::Pickaxe || C->Type == ESDCos::Glider ? TEXT("Inspect: the item spins on the stage")
				: C->Type == ESDCos::Emote ? TEXT("Your outfit is performing the emote") : TEXT("Shown on the stage");
			Txt(How, PX, Y + 70 * U, 14.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.65f));
		}
	}
	else if (Tab == 4) // ---------------- item shop
	{
		Box(0, 92 * U, W, H - 92 * U, FLinearColor(0.08f, 0.04f, 0.14f, 0.93f));
		TArray<FName> Featured, Daily;
		SDProfile::ShopToday(Featured, Daily);
		const FTimespan Left = SDProfile::TimeToReset();
		const FString Timer = FString::Printf(TEXT("%d Hours"), FMath::Max(1, (int32)Left.GetTotalHours()));
		const float FX = 50 * U, FY = 150 * U, FW = W * 0.55f;
		Box(FX, FY, FW, 40 * U, FLinearColor(0.25f, 0.3f, 0.45f, 0.95f));
		Text(TEXT("FEATURED ITEMS"), FX + 14 * U, FY + 7 * U, White, M, 1.25f);
		Text(Timer, FX + FW - 100 * U, FY + 9 * U, White, M, 1.05f);
		const float DX = FX + FW + 40 * U, DW = W - DX - 50 * U;
		Box(DX, FY, DW, 40 * U, FLinearColor(0.25f, 0.3f, 0.45f, 0.95f));
		Text(TEXT("DAILY ITEMS"), DX + 14 * U, FY + 7 * U, White, M, 1.25f);
		Text(Timer, DX + DW - 100 * U, FY + 9 * U, White, M, 1.05f);
		auto ShopCard = [&](FName Id, float X, float Y, float CW, float CH, bool bBig)
		{
			const FSDCosmetic* C = SDCos::Find(Id);
			if (!C) return;
			const bool bOwned = Sv->Owns(Id);
			Card(FName(*FString::Printf(TEXT("shop:%s"), *Id.ToString())), X, Y, CW, CH, C, false, FString(), false, GM);
			const float BarH = bBig ? 58 * U : 44 * U;
			Box(X, Y + CH - BarH - 64 * U, CW, 64 * U, FLinearColor(0, 0, 0, 0.35f));
			Text(FString(C->Name).ToUpper(), X + CW * 0.5f, Y + CH - BarH - 58 * U, White, L, bBig ? 1.35f : 1.0f, true);
			Text(SDCos::TypeName(C->Type), X + CW * 0.5f, Y + CH - BarH - 22 * U, FLinearColor(1, 1, 1, 0.75f), S, 1.f, true);
			Box(X, Y + CH - BarH, CW, BarH, FLinearColor(0.02f, 0.04f, 0.12f, 0.95f));
			if (bOwned)
			{
				Text(TEXT("OWNED"), X + CW * 0.5f, Y + CH - BarH + (BarH - 22 * U) * 0.5f, FLinearColor(0.5f, 1.f, 0.6f), M, 1.1f, true);
			}
			else
			{
				const FString P = FText::AsNumber(C->Price).ToString();
				CoinIcon(X + CW * 0.5f - 44 * U, Y + CH - BarH * 0.5f, (bBig ? 13.f : 10.f) * U);
				Text(P, X + CW * 0.5f + 12 * U, Y + CH - BarH + (BarH - 24 * U) * 0.5f, White, M, bBig ? 1.35f : 1.1f, true);
			}
			if (GM->ShopNewId == Id) { Box(X, Y, 70 * U, 26 * U, FLinearColor(0.9f, 0.2f, 0.3f, 1.f)); Text(TEXT("NEW!"), X + 35 * U, Y + 3 * U, White, S, 1.1f, true); }
		};
		const float BigW = (FW - 2 * 16 * U) / 3.f, BigH = H - FY - 160 * U;
		for (int32 i = 0; i < Featured.Num(); ++i) ShopCard(Featured[i], FX + i * (BigW + 16 * U), FY + 56 * U, BigW, BigH, true);
		const float SmW = (DW - 2 * 14 * U) / 3.f, SmH = (BigH - 14 * U) / 2.f;
		for (int32 i = 0; i < Daily.Num(); ++i) ShopCard(Daily[i], DX + (i % 3) * (SmW + 14 * U), FY + 56 * U + (i / 3) * (SmH + 14 * U), SmW, SmH, false);
		Text(TEXT("Note: these items are cosmetic only and grant no competitive advantage. Coins are earned by playing."), W * 0.5f, H - 86 * U, FLinearColor(1, 1, 1, 0.6f), S, 1.05f, true);
	}

	// ---------------- purchase popup
	if (!GM->PendingBuy.IsNone())
	{
		Box(0, 0, W, H, FLinearColor(0, 0, 0, 0.6f));
		const float BW = 560 * U, BH = 250 * U, BX = W * 0.5f - BW * 0.5f, BYp = H * 0.5f - BH * 0.5f;
		Box(BX, BYp, BW, BH, FLinearColor(0.04f, 0.08f, 0.2f, 1.f));
		Frame(BX, BYp, BW, BH, FLinearColor(1, 1, 1, 0.6f), 2.f);
		const bool bPass = GM->PendingBuy == TEXT("pass");
		const FSDCosmetic* C = bPass ? nullptr : SDCos::Find(GM->PendingBuy);
		const int32 Price = bPass ? SDCos::PassPrice : (C ? C->Price : 0);
		Text(TEXT("CONFIRM PURCHASE"), W * 0.5f, BYp + 18 * U, White, L, 1.3f, true);
		Text(bPass ? FString(TEXT("Season 1 Battle Pass")) : FString(C ? C->Name : TEXT("")), W * 0.5f, BYp + 72 * U, Accent, M, 1.3f, true);
		CoinIcon(W * 0.5f - 46 * U, BYp + 126 * U, 12 * U);
		Text(FText::AsNumber(Price).ToString(), W * 0.5f + 10 * U, BYp + 112 * U, White, M, 1.3f, true);
		const bool bAfford = Sv->Coins >= Price;
		if (bAfford) Button(TEXT("confirm"), BX + 40 * U, BYp + BH - 70 * U, 220 * U, 48 * U, TEXT("PURCHASE"), Accent, Dark, M, 1.2f);
		else Text(TEXT("Not enough coins. Play matches to earn more!"), W * 0.5f, BYp + 150 * U, FLinearColor(1.f, 0.5f, 0.5f), S, 1.1f, true);
		Button(TEXT("cancel"), BX + BW - 260 * U, BYp + BH - 70 * U, 220 * U, 48 * U, TEXT("CANCEL"), FLinearColor(0.2f, 0.25f, 0.4f, 1.f), White, M, 1.2f);
	}
	if (GM->Toast.Len() > 0 && GM->ToastTime > 0.f)
	{
		Box(W * 0.5f - 300 * U, H - 140 * U, 600 * U, 44 * U, FLinearColor(0.1f, 0.5f, 0.25f, 0.95f));
		Text(GM->Toast, W * 0.5f, H - 132 * U, White, M, 1.1f, true);
	}

	LobbyBottomBar(GM);
	DrawFPS(Cast<ASDPlayerController>(GetOwningPlayerController()));
}

void ASDHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	U = Canvas->SizeY / 1080.f;
	Buttons.Reset();
	ASDGameMode* GM = GetWorld()->GetAuthGameMode<ASDGameMode>();
	if (GM && (GM->Phase == ESDPhase::Lobby || GM->Phase == ESDPhase::Matchmaking))
	{
		DrawLobby(GM);
		return;
	}
	ASDPlayerController* PC = Cast<ASDPlayerController>(GetOwningPlayerController());
	UFont* Large = GEngine->GetLargeFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float Now = GetWorld()->GetTimeSeconds();

	if (!GM || GM->Phase == ESDPhase::Loading)
	{
		Box(0, 0, W, H, FLinearColor(0.05f, 0.08f, 0.15f, 1.f));
		Text(TEXT("SKYDROP"), W * 0.5f, H * 0.4f, White, Large, 3.f, true);
		Text(TEXT("Generating island..."), W * 0.5f, H * 0.4f + 90 * U, FLinearColor(1, 1, 1, 0.7f), Med, 1.2f, true);
		return;
	}
	ASDCharacter* C = PC ? Cast<ASDCharacter>(PC->GetPawn()) : nullptr;

	// screen effects
	if (C && !C->bDead && GM->Storm && C->Air != ESDAir::Bus && !GM->Storm->IsInside(C->GetActorLocation()))
	{
		Box(0, 0, W, H, FLinearColor(0.4f, 0.05f, 0.6f, 0.28f));
	}
	if (C && C->DamagedFlash > 0.f)
	{
		const float A = C->DamagedFlash * 0.9f;
		const float E = 60 * U;
		Box(0, 0, W, E, FLinearColor(0.8f, 0.f, 0.f, A * 0.5f));
		Box(0, H - E, W, E, FLinearColor(0.8f, 0.f, 0.f, A * 0.5f));
		Box(0, 0, E, H, FLinearColor(0.8f, 0.f, 0.f, A * 0.5f));
		Box(W - E, 0, E, H, FLinearColor(0.8f, 0.f, 0.f, A * 0.5f));
	}

	if (C && !C->bDead)
	{
		// damage / harvest numbers
		for (const FSDDamageNumber& N : C->DamageNumbers)
		{
			const float Age = Now - N.Time;
			const FVector P = Project(N.Pos, true);
			if (P.Z <= 0.f) continue;
			FLinearColor Col = N.Color;
			Col.A = FMath::Clamp(1.2f - Age, 0.f, 1.f);
			Text(N.Text, P.X, P.Y - Age * 70.f * U, Col, Large, 1.0f + FMath::Max(0.f, 0.25f - Age) * 2.f, true);
		}
		if (C->ScopeT > 0.f) DrawScope(C);
		else if (C->Air == ESDAir::Ground) DrawCrosshair(C);
		DrawVitals(C);
		DrawHotbar(C);

		// aimed structure health
		if (ASDBuildPiece* P = C->AimedPiece(1500.f))
		{
			const float BW = 90 * U;
			Box(W * 0.5f - BW * 0.5f, H * 0.5f + 28 * U, BW, 6 * U, Dim);
			Box(W * 0.5f - BW * 0.5f, H * 0.5f + 28 * U, BW * P->HealthFraction(), 6 * U, FLinearColor(0.9f, 0.9f, 0.9f, 0.9f));
			Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(P->HP)), W * 0.5f, H * 0.5f + 36 * U, FLinearColor(1, 1, 1, 0.8f), Small, 1.f, true);
		}

		// consumable progress
		if (C->UseLeft > 0.f)
		{
			if (const FSDItemStack* It = C->ActiveItem())
			{
				const float Tot = SDData::UseTime(It->Item);
				Box(W * 0.5f - 100 * U, H * 0.62f, 200 * U, 10 * U, Dim);
				Box(W * 0.5f - 100 * U, H * 0.62f, 200 * U * FMath::Clamp(C->UseLeft / Tot, 0.f, 1.f), 10 * U, FLinearColor(0.4f, 0.8f, 1.f));
				Text(FString::Printf(TEXT("Using %s  %.1f"), *SDData::ItemName(*It), FMath::Max(0.f, Tot - C->UseLeft)), W * 0.5f, H * 0.62f - 30 * U, White, Med, 1.f, true);
			}
		}

		// prompts
		FString Prompt;
		if (C->Air == ESDAir::Bus && GM->Bus)
		{
			Prompt = GM->Bus->DoorsOpen() ? TEXT("[SPACE] Jump from the airship") : FString::Printf(TEXT("Doors open in %d"), FMath::CeilToInt(-GM->Bus->Time));
		}
		else if (C->Air == ESDAir::Skydive)
		{
			Prompt = FString::Printf(TEXT("[SPACE] Open glider     Altitude %dm"), FMath::RoundToInt(C->GroundDistance() / 100.f));
		}
		else
		{
			Prompt = C->InteractPrompt();
		}
		if (!Prompt.IsEmpty()) Text(Prompt, W * 0.5f, H * 0.68f, White, Med, 1.2f, true);
	}

	// minimap + status
	const float MapS = 230 * U;
	const float MapX = W - MapS - 24 * U, MapY = 24 * U;
	if (USDSettings::Get()->bShowMinimap) DrawMap(GM, C, MapX, MapY, MapS, (C && C->Air == ESDAir::Bus) ? 0.f : 22000.f, false);
	if (C && !C->bDead) DrawCompass(C);
	int32 AliveN = GM->Alive.Num();
	FString Status = FString::Printf(TEXT("Alive %d     Kills %d"), AliveN, C ? C->Kills : 0);
	Text(Status, MapX + MapS * 0.5f, MapY + MapS + 10 * U, White, Med, 1.f, true);
	if (ASDStorm* S = GM->Storm)
	{
		FString St;
		if (!S->bActive) St = TEXT("Storm forms after the drop");
		else if (S->Phases.IsValidIndex(S->Index)) St = S->bShrinking ? FString::Printf(TEXT("Storm closing  %s"), *Clock(S->Timer)) : FString::Printf(TEXT("Storm shrinks in  %s"), *Clock(S->Timer));
		else St = TEXT("Final circle");
		Text(St, MapX + MapS * 0.5f, MapY + MapS + 38 * U, FLinearColor(0.85f, 0.6f, 1.f), Med, 1.f, true);
	}

	// kill feed
	float FY = H * 0.35f;
	for (const FSDFeed& F : GM->Feed)
	{
		Text(F.Text, 30 * U, FY, F.bPlayer ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(1, 1, 1, 0.85f), Small, 1.2f);
		FY += 24 * U;
	}

	// help
	if (USDSettings::Get()->bShowHelp && !(PC && PC->bInventory))
	{
		auto K = [this](ESDAction A) { return KeyLabel((int32)A); };
		const FString Lines[] = {
			FString::Printf(TEXT("%s%s%s%s move   %s jump   %s crouch"), *K(ESDAction::Forward), *K(ESDAction::Left), *K(ESDAction::Back), *K(ESDAction::Right), *K(ESDAction::Jump), *K(ESDAction::Crouch)),
			FString::Printf(TEXT("%s fire / place   %s aim   %s reload"), *K(ESDAction::Fire), *K(ESDAction::Aim), *K(ESDAction::Reload)),
			FString::Printf(TEXT("%s pickaxe   %s-%s slots   %s inventory"), *K(ESDAction::Pickaxe), *K(ESDAction::Slot1), *K(ESDAction::Slot5), *K(ESDAction::Inventory)),
			FString::Printf(TEXT("%s build   %s %s %s %s pieces   %s edit (drag %s, %s reset)"), *K(ESDAction::Build), *K(ESDAction::Wall), *K(ESDAction::Floor), *K(ESDAction::Ramp), *K(ESDAction::Cone), *K(ESDAction::Edit), *K(ESDAction::Fire), *K(ESDAction::Aim)),
			FString::Printf(TEXT("%s use   %s map   %s emote"), *K(ESDAction::Interact), *K(ESDAction::Map), *K(ESDAction::Emote)),
			FString::Printf(TEXT("ESC menu & settings   %s hide help"), *K(ESDAction::Help)),
		};
		float HY = 24 * U;
		Box(20 * U, 18 * U, 420 * U, 6 * 22 * U + 14 * U, FLinearColor(0, 0, 0, 0.35f));
		for (const FString& L : Lines)
		{
			Text(L, 30 * U, HY, FLinearColor(1, 1, 1, 0.85f), Small, 1.05f);
			HY += 22 * U;
		}
	}
	if (PC && PC->bBigMap) DrawBigMap(GM, C, PC);

	if (C && C->IsEditing()) DrawEditOverlay(C);
	if (GM->bPlayerOut || GM->bPlayerWon) DrawEnd(GM);
	if (PC && PC->bInventory && C && !C->bDead) DrawInventory(GM, C, PC);
	if (GM->bMenuOpen) DrawPauseMenu(GM);
	DrawFPS(PC);
}

// ------------------------------------------------------------------ settings screen

void ASDHUD::SliderHot(FName Id, float X, float Y, float W, float H)
{
	FSDButton B; B.Min = FVector2D(X, Y); B.Max = FVector2D(X + W, Y + H); B.Id = Id; B.bSlider = true;
	Buttons.Add(B);
}

FString ASDHUD::KeyLabel(int32 Action) const
{
	const FKey K = USDSettings::Get()->Key((ESDAction)Action);
	FString S = K.GetDisplayName(false).ToString();
	return S.IsEmpty() ? FString(TEXT("-")) : S;
}

void ASDHUD::DrawSettings(ASDGameMode* GM, float X, float Y, float W, float H, bool bBack)
{
	USDSettings* St = USDSettings::Get();
	const FLinearColor Panel(0.03f, 0.07f, 0.18f, 0.9f);
	const FLinearColor HeaderC(0.16f, 0.3f, 0.58f, 0.95f);
	const FLinearColor Yellow(1.f, 0.9f, 0.16f, 1.f);
	const FLinearColor Ink(0.03f, 0.07f, 0.2f, 1.f);
	const FLinearColor Soft(0.75f, 0.83f, 1.f, 0.9f);
	const FLinearColor Idle(0.12f, 0.26f, 0.55f, 1.f);
	const FLinearColor HoverC(0.25f, 0.5f, 0.92f, 1.f);

	// section column
	static const TCHAR* Sections[] = { TEXT("VIDEO"), TEXT("GAME"), TEXT("AUDIO"), TEXT("CONTROLS") };
	const float SW = 240 * U;
	Slant(X, Y, SW - 14 * U, 40 * U, 14 * U, HeaderC);
	Txt(TEXT("SETTINGS"), X + 14 * U, Y + 7 * U, 20.f, TEXT("BlackItalic"), FLinearColor::White);
	for (int32 i = 0; i < 4; ++i)
	{
		const float BY = Y + 52 * U + i * 54 * U;
		const bool bSel = GM->SettingsTab == i;
		const bool bHov = Hot(FName(*FString::Printf(TEXT("stab:%d"), i)), X, BY, SW, 46 * U);
		Box(X, BY, SW, 46 * U, bSel ? Yellow : bHov ? HoverC : FLinearColor(0.05f, 0.11f, 0.26f, 0.92f));
		Txt(Sections[i], X + 18 * U, BY + 11 * U, 18.f, TEXT("BlackItalic"), bSel ? Ink : FLinearColor::White, 0, bSel ? 0.f : 2.f);
	}
	{
		const float BY = Y + H - (bBack ? 108 : 50) * U;
		const bool bHov = Hot(TEXT("sreset"), X, BY, SW, 44 * U);
		Box(X, BY, SW, 44 * U, bHov ? HoverC : Idle);
		Frame(X, BY, SW, 44 * U, FLinearColor(1, 1, 1, 0.5f), 1.f * U);
		Txt(TEXT("Reset to defaults"), X + SW * 0.5f, BY + 11 * U, 16.f, TEXT("Bold"), FLinearColor::White, 1);
		if (bBack)
		{
			const float BY2 = Y + H - 50 * U;
			const bool bH2 = Hot(TEXT("mback"), X, BY2, SW, 44 * U);
			Box(X, BY2, SW, 44 * U, bH2 ? FLinearColor(1.f, 0.95f, 0.5f, 1.f) : Yellow);
			Txt(TEXT("Back"), X + SW * 0.5f, BY2 + 10 * U, 18.f, TEXT("BlackItalic"), Ink, 1, 0.f);
		}
	}

	// content
	const float CX = X + SW + 20 * U, CW = W - SW - 20 * U;
	Box(CX, Y, CW, H, Panel);
	float MX = -1.f, MY = -1.f;
	if (PlayerOwner) PlayerOwner->GetMousePosition(MX, MY);

	if (GM->SettingsTab < 3)
	{
		float RY = Y + 16 * U;
		const float RH = 46 * U;
		const float CtrlW = 380 * U, CtrlX = CX + CW - CtrlW - 24 * U;
		for (const FSDOption& O : USDSettings::Options())
		{
			if (O.Section != GM->SettingsTab) continue;
			const bool bRowHover = MY >= RY && MY <= RY + RH && MX >= CX && MX <= CX + CW;
			Box(CX + 12 * U, RY, CW - 24 * U, RH, bRowHover ? FLinearColor(0.12f, 0.24f, 0.5f, 0.6f) : FLinearColor(0.06f, 0.12f, 0.28f, 0.55f));
			Txt(O.Label, CX + 30 * U, RY + 12 * U, 17.f, TEXT("Bold"), FLinearColor::White);
			const FString Val = O.ValueText(St);
			if (!O.bSlider)
			{
				const FString Base = FString::Printf(TEXT("set:%s:"), *O.Id.ToString());
				const bool bL = Hot(FName(*(Base + TEXT("-1"))), CtrlX, RY + 6 * U, 40 * U, RH - 12 * U);
				Box(CtrlX, RY + 6 * U, 40 * U, RH - 12 * U, bL ? HoverC : Idle);
				Txt(TEXT("<"), CtrlX + 20 * U, RY + 10 * U, 17.f, TEXT("Black"), FLinearColor::White, 1, 0.f);
				const bool bR = Hot(FName(*(Base + TEXT("1"))), CtrlX + CtrlW - 40 * U, RY + 6 * U, 40 * U, RH - 12 * U);
				Box(CtrlX + CtrlW - 40 * U, RY + 6 * U, 40 * U, RH - 12 * U, bR ? HoverC : Idle);
				Txt(TEXT(">"), CtrlX + CtrlW - 20 * U, RY + 10 * U, 17.f, TEXT("Black"), FLinearColor::White, 1, 0.f);
				const bool bOn = O.Choices.Num() == 2 && O.Choices[1] == TEXT("On") && O.Get(St) > 0.5f;
				Txt(Val, CtrlX + CtrlW * 0.5f, RY + 11 * U, 17.f, TEXT("Bold"), bOn ? Yellow : FLinearColor::White, 1);
			}
			else
			{
				const float BX = CtrlX, BW = CtrlW - 90 * U, BYc = RY + RH * 0.5f;
				const float Fr = FMath::Clamp((O.Get(St) - O.Min) / FMath::Max(O.Max - O.Min, 0.0001f), 0.f, 1.f);
				SliderHot(FName(*FString::Printf(TEXT("slide:%s"), *O.Id.ToString())), BX, RY + 4 * U, BW, RH - 8 * U);
				Box(BX, BYc - 4 * U, BW, 8 * U, FLinearColor(0.f, 0.f, 0.f, 0.5f));
				Box(BX, BYc - 4 * U, BW * Fr, 8 * U, Yellow);
				Box(BX + BW * Fr - 7 * U, BYc - 13 * U, 14 * U, 26 * U, FLinearColor::White);
				Txt(Val, CtrlX + CtrlW, RY + 11 * U, 17.f, TEXT("Bold"), FLinearColor::White, 2);
			}
			RY += RH + 6 * U;
		}
		if (GM->SettingsTab == 0)
		{
			Txt(TEXT("Window and resolution changes apply straight away. Lower presets turn off Lumen lighting for speed."),
				CX + 24 * U, Y + H - 34 * U, 13.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.55f));
		}
	}
	else
	{
		// key bindings in two columns
		const int32 N = (int32)ESDAction::Count;
		const int32 PerCol = (N + 1) / 2;
		const float ColW = (CW - 60 * U) * 0.5f;
		const float RH = 40 * U;
		for (int32 i = 0; i < N; ++i)
		{
			const float RX = CX + 20 * U + (i / PerCol) * (ColW + 20 * U);
			const float RY = Y + 16 * U + (i % PerCol) * (RH + 5 * U);
			Box(RX, RY, ColW, RH, FLinearColor(0.06f, 0.12f, 0.28f, 0.55f));
			Txt(USDSettings::ActionName((ESDAction)i), RX + 14 * U, RY + 9 * U, 16.f, TEXT("Bold"), FLinearColor::White);
			const float KW = 190 * U, KX = RX + ColW - KW - 6 * U;
			const bool bWait = GM->BindingAction == i;
			const bool bHov = Hot(FName(*FString::Printf(TEXT("bind:%d"), i)), KX, RY + 4 * U, KW, RH - 8 * U);
			Box(KX, RY + 4 * U, KW, RH - 8 * U, bWait ? Yellow : bHov ? HoverC : Idle);
			Frame(KX, RY + 4 * U, KW, RH - 8 * U, FLinearColor(1, 1, 1, bWait ? 0.9f : 0.35f), 1.f * U);
			Txt(bWait ? FString(TEXT("Press a key...")) : KeyLabel(i), KX + KW * 0.5f, RY + 9 * U, 15.f, TEXT("Bold"), bWait ? Ink : FLinearColor::White, 1, bWait ? 0.f : 1.5f);
		}
		Txt(TEXT("Click a key, then press the new one. ESC cancels. Keys already in use swap with the old one."),
			CX + 24 * U, Y + H - 34 * U, 13.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.55f));
	}
}

// ------------------------------------------------------------------ in-match pause menu

void ASDHUD::DrawPauseMenu(ASDGameMode* GM)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	Box(0, 0, W, H, FLinearColor(0.01f, 0.02f, 0.06f, 0.72f));
	if (GM->MenuView == 1)
	{
		DrawSettings(GM, 80 * U, 60 * U, W - 160 * U, H - 120 * U, true);
		return;
	}
	const float PW = 420 * U, PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - 170 * U;
	Slant(PX, PY, PW - 16 * U, 50 * U, 16 * U, FLinearColor(0.16f, 0.3f, 0.58f, 0.95f));
	Txt(TEXT("PAUSED"), PX + 18 * U, PY + 8 * U, 26.f, TEXT("BlackItalic"), FLinearColor::White);
	Box(PX, PY + 50 * U, PW, 270 * U, FLinearColor(0.03f, 0.07f, 0.18f, 0.94f));
	struct FB { const TCHAR* Id; const TCHAR* Label; FLinearColor Fill; FLinearColor TextC; };
	const FB Bs[] = {
		{ TEXT("resume"), TEXT("Resume"), FLinearColor(1.f, 0.9f, 0.16f, 1.f), FLinearColor(0.03f, 0.07f, 0.2f, 1.f) },
		{ TEXT("msettings"), TEXT("Settings"), FLinearColor(0.16f, 0.36f, 0.72f, 1.f), FLinearColor::White },
		{ TEXT("leave"), TEXT("Leave match"), FLinearColor(0.7f, 0.18f, 0.2f, 1.f), FLinearColor::White } };
	for (int32 i = 0; i < 3; ++i)
	{
		const float BY = PY + 72 * U + i * 80 * U;
		const bool bHov = Hot(Bs[i].Id, PX + 24 * U, BY, PW - 48 * U, 62 * U);
		FLinearColor F = Bs[i].Fill;
		if (bHov) F = FLinearColor(FMath::Min(F.R * 1.15f + 0.06f, 1.f), FMath::Min(F.G * 1.15f + 0.06f, 1.f), FMath::Min(F.B * 1.15f + 0.06f, 1.f), 1.f);
		Box(PX + 24 * U, BY, PW - 48 * U, 62 * U, F);
		if (bHov) Frame(PX + 24 * U, BY, PW - 48 * U, 62 * U, FLinearColor::White, 2.f * U);
		Txt(Bs[i].Label, W * 0.5f, BY + 13 * U, 24.f, TEXT("BlackItalic"), Bs[i].TextC, 1, i == 0 ? 0.f : 2.f);
	}
	Txt(TEXT("The match is paused while this menu is open."), W * 0.5f, PY + 336 * U, 14.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.6f), 1);
}

// ------------------------------------------------------------------ inventory

void ASDHUD::DrawInventory(ASDGameMode* GM, ASDCharacter* C, ASDPlayerController* PC)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const FLinearColor Panel(0.04f, 0.08f, 0.2f, 0.88f);
	const FLinearColor HeaderC(0.18f, 0.28f, 0.48f, 0.95f);
	const FLinearColor Yellow(1.f, 0.9f, 0.16f, 1.f);
	Grad(0, 0, W, H, FLinearColor(0.01f, 0.03f, 0.1f, 0.55f), FLinearColor(0.01f, 0.03f, 0.1f, 0.75f));

	auto Tile = [&](FName Id, float X, float Y, float S, const FLinearColor& Fill, const FString& Big, const FString& Count, bool bSel, const FString& Corner)
	{
		const bool bHov = Hot(Id, X, Y, S, S);
		Grad(X, Y, S, S, Fill * 1.15f + FLinearColor(0, 0, 0, 1), Fill * 0.7f + FLinearColor(0, 0, 0, 1));
		Txt(Big, X + S * 0.5f, Y + S * 0.3f, S > 100 * U ? 18.f : 15.f, TEXT("BlackItalic"), FLinearColor::White, 1);
		if (!Count.IsEmpty()) Txt(Count, X + S - 8 * U, Y + S - 26 * U, 15.f, TEXT("Bold"), FLinearColor::White, 2);
		if (!Corner.IsEmpty()) Txt(Corner, X + 8 * U, Y + S - 26 * U, 12.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.8f));
		Frame(X, Y, S, S, bSel ? Yellow : bHov ? FLinearColor::White : FLinearColor(1, 1, 1, 0.45f), bSel ? 4.f * U : 2.f * U);
	};
	auto Section = [&](const TCHAR* Title, float X, float Y, float SW, float SH)
	{
		Box(X, Y, SW, SH, Panel);
		Slant(X, Y, 280 * U, 38 * U, 26 * U, HeaderC);
		Txt(Title, X + 16 * U, Y + 7 * U, 19.f, TEXT("Bold"), FLinearColor(0.85f, 0.9f, 1.f));
	};

	// --- right column: resources, ammo, equipment
	const float RW = 720 * U, RX = W - RW - 50 * U;
	// panel areas (registered first so tiles sit on top): releasing a drag here does not drop
	Hot(TEXT("inv:panel"), RX, 90 * U, RW, 572 * U);
	Hot(TEXT("inv:panel"), 50 * U, 90 * U, 520 * U, 500 * U);
	Hot(TEXT("inv:panel"), 0, H - 52 * U, W, 52 * U);
	float RY = 90 * U;
	const float TS = 96 * U;
	static const FLinearColor MatCol[3] = { FLinearColor(0.55f, 0.36f, 0.18f), FLinearColor(0.6f, 0.25f, 0.18f), FLinearColor(0.45f, 0.5f, 0.56f) };
	static const TCHAR* MatShort[3] = { TEXT("WOOD"), TEXT("BRICK"), TEXT("METAL") };
	Section(TEXT("Resources"), RX, RY, RW, 170 * U);
	for (int32 i = 0; i < 3; ++i)
	{
		Tile(FName(*FString::Printf(TEXT("inv:mat:%d"), i)), RX + 20 * U + i * (TS + 12 * U), RY + 54 * U, TS, MatCol[i],
			MatShort[i], FString::FromInt(C->Mats[i]), PC->InvSel == 20 + i, TEXT(""));
	}
	RY += 186 * U;
	static const TCHAR* AmmoShort[4] = { TEXT("LIGHT"), TEXT("MEDIUM"), TEXT("HEAVY"), TEXT("SHELLS") };
	Section(TEXT("Ammo"), RX, RY, RW, 170 * U);
	for (int32 i = 0; i < (int32)ESDAmmo::Count; ++i)
	{
		Tile(FName(*FString::Printf(TEXT("inv:ammo:%d"), i)), RX + 20 * U + i * (TS + 12 * U), RY + 54 * U, TS, FLinearColor(0.42f, 0.44f, 0.48f),
			AmmoShort[i], FString::FromInt(C->Ammo[i]), PC->InvSel == 10 + i, TEXT(""));
	}
	RY += 186 * U;
	Section(TEXT("Equipment"), RX, RY, RW, 200 * U);
	const float ES = 122 * U;
	for (int32 i = 0; i < 5; ++i)
	{
		const FSDItemStack& S = C->Slots[i];
		FLinearColor Fill = FLinearColor(0.3f, 0.32f, 0.38f);
		FString Big = TEXT(""), Count = TEXT(""), Corner = TEXT("");
		if (!S.IsEmpty())
		{
			Fill = SDData::RarityColor(S.Rarity) * 0.75f;
			if (S.Item == ESDItem::Weapon)
			{
				static const TCHAR* Short[] = { TEXT(""), TEXT("AR"), TEXT("PUMP"), TEXT("SMG"), TEXT("PISTOL"), TEXT("SNIPER"), TEXT("TACTICAL") };
				Big = Short[FMath::Clamp((int32)S.Weapon, 0, 6)];
				Count = FString::FromInt(S.Count);
				Corner = FString::FromInt(C->Ammo[(int32)SDData::Weapon(S.Weapon).Ammo]);
			}
			else
			{
				Big = SDData::ItemName(S).ToUpper();
				Count = FString::FromInt(S.Count);
			}
		}
		const float TX = RX + 20 * U + i * (ES + 14 * U);
		Tile(FName(*FString::Printf(TEXT("inv:slot:%d"), i)), TX, RY + 56 * U, ES, Fill, Big, Count, PC->InvSel == i, Corner);
		Txt(FString::FromInt(i + 1), TX + 6 * U, RY + 58 * U, 12.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.7f));
		if (C->Active == i) Box(TX, RY + 56 * U + ES + 4 * U, ES, 4 * U, Yellow);
	}

	// --- left: details of the selected item
	const float LX = 50 * U, LW = 520 * U;
	float LY = 90 * U;
	FString Rar, Kind, Name;
	FLinearColor Rc(0.25f, 0.35f, 0.55f);
	TArray<TPair<FString, FString>> Rows;
	const int32 Sel = PC->InvSel;
	if (Sel >= 0 && Sel < 10 && !C->Slots[Sel].IsEmpty())
	{
		const FSDItemStack& S = C->Slots[Sel];
		Rc = SDData::RarityColor(S.Rarity);
		Rar = SDData::RarityName(S.Rarity);
		Name = SDData::ItemName(S).ToUpper();
		if (S.Item == ESDItem::Weapon)
		{
			const FSDWeaponDef& D = SDData::Weapon(S.Weapon);
			const float Dmg = SDData::WeaponDamage(S.Weapon, S.Rarity);
			Kind = TEXT("Ranged Weapon");
			Rows.Add({ TEXT("DPS"), FString::Printf(TEXT("%.1f"), Dmg * D.Pellets / FMath::Max(D.FireInterval, 0.01f)) });
			Rows.Add({ TEXT("Damage"), D.Pellets > 1 ? FString::Printf(TEXT("%.0f x %d"), Dmg, D.Pellets) : FString::Printf(TEXT("%.1f"), Dmg) });
			Rows.Add({ TEXT("Headshot Damage"), FString::Printf(TEXT("x%.1f"), D.HeadMult) });
			Rows.Add({ TEXT("Fire Rate"), FString::Printf(TEXT("%.2f"), 1.f / FMath::Max(D.FireInterval, 0.01f)) });
			Rows.Add({ TEXT("Magazine Size"), FString::FromInt(D.Mag) });
			Rows.Add({ TEXT("Loaded"), FString::FromInt(S.Count) });
			Rows.Add({ TEXT("Reload Time"), FString::Printf(TEXT("%.1f s"), D.Reload) });
			Rows.Add({ TEXT("Range"), FString::Printf(TEXT("%s m"), *FText::AsNumber(FMath::RoundToInt(D.Range / 100.f)).ToString()) });
			Rows.Add({ TEXT("Ammo"), FString::Printf(TEXT("%s (%d)"), SDData::AmmoName(D.Ammo), C->Ammo[(int32)D.Ammo]) });
			Rows.Add({ TEXT("Fire Mode"), D.bAuto ? TEXT("Automatic") : TEXT("Single") });
		}
		else
		{
			Kind = TEXT("Consumable");
			const TCHAR* Desc = S.Item == ESDItem::Bandage ? TEXT("Heals 15 health, up to 75")
				: S.Item == ESDItem::Medkit ? TEXT("Heals to full health")
				: S.Item == ESDItem::MiniShield ? TEXT("+25 shield, up to 50")
				: TEXT("+50 shield");
			Rows.Add({ TEXT("Effect"), Desc });
			Rows.Add({ TEXT("Stack"), FString::FromInt(S.Count) });
		}
	}
	else if (Sel >= 10 && Sel < 20)
	{
		Name = FString(SDData::AmmoName((ESDAmmo)(Sel - 10))).ToUpper();
		Kind = TEXT("Ammo"); Rar = TEXT("Common"); Rc = SDData::RarityColor(0);
		Rows.Add({ TEXT("Amount"), FString::FromInt(C->Ammo[Sel - 10]) });
	}
	else if (Sel >= 20)
	{
		Name = FString(SDData::MatName((ESDMat)(Sel - 20))).ToUpper();
		Kind = TEXT("Building Material"); Rar = TEXT("Common"); Rc = SDData::RarityColor(0);
		Rows.Add({ TEXT("Amount"), FString::Printf(TEXT("%d / 999"), C->Mats[Sel - 20]) });
	}
	if (!Name.IsEmpty())
	{
		Grad(LX, LY, LW, 104 * U, Rc * 0.9f + FLinearColor(0, 0, 0, 1), Rc * 0.6f + FLinearColor(0, 0, 0, 1));
		Txt(FString::Printf(TEXT("%s  |  %s"), *Rar, *Kind), LX + 16 * U, LY + 10 * U, 15.f, TEXT("Bold"), FLinearColor(1, 1, 1, 0.9f));
		Txt(Name, LX + 16 * U, LY + 34 * U, 30.f, TEXT("BlackItalic"), FLinearColor::White, 0, 3.f);
		LY += 104 * U;
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			Box(LX, LY, LW, 38 * U, i % 2 ? FLinearColor(0.08f, 0.13f, 0.27f, 0.9f) : FLinearColor(0.11f, 0.17f, 0.32f, 0.9f));
			Txt(Rows[i].Key, LX + 14 * U, LY + 9 * U, 15.f, TEXT("Medium"), FLinearColor(0.85f, 0.9f, 1.f));
			Txt(Rows[i].Value, LX + LW - 14 * U, LY + 9 * U, 15.f, TEXT("Bold"), FLinearColor::White, 2);
			LY += 38 * U;
		}
	}
	else
	{
		Box(LX, LY, LW, 80 * U, Panel);
		Txt(TEXT("Select an item"), LX + 16 * U, LY + 14 * U, 20.f, TEXT("BoldItalic"), FLinearColor::White);
		Txt(TEXT("Click a slot, then click another slot to swap them"), LX + 16 * U, LY + 46 * U, 14.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.7f));
	}

	// --- dragged item follows the cursor
	if (PC->DragFrom >= 0 && PC->bDragMoved)
	{
		float MX = 0.f, MY = 0.f;
		PC->GetMousePosition(MX, MY);
		Box(MX - 40 * U, MY - 40 * U, 80 * U, 80 * U, FLinearColor(0.2f, 0.45f, 0.9f, 0.6f));
		Frame(MX - 40 * U, MY - 40 * U, 80 * U, 80 * U, Yellow, 2.f * U);
		const bool bOver = !LobbyHit(FVector2D(MX, MY)).IsNone();
		Txt(bOver ? TEXT("MOVE") : TEXT("DROP"), MX, MY - 10 * U, 15.f, TEXT("BlackItalic"), FLinearColor::White, 1);
	}

	// --- bottom bar
	const float BY = H - 52 * U;
	Grad(0, BY, W, 52 * U, FLinearColor(0.02f, 0.04f, 0.1f, 0.9f), FLinearColor(0.01f, 0.02f, 0.06f, 0.97f));
	Txt(TEXT("Click an item, then click another slot to swap  -  drag onto a slot to swap, drag outside to drop"), 30 * U, BY + 16 * U, 14.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.6f));
	struct FB { const TCHAR* Key; const TCHAR* Label; const TCHAR* Id; };
	const FString InvKey = KeyLabel((int32)ESDAction::Inventory);
	const FB Bs[] = { { TEXT("Z"), TEXT("Split"), TEXT("inv:split") }, { TEXT("X"), TEXT("Drop"), TEXT("inv:drop") }, { TEXT("ESC"), TEXT("Back"), TEXT("inv:close") } };
	float X = W - 30 * U;
	for (int32 i = 2; i >= 0; --i)
	{
		const float BW = 150 * U;
		X -= BW;
		const bool bHov = Hot(Bs[i].Id, X, BY + 8 * U, BW, 36 * U);
		Box(X, BY + 8 * U, BW, 36 * U, bHov ? FLinearColor(0.2f, 0.4f, 0.8f, 0.7f) : FLinearColor(0.06f, 0.1f, 0.22f, 0.9f));
		Frame(X, BY + 8 * U, BW, 36 * U, FLinearColor(1, 1, 1, 0.45f), 1.f * U);
		Box(X + 8 * U, BY + 14 * U, 40 * U, 24 * U, FLinearColor(0.85f, 0.9f, 1.f, 1.f));
		Txt(Bs[i].Key, X + 28 * U, BY + 16 * U, 12.f, TEXT("Black"), FLinearColor(0.05f, 0.08f, 0.2f), 1, 0.f);
		Txt(Bs[i].Label, X + 58 * U, BY + 14 * U, 16.f, TEXT("Bold"), FLinearColor::White);
		X -= 10 * U;
	}
}

void ASDHUD::DrawFPS(ASDPlayerController* PC)
{
	if (!PC || !USDSettings::Get()->bShowFPS) return;
	const FString S = FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(PC->SmoothFps));
	const float W = Canvas->SizeX;
	Box(W * 0.5f - 46 * U, Canvas->SizeY - 92 * U, 92 * U, 26 * U, FLinearColor(0, 0, 0, 0.45f));
	Txt(S, W * 0.5f, Canvas->SizeY - 89 * U, 14.f, TEXT("Bold"), FLinearColor(0.6f, 1.f, 0.6f), 1);
}

// ------------------------------------------------------------------ edit overlay

void ASDHUD::DrawEditOverlay(ASDCharacter* C)
{
	ASDBuildPiece* P = C->EditTarget.Get();
	if (!P) return;
	const FVector View = C->Camera ? C->Camera->GetComponentLocation() : C->GetActorLocation();
	TArray<FSDEditTile> Tiles;
	P->GetEditTiles(View, Tiles);
	const bool bBad = C->EditSel != 0 && !P->IsValidEdit(C->EditSel, C->EditPath);
	const bool bRamp = P->Slot.Type == ESDBuildType::Ramp;
	const int32 Shown = C->EditSel;
	for (const FSDEditTile& Tl : Tiles)
	{
		// inset each tile so the grid reads, then project to the screen
		const FVector Cen = (Tl.C[0] + Tl.C[1] + Tl.C[2] + Tl.C[3]) * 0.25f;
		FVector2D S[4];
		bool bOk = true;
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector W = FMath::Lerp(Tl.C[k], Cen, 0.06f);
			const FVector Pr = Project(W, false);
			if (Pr.Z <= 0.f) { bOk = false; break; }
			S[k] = FVector2D(Pr.X, Pr.Y);
		}
		if (!bOk) continue;
		const bool bSel = (Shown >> Tl.Idx) & 1;
		FLinearColor Col = bSel ? (bBad ? FLinearColor(1.f, 0.23f, 0.23f, 0.6f) : FLinearColor(0.18f, 0.63f, 1.f, 0.62f))
			: Tl.Idx == C->EditHover ? FLinearColor(1.f, 1.f, 1.f, 0.5f)
			: FLinearColor(0.56f, 0.83f, 1.f, 0.3f);
		Quad(S[0], S[1], S[2], S[3], Col, Col, Col, Col);
		const FLinearColor Edge(0.75f, 0.9f, 1.f, 0.9f);
		for (int32 k = 0; k < 4; ++k) DrawLine(S[k].X, S[k].Y, S[(k + 1) % 4].X, S[(k + 1) % 4].Y, Edge, 1.5f * U);
		if (bRamp && bSel)
		{
			// order the stairs will climb
			const int32 Order = C->EditPath.Find(Tl.Idx);
			if (Order != INDEX_NONE)
			{
				const FVector Pc = Project(Cen, false);
				Txt(FString::FromInt(Order + 1), Pc.X, Pc.Y - 10 * U, 16.f, TEXT("Black"), FLinearColor::White, 1);
			}
		}
	}
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const int32 Mode = USDSettings::Get()->EditConfirm;
	const FString Confirm = Mode == 1 ? FString::Printf(TEXT("release %s to confirm"), *KeyLabel((int32)ESDAction::Edit))
		: Mode == 2 ? FString::Printf(TEXT("release %s to confirm"), *KeyLabel((int32)ESDAction::Fire))
		: FString::Printf(TEXT("%s confirm"), *KeyLabel((int32)ESDAction::Edit));
	const FString Hint = FString::Printf(TEXT("EDITING    drag %s to select tiles    %s    %s reset"),
		*KeyLabel((int32)ESDAction::Fire), *Confirm, *KeyLabel((int32)ESDAction::Aim));
	Txt(Hint, W * 0.5f, H * 0.62f, 15.f, TEXT("Bold"), FLinearColor(0.85f, 0.93f, 1.f), 1);
	if (bRamp) Txt(TEXT("Ramps: draw the path the stairs climb (one side = half, corner = L, three sides = U)"), W * 0.5f, H * 0.62f + 24 * U, 13.f, TEXT("Regular"), FLinearColor(1, 1, 1, 0.75f), 1);
	if (bBad) Txt(TEXT("Not a valid edit"), W * 0.5f, H * 0.62f + (bRamp ? 46 : 24) * U, 14.f, TEXT("Bold"), FLinearColor(1.f, 0.45f, 0.45f), 1);
}
// ------------------------------------------------------------------ sniper scope

void ASDHUD::Ring(float Cx, float Cy, float R0, float R1, const FLinearColor& In, const FLinearColor& Out, int32 N)
{
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(N * 2);
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
		const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
		const FVector2D C(Cx, Cy);
		FCanvasUVTri T0; T0.V0_Pos = C + D0 * R0; T0.V1_Pos = C + D1 * R0; T0.V2_Pos = C + D1 * R1;
		T0.V0_Color = In; T0.V1_Color = In; T0.V2_Color = Out;
		FCanvasUVTri T1; T1.V0_Pos = C + D0 * R0; T1.V1_Pos = C + D1 * R1; T1.V2_Pos = C + D0 * R1;
		T1.V0_Color = In; T1.V1_Color = Out; T1.V2_Color = Out;
		Tris.Add(T0); Tris.Add(T1);
	}
	FCanvasTriangleItem It(Tris, GWhiteTexture);
	It.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(It);
}

void ASDHUD::DrawScope(ASDCharacter* C)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float Cx = W * 0.5f, Cy = H * 0.5f;
	// entry animation: the scope tube closes in from the edges while the view darkens and clears
	const float K = FMath::Clamp(C->ScopeT / 0.24f, 0.f, 1.f);
	const float E = 1.f - FMath::Pow(1.f - K, 3.f);
	const float R = FMath::Lerp(H * 0.95f, H * 0.5f, E);           // lens radius
	const float Body = R * 1.32f;                                   // outer scope body
	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);
	const FLinearColor Metal(0.004f, 0.006f, 0.01f, 1.f);
	const FLinearColor MetalHi(0.025f, 0.035f, 0.05f, 1.f);
	// beyond the body: black
	Ring(Cx, Cy, Body, FMath::Max(W, H) * 1.5f, Black, Black, 96);
	// scope body with a soft highlight, then the dark lens rim
	Ring(Cx, Cy, R * 1.04f, Body, MetalHi, Metal, 96);
	Ring(Cx, Cy, R, R * 1.04f, FLinearColor(0.002f, 0.005f, 0.012f, 1.f), MetalHi, 96);
	// inner shadow on the glass edge
	Ring(Cx, Cy, R * 0.86f, R, FLinearColor(0.f, 0.02f, 0.06f, 0.f), FLinearColor(0.f, 0.03f, 0.08f, 0.75f), 96);
	// cross hair: thin black lines through the whole view, mil marks below the centre
	const float Lt = FMath::Max(2.f, 3.f * U);
	Box(0, Cy - Lt * 0.5f, W, Lt, Black);
	Box(Cx - Lt * 0.5f, 0, Lt, H, Black);
	for (int32 i = 1; i <= 7; ++i)
	{
		const float Y = Cy + R * 0.115f * i;
		const float Half = (i % 2 == 0) ? R * 0.065f : R * 0.03f;
		Box(Cx - Half, Y - Lt * 0.5f, Half * 2.f, Lt, Black);
	}
	// darkness fading out as the scope settles
	if (K < 1.f) Box(0, 0, W, H, FLinearColor(0.f, 0.f, 0.f, (1.f - E) * 0.85f));
	// hit marker stays visible while scoped
	if (GetWorld()->GetTimeSeconds() - C->HitMarkerTime < 0.18f)
	{
		const FLinearColor Hm = C->bHitMarkerHead ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor::White;
		const float A = 10.f * U, B = 22.f * U;
		DrawLine(Cx - B, Cy - B, Cx - A, Cy - A, Hm, 3.f); DrawLine(Cx + B, Cy - B, Cx + A, Cy - A, Hm, 3.f);
		DrawLine(Cx - B, Cy + B, Cx - A, Cy + A, Hm, 3.f); DrawLine(Cx + B, Cy + B, Cx + A, Cy + A, Hm, 3.f);
	}
}
// ------------------------------------------------------------------ full map

void ASDHUD::MarkerPin(float X, float Y, float S)
{
	// blue drop pin, point at (X, Y)
	const FLinearColor Blue(0.1f, 0.55f, 1.f, 1.f), Edge(1.f, 1.f, 1.f, 1.f);
	Quad(FVector2D(X - S * 0.55f, Y - S * 1.3f), FVector2D(X + S * 0.55f, Y - S * 1.3f), FVector2D(X, Y), FVector2D(X, Y), Blue, Blue, Blue, Blue);
	Ring(X, Y - S * 1.45f, 0.f, S * 0.7f, Blue, Blue, 24);
	Ring(X, Y - S * 1.45f, S * 0.7f, S * 0.82f, Edge, Edge, 24);
	Ring(X, Y - S * 1.45f, 0.f, S * 0.28f, Edge, Edge, 16);
}

bool ASDHUD::MapScreenToWorld(const FVector2D& S, FVector2D& OutWorld) const
{
	if (BigMapSize <= 0.f) return false;
	if (S.X < BigMapX || S.X > BigMapX + BigMapSize || S.Y < BigMapY || S.Y > BigMapY + BigMapSize) return false;
	const float Fx = (S.X - BigMapX) / BigMapSize, Fy = (S.Y - BigMapY) / BigMapSize;
	OutWorld = FVector2D(BigMapCtr.X + BigMapHalf - Fy * 2.f * BigMapHalf, BigMapCtr.Y - BigMapHalf + Fx * 2.f * BigMapHalf);
	return true;
}

bool ASDHUD::MapWorldToScreen(const FVector2D& Wp, FVector2D& OutScreen) const
{
	if (BigMapSize <= 0.f) return false;
	OutScreen = FVector2D(BigMapX + (Wp.Y - (BigMapCtr.Y - BigMapHalf)) / (2.f * BigMapHalf) * BigMapSize,
		BigMapY + ((BigMapCtr.X + BigMapHalf) - Wp.X) / (2.f * BigMapHalf) * BigMapSize);
	return true;
}

void ASDHUD::DrawBigMap(ASDGameMode* GM, ASDCharacter* C, ASDPlayerController* PC)
{
	if (!GM->Island) return;
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	Box(0, 0, W, H, FLinearColor(0.02f, 0.05f, 0.12f, 0.82f));
	const float BS = H * 0.82f;
	BigMapX = W * 0.5f - BS * 0.5f;
	BigMapY = H * 0.5f - BS * 0.5f - 10 * U;
	BigMapSize = BS;
	BigMapHalf = GM->Island->MapExtent() / FMath::Max(PC->MapZoom, 1.f);
	BigMapCtr = PC->MapCenter;
	DrawMap(GM, C, BigMapX, BigMapY, BS, 0.f, true, &BigMapCtr, BigMapHalf);

	// cursor crosshair on the map
	float MX = 0.f, MY = 0.f;
	PC->GetMousePosition(MX, MY);
	FVector2D Wp;
	if (MapScreenToWorld(FVector2D(MX, MY), Wp))
	{
		const FLinearColor Cc(1.f, 1.f, 1.f, 0.85f);
		DrawLine(MX - 14 * U, MY, MX - 4 * U, MY, Cc, 2.f); DrawLine(MX + 4 * U, MY, MX + 14 * U, MY, Cc, 2.f);
		DrawLine(MX, MY - 14 * U, MX, MY - 4 * U, Cc, 2.f); DrawLine(MX, MY + 4 * U, MX, MY + 14 * U, Cc, 2.f);
		if (C)
		{
			const float Dist = FVector2D::Distance(FVector2D(C->GetActorLocation()), Wp) / 100.f;
			Txt(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist)), MX + 18 * U, MY + 10 * U, 13.f, TEXT("Bold"), FLinearColor::White);
		}
	}

	// side panel: zoom and help
	const float PX = BigMapX + BS + 24 * U;
	Txt(TEXT("MAP"), PX, BigMapY, 30.f, TEXT("BlackItalic"), FLinearColor::White);
	Txt(FString::Printf(TEXT("Zoom  x%.1f"), PC->MapZoom), PX, BigMapY + 46 * U, 16.f, TEXT("Bold"), FLinearColor(0.8f, 0.88f, 1.f));
	const TCHAR* Help[] = { TEXT("Mouse wheel  zoom"), TEXT("Drag  move the map"), TEXT("Left click  place marker"), TEXT("Right click  remove marker"), TEXT("M / ESC  close") };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Help); ++i) Txt(Help[i], PX, BigMapY + 84 * U + i * 24 * U, 14.f, TEXT("Medium"), FLinearColor(1, 1, 1, 0.75f));
	if (PC->bHasMarker && C)
	{
		MarkerPin(PX + 12 * U, BigMapY + 240 * U, 12 * U);
		Txt(FString::Printf(TEXT("Marker  %d m"), FMath::RoundToInt(FVector2D::Distance(FVector2D(C->GetActorLocation()), PC->MarkerPos) / 100.f)),
			PX + 30 * U, BigMapY + 212 * U, 16.f, TEXT("Bold"), FLinearColor(0.55f, 0.8f, 1.f));
	}
}