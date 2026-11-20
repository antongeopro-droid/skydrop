#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDLobby.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UPointLightComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
struct FSDCosmetic;

// Pre-match lobby stage: grid room, glowing pads, the player's character, a camera that frames
// the hero per menu, and a small photo studio that renders card thumbnails for every cosmetic.
UCLASS()
class SKYDROP_API ASDLobby : public AActor
{
	GENERATED_BODY()
public:
	ASDLobby();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	void ShowLoadout(FName Outfit, FName BackBling);
	void PlayEmote(FName Emote);
	// inspect a cosmetic: outfits/back blings go on the hero (turned to show the back), pickaxes and gliders
	// float and spin next to the hero, emotes play. BaseOutfit/BaseBling = what the hero wears otherwise.
	void Preview(FName Id, FName BaseOutfit, FName BaseBling);
	void ClearPreview(FName BaseOutfit, FName BaseBling);
	FName PreviewId;
	void SetSkin(int32 Skin) {}
	FVector PadLocation(int32 Index) const; // 0 = centre, 1 = left, 2 = right
	void SetFocus(int32 Mode) { Focus = Mode; } // 0 centred, 1 hero on the right, 2 hidden behind menus
	UTextureRenderTarget2D* Thumb(FName Id) const;
	bool ThumbsReady() const { return Queue.Num() == 0 && bStarted; }

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Hero;
	UPROPERTY() TObjectPtr<USceneComponent> HeroBling;
	UPROPERTY() TObjectPtr<UCameraComponent> Cam;
	UPROPERTY() TObjectPtr<UPointLightComponent> Key;
	UPROPERTY() TObjectPtr<UPointLightComponent> Rim;
	UPROPERTY() TObjectPtr<UPointLightComponent> PadGlow;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<class UExponentialHeightFogComponent> Fog;
	// honeycomb backdrop: hexes are split into groups that slowly light up and fade out
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> HexMats;
	TArray<FVector4f> HexAnim; // x phase, y speed, z base, w peak

	// studio
	UPROPERTY() TObjectPtr<USceneComponent> Studio;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> StudioHero;
	UPROPERTY() TObjectPtr<USceneComponent> StudioItem;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY() TObjectPtr<UPointLightComponent> StudioKey;
	UPROPERTY() TObjectPtr<UPointLightComponent> StudioFill;
	UPROPERTY() TMap<FName, TObjectPtr<UTextureRenderTarget2D>> Thumbs;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StudioParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BlingParts;
	UPROPERTY() TObjectPtr<USceneComponent> ItemStage;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ItemParts;
	float HeroYaw = 98.f, HeroYawTarget = 98.f;

	float T = 0.f;
	int32 Focus = 0;
	FName ShownOutfit, ShownBling;
	float EmoteLeft = 0.f;

private:
	TArray<FName> Queue;
	FName Staged;
	int32 StageFrames = 0;
	bool bStarted = false;
	void StageItem(const FSDCosmetic* C);
	void PlayIdle(USkeletalMeshComponent* M);
};
