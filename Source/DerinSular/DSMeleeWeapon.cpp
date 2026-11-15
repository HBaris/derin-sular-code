// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSMeleeWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "CombatDamageable.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADSMeleeWeapon::ADSMeleeWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;

	// isabet tespiti SphereTrace ile ayrica yapiliyor; mesh'in kendisi fiziksel collision uretmesin
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ZDBG: onceki commit WeaponMesh'e hicbir StaticMesh atamiyordu, bu yuzden BP alt sinifi
	// olusturulup mesh elle atanmadan silah goze gorunmezdi. Engine'in kup mesh'ini varsayilan
	// olarak atayarak ADSMeleeWeapon'u BP'siz, dogrudan native class olarak da kullanilabilir
	// yapiyoruz; balta mesh'i gelince bu ya BP alt sinifinda ya da burada degistirilir.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> AxeMeshFinder(TEXT("/Game/Variant_Combat/Weapons/SM_BattleAxe.SM_BattleAxe"));
	if (AxeMeshFinder.Succeeded())
	{
		WeaponMesh->SetStaticMesh(AxeMeshFinder.Object);
		WeaponMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (PlaceholderMeshFinder.Succeeded())
		{
			WeaponMesh->SetStaticMesh(PlaceholderMeshFinder.Object);

			// kup varsayilan olarak 100x100x100cm; silaha benzesin diye ince/uzun bir olcek uygula
			WeaponMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.6f));

			// pivot kupun merkezinde oldugundan yukari kaydirip elin biraz onune al
			WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ADSMeleeWeapon: placeholder kup mesh (/Engine/BasicShapes/Cube) bulunamadi, WeaponMesh gorsel olarak bos kalacak"));
		}
	}
}

void ADSMeleeWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (!WeaponMesh)
	{
		return;
	}

	static const TCHAR* BattleAxePath = TEXT("/Game/Variant_Combat/Weapons/SM_BattleAxe.SM_BattleAxe");
	UStaticMesh* CurrentMesh = WeaponMesh->GetStaticMesh();
	const bool bMeshEmpty = !CurrentMesh || CurrentMesh->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube");

	// BP alt sinifinda farkli bir mesh atanmissa ezme; yalnizca bos/placeholder kupse balta yukle
	if (!bMeshEmpty && CurrentMesh->GetPathName() != BattleAxePath)
	{
		UE_LOG(LogTemp, Log, TEXT("[DSMeleeWeapon] BP'de atanmis mesh korunuyor (%s)"), *CurrentMesh->GetName());
		return;
	}

	// constructor varsayilani SM_BattleAxe ise yeniden yuklemeden sadece el boyuna olceklenir
	UStaticMesh* AxeMesh = bMeshEmpty ? LoadObject<UStaticMesh>(nullptr, BattleAxePath) : CurrentMesh;
	if (!AxeMesh)
	{
		AxeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_SurvivalAxe.SM_SurvivalAxe"));
	}

	if (AxeMesh)
	{
		WeaponMesh->SetStaticMesh(AxeMesh);

		// Mesh boyutuna gore el icin ideal ~75cm uzunluga otomatik olcekle
		const FBoxSphereBounds Bounds = AxeMesh->GetBounds();
		const float MaxExtent = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
		const float TargetSize = 75.0f;
		const float AutoScale = (MaxExtent > 0.01f) ? FMath::Clamp(TargetSize / MaxExtent, 0.01f, 120.0f) : 1.0f;

		WeaponMesh->SetRelativeScale3D(FVector(AutoScale));
		WeaponMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
		WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
		UE_LOG(LogTemp, Log, TEXT("[DSMeleeWeapon] Gercek 3D balta mesh'i yuklendi (%s, AutoScale=%.2f)"), *AxeMesh->GetName(), AutoScale);
	}
}

void ADSMeleeWeapon::DoAttackTrace(AActor* Wielder)
{
	// sweep for objects in front of the weapon to be hit by the attack
	TArray<FHitResult> OutHits;

	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * TraceRange);

	// pawns and destructible dynamic objects can be damaged by this trace
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(TraceRadius);

	// ignore the weapon itself and whoever is wielding it
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (Wielder)
	{
		QueryParams.AddIgnoredActor(Wielder);
	}

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor()))
			{
				// small forward push on hit, mirrors the existing melee attack impulses
				const FVector Impulse = GetActorForwardVector() * 200.0f;

				// pass the damage event through the existing damage interface (ApplyDamage -> TakeDamage)
				Damageable->ApplyDamage(DamageAmount, Wielder ? Wielder : this, CurrentHit.ImpactPoint, Impulse);
			}
		}
	}
}
