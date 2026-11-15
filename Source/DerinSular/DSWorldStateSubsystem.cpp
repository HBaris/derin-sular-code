// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSWorldStateSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	UDSWorldStateSubsystem* FindWorldStateSubsystem(const UObject* WorldContext)
	{
		if (!WorldContext || !GEngine)
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
		return World ? World->GetSubsystem<UDSWorldStateSubsystem>() : nullptr;
	}
}

FDSColonyAndWorldState* FDSColonyAndWorldState::Find(const UObject* WorldContext)
{
	UDSWorldStateSubsystem* Subsystem = FindWorldStateSubsystem(WorldContext);
	return Subsystem ? &Subsystem->GetColonyState() : nullptr;
}

FDSColonyAndWorldState& FDSColonyAndWorldState::Get(const UObject* WorldContext)
{
	if (FDSColonyAndWorldState* WorldState = Find(WorldContext))
	{
		return *WorldState;
	}

	// dunyasiz cagri (normalde olmamali): eski singleton gibi durum tasimasin diye her seferinde sifirlanir
	ensureMsgf(false, TEXT("FDSColonyAndWorldState::Get: '%s' icin dunya/UDSWorldStateSubsystem bulunamadi"), *GetNameSafe(WorldContext));
	static FDSColonyAndWorldState DetachedState;
	DetachedState = FDSColonyAndWorldState();
	return DetachedState;
}

FDSCharacterBodyRig* FDSCharacterBodyRig::GetOrCreateRig(AActor* Owner)
{
	UDSWorldStateSubsystem* Subsystem = FindWorldStateSubsystem(Owner);
	return Subsystem ? Subsystem->GetOrCreateRig(Owner) : nullptr;
}

void UDSWorldStateSubsystem::Deinitialize()
{
	RigMap.Empty();
	ColonyState = FDSColonyAndWorldState();

	Super::Deinitialize();
}

FDSCharacterBodyRig* UDSWorldStateSubsystem::GetOrCreateRig(AActor* Owner)
{
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	const FObjectKey OwnerKey(Owner);
	if (TUniquePtr<FDSCharacterBodyRig>* Existing = RigMap.Find(OwnerKey))
	{
		return Existing->Get();
	}

	FDSCharacterBodyRig* NewRig = RigMap.Add(OwnerKey, MakeUnique<FDSCharacterBodyRig>()).Get();

	// yok edilen aktorun kaydi birikmesin (eski static GlobalRigMap hic temizlenmiyordu)
	Owner->OnDestroyed.AddUniqueDynamic(this, &UDSWorldStateSubsystem::HandleRigOwnerDestroyed);

	return NewRig;
}

bool UDSWorldStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// editor onizleme dunyalarinda da rig/durum erisimi nullptr donmesin
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor
		|| WorldType == EWorldType::EditorPreview || WorldType == EWorldType::GamePreview;
}

void UDSWorldStateSubsystem::HandleRigOwnerDestroyed(AActor* DestroyedActor)
{
	RigMap.Remove(FObjectKey(DestroyedActor));
}
