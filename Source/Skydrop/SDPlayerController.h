#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SDPlayerController.generated.h"

enum class ESDAction : uint8;

UCLASS()
class SKYDROP_API ASDPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASDPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float Dt) override;

	bool bBigMap = false;

	// full map: zoom / pan with the mouse, place a marker (shown on both maps, the compass and as a beam in the world)
	FVector2D MapCenter = FVector2D::ZeroVector;
	float MapZoom = 1.f;
	bool bHasMarker = false;
	FVector2D MarkerPos = FVector2D::ZeroVector;
	void SetBigMap(bool bOpen);
	void SetMarker(const FVector2D& P);
	void ClearMarker();
	UPROPERTY() TObjectPtr<class AStaticMeshActor> MarkerBeam;
	bool bEmotePicked = false;

	// inventory screen
	bool bInventory = false;
	int32 InvSel = -1;          // 0-4 equipment slot, 10+ ammo type, 20+ material
	void SetInventory(bool bOpen);
	void InventoryAction(const FString& A);
	// mouse in the inventory: click to select, click another slot (or drag onto it) to swap, drag out to drop
	void InventoryMouse();
	int32 DragFrom = -1;        // inventory id being dragged (-1 none)
	FVector2D DragStart = FVector2D::ZeroVector;
	bool bDragMoved = false;
	static int32 InvIdOf(const FString& Hit); // "inv:slot:2" -> 2, "inv:ammo:1" -> 11, "inv:mat:0" -> 20, else -1

	float SmoothFps = 60.f;

private:
	bool Pressed(ESDAction A) const;
	bool Released(ESDAction A) const;
	bool Down(ESDAction A) const;
	bool HandleClick(class ASDGameMode* GM);
	bool CaptureBinding(class ASDGameMode* GM);
	FName DragId;
	void TickBigMap();
	bool bMapDrag = false;
	bool bMapMoved = false;
	FVector2D MapDragStart = FVector2D::ZeroVector;
	FVector2D MapDragCenter = FVector2D::ZeroVector;
};
