#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "SDHUD.generated.h"

class ASDCharacter;
class ASDGameMode;
class UFont;

UCLASS()
class SKYDROP_API ASDHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	// topmost button under P; sliders return "slide:<id>:<fraction>". Filter limits the search to one id (slider drag).
	FName LobbyHit(const FVector2D& P, FName Filter = NAME_None) const;
	bool ButtonCenter(FName Id, FVector2D& Out) const; // tests
	// full map: last drawn rect + view (world centre, half extent), for mouse <-> world
	float BigMapX = 0.f, BigMapY = 0.f, BigMapSize = 0.f, BigMapHalf = 1.f;
	FVector2D BigMapCtr = FVector2D::ZeroVector;
	bool MapScreenToWorld(const FVector2D& S, FVector2D& OutWorld) const;
	bool MapWorldToScreen(const FVector2D& W, FVector2D& OutScreen) const;

private:
	struct FSDButton { FVector2D Min; FVector2D Max; FName Id; bool bSlider = false; };
	TArray<FSDButton> Buttons;
	bool Button(FName Id, float X, float Y, float W, float H, const FString& Label, const FLinearColor& Fill, const FLinearColor& TextC, class UFont* F, float Scale);
	void DrawLobby(ASDGameMode* GM);
	void CoinIcon(float X, float Y, float R);
	void Card(FName ClickId, float X, float Y, float W, float H, const struct FSDCosmetic* C, bool bSel, const FString& Bottom, bool bDim, ASDGameMode* GM);
	void InfoBox(float X, float Y, float W, const struct FSDCosmetic* C);
	float U = 1.f;
	UPROPERTY() TObjectPtr<UFont> UIFont;
	FSlateFontInfo UIFontInfo(float Size, const TCHAR* Face);
	// styled text: Size in 1080p points, Face = Roboto typeface (Regular, Bold, BoldItalic, BoldCondensed, Black, BlackItalic...), Align 0 left 1 centre 2 right
	float Txt(const FString& S, float X, float Y, float Size, const TCHAR* Face, const FLinearColor& C, int32 Align = 0, float Shadow = 2.f);
	FVector2D TxtSize(const FString& S, float Size, const TCHAR* Face) const;
	void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD);
	void Slant(float X, float Y, float W, float H, float Skew, const FLinearColor& C);
	void Grad(float X, float Y, float W, float H, const FLinearColor& Top, const FLinearColor& Bot);
	void Star(float X, float Y, float R, const FLinearColor& C);
	void Shield(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Edge);
	bool Hot(FName Id, float X, float Y, float W, float H);
	void LobbyTopBar(ASDGameMode* GM);
	void LobbyHome(ASDGameMode* GM);
	void LobbyBottomBar(ASDGameMode* GM);
	void SliderHot(FName Id, float X, float Y, float W, float H);
	FString KeyLabel(int32 Action) const;
	void DrawSettings(ASDGameMode* GM, float X, float Y, float W, float H, bool bBack);
	void DrawPauseMenu(ASDGameMode* GM);
	void DrawInventory(ASDGameMode* GM, ASDCharacter* C, class ASDPlayerController* PC);
	void DrawFPS(class ASDPlayerController* PC);
	void DrawEditOverlay(ASDCharacter* C);
	void DrawScope(ASDCharacter* C);
	void Ring(float Cx, float Cy, float R0, float R1, const FLinearColor& In, const FLinearColor& Out, int32 N);
	void Text(const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale = 1.f, bool bCenter = false);
	void Box(float X, float Y, float W, float H, const FLinearColor& C);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float T = 2.f);
	void Circle(const FVector2D& C, float R, const FLinearColor& Col, float T = 2.f);
	void DrawMap(ASDGameMode* GM, ASDCharacter* C, float X, float Y, float Size, float View, bool bLabels, const FVector2D* CenterIn = nullptr, float HalfIn = 0.f);
	void DrawBigMap(ASDGameMode* GM, ASDCharacter* C, class ASDPlayerController* PC);
	void MarkerPin(float X, float Y, float S);
	void DrawCompass(ASDCharacter* C);
	void DrawCrosshair(ASDCharacter* C);
	void DrawVitals(ASDCharacter* C);
	void DrawHotbar(ASDCharacter* C);
	void DrawEnd(ASDGameMode* GM);
};
