#pragma once

#include "CoreMinimal.h"
#include "SDLoot.h"
#include "SDInterior.generated.h"

// Furniture pieces, built from simple shapes. Local frame: back against the wall at -X, front facing +X.
enum class ESDFurn : uint8
{
	Bed, Nightstand, Desk, OfficeChair, Couch, Armchair, CoffeeTable, Bookshelf, FloorLamp, Plant,
	Crate, Dresser, Fridge, Counter, TVStand, FilingCabinet, DiningTable,
	Count
};

// Breakable furniture: two pickaxe hits (100 HP, 50 per swing), or instantly when a ramp/cone is built through it.
UCLASS()
class SKYDROP_API ASDFurniture : public ASDResource
{
	GENERATED_BODY()
public:
	void Build(ESDFurn InKind, int32 Seed, const FLinearColor& Accent);
	static FVector Size(ESDFurn K); // depth (X), width (Y), height (Z)
	// break apart with a puff of planks; called when a build piece is placed through it
	void Smash();

	ESDFurn Kind = ESDFurn::Crate;
};
