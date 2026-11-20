#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SDGrass.generated.h"

class ASDWorld;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;

// Streams tall grass tufts in a ring of cells around the camera
UCLASS()
class SKYDROP_API ASDGrass : public AActor
{
	GENERATED_BODY()
public:
	ASDGrass();
	virtual void Tick(float Dt) override;

	UPROPERTY() TObjectPtr<ASDWorld> Island;
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FIntPoint, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Cells;
	UPROPERTY() TObjectPtr<UMaterialInterface> Mat;

	float CellSize = 800.f;
	float Radius = 5000.f;
	int32 TuftsPerCell = 230;
	int32 AppliedLevel = -1;

private:
	void BuildCell(const FIntPoint& Key);
};
