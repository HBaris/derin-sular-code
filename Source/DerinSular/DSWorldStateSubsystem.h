// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DSCharacterBodyRig.h"
#include "DSWorldStateSubsystem.generated.h"

/**
 *  Dunyaya bagli koloni/dunya durumu ve govde rig kayitlari.
 *  Eskiden function-static singleton (FDSColonyAndWorldState::Get) ve static GlobalRigMap idi;
 *  artik her dunya kendi kopyasini tasir, restart (OpenLevel) ve yeni PIE oturumu sifirdan baslar.
 *  Erisim: FDSColonyAndWorldState::Get(WorldContext) ve FDSCharacterBodyRig::GetOrCreateRig(Actor).
 */
UCLASS()
class UDSWorldStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual void Deinitialize() override;

	FDSColonyAndWorldState& GetColonyState() { return ColonyState; }

	/** Owner icin rig kaydi dondurur, yoksa olusturur; Owner yok edilince kayit silinir */
	FDSCharacterBodyRig* GetOrCreateRig(AActor* Owner);

protected:

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	UFUNCTION()
	void HandleRigOwnerDestroyed(AActor* DestroyedActor);

	FDSColonyAndWorldState ColonyState;

	/** TUniquePtr: TMap rehash'inde rig adresleri sabit kalir, donen pointer gecersizlesmez */
	TMap<FObjectKey, TUniquePtr<FDSCharacterBodyRig>> RigMap;
};
