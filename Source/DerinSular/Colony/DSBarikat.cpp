// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSBarikat.h"
#include "DSVillager.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/OverlapResult.h"
#include "Sound/SoundBase.h"
#include "InputCoreTypes.h"
#include "CombatGameMode.h"
#include "DSTrustComponent.h"

ADSBarikat::ADSBarikat()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the mesh
	RootComponent = Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));

	// set the collision properties
	Mesh->SetCollisionProfileName(FName("BlockAllDynamic"));

	// disable navigation relevance so barikat doesn't affect NavMesh generation
	Mesh->bNavigationRelevant = false;
}

void ADSBarikat::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	UE_LOG(LogTemp, Warning, TEXT("[DSBarikat] TakeDamage: %.1f"), Damage);

	// zaten yikilmis barikata hasar islenmez
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	// Dost atesi (oyuncu baltasi) barikata zarar vermesin
	if (DamageCauser && DamageCauser->ActorHasTag(FName("Player")))
	{
		return;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - Damage);

	// Yakindaki koyluleri ve HUD'u uyar (Secenek C Hibrit)
	NotifyDanger(GetActorLocation(), DamageCauser);

	if (CurrentHP <= 0.0f)
	{
		HandleDeath();
	}
}

void ADSBarikat::HandleDeath()
{
	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
	}

	OnBarikatDestroyed.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("%s: barikat yikildi"), *GetName());

	Destroy();
}

void ADSBarikat::ApplyHealing(float Healing, AActor* Healer)
{
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Healing);
}

void ADSBarikat::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - SonTehlikeZamani < TehlikeCooldown)
	{
		return;
	}
	SonTehlikeZamani = Now;

	// 1. Yakindaki koyluleri uyar (Secenek C Hibrit - docs/BARIKAT_TEHLIKE_TASARIM.md)
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionShape Sphere = FCollisionShape::MakeSphere(TehlikeYaricapi);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld() && GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams, Sphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ADSVillager* Villager = Cast<ADSVillager>(Overlap.GetActor()))
			{
				Villager->NotifyDanger(DangerLocation.IsNearlyZero() ? GetActorLocation() : DangerLocation, DangerSource ? DangerSource : this);
			}
		}
	}

	// 2. HUD/Ses delegesini yayinla
	OnBarikatTehlikede.Broadcast(this, CurrentHP / FMath::Max(1.0f, MaxHP));
}

void ADSBarikat::TamirEt(AActor* Tamirci)
{
	if (CurrentHP <= 0.0f || CurrentHP >= MaxHP)
	{
		return;
	}

	ApplyHealing(TamirMiktari, Tamirci);

	if (RepairSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RepairSound, GetActorLocation());
	}

	if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
	{
		GM->AddScore(25);
		if (GM->TrustComponent)
		{
			GM->TrustComponent->AddTrust(5.0f, TEXT("Barikat tamir edildi"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("%s: tamir edildi, CurrentHP=%.0f/%.0f"), *GetName(), CurrentHP, MaxHP);
}

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

void ADSBarikat::BeginPlay()
{
	Super::BeginPlay();

	if (!RepairSound)
	{
		RepairSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Barikat_Repair.SFX_Barikat_Repair"));
	}
	if (!BreakSound)
	{
		BreakSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Zombie_Explosion.SFX_Zombie_Explosion"));
	}

	// Kutu carpismasini koruyarak ustune gercek 3B ahsap barikat modeli (SM_Barikat_WoodFence / SM_Barikat_Boards) yerlestir
	if (Mesh)
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		UStaticMesh* WoodFenceMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_WoodFence.SM_Barikat_WoodFence"));
		if (!WoodFenceMesh)
		{
			WoodFenceMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_Boards.SM_Barikat_Boards"));
		}

		if (WoodFenceMesh)
		{
			Mesh->SetHiddenInGame(true);

			UStaticMeshComponent* VisualFence = NewObject<UStaticMeshComponent>(this, TEXT("VisualWoodBarricade"));
			if (VisualFence)
			{
				VisualFence->SetupAttachment(Mesh);
				VisualFence->SetMobility(EComponentMobility::Movable);
				VisualFence->SetStaticMesh(WoodFenceMesh);
				VisualFence->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				VisualFence->bNavigationRelevant = false;
				VisualFence->SetCastShadow(true);
				VisualFence->RegisterComponent();

				const FBoxSphereBounds Bounds = WoodFenceMesh->GetBounds();
				const float RawHeight = FMath::Max(Bounds.BoxExtent.Z * 2.0f, 1.0f);
				const float S = FMath::Clamp(135.0f / RawHeight, 0.05f, 25.0f);
				const FVector WorldScale(S * 1.15f, S * 1.15f, S);
				VisualFence->SetWorldScale3D(WorldScale);

				float GroundZ = GetActorLocation().Z;
				if (GetWorld())
				{
					FHitResult Hit;
					FCollisionQueryParams Params;
					Params.AddIgnoredActor(this);
					const FVector Loc = GetActorLocation();
					if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(Loc.X, Loc.Y, 1500.0f), FVector(Loc.X, Loc.Y, -500.0f), ECC_WorldStatic, Params))
					{
						GroundZ = Hit.ImpactPoint.Z;
					}
				}
				const float BottomOffset = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * WorldScale.Z;
				VisualFence->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ - BottomOffset));

				UMaterialInterface* MatWood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeatheredWood.M_Env_WeatheredWood"));
				UMaterialInterface* MatDarkWood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_DarkWood.M_Env_DarkWood"));
				if (MatWood)
				{
					const int32 NumMats = VisualFence->GetNumMaterials();
					for (int32 i = 0; i < NumMats; ++i)
					{
						VisualFence->SetMaterial(i, (i % 2 == 1 && MatDarkWood) ? MatDarkWood : MatWood);
					}
				}
			}
		}
	}

	// Test amacli baslangicta can yariya dusuruldu (15.0 / 30.0)
	CurrentHP = MaxHP * 0.5f;
	UE_LOG(LogTemp, Log, TEXT("[DSBarikat] %s baslatildi, test cani: %.1f / %.1f"), *GetName(), CurrentHP, MaxHP);
}

void ADSBarikat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// yikilmis veya zaten tam canli barikat icin tamir kontrolune gerek yok
	if (CurrentHP <= 0.0f || CurrentHP >= MaxHP)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || !PlayerController->GetPawn())
	{
		return;
	}

	if (!PlayerController->WasInputKeyJustPressed(EKeys::E))
	{
		return;
	}

	APawn* PlayerPawn = PlayerController->GetPawn();
	const float Distance = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
	if (Distance <= TamirMesafesi)
	{
		TamirEt(PlayerPawn);
	}
}
