// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "DSWaveManager.h"
#include "DSTrustComponent.h"
#include "DSVillager.h"
#include "DSBarikat.h"
#include "CombatEnemySpawner.h"
#include "CombatEnemy.h"
#include "DSCharacterBodyRig.h"
#include "Engine/Engine.h"

namespace
{
	/** Oyun bitince (olum veya kazanma) yeniden baslamadan once beklenecek sure (Asama 10) */
	constexpr float RestartDelaySeconds = 5.0f;

	float FindGroundZ(UWorld* World, float X, float Y, float FallbackZ)
	{
		if (!World)
		{
			return FallbackZ;
		}
		FHitResult Hit;
		FCollisionQueryParams Params;
		if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, 250.0f), FVector(X, Y, -500.0f), ECC_WorldStatic, Params))
		{
			if (Hit.ImpactPoint.Z >= 150.0f && Hit.ImpactPoint.Z <= 235.0f)
			{
				return Hit.ImpactPoint.Z;
			}
		}
		return FallbackZ;
	}

	void ApplyLoadedMatBySlot(UStaticMeshComponent* SMC, const TMap<FString, UMaterialInterface*>& SlotMap, UMaterialInterface* DefaultMat)
	{
		if (!SMC)
		{
			return;
		}
		const TArray<FName> SlotNames = SMC->GetMaterialSlotNames();
		const int32 NumMats = SMC->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			const FString SlotStr = SlotNames.IsValidIndex(i) ? SlotNames[i].ToString().ToLower() : TEXT("");
			UMaterialInterface* Chosen = DefaultMat;
			for (const TPair<FString, UMaterialInterface*>& Pair : SlotMap)
			{
				if (Pair.Value && SlotStr.Contains(Pair.Key))
				{
					Chosen = Pair.Value;
					break;
				}
			}
			if (Chosen)
			{
				SMC->SetMaterial(i, Chosen);
			}
		}
	}

	FVector ComputeTargetHeightScale(UStaticMesh* Mesh, float TargetHeightCm, float XYScaleMult = 1.0f)
	{
		if (!Mesh)
		{
			return FVector(1.0f);
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FString MName = Mesh->GetName();
		const bool bHorizontalMesh = MName.Contains(TEXT("Log")) || MName.Contains(TEXT("DeadTree")) || MName.Contains(TEXT("Boards"));
		const float RawDim = bHorizontalMesh
			? FMath::Max(FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f, 1.0f)
			: FMath::Max(Bounds.BoxExtent.Z * 2.0f, 1.0f);
		const float S = FMath::Clamp(TargetHeightCm / RawDim, 0.02f, 25.0f);
		return FVector(S * XYScaleMult, S * XYScaleMult, S);
	}

	float ComputeMeshGroundLocationZ(UStaticMesh* Mesh, const FVector& Scale3D, float GroundZ)
	{
		if (!Mesh)
		{
			return GroundZ;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float BottomOffset = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale3D.Z;
		return GroundZ - BottomOffset;
	}

	AStaticMeshActor* SpawnForestProp(UWorld* World, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, float TargetHeightCm, const TMap<FString, UMaterialInterface*>& SlotMap, UMaterialInterface* DefaultMat, float XYScaleMult = 1.0f)
	{
		if (!World || !Mesh)
		{
			return nullptr;
		}
		const FVector Scale3D = (TargetHeightCm > 0.0f) ? ComputeTargetHeightScale(Mesh, TargetHeightCm, XYScaleMult) : FVector(1.0f);
		const FVector AlignedLoc(Loc.X, Loc.Y, (TargetHeightCm > 0.0f) ? ComputeMeshGroundLocationZ(Mesh, Scale3D, Loc.Z) : Loc.Z);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), AlignedLoc, Rot, Params);
		if (Prop && Prop->GetStaticMeshComponent())
		{
			UStaticMeshComponent* SMC = Prop->GetStaticMeshComponent();
			SMC->SetMobility(EComponentMobility::Movable);
			SMC->SetStaticMesh(Mesh);
			SMC->SetWorldScale3D(Scale3D);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SMC->SetCastShadow(true);
			ApplyLoadedMatBySlot(SMC, SlotMap, DefaultMat);
		}
		return Prop;
	}

	void EnrichRealisticForestMap(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		UMaterialInterface* MatGround   = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_ForestGround.M_Env_ForestGround"));
		UMaterialInterface* MatFoliage  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_PineFoliage.M_Env_PineFoliage"));
		UMaterialInterface* MatBark     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_PineBark.M_Env_PineBark"));
		UMaterialInterface* MatWood     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeatheredWood.M_Env_WeatheredWood"));
		UMaterialInterface* MatDarkWood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_DarkWood.M_Env_DarkWood"));
		UMaterialInterface* MatRock     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_MossyRock.M_Env_MossyRock"));
		UMaterialInterface* MatStone    = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_DarkStone.M_Env_DarkStone"));
		UMaterialInterface* MatPlaster  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_VillagePlaster.M_Env_VillagePlaster"));
		UMaterialInterface* MatRoof     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_VillageRoof.M_Env_VillageRoof"));
		UMaterialInterface* MatTent     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_TentCanvas.M_Env_TentCanvas"));
		UMaterialInterface* MatFire      = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_BonfireFire.M_Env_BonfireFire"));
		UMaterialInterface* MatWindow    = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_HouseWindow.M_Env_HouseWindow"));
		UMaterialInterface* MatPineAtlas = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_PineTreeAtlas_PBR.M_Env_PineTreeAtlas_PBR"));
		UMaterialInterface* MatSunflower = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Char_BossSunflower_PBR.M_Char_BossSunflower_PBR"));
		UMaterialInterface* MatTreeguard = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Char_BossTreeguard_PBR.M_Char_BossTreeguard_PBR"));

		TMap<FString, UMaterialInterface*> TreeSlots  = { {TEXT("green"), MatFoliage}, {TEXT("wood"), MatBark} };
		TMap<FString, UMaterialInterface*> RockSlots  = { {TEXT("green"), MatFoliage}, {TEXT("rock"), MatRock} };
		TMap<FString, UMaterialInterface*> HouseSlots = { {TEXT("plaster"), MatPlaster}, {TEXT("roof"), MatRoof}, {TEXT("stone"), MatStone}, {TEXT("window"), MatWindow}, {TEXT("wood"), MatWood} };
		TMap<FString, UMaterialInterface*> FireSlots  = { {TEXT("fire"), MatFire}, {TEXT("stone"), MatStone}, {TEXT("wood"), MatDarkWood} };
		TMap<FString, UMaterialInterface*> TentSlots  = { {TEXT("green"), MatTent}, {TEXT("wood"), MatWood}, {TEXT("black"), MatStone} };
		TMap<FString, UMaterialInterface*> CrateSlots = { {TEXT("dark"), MatDarkWood}, {TEXT("wood"), MatWood} };

		const FVector CourtyardCenter(1350.0f, 1680.0f, 0.0f);
		const float BaseZ = FindGroundZ(World, 1350.0f, 1680.0f, 200.0f);

		auto PushOutOfCourtyard = [&](FVector Loc, float MinRadius) -> FVector
		{
			FVector Delta2D(Loc.X - CourtyardCenter.X, Loc.Y - CourtyardCenter.Y, 0.0f);
			const float Dist2D = Delta2D.Size2D();
			if (Dist2D < MinRadius)
			{
				const FVector Dir = (Dist2D > 1.0f) ? (Delta2D / Dist2D) : FVector(-0.7f, 0.7f, 0.0f);
				Loc.X = CourtyardCenter.X + Dir.X * MinRadius;
				Loc.Y = CourtyardCenter.Y + Dir.Y * MinRadius;
			}
			return Loc;
		};

		// 1. Sahnedeki mevcut statik mesh aktorlerini gercekci olcek, duz aci, PBR materyal ve acik avlu duzenine getir
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* SMA = *It;
			if (!SMA || !SMA->GetStaticMeshComponent())
			{
				continue;
			}
			UStaticMeshComponent* SMC = SMA->GetStaticMeshComponent();
			UStaticMesh* SM = SMC->GetStaticMesh();
			const FString MeshName = SM ? SM->GetName() : TEXT("");
			const FString ActorName = SMA->GetName();

			if (MeshName.Contains(TEXT("VillageHouse")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FVector Scale3D = ComputeTargetHeightScale(SM, 430.0f);
				SMC->SetWorldScale3D(Scale3D);
				const FVector SafeLoc = PushOutOfCourtyard(SMA->GetActorLocation(), 920.0f);
				const FRotator FaceCenterRot = (CourtyardCenter - SafeLoc).Rotation();
				SMA->SetActorLocationAndRotation(
					FVector(SafeLoc.X, SafeLoc.Y, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ)),
					FRotator(0.0f, FaceCenterRot.Yaw, 0.0f)
				);
				ApplyLoadedMatBySlot(SMC, HouseSlots, MatWood);
			}
			else if (MeshName.Contains(TEXT("Bonfire")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FVector Scale3D = ComputeTargetHeightScale(SM, 85.0f);
				SMC->SetWorldScale3D(Scale3D);
				const FVector BonfireLoc(1490.0f, 1890.0f, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ));
				SMA->SetActorLocationAndRotation(BonfireLoc, FRotator::ZeroRotator);
				ApplyLoadedMatBySlot(SMC, FireSlots, MatDarkWood);

				UPointLightComponent* FireLight = NewObject<UPointLightComponent>(SMA, TEXT("CampfireWarmLight"));
				if (FireLight)
				{
					FireLight->SetupAttachment(SMC);
					FireLight->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
					FireLight->SetIntensity(3600.0f);
					FireLight->SetLightColor(FLinearColor(1.0f, 0.54f, 0.18f));
					FireLight->SetAttenuationRadius(1200.0f);
					FireLight->SetCastShadows(true);
					FireLight->RegisterComponent();
				}
			}
			else if (MeshName.Contains(TEXT("SurvivorTent")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FVector Scale3D = ComputeTargetHeightScale(SM, 210.0f);
				SMC->SetWorldScale3D(Scale3D);
				FVector SafeLoc = PushOutOfCourtyard(SMA->GetActorLocation(), 760.0f);
				if (SafeLoc.X < 1200.0f && SafeLoc.Y > 1800.0f)
				{
					SafeLoc = FVector(1880.0f, 2180.0f, BaseZ);
				}
				const FRotator FaceCenterRot = (CourtyardCenter - SafeLoc).Rotation();
				SMA->SetActorLocationAndRotation(
					FVector(SafeLoc.X, SafeLoc.Y, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ)),
					FRotator(0.0f, FaceCenterRot.Yaw, 0.0f)
				);
				ApplyLoadedMatBySlot(SMC, TentSlots, MatTent);
			}
			else if (MeshName.Contains(TEXT("ForestTerrain")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SMA->SetActorHiddenInGame(true);
			}
			else if (MeshName.Contains(TEXT("PineTree")) || MeshName.Contains(TEXT("DeadTree")) || MeshName.Contains(TEXT("TreeStump")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const float TargetH = MeshName.Contains(TEXT("DeadTree")) ? 42.0f : (MeshName.Contains(TEXT("Stump")) ? 75.0f : 860.0f);
				FVector Scale3D = ComputeTargetHeightScale(SM, TargetH);
				if (!MeshName.Contains(TEXT("Stump")))
				{
					SMA->Tags.AddUnique(FName("ChopTree"));
					SMA->Tags.AddUnique(FName("ElderTree"));
				}
				SMC->SetWorldScale3D(Scale3D);
				// Mevcut sahne agaclarini genis acik meydanin (2450 cm yaricap) disina tasi ki Boss_Sunflower ile acik alanda rahatca karsilasilabilsin
				const FVector SafeLoc = PushOutOfCourtyard(SMA->GetActorLocation(), 2450.0f);
				SMA->SetActorLocationAndRotation(
					FVector(SafeLoc.X, SafeLoc.Y, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ)),
					FRotator(0.0f, SMA->GetActorRotation().Yaw, 0.0f)
				);
				if (!MeshName.Contains(TEXT("PineTree")) && !MeshName.Contains(TEXT("DeadTree")) && !MeshName.Contains(TEXT("Sapling")))
				{
					UMaterialInterface* DefaultTreeMat = (MeshName.Contains(TEXT("Dead")) || MeshName.Contains(TEXT("Stump"))) ? MatBark : MatFoliage;
					ApplyLoadedMatBySlot(SMC, TreeSlots, DefaultTreeMat);
				}
			}
			else if (MeshName.Contains(TEXT("ForestRock")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FVector Scale3D = ComputeTargetHeightScale(SM, 125.0f);
				SMC->SetWorldScale3D(Scale3D);
				const FVector SafeLoc = PushOutOfCourtyard(SMA->GetActorLocation(), 2200.0f);
				SMA->SetActorLocation(FVector(SafeLoc.X, SafeLoc.Y, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ)));
				ApplyLoadedMatBySlot(SMC, RockSlots, MatRock);
				SMA->Tags.AddUnique(FName("OreVein"));
				SMA->Tags.AddUnique(FName("OreVein_Stone"));
			}
			else if (MeshName.Contains(TEXT("Crate")) || MeshName.Contains(TEXT("Barrel")) || MeshName.Contains(TEXT("WoodLog")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const float TargetH = MeshName.Contains(TEXT("Log")) ? 140.0f : 88.0f;
				const FVector Scale3D = ComputeTargetHeightScale(SM, TargetH);
				SMC->SetWorldScale3D(Scale3D);
				const FVector SafeLoc = PushOutOfCourtyard(SMA->GetActorLocation(), 2250.0f);
				SMA->SetActorLocation(FVector(SafeLoc.X, SafeLoc.Y, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ)));
				ApplyLoadedMatBySlot(SMC, CrateSlots, MatWood);
			}
			else if (MeshName.Contains(TEXT("FirstAidKit")))
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FVector Scale3D = ComputeTargetHeightScale(SM, 30.0f);
				SMC->SetWorldScale3D(Scale3D);
				SMA->SetActorLocationAndRotation(
					FVector(980.0f, 1460.0f, ComputeMeshGroundLocationZ(SM, Scale3D, BaseZ + 85.0f)),
					FRotator(0.0f, 20.0f, 0.0f)
				);
			}
			else if (MatGround && (ActorName.Contains(TEXT("Floor")) || MeshName.Contains(TEXT("Cube")) || MeshName.Contains(TEXT("Plane")) || MeshName.Contains(TEXT("Floor")) || MeshName.Contains(TEXT("Socket"))))
			{
				const int32 NumMats = FMath::Max(1, SMC->GetNumMaterials());
				for (int32 i = 0; i < NumMats; ++i)
				{
					SMC->SetMaterial(i, MatGround);
				}
			}
		}

		// 1b. Sahnedeki ADSBarikat aktorunu kuzey orman savunma hattina hizala (acik Boss meydanini kapatmasin)
		UStaticMesh* WoodFenceMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_WoodFence.SM_Barikat_WoodFence"));
		for (TActorIterator<ADSBarikat> It(World); It; ++It)
		{
			ADSBarikat* Barikat = *It;
			if (!IsValid(Barikat))
			{
				continue;
			}
			if (USceneComponent* RootSC = Barikat->GetRootComponent())
			{
				RootSC->SetMobility(EComponentMobility::Movable);
			}
			const FVector BarikatLoc(1480.0f, 1020.0f, BaseZ + 50.0f);
			const FRotator BarikatRot(0.0f, 0.0f, 0.0f);
			Barikat->SetActorLocationAndRotation(BarikatLoc, BarikatRot);

			TArray<UStaticMeshComponent*> BarikatComps;
			Barikat->GetComponents<UStaticMeshComponent>(BarikatComps);
			for (UStaticMeshComponent* Comp : BarikatComps)
			{
				if (Comp && Comp != Barikat->GetRootComponent() && Comp->GetStaticMesh())
				{
					Comp->SetMobility(EComponentMobility::Movable);
					const FBoxSphereBounds Bounds = Comp->GetStaticMesh()->GetBounds();
					const float RawHeight = FMath::Max(Bounds.BoxExtent.Z * 2.0f, 1.0f);
					const float S = FMath::Clamp(135.0f / RawHeight, 0.05f, 25.0f);
					const FVector WorldScale(S * 1.15f, S * 1.15f, S);
					Comp->SetWorldScale3D(WorldScale);
					const float BottomOffset = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * WorldScale.Z;
					Comp->SetWorldLocationAndRotation(FVector(BarikatLoc.X, BarikatLoc.Y, BaseZ - BottomOffset), BarikatRot);
					ApplyLoadedMatBySlot(Comp, CrateSlots, MatWood);
				}
			}
		}
		if (WoodFenceMesh)
		{
			SpawnForestProp(World, WoodFenceMesh, FVector(1180.0f, 1020.0f, BaseZ), FRotator(0.0f, 5.0f, 0.0f),  135.0f, CrateSlots, MatWood, 1.35f);
			SpawnForestProp(World, WoodFenceMesh, FVector(1780.0f, 1020.0f, BaseZ), FRotator(0.0f, -5.0f, 0.0f), 135.0f, CrateSlots, MatWood, 1.35f);
		}

		// 1c. Lvl_Forest sahnesinde ADSWaveManager ve Spawner ayarlari (Bariscil model inceleme modunda dalgalar otomatik baslamaz)
		ADSWaveManager* WaveManager = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(World, ADSWaveManager::StaticClass()));
		if (!WaveManager)
		{
			FActorSpawnParameters WaveParams;
			WaveParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			WaveManager = World->SpawnActor<ADSWaveManager>(ADSWaveManager::StaticClass(), FVector(1350.0f, 1680.0f, BaseZ + 100.0f), FRotator::ZeroRotator, WaveParams);
		}
		if (WaveManager)
		{
			WaveManager->BaseEnemiesPerWave = 2;
			WaveManager->EnemiesPerWaveIncrement = 1;
			// ilk dalgayi yalnizca WaveManager kendi FirstWaveDelay timer'iyla baslatir (burada ikinci bir StartNextWave cift dalga aciyordu)
			if (ACombatGameMode::IsPeacefulShowcaseMode(World))
			{
				WaveManager->bAutoStartFirstWave = false;
			}
		}

		// 2. Duz ve temiz zemin uzerinde 3B Cimen & Poly Haven 2K Orman zenginlestirmesi (SM_ForestTerrain engebe kabugu cimenleri ve bacaklari gomdugu icin devre disi)

		UStaticMesh* Pine01 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_PineTree_01.SM_PineTree_01"));
		UStaticMesh* Pine02 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_PineTree_02.SM_PineTree_02"));
		UStaticMesh* SaplingPine = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Sapling_Pine.SM_Sapling_Pine"));
		UStaticMesh* DeadTree = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_DeadTree_01.SM_DeadTree_01"));
		UStaticMesh* Grass01 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Grass_Clump_01.SM_Grass_Clump_01"));
		UStaticMesh* Grass02 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Grass_Clump_02.SM_Grass_Clump_02"));
		UStaticMesh* Fern01  = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Fern_01.SM_Fern_01"));
		UStaticMesh* Rock01 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01"));
		UStaticMesh* RockMoss = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_ForestRock_Moss.SM_ForestRock_Moss"));
		UStaticMesh* HouseMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_VillageHouse_01.SM_VillageHouse_01"));
		UStaticMesh* TentMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_SurvivorTent.SM_SurvivorTent"));
		UStaticMesh* CrateMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Crate.SM_Crate"));
		UStaticMesh* BarrelMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Barrel.SM_Barrel"));
		UStaticMesh* LogMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_WoodLog.SM_WoodLog"));

		// Koy evleri (430 cm gercekci boyutta, genis acik meydanin kenarlarina yerlesim)
		if (HouseMesh)
		{
			SpawnForestProp(World, HouseMesh, FVector(320.0f, 920.0f, BaseZ),   FRotator(0.0f, 38.0f, 0.0f),   430.0f, HouseSlots, MatWood);
			SpawnForestProp(World, HouseMesh, FVector(2520.0f, 940.0f, BaseZ),  FRotator(0.0f, 142.0f, 0.0f),  420.0f, HouseSlots, MatWood);
			SpawnForestProp(World, HouseMesh, FVector(2480.0f, 2520.0f, BaseZ), FRotator(0.0f, -135.0f, 0.0f), 440.0f, HouseSlots, MatWood);
		}
		if (TentMesh)
		{
			SpawnForestProp(World, TentMesh, FVector(680.0f, 1580.0f, BaseZ),  FRotator(0.0f, -35.0f, 0.0f), 210.0f, TentSlots, MatTent);
			SpawnForestProp(World, TentMesh, FVector(1980.0f, 2280.0f, BaseZ), FRotator(0.0f, -120.0f, 0.0f),205.0f, TentSlots, MatTent);
		}
		if (CrateMesh && BarrelMesh && LogMesh)
		{
			SpawnForestProp(World, CrateMesh,  FVector(680.0f, 1220.0f, BaseZ),  FRotator(0.0f, 18.0f, 0.0f), 85.0f, CrateSlots, MatWood);
			SpawnForestProp(World, BarrelMesh, FVector(745.0f, 1180.0f, BaseZ),  FRotator(0.0f, 0.0f, 0.0f),  92.0f, CrateSlots, MatWood);
			SpawnForestProp(World, LogMesh,    FVector(660.0f, 1650.0f, BaseZ),  FRotator(0.0f, 75.0f, 0.0f), 140.0f, CrateSlots, MatBark);

			// ============================================================================================
			// FAZ 1 (KOLONI TEMELI): MADEN DAMARLARI, KOY AMBARI, TUZ AMBARI & INSAAT SANTIYESI
			// ============================================================================================
			UStaticMesh* BoardsPropMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_Boards.SM_Barikat_Boards"));
			UStaticMesh* FencePropMesh  = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_WoodFence.SM_Barikat_WoodFence"));

			// 1. Demir Damarlari (OreVein_Iron) — Koyun bati/kuzeybati kayalik yamacinda
			if (Rock01)
			{
				const FVector IronVeinLocs[] = {
					FVector(910.0f, 1360.0f, BaseZ),
					FVector(835.0f, 1445.0f, BaseZ),
					FVector(955.0f, 1275.0f, BaseZ)
				};
				for (int32 v = 0; v < 3; ++v)
				{
					if (AStaticMeshActor* IronVein = SpawnForestProp(World, Rock01, IronVeinLocs[v], FRotator(0.0f, float(v * 68), 0.0f), 145.0f - float(v * 15), RockSlots, MatStone, 1.18f))
					{
						IronVein->Tags.AddUnique(FName("OreVein"));
						IronVein->Tags.AddUnique(FName("OreVein_Iron"));
						if (UPointLightComponent* OreGlow = NewObject<UPointLightComponent>(IronVein, TEXT("IronOreVeinGlow")))
						{
							OreGlow->SetupAttachment(IronVein->GetStaticMeshComponent());
							OreGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
							OreGlow->SetIntensity(2200.0f);
							OreGlow->SetLightColor(FLinearColor(1.0f, 0.42f, 0.12f));
							OreGlow->SetAttenuationRadius(260.0f);
							OreGlow->SetCastShadows(false);
							OreGlow->RegisterComponent();
						}
					}
				}
			}

			// 2. Kiyi Tuz Kayalari (OreVein_Salt — GDD Bolum 7 Tuz Ekonomisi) — Guneydogu kiyi kumsali
			if (RockMoss || Rock01)
			{
				UStaticMesh* SaltMesh = RockMoss ? RockMoss : Rock01;
				const FVector SaltVeinLocs[] = {
					FVector(1165.0f, 2065.0f, BaseZ),
					FVector(1225.0f, 2135.0f, BaseZ)
				};
				for (int32 s = 0; s < 2; ++s)
				{
					if (AStaticMeshActor* SaltVein = SpawnForestProp(World, SaltMesh, SaltVeinLocs[s], FRotator(0.0f, float(s * 95 + 25), 0.0f), 125.0f, RockSlots, MatPlaster, 1.15f))
					{
						SaltVein->Tags.AddUnique(FName("OreVein"));
						SaltVein->Tags.AddUnique(FName("OreVein_Salt"));
						if (UPointLightComponent* SaltGlow = NewObject<UPointLightComponent>(SaltVein, TEXT("SaltRockVeinGlow")))
						{
							SaltGlow->SetupAttachment(SaltVein->GetStaticMeshComponent());
							SaltGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
							SaltGlow->SetIntensity(2400.0f);
							SaltGlow->SetLightColor(FLinearColor(0.75f, 0.96f, 1.0f));
							SaltGlow->SetAttenuationRadius(250.0f);
							SaltGlow->SetCastShadows(false);
							SaltGlow->RegisterComponent();
						}
					}
				}
			}

			// 3. Koy Depolari (ColonyStorageChest) ve Tuz Ambari (SaltGranary — GDD 7 Hazine Yapisi)
			if (AStaticMeshActor* StorageChest = SpawnForestProp(World, CrateMesh, FVector(1035.0f, 1545.0f, BaseZ), FRotator(0.0f, 22.0f, 0.0f), 96.0f, CrateSlots, MatWood, 1.25f))
			{
				StorageChest->Tags.AddUnique(FName("ColonyStorageChest"));
				if (UPointLightComponent* StorageLight = NewObject<UPointLightComponent>(StorageChest, TEXT("ColonyStorageBeacon")))
				{
					StorageLight->SetupAttachment(StorageChest->GetStaticMeshComponent());
					StorageLight->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
					StorageLight->SetIntensity(2600.0f);
					StorageLight->SetLightColor(FLinearColor(1.0f, 0.78f, 0.25f));
					StorageLight->SetAttenuationRadius(280.0f);
					StorageLight->SetCastShadows(false);
					StorageLight->RegisterComponent();
				}
			}
			if (AStaticMeshActor* SaltGranary = SpawnForestProp(World, BarrelMesh, FVector(985.0f, 1615.0f, BaseZ), FRotator(0.0f, -15.0f, 0.0f), 108.0f, CrateSlots, MatDarkWood, 1.25f))
			{
				SaltGranary->Tags.AddUnique(FName("ColonyStorageChest"));
				SaltGranary->Tags.AddUnique(FName("SaltGranary"));
			}
			if (AStaticMeshActor* LogPile = SpawnForestProp(World, LogMesh, FVector(995.0f, 1485.0f, BaseZ), FRotator(0.0f, 65.0f, 0.0f), 115.0f, CrateSlots, MatBark, 1.1f))
			{
				LogPile->Tags.AddUnique(FName("ColonyStorageChest"));
			}

			// 4. Koloni Insaat Santiyesi (ColonyBuildSite: 3 Kisilik Demirci Ocagi -> Kiyi Tuzlasi -> Kutuphane)
			const FVector BuildSiteCenter(1065.0f, 1745.0f, BaseZ);
			if (AStaticMeshActor* BuildSiteFrame = SpawnForestProp(World, FencePropMesh ? FencePropMesh : (BoardsPropMesh ? BoardsPropMesh : CrateMesh), BuildSiteCenter, FRotator(0.0f, 35.0f, 0.0f), 155.0f, CrateSlots, MatWood, 1.25f))
			{
				BuildSiteFrame->Tags.AddUnique(FName("ColonyBuildSite"));
				if (UPointLightComponent* SiteLight = NewObject<UPointLightComponent>(BuildSiteFrame, TEXT("BuildSiteForgeGlow")))
				{
					SiteLight->SetupAttachment(BuildSiteFrame->GetStaticMeshComponent());
					SiteLight->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
					SiteLight->SetIntensity(3200.0f);
					SiteLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.15f));
					SiteLight->SetAttenuationRadius(340.0f);
					SiteLight->SetCastShadows(false);
					SiteLight->RegisterComponent();
				}
			}
			if (Rock01)
			{
				if (AStaticMeshActor* ForgeAnvilStone = SpawnForestProp(World, Rock01, BuildSiteCenter + FVector(55.0f, -35.0f, 0.0f), FRotator(0.0f, 15.0f, 0.0f), 82.0f, RockSlots, MatStone, 1.15f))
				{
					ForgeAnvilStone->Tags.AddUnique(FName("ColonyBuildSite"));
				}
			}
			if (BoardsPropMesh)
			{
				if (AStaticMeshActor* SitePlanks = SpawnForestProp(World, BoardsPropMesh, BuildSiteCenter + FVector(-50.0f, 45.0f, 0.0f), FRotator(0.0f, -40.0f, 0.0f), 65.0f, CrateSlots, MatWood, 1.15f))
				{
					SitePlanks->Tags.AddUnique(FName("ColonyBuildSite"));
				}
			}

			// 5. Baslangicta yerde hazir duran fiziksel Dunya Loot paketleri (Oyuncu [E] ile toplayabilir)
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(World, FVector(955.0f, 1405.0f, BaseZ), FName("Loot_IronOre"), 0);
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(World, FVector(985.0f, 1435.0f, BaseZ), FName("Loot_Stone"),   1);
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(World, FVector(1095.0f, 1465.0f, BaseZ), FName("Loot_Wood"),   2);
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(World, FVector(895.0f, 1895.0f, BaseZ), FName("Loot_Salt"),    3);

			// 6. FAZ 2 (KADIM TARIFLER + GDD 16): Kadim Boss Uretim Tezgahi & Koloni Kutuphanesi Kursusu (AncientCraftingAltar / ColonyLibraryLectern)
			const FVector AltarCenter(1115.0f, 1655.0f, BaseZ);
			if (AStaticMeshActor* AltarBase = SpawnForestProp(World, CrateMesh, AltarCenter, FRotator(0.0f, -25.0f, 0.0f), 92.0f, CrateSlots, MatDarkWood, 1.35f))
			{
				AltarBase->Tags.AddUnique(FName("AncientCraftingAltar"));
				AltarBase->Tags.AddUnique(FName("ColonyLibraryLectern"));
				if (UPointLightComponent* AltarGlow = NewObject<UPointLightComponent>(AltarBase, TEXT("AncientAltarLibraryGlow")))
				{
					AltarGlow->SetupAttachment(AltarBase->GetStaticMeshComponent());
					AltarGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
					AltarGlow->SetIntensity(3600.0f);
					AltarGlow->SetLightColor(FLinearColor(0.68f, 0.32f, 1.0f));
					AltarGlow->SetAttenuationRadius(310.0f);
					AltarGlow->SetCastShadows(false);
					AltarGlow->RegisterComponent();
				}
			}
		}

		// Poly Haven 2K PBR materyallerini yukle (agaclar ve 3B cimenler icin)
		UMaterialInterface* MatPH_PineBark  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_PineBark.M_PH_PineBark"));
		UMaterialInterface* MatPH_PineTrunk = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_PineTrunk.M_PH_PineTrunk"));
		UMaterialInterface* MatPH_PineTwig  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_PineTwig.M_PH_PineTwig"));
		UMaterialInterface* MatPH_FirBr     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_FirBranches.M_PH_FirBranches"));
		UMaterialInterface* MatPH_FirTw     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_FirTwigs.M_PH_FirTwigs"));
		UMaterialInterface* MatPH_SapBk     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_SaplingBark.M_PH_SaplingBark"));
		UMaterialInterface* MatPH_SapTw     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_SaplingTwig.M_PH_SaplingTwig"));
		UMaterialInterface* MatPH_Dead      = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_DeadTree.M_PH_DeadTree"));
		UMaterialInterface* MatPH_Grass1    = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_Grass01.M_PH_Grass01"));
		UMaterialInterface* MatPH_Grass2    = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_Grass02.M_PH_Grass02"));
		UMaterialInterface* MatPH_Fern1     = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_PH_Fern01.M_PH_Fern01"));

		auto ApplyPolyHavenTreeMats = [&](AStaticMeshActor* TreeAct, UStaticMesh* TMesh)
		{
			if (!TreeAct || !TreeAct->GetStaticMeshComponent() || !TMesh)
			{
				return;
			}
			UStaticMeshComponent* TSMC = TreeAct->GetStaticMeshComponent();
			if (TMesh == Pine01)
			{
				if (MatPH_PineBark)  TSMC->SetMaterial(0, MatPH_PineBark);
				if (MatPH_PineTrunk) TSMC->SetMaterial(1, MatPH_PineTrunk);
				if (MatPH_PineTwig)  TSMC->SetMaterial(2, MatPH_PineTwig);
				if (MatPH_PineBark)  TSMC->SetMaterial(3, MatPH_PineBark);
			}
			else if (TMesh == Pine02)
			{
				if (MatPH_FirBr) TSMC->SetMaterial(0, MatPH_FirBr);
				if (MatPH_FirTw) TSMC->SetMaterial(1, MatPH_FirTw);
			}
			else if (TMesh == SaplingPine)
			{
				if (MatPH_SapBk) TSMC->SetMaterial(0, MatPH_SapBk);
				if (MatPH_SapTw) TSMC->SetMaterial(1, MatPH_SapTw);
			}
			else if (TMesh == DeadTree)
			{
				if (MatPH_Dead) TSMC->SetMaterial(0, MatPH_Dead);
			}
		};

		// 2a. Gercekci 3B Cimen & Egrelti Otu ortusu (Poly Haven 2K PBR glTF — UInstancedStaticMeshComponent ile yuksek performansli 1800 kume)
		const FVector Center(1380.0f, 1680.0f, BaseZ);
		if (Grass01 || Grass02 || Fern01)
		{
			FActorSpawnParameters GrassSP;
			GrassSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* FoliageHolder = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(Center.X, Center.Y, BaseZ), FRotator::ZeroRotator, GrassSP))
			{
				FoliageHolder->SetMobility(EComponentMobility::Movable);
				auto CreateGrassISMC = [&](UStaticMesh* GMesh, FName CompName, UMaterialInterface* OverrideMat) -> UInstancedStaticMeshComponent*
				{
					if (!GMesh)
					{
						return nullptr;
					}
					UInstancedStaticMeshComponent* ISMC = NewObject<UInstancedStaticMeshComponent>(FoliageHolder, CompName);
					ISMC->SetMobility(EComponentMobility::Movable);
					ISMC->SetStaticMesh(GMesh);
					if (OverrideMat)
					{
						ISMC->SetMaterial(0, OverrideMat);
					}
					ISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					ISMC->SetCastShadow(false);
					ISMC->SetupAttachment(FoliageHolder->GetRootComponent());
					ISMC->RegisterComponent();
					return ISMC;
				};

				UInstancedStaticMeshComponent* ISMC_Grass1 = CreateGrassISMC(Grass01, TEXT("ISMC_GrassClump01"), MatPH_Grass1);
				UInstancedStaticMeshComponent* ISMC_Grass2 = CreateGrassISMC(Grass02 ? Grass02 : Grass01, TEXT("ISMC_GrassClump02"), MatPH_Grass2 ? MatPH_Grass2 : MatPH_Grass1);
				UInstancedStaticMeshComponent* ISMC_Fern1  = CreateGrassISMC(Fern01 ? Fern01 : Grass01, TEXT("ISMC_Fern01"), MatPH_Fern1 ? MatPH_Fern1 : MatPH_Grass1);

				const int32 TotalGrassClusters = 1800;
				for (int32 g = 0; g < TotalGrassClusters; ++g)
				{
					// Altin oran spirali + dogal kumelenme ile hem acik cayirda hem orman zemini boyunca yogun 3B cimen yerlesimi
					const float Frac = float(g) / float(TotalGrassClusters);
					const float GAngle = float(g) * 2.3999632f;
					const float GRadius = 25.0f + FMath::Sqrt(Frac) * 2950.0f + FMath::Sin(float(g * 19)) * 45.0f;
					const float GX = Center.X + FMath::Cos(GAngle) * GRadius;
					const float GY = Center.Y + FMath::Sin(GAngle) * GRadius;

					// Derin su bolgesinin (Guneydogu kiyisi) icine cimen koyma
					if (FVector::Dist2D(FVector(GX, GY, 0.0f), FVector(920.0f, 2120.0f, 0.0f)) < 460.0f)
					{
						continue;
					}

					UInstancedStaticMeshComponent* TargetISMC = (g % 7 == 0 && ISMC_Fern1) ? ISMC_Fern1 : ((g % 2 == 0 && ISMC_Grass1) ? ISMC_Grass1 : ISMC_Grass2);
					if (!TargetISMC || !TargetISMC->GetStaticMesh())
					{
						continue;
					}
					UStaticMesh* GMesh = TargetISMC->GetStaticMesh();
					const float DesiredH = (TargetISMC == ISMC_Fern1)
						? (56.0f + float((g * 13) % 32))
						: (48.0f + float((g * 17) % 30));
					const float XYMult = (TargetISMC == ISMC_Fern1) ? 1.15f : 1.75f;
					const FVector GScale = ComputeTargetHeightScale(GMesh, DesiredH, XYMult);
					const float GZ = ComputeMeshGroundLocationZ(GMesh, GScale, BaseZ);
					const FTransform InstTransform(
						FRotator(FMath::Sin(float(g)) * 3.5f, float((g * 73) % 360), FMath::Cos(float(g)) * 3.5f),
						FVector(GX, GY, GZ),
						GScale
					);
					TargetISMC->AddInstance(InstTransform, true);
				}
			}
		}

		// Ferah ve gercekci orman kusagi: Merkezde ~42 metrelik genis acik cayir/arena birakir (Radius = 2120 cm'den baslar!)
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			const int32 TreesInRing = 14 + Ring * 6;
			const float Radius = 2120.0f + Ring * 760.0f;
			for (int32 i = 0; i < TreesInRing; ++i)
			{
				const float Angle = (2.0f * PI * float(i) / float(TreesInRing)) + float(Ring) * 0.42f;
				const float JitterR = Radius + FMath::Sin(float(i * 7 + Ring * 13)) * 120.0f;
				const FVector TreeLoc(Center.X + FMath::Cos(Angle) * JitterR, Center.Y + FMath::Sin(Angle) * JitterR, BaseZ);
				const FRotator TreeRot(0.0f, float((i * 53 + Ring * 97) % 360), 0.0f);
				// Yasli Ulu Agaclar (ElderTree): Poly Haven 2K fotogrametri cam/goknar modelleri (920 - 1220 cm)
				const float ElderTreeHeight = 920.0f + float((i * 29 + Ring * 41) % 300);

				UStaticMesh* ChosenTree = (i % 2 == 0 && Pine01) ? Pine01 : (Pine02 ? Pine02 : Pine01);
				if (AStaticMeshActor* ElderTreeAct = SpawnForestProp(World, ChosenTree, TreeLoc, TreeRot, ElderTreeHeight, TreeSlots, nullptr))
				{
					ApplyPolyHavenTreeMats(ElderTreeAct, ChosenTree);
					ElderTreeAct->Tags.AddUnique(FName("ChopTree"));
					ElderTreeAct->Tags.AddUnique(FName("ElderTree"));
				}
				// Devrilmis kutukler (SM_DeadTree_01: Poly Haven yatay devrilmis agac govdesi, 260 cm uzunlukta yerde durur)
				if (Ring == 0 && i % 4 == 0 && DeadTree)
				{
					const FVector LogLoc = TreeLoc + FVector(FMath::Cos(Angle + 0.3f) * 190.0f, FMath::Sin(Angle + 0.3f) * 190.0f, 0.0f);
					if (AStaticMeshActor* DeadAct = SpawnForestProp(World, DeadTree, LogLoc, FRotator(0.0f, float((i * 67) % 360), 0.0f), 260.0f, TreeSlots, nullptr))
					{
						ApplyPolyHavenTreeMats(DeadAct, DeadTree);
					}
				}

				// Yasli agaclarin yamacinda buyuyen genc ve kisa fidanlar (YoungTree: 235 - 295 cm)
				if (i % 2 == 0 && (SaplingPine || Pine02 || Pine01))
				{
					UStaticMesh* YoungMesh = SaplingPine ? SaplingPine : (Pine02 ? Pine02 : Pine01);
					const FVector ToCenterDir = (Center - TreeLoc).GetSafeNormal2D();
					const FVector YoungTreeLoc = TreeLoc + ToCenterDir * 155.0f + FVector(FMath::Sin(Angle) * 55.0f, FMath::Cos(Angle) * 55.0f, 0.0f);
					const float YoungTreeHeight = 235.0f + float((i * 17) % 60);
					if (AStaticMeshActor* YoungTreeAct = SpawnForestProp(World, YoungMesh, YoungTreeLoc, FRotator(0.0f, float((i * 113) % 360), 0.0f), YoungTreeHeight, TreeSlots, nullptr))
					{
						ApplyPolyHavenTreeMats(YoungTreeAct, YoungMesh);
						YoungTreeAct->Tags.AddUnique(FName("ChopTree"));
						YoungTreeAct->Tags.AddUnique(FName("YoungTree"));
					}
				}

				if (i % 3 == 0 && (RockMoss || Rock01))
				{
					const FVector RockLoc = TreeLoc + FVector(FMath::Cos(Angle + 0.5f) * 145.0f, FMath::Sin(Angle + 0.5f) * 145.0f, 0.0f);
					UStaticMesh* ChosenRock = (i % 2 == 0 && RockMoss) ? RockMoss : Rock01;
					if (AStaticMeshActor* RingRock = SpawnForestProp(World, ChosenRock, RockLoc, FRotator(0.0f, float((i * 83) % 360), 0.0f), 110.0f + float((i % 4) * 28), RockSlots, MatRock))
					{
						RingRock->Tags.AddUnique(FName("OreVein"));
						RingRock->Tags.AddUnique(FName("OreVein_Stone"));
					}
				}
			}
		}

		// 2b. V2 ONCELIK #1 BOSS SISTEMI VE 5 FARKLI ZOMBI TURU (Aylak, Kosucu, Siskin, Cigirtkan, Zirhli)
		UClass* ZirhliBPClass    = LoadClass<ACombatEnemy>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Zirhli.BP_Zombie_Zirhli_C"));
		UClass* AylakBPClass     = LoadClass<ACombatEnemy>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Aylak.BP_Zombie_Aylak_C"));
		UClass* KosucuBPClass    = LoadClass<ACombatEnemy>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Kosucu.BP_Zombie_Kosucu_C"));
		UClass* SiskinBPClass    = LoadClass<ACombatEnemy>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Siskin.BP_Zombie_Siskin_C"));
		UClass* CigirtkanBPClass = LoadClass<ACombatEnemy>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Cigirtkan.BP_Zombie_Cigirtkan_C"));
		if (!ZirhliBPClass)
		{
			ZirhliBPClass = AylakBPClass;
		}

		struct FZombieTypeSpec
		{
			UClass* BPClass;
			FName RowName;
			const TCHAR* ModelPrefix;
			FVector RigScale;
			FVector Spot;
		};
		const FZombieTypeSpec ZombieSpecs[] = {
			{ AylakBPClass,                              FName("Aylak"),     TEXT("ZomWalker"),   FVector(1.00f, 1.00f, 1.00f), FVector(1220.0f, 860.0f, BaseZ + 92.0f) },
			{ KosucuBPClass ? KosucuBPClass : AylakBPClass, FName("Kosucu"),    TEXT("ZomRunner"),   FVector(0.94f, 0.94f, 1.06f), FVector(1360.0f, 820.0f, BaseZ + 92.0f) },
			{ SiskinBPClass ? SiskinBPClass : AylakBPClass, FName("Siskin"),    TEXT("ZomBloater"),  FVector(1.26f, 1.26f, 1.14f), FVector(1500.0f, 810.0f, BaseZ + 92.0f) },
			{ CigirtkanBPClass ? CigirtkanBPClass : AylakBPClass, FName("Cigirtkan"), TEXT("ZomScreamer"), FVector(0.90f, 0.90f, 1.15f), FVector(1640.0f, 835.0f, BaseZ + 92.0f) },
			{ ZirhliBPClass,                             FName("Zirhli"),    TEXT("ZomTank"),     FVector(1.20f, 1.20f, 1.10f), FVector(1780.0f, 880.0f, BaseZ + 92.0f) }
		};

		// Sahnedeki mevcut zombileri ve 5 farkli zombi turunu kuzey barikat hattinin disina yerlestir
		TArray<ACombatEnemy*> SceneZombies;
		for (TActorIterator<ACombatEnemy> ZIt(World); ZIt; ++ZIt)
		{
			ACombatEnemy* ExistingEnemy = *ZIt;
			if (IsValid(ExistingEnemy) &&
				!ExistingEnemy->ActorHasTag(FName("Boss_Sunflower")) &&
				!ExistingEnemy->ActorHasTag(FName("Boss_Treeguard")) &&
				!ExistingEnemy->ActorHasTag(FName("Boss_Pirate")) &&
				!ExistingEnemy->ActorHasTag(FName("Boss_AncientGuardian")) &&
				!ExistingEnemy->ActorHasTag(FName("WanderingKnight")))
			{
				SceneZombies.Add(ExistingEnemy);
			}
		}

		for (int32 zIdx = 0; zIdx < 5; ++zIdx)
		{
			const FZombieTypeSpec& Spec = ZombieSpecs[zIdx];
			ACombatEnemy* ZActor = SceneZombies.IsValidIndex(zIdx) ? SceneZombies[zIdx] : nullptr;
			if (!ZActor && Spec.BPClass)
			{
				FActorSpawnParameters ZSP;
				ZSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ZActor = World->SpawnActor<ACombatEnemy>(Spec.BPClass, Spec.Spot, FRotator(0.0f, 95.0f, 0.0f), ZSP);
			}
			if (ZActor)
			{
				ZActor->Tags.AddUnique(Spec.RowName);
				if (FNameProperty* RowProp = FindFProperty<FNameProperty>(ACombatEnemy::StaticClass(), TEXT("ZombiTipiSatirAdi")))
				{
					RowProp->SetPropertyValue_InContainer(ZActor, Spec.RowName);
				}
				ZActor->SetActorLocationAndRotation(Spec.Spot, FRotator(0.0f, 95.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(ZActor))
				{
					OwnerRig->InitializeRig(ZActor, ZActor->GetMesh(), Spec.ModelPrefix, true, Spec.RigScale);
				}
			}
		}

		// Boss 1: Aycicegi Tarlasinin Koruyucusu (8.75 Metre Dev Pehlivan — Acik Cayir Arenasinda!)
		if (ZirhliBPClass)
		{
			const FTransform Boss1Transform(FRotator(0.0f, -168.0f, 0.0f), FVector(1680.0f, 1620.0f, BaseZ + 92.0f));
			if (ACombatEnemy* SunflowerBoss = World->SpawnActorDeferred<ACombatEnemy>(ZirhliBPClass, Boss1Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				SunflowerBoss->Tags.Add(FName("Boss_Sunflower"));
				UGameplayStatics::FinishSpawningActor(SunflowerBoss, Boss1Transform);

				if (UPointLightComponent* GoddessAura = NewObject<UPointLightComponent>(SunflowerBoss, TEXT("GoddessSunflowerAura")))
				{
					GoddessAura->SetupAttachment(SunflowerBoss->GetRootComponent());
					GoddessAura->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
					GoddessAura->SetIntensity(7500.0f);
					GoddessAura->SetLightColor(FLinearColor(1.0f, 0.78f, 0.14f));
					GoddessAura->SetAttenuationRadius(820.0f);
					GoddessAura->SetCastShadows(false);
					GoddessAura->RegisterComponent();
				}
			}
		}

		// Minik Aycicegi Yancilari (SM_Sunflower_Crown) ve Sivi Yag Surulmus Ayakkabi (SM_Boots_Oiled) modellerini Boss 1'in acik arenasinda sergile
		if (UStaticMesh* CrownMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Sunflower_Crown.SM_Sunflower_Crown")))
		{
			UMaterialInterface* SunMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR"));
			const FBoxSphereBounds CBounds = CrownMesh->GetBounds();
			const float CMaxExt = FMath::Max3(CBounds.BoxExtent.X, CBounds.BoxExtent.Y, CBounds.BoxExtent.Z) * 2.0f;
			const float MinionScale = (CMaxExt > 0.01f) ? FMath::Clamp(58.0f / CMaxExt, 0.1f, 8.0f) : 0.78f;
			for (int32 m = 0; m < 3; ++m)
			{
				const float MAng = (float(m) - 1.0f) * 0.65f;
				const FVector MinionLoc(1520.0f + FMath::Sin(MAng) * 85.0f, 1620.0f + float(m - 1) * 110.0f, BaseZ + 32.0f);
				FActorSpawnParameters MSP;
				MSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* MinionAct = World->SpawnActor<AStaticMeshActor>(MinionLoc, FRotator(22.0f, -165.0f, 0.0f), MSP))
				{
					MinionAct->SetMobility(EComponentMobility::Movable);
					MinionAct->GetStaticMeshComponent()->SetStaticMesh(CrownMesh);
					MinionAct->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					if (SunMat)
					{
						MinionAct->GetStaticMeshComponent()->SetMaterial(0, SunMat);
					}
					MinionAct->SetActorScale3D(FVector(MinionScale, MinionScale, MinionScale * 1.15f));
					MinionAct->Tags.AddUnique(FName("SunflowerMinion"));
				}
			}
		}
		if (UStaticMesh* BootsMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Boots_Oiled.SM_Boots_Oiled")))
		{
			UMaterialInterface* OilMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR"));
			TMap<FString, UMaterialInterface*> BootSlots;
			if (AStaticMeshActor* BootsAct = SpawnForestProp(World, BootsMesh, FVector(1465.0f, 1560.0f, BaseZ), FRotator(0.0f, -155.0f, 0.0f), 38.0f, BootSlots, OilMat ? OilMat : MatSunflower))
			{
				BootsAct->Tags.AddUnique(FName("OiledBootsDrop"));
			}
		}

		// Aycicegi Yag Tenekesi (Projectile Prop - gercekci 32 cm boyutta)
		if (UStaticMesh* OilTinMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Prop_OilTin.SM_Prop_OilTin")))
		{
			TMap<FString, UMaterialInterface*> TinSlots;
			if (AStaticMeshActor* TinProp = SpawnForestProp(World, OilTinMesh, FVector(1545.0f, 1740.0f, BaseZ), FRotator(18.0f, -35.0f, 14.0f), 32.0f, TinSlots, MatSunflower))
			{
				TinProp->Tags.Add(FName("OilTinProjectile"));
			}
		}

		// Boss 2: Treeguard — Orman Muhafizi (8.8 Metre Dev — Orman sinirinda Yasli Ulu Cam ve 2 Genc Fidanin yaninda)
		if (Pine01)
		{
			if (AStaticMeshActor* CourtyardElderPine = SpawnForestProp(World, Pine01, FVector(2260.0f, 1220.0f, BaseZ), FRotator(0.0f, 15.0f, 0.0f), 940.0f, TreeSlots, nullptr))
			{
				CourtyardElderPine->Tags.AddUnique(FName("ChopTree"));
				CourtyardElderPine->Tags.AddUnique(FName("ElderTree"));
			}
			UStaticMesh* YoungSampleMesh = SaplingPine ? SaplingPine : (Pine02 ? Pine02 : Pine01);
			if (AStaticMeshActor* SlopeYoungPine1 = SpawnForestProp(World, YoungSampleMesh, FVector(2040.0f, 1290.0f, BaseZ), FRotator(0.0f, 45.0f, 0.0f), 225.0f, TreeSlots, nullptr))
			{
				SlopeYoungPine1->Tags.AddUnique(FName("ChopTree"));
				SlopeYoungPine1->Tags.AddUnique(FName("YoungTree"));
			}
			if (AStaticMeshActor* SlopeYoungPine2 = SpawnForestProp(World, YoungSampleMesh, FVector(2080.0f, 1140.0f, BaseZ), FRotator(0.0f, -35.0f, 0.0f), 250.0f, TreeSlots, nullptr))
			{
				SlopeYoungPine2->Tags.AddUnique(FName("ChopTree"));
				SlopeYoungPine2->Tags.AddUnique(FName("YoungTree"));
			}
		}
		if (AylakBPClass)
		{
			const FTransform Boss2Transform(FRotator(0.0f, -150.0f, 0.0f), FVector(2120.0f, 1220.0f, BaseZ + 92.0f));
			if (ACombatEnemy* TreeguardBoss = World->SpawnActorDeferred<ACombatEnemy>(AylakBPClass, Boss2Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				TreeguardBoss->Tags.Add(FName("Boss_Treeguard"));
				TreeguardBoss->Tags.Add(FName("Boss_Treeguard_Passive"));
				UGameplayStatics::FinishSpawningActor(TreeguardBoss, Boss2Transform);

				if (UPointLightComponent* ForestAura = NewObject<UPointLightComponent>(TreeguardBoss, TEXT("TreeguardEmeraldAura")))
				{
					ForestAura->SetupAttachment(TreeguardBoss->GetRootComponent());
					ForestAura->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
					ForestAura->SetIntensity(5800.0f);
					ForestAura->SetLightColor(FLinearColor(0.22f, 0.95f, 0.35f));
					ForestAura->SetAttenuationRadius(720.0f);
					ForestAura->SetCastShadows(false);
					ForestAura->RegisterComponent();
				}
			}
		}

		// Suspicious Boulder (Supheli Kaya — Dokununca Acilan Gizli Kadim Sehir Portali)
		if (UStaticMesh* BoulderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Prop_SuspiciousBoulder.SM_Prop_SuspiciousBoulder")))
		{
			TMap<FString, UMaterialInterface*> BoulderSlots;
			if (AStaticMeshActor* PortalBoulder = SpawnForestProp(World, BoulderMesh, FVector(2180.0f, 2150.0f, BaseZ), FRotator(0.0f, -145.0f, 0.0f), 145.0f, BoulderSlots, MatRock))
			{
				PortalBoulder->Tags.Add(FName("SuspiciousBoulder"));
				if (UPointLightComponent* PortalGlow = NewObject<UPointLightComponent>(PortalBoulder, TEXT("WakandaPortalGlow")))
				{
					PortalGlow->SetupAttachment(PortalBoulder->GetStaticMeshComponent());
					PortalGlow->SetRelativeLocation(FVector(55.0f, 0.0f, 85.0f));
					PortalGlow->SetIntensity(11000.0f);
					PortalGlow->SetLightColor(FLinearColor(0.15f, 0.88f, 1.0f));
					PortalGlow->SetAttenuationRadius(680.0f);
					PortalGlow->SetCastShadows(false);
					PortalGlow->RegisterComponent();
				}
			}
		}

		// Gezgin Sovalye (Wandering Knight — Onur Duellosu Teklif Eden Zirhli Sovalye)
		if (ZirhliBPClass)
		{
			const FTransform KnightTransform(FRotator(0.0f, -125.0f, 0.0f), FVector(1290.0f, 1835.0f, BaseZ + 92.0f));
			if (ACombatEnemy* KnightActor = World->SpawnActorDeferred<ACombatEnemy>(ZirhliBPClass, KnightTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				KnightActor->Tags.Add(FName("WanderingKnight"));
				UGameplayStatics::FinishSpawningActor(KnightActor, KnightTransform);

				if (UPointLightComponent* KnightAura = NewObject<UPointLightComponent>(KnightActor, TEXT("WanderingKnightAura")))
				{
					KnightAura->SetupAttachment(KnightActor->GetRootComponent());
					KnightAura->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
					KnightAura->SetIntensity(450.0f);
					KnightAura->SetLightColor(FLinearColor(0.25f, 0.78f, 1.0f));
					KnightAura->SetAttenuationRadius(420.0f);
					KnightAura->SetCastShadows(false);
					KnightAura->RegisterComponent();
				}
			}
		}

		// Boss 3: Korsan Boss (Boss_Pirate — Derin Sular Iskelesinde Cift Elli Kilic Kullanan, Yenilince Cift Elli Kilic Ustaligi & Daha Guclu Olta/Balik Agi Ogreten Korsan Kaptan)
		// Boss 4 (FAZ 2 KADIM TARIFLER + BOSS FAZLARI): Kadim Labirent & Yer Alti Bekcisi (Boss_AncientGuardian) ve Yikilabilir Tas Sutunlar (AncientLabyrinthPillar)
		if (Rock01)
		{
			const FVector PillarLocs[] = {
				FVector(1495.0f, 1285.0f, BaseZ),
				FVector(1685.0f, 1285.0f, BaseZ),
				FVector(1590.0f, 1175.0f, BaseZ)
			};
			for (int32 p = 0; p < 3; ++p)
			{
				if (AStaticMeshActor* PillarAct = SpawnForestProp(World, Rock01, PillarLocs[p], FRotator(0.0f, float(p * 60), 0.0f), 265.0f, RockSlots, MatStone, 0.48f))
				{
					PillarAct->Tags.AddUnique(FName("AncientLabyrinthPillar"));
				}
			}
		}
		if (ZirhliBPClass)
		{
			const FTransform GuardianTransform(FRotator(0.0f, 95.0f, 0.0f), FVector(1590.0f, 1245.0f, BaseZ + 92.0f));
			if (ACombatEnemy* GuardianBoss = World->SpawnActorDeferred<ACombatEnemy>(ZirhliBPClass, GuardianTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				GuardianBoss->Tags.Add(FName("Boss_AncientGuardian"));
				UGameplayStatics::FinishSpawningActor(GuardianBoss, GuardianTransform);

				if (UPointLightComponent* GuardianAura = NewObject<UPointLightComponent>(GuardianBoss, TEXT("AncientGuardianObsidianAura")))
				{
					GuardianAura->SetupAttachment(GuardianBoss->GetRootComponent());
					GuardianAura->SetRelativeLocation(FVector(0.0f, 0.0f, 185.0f));
					GuardianAura->SetIntensity(4800.0f);
					GuardianAura->SetLightColor(FLinearColor(0.65f, 0.25f, 1.0f));
					GuardianAura->SetAttenuationRadius(580.0f);
					GuardianAura->SetCastShadows(false);
					GuardianAura->RegisterComponent();
				}
			}
		}

		if (ZirhliBPClass)
		{
			const FTransform PirateBossTransform(FRotator(0.0f, 162.0f, 0.0f), FVector(1125.0f, 1995.0f, BaseZ + 92.0f));
			if (ACombatEnemy* PirateBoss = World->SpawnActorDeferred<ACombatEnemy>(ZirhliBPClass, PirateBossTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				PirateBoss->Tags.Add(FName("Boss_Pirate"));
				UGameplayStatics::FinishSpawningActor(PirateBoss, PirateBossTransform);

				if (UPointLightComponent* PirateAura = NewObject<UPointLightComponent>(PirateBoss, TEXT("BossPirateCrimsonAura")))
				{
					PirateAura->SetupAttachment(PirateBoss->GetRootComponent());
					PirateAura->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
					PirateAura->SetIntensity(450.0f);
					PirateAura->SetLightColor(FLinearColor(0.95f, 0.32f, 0.18f));
					PirateAura->SetAttenuationRadius(480.0f);
					PirateAura->SetCastShadows(false);
					PirateAura->RegisterComponent();
				}
			}
		}

		// Iblis Kuzgun Easter Egg (Vuruldugunda Dev Iblis-Kuzgun Formuna Donusen Karga)
		if (UStaticMesh* CrowMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01")))
		{
			TMap<FString, UMaterialInterface*> CrowSlots;
			if (AStaticMeshActor* CrowActor = SpawnForestProp(World, CrowMesh, FVector(1425.0f, 1360.0f, BaseZ + 65.0f), FRotator(0.0f, -45.0f, 0.0f), 38.0f, CrowSlots, MatDarkWood))
			{
				CrowActor->Tags.Add(FName("SwainCrow"));
				if (UPointLightComponent* CrowGlow = NewObject<UPointLightComponent>(CrowActor, TEXT("SwainCrowCrimsonGlow")))
				{
					CrowGlow->SetupAttachment(CrowActor->GetStaticMeshComponent());
					CrowGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
					CrowGlow->SetIntensity(450.0f);
					CrowGlow->SetLightColor(FLinearColor(0.95f, 0.12f, 0.18f));
					CrowGlow->SetAttenuationRadius(320.0f);
					CrowGlow->SetCastShadows(false);
					CrowGlow->RegisterComponent();
				}
			}
		}

		// 2c. "DERIN SULAR" KIYISI, TERSANE ISKELESI, GEMI FILOSU VE SUDA YASAM EKOSISTEMI
		// Oyuncu isterse kolonisini kucuk tutup Gemi Filosuyla (Keşif Doktrini) denizden yasayabilir ve kesif yapabilir!
		UStaticMesh* BasicCube   = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMesh* BasicSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UStaticMesh* WoodLogMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_WoodLog.SM_WoodLog"));
		UStaticMesh* BoardsMesh  = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Barikat_Boards.SM_Barikat_Boards"));

		// Derin Su Yuzeyi (Koyun guneydogu kiyisinda parlayan turkuaz-lacivert derin deniz koyu)
		UStaticMesh* BasicCylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		UMaterialInterface* BasicShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicCube)
		{
			FActorSpawnParameters WaterSP;
			WaterSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* DeepWaterSurface = World->SpawnActor<AStaticMeshActor>(FVector(780.0f, 2240.0f, BaseZ + 4.0f), FRotator(0.0f, 22.0f, 0.0f), WaterSP))
			{
				DeepWaterSurface->SetMobility(EComponentMobility::Movable);
				DeepWaterSurface->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
				DeepWaterSurface->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				DeepWaterSurface->SetActorScale3D(FVector(11.5f, 9.2f, 0.06f));
				if (BasicShapeMat)
				{
					if (UMaterialInstanceDynamic* WaterMID = UMaterialInstanceDynamic::Create(BasicShapeMat, DeepWaterSurface))
					{
						WaterMID->SetVectorParameterValue(FName("Color"), FLinearColor(0.02f, 0.32f, 0.52f, 1.0f));
						DeepWaterSurface->GetStaticMeshComponent()->SetMaterial(0, WaterMID);
					}
				}
				DeepWaterSurface->Tags.Add(FName("DeepWaterZone"));

				if (UPointLightComponent* SeaGlow = NewObject<UPointLightComponent>(DeepWaterSurface, TEXT("DeepWaterBioluminescentGlow")))
				{
					SeaGlow->SetupAttachment(DeepWaterSurface->GetRootComponent());
					SeaGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
					SeaGlow->SetIntensity(9500.0f);
					SeaGlow->SetLightColor(FLinearColor(0.06f, 0.72f, 0.92f));
					SeaGlow->SetAttenuationRadius(1250.0f);
					SeaGlow->SetCastShadows(false);
					SeaGlow->RegisterComponent();
				}
			}

			// Yatay Ahsap Tersane Iskelesi (ShipyardPier) — Kiyidan dogrudan turkuaz koyun icine uzanir ([E], [G], [B] etkilesim noktasi)
			if (AStaticMeshActor* PierDeck = World->SpawnActor<AStaticMeshActor>(FVector(955.0f, 2035.0f, BaseZ + 12.0f), FRotator(0.0f, 128.0f, 0.0f), WaterSP))
			{
				PierDeck->SetMobility(EComponentMobility::Movable);
				PierDeck->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
				PierDeck->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PierDeck->SetActorScale3D(FVector(2.8f, 1.15f, 0.12f));
				if (MatWood)
				{
					PierDeck->GetStaticMeshComponent()->SetMaterial(0, MatWood);
				}
				PierDeck->Tags.Add(FName("ShipyardPier"));
			}

			// Pruva Zipkin Topu & Tersane Kaptan Kursusu (HarpoonLauncher) — Iskele ucunda Dev Balinaya nisan alir
			if (AStaticMeshActor* HarpoonGun = World->SpawnActor<AStaticMeshActor>(FVector(935.0f, 2095.0f, BaseZ + 32.0f), FRotator(18.0f, 132.0f, 0.0f), WaterSP))
			{
				HarpoonGun->SetMobility(EComponentMobility::Movable);
				UStaticMesh* HarpoonBaseMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_WarHammer.SM_WarHammer"));
				HarpoonGun->GetStaticMeshComponent()->SetStaticMesh(HarpoonBaseMesh ? HarpoonBaseMesh : BasicCube);
				HarpoonGun->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				HarpoonGun->SetActorScale3D(HarpoonBaseMesh ? FVector(1.45f, 1.45f, 1.75f) : FVector(0.65f, 0.25f, 0.35f));
				HarpoonGun->Tags.AddUnique(FName("HarpoonLauncher"));
				HarpoonGun->Tags.AddUnique(FName("ShipyardPier"));
			}

			// Gemi Filosu Sancak Gemisi (PrimaryFlagshipHull — Iskele saginda demirli, [G] ile Sinif 1 Sal -> 5 Amiral Gemisine evrimlesir)
			if (AStaticMeshActor* FlagshipHull = World->SpawnActor<AStaticMeshActor>(FVector(845.0f, 2005.0f, BaseZ + 18.0f), FRotator(0.0f, 132.0f, 0.0f), WaterSP))
			{
				FlagshipHull->SetMobility(EComponentMobility::Movable);
				FlagshipHull->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
				FlagshipHull->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FlagshipHull->SetActorScale3D(FVector(1.85f, 0.72f, 0.34f));
				if (MatDarkWood)
				{
					FlagshipHull->GetStaticMeshComponent()->SetMaterial(0, MatDarkWood);
				}
				FlagshipHull->Tags.AddUnique(FName("PrimaryFlagshipHull"));
				FlagshipHull->Tags.AddUnique(FName("FleetShip"));
				FlagshipHull->Tags.AddUnique(FName("ShipyardPier"));

				if (UPointLightComponent* ShipLantern = NewObject<UPointLightComponent>(FlagshipHull, TEXT("FleetShipLantern")))
				{
					ShipLantern->SetupAttachment(FlagshipHull->GetRootComponent());
					ShipLantern->SetRelativeLocation(FVector(35.0f, 0.0f, 55.0f));
					ShipLantern->SetIntensity(5200.0f);
					ShipLantern->SetLightColor(FLinearColor(1.0f, 0.72f, 0.22f));
					ShipLantern->SetAttenuationRadius(540.0f);
					ShipLantern->SetCastShadows(false);
					ShipLantern->RegisterComponent();
				}
			}

			// Sancak Gemisi Ahsap Ana Diregi (ShipMastPole) ve Ruzgarla Sisen Yelkeni (ShipMastAndSail)
			if (BasicCylinder)
			{
				if (AStaticMeshActor* MastPoleAct = World->SpawnActor<AStaticMeshActor>(FVector(845.0f, 2005.0f, BaseZ + 85.0f), FRotator::ZeroRotator, WaterSP))
				{
					MastPoleAct->SetMobility(EComponentMobility::Movable);
					MastPoleAct->GetStaticMeshComponent()->SetStaticMesh(BasicCylinder);
					MastPoleAct->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					MastPoleAct->SetActorScale3D(FVector(0.14f, 0.14f, 1.30f));
					if (MatWood)
					{
						MastPoleAct->GetStaticMeshComponent()->SetMaterial(0, MatWood);
					}
					MastPoleAct->Tags.AddUnique(FName("ShipMastPole"));
				}
			}
			if (AStaticMeshActor* ShipSailAct = World->SpawnActor<AStaticMeshActor>(FVector(845.0f, 2005.0f, BaseZ + 102.0f), FRotator(0.0f, 132.0f, 0.0f), WaterSP))
			{
				ShipSailAct->SetMobility(EComponentMobility::Movable);
				ShipSailAct->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
				ShipSailAct->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ShipSailAct->SetActorScale3D(FVector(0.10f, 1.35f, 1.05f));
				if (MatPlaster)
				{
					ShipSailAct->GetStaticMeshComponent()->SetMaterial(0, MatPlaster);
				}
				ShipSailAct->Tags.AddUnique(FName("ShipMastAndSail"));
			}

			// Filo Refakatci Maden & Tuz Gemisi (FleetShip #2)
			if (AStaticMeshActor* EscortHull = World->SpawnActor<AStaticMeshActor>(FVector(745.0f, 1955.0f, BaseZ + 16.0f), FRotator(0.0f, 132.0f, 0.0f), WaterSP))
			{
				EscortHull->SetMobility(EComponentMobility::Movable);
				EscortHull->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
				EscortHull->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				EscortHull->SetActorScale3D(FVector(1.55f, 0.62f, 0.28f));
				if (MatWood)
				{
					EscortHull->GetStaticMeshComponent()->SetMaterial(0, MatWood);
				}
				EscortHull->Tags.AddUnique(FName("FleetShip"));
			}

			// FAZ 3 (C2 - DENIZCILIK): Derin Sular Koyunda Dev Balina (LeviathanWhale)
			if (BasicSphere)
			{
				UMaterialInstanceDynamic* WhaleSkinMID = BasicShapeMat ? UMaterialInstanceDynamic::Create(BasicShapeMat, World) : nullptr;
				if (WhaleSkinMID)
				{
					WhaleSkinMID->SetVectorParameterValue(FName("Color"), FLinearColor(0.03f, 0.09f, 0.16f, 1.0f));
				}
				if (AStaticMeshActor* WhaleBody = World->SpawnActor<AStaticMeshActor>(FVector(760.0f, 2245.0f, BaseZ + 32.0f), FRotator(-6.0f, 42.0f, 0.0f), WaterSP))
				{
					WhaleBody->SetMobility(EComponentMobility::Movable);
					WhaleBody->GetStaticMeshComponent()->SetStaticMesh(BasicSphere);
					WhaleBody->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					WhaleBody->SetActorScale3D(FVector(2.85f, 1.22f, 0.98f));
					if (WhaleSkinMID)
					{
						WhaleBody->GetStaticMeshComponent()->SetMaterial(0, WhaleSkinMID);
					}
					else if (MatStone)
					{
						WhaleBody->GetStaticMeshComponent()->SetMaterial(0, MatStone);
					}
					WhaleBody->Tags.AddUnique(FName("LeviathanWhale"));
					WhaleBody->Tags.AddUnique(FName("DeepWaterLife"));

					if (UPointLightComponent* WhaleGlow = NewObject<UPointLightComponent>(WhaleBody, TEXT("LeviathanWhaleBioAura")))
					{
						WhaleGlow->SetupAttachment(WhaleBody->GetRootComponent());
						WhaleGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
						WhaleGlow->SetIntensity(6800.0f);
						WhaleGlow->SetLightColor(FLinearColor(0.20f, 0.92f, 1.0f));
						WhaleGlow->SetAttenuationRadius(640.0f);
						WhaleGlow->SetCastShadows(false);
						WhaleGlow->RegisterComponent();
					}
				}
				// Dev Balina Kuyruk Yuzgeci (LeviathanWhaleTail)
				if (AStaticMeshActor* WhaleTail = World->SpawnActor<AStaticMeshActor>(FVector(685.0f, 2315.0f, BaseZ + 62.0f), FRotator(34.0f, 42.0f, 0.0f), WaterSP))
				{
					WhaleTail->SetMobility(EComponentMobility::Movable);
					WhaleTail->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
					WhaleTail->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					WhaleTail->SetActorScale3D(FVector(0.22f, 1.35f, 0.65f));
					if (WhaleSkinMID)
					{
						WhaleTail->GetStaticMeshComponent()->SetMaterial(0, WhaleSkinMID);
					}
					else if (MatStone)
					{
						WhaleTail->GetStaticMeshComponent()->SetMaterial(0, MatStone);
					}
					WhaleTail->Tags.AddUnique(FName("LeviathanWhaleTail"));
				}
			}

			// Suda Yasam (DeepWaterLife): Suyun icinde parlayan biyolüminesan balik suruleri, mercan kayaliklari ve sualti yasam izleri
			if (BasicSphere)
			{
				for (int32 FishIdx = 0; FishIdx < 6; ++FishIdx)
				{
					const float FAngle = float(FishIdx) * 1.04f;
					const FVector ShoalLoc(910.0f + FMath::Cos(FAngle) * 185.0f, 2095.0f + FMath::Sin(FAngle) * 165.0f, BaseZ - 6.0f);
					if (AStaticMeshActor* MarineLife = World->SpawnActor<AStaticMeshActor>(ShoalLoc, FRotator(0.0f, FAngle * 57.3f, 0.0f), WaterSP))
					{
						MarineLife->SetMobility(EComponentMobility::Movable);
						MarineLife->GetStaticMeshComponent()->SetStaticMesh(BasicSphere);
						MarineLife->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						MarineLife->SetActorScale3D(FVector(0.42f, 0.16f, 0.12f));
						MarineLife->Tags.Add(FName("DeepWaterLife"));

						if (FishIdx % 2 == 0)
						{
							if (UPointLightComponent* BioLight = NewObject<UPointLightComponent>(MarineLife, TEXT("MarineBioLight")))
							{
								BioLight->SetupAttachment(MarineLife->GetRootComponent());
								BioLight->SetIntensity(2400.0f);
								BioLight->SetLightColor(FLinearColor(0.12f, 0.95f, 0.78f));
								BioLight->SetAttenuationRadius(260.0f);
								BioLight->SetCastShadows(false);
								BioLight->RegisterComponent();
							}
						}
					}
				}
			}
		}

		// Tum Insan / Koylu Modellerini (Villager01, Villager02 Sakalli Savasci, Villager03, Villager04 ve AkilHocasi_Baba) avluda bariscil dolasacak sekilde konumlandir
		int32 VillagerPlaceIdx = 0;
		UClass* VillagerSpawnClass = nullptr;
		for (TActorIterator<ADSVillager> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				if (!VillagerSpawnClass)
				{
					VillagerSpawnClass = It->GetClass();
				}
				if (VillagerPlaceIdx == 0)
				{
					It->SetActorLocationAndRotation(FVector(1335.0f, 1525.0f, BaseZ + 92.0f), FRotator(0.0f, -155.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
					if (FDSCharacterBodyRig* VRig = FDSCharacterBodyRig::GetOrCreateRig(*It))
					{
						VRig->InitializeRig(*It, It->GetMesh(), TEXT("Villager02"), false, FVector(1.0f));
						VRig->WeaponStyle = EDSCombatWeaponStyle::BattleAxe;
						VRig->ApplyWeaponMeshToComponent();
					}
				}
				else if (VillagerPlaceIdx == 1)
				{
					// Kamp atesi yanindaki bilge figur: Akil Hocasi Baba (/Game/Colony/Mentor/SM_FatherHead + M_FatherHead — Olumsuz Ani NPC)
					It->Tags.AddUnique(FName("AkilHocasi_Baba"));
					It->Tags.AddUnique(FName("ImmortalMemoryNPC"));
					It->SetActorLocationAndRotation(FVector(1315.0f, 1765.0f, BaseZ + 92.0f), FRotator(0.0f, -135.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
					if (FDSCharacterBodyRig* VRig = FDSCharacterBodyRig::GetOrCreateRig(*It))
					{
						VRig->InitializeRig(*It, It->GetMesh(), TEXT("Villager02"), false, FVector(1.04f));
						VRig->WeaponStyle = EDSCombatWeaponStyle::BattleAxe;
						VRig->ApplyWeaponMeshToComponent();
					}
				}
				else
				{
					It->SetActorLocationAndRotation(FVector(1385.0f, 1465.0f, BaseZ + 92.0f), FRotator(0.0f, -110.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
					if (FDSCharacterBodyRig* VRig = FDSCharacterBodyRig::GetOrCreateRig(*It))
					{
						VRig->InitializeRig(*It, It->GetMesh(), TEXT("Villager01"), false, FVector(1.0f));
						VRig->WeaponStyle = EDSCombatWeaponStyle::SurvivalAxe;
						VRig->ApplyWeaponMeshToComponent();
					}
				}
				++VillagerPlaceIdx;
			}
		}

		if (!VillagerSpawnClass)
		{
			VillagerSpawnClass = LoadClass<ADSVillager>(nullptr, TEXT("/Game/Colony/BP_Villager_01.BP_Villager_01_C"));
		}
		if (VillagerSpawnClass)
		{
			struct FExtraVillagerShowcase
			{
				FVector Loc;
				FRotator Rot;
				const TCHAR* Prefix;
				FName ProfessionTag;
				EDSCombatWeaponStyle ToolStyle;
			};
			const FExtraVillagerShowcase ExtraHumans[] = {
				{ FVector(1250.0f, 1410.0f, BaseZ + 92.0f), FRotator(0.0f, -120.0f, 0.0f), TEXT("Villager01"), FName("Profession_Lumberjack"), EDSCombatWeaponStyle::SurvivalAxe },
				{ FVector(1185.0f, 1345.0f, BaseZ + 92.0f), FRotator(0.0f, -95.0f, 0.0f),  TEXT("Villager03"), FName("Profession_Miner"),      EDSCombatWeaponStyle::WarHammer   },
				{ FVector(1055.0f, 1525.0f, BaseZ + 92.0f), FRotator(0.0f, 180.0f, 0.0f),  TEXT("Villager02"), FName("Profession_Builder"),    EDSCombatWeaponStyle::WarHammer   },
				// FAZ 4 (D1): Komsu Koy Elcisi (Vicdan Secim Masasi onunde [E] Ittifak / [X] Karanlik Yol Demirci Kacirma / [Z] Reddetme)
				{ FVector(1235.0f, 1585.0f, BaseZ + 92.0f), FRotator(0.0f, -135.0f, 0.0f), TEXT("Villager03"), FName("NeighborVillageEnvoy"),  EDSCombatWeaponStyle::MedievalAxe },
				// FAZ 4 (D3): Korunakli Ani Koyu Kosesi — Olumsuz Ani NPC'leri (Bilge Dede & Sefkatli Anneanne)
				{ FVector(1385.0f, 1745.0f, BaseZ + 92.0f), FRotator(0.0f, -135.0f, 0.0f), TEXT("Villager02"), FName("AniNPC_Dede"),           EDSCombatWeaponStyle::BattleAxe   },
				{ FVector(1355.0f, 1825.0f, BaseZ + 92.0f), FRotator(0.0f, -145.0f, 0.0f), TEXT("Villager01"), FName("AniNPC_Anneanne"),       EDSCombatWeaponStyle::SurvivalAxe }
			};
			for (const FExtraVillagerShowcase& EH : ExtraHumans)
			{
				FActorSpawnParameters VSP;
				VSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ADSVillager* ExtraV = World->SpawnActor<ADSVillager>(VillagerSpawnClass, EH.Loc, EH.Rot, VSP))
				{
					ExtraV->Tags.AddUnique(EH.ProfessionTag);
					if (EH.ProfessionTag == FName("AniNPC_Dede") || EH.ProfessionTag == FName("AniNPC_Anneanne"))
					{
						ExtraV->Tags.AddUnique(FName("ImmortalMemoryNPC"));
					}
					if (EH.ProfessionTag == FName("NeighborVillageEnvoy"))
					{
						FDSColonyAndWorldState::Get(World).HiddenBiteSuspectMap.FindOrAdd(FObjectKey(ExtraV)) = true;
					}
					if (FDSCharacterBodyRig* VRig = FDSCharacterBodyRig::GetOrCreateRig(ExtraV))
					{
						VRig->InitializeRig(ExtraV, ExtraV->GetMesh(), EH.Prefix, false, FVector(1.0f));
						VRig->WeaponStyle = EH.ToolStyle;
						VRig->ApplyWeaponMeshToComponent();
					}
				}
			}

			// FAZ 4 (D1 & D3): Komsu Koy Diplomasi Masasi (NeighborDiplomacyTable) ve Korunakli Ani Kosesi Ocagi (MemorySanctuaryHearth)
			if (BasicCube)
			{
				FActorSpawnParameters F4SP;
				F4SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* DipTable = World->SpawnActor<AStaticMeshActor>(FVector(1205.0f, 1615.0f, BaseZ + 22.0f), FRotator(0.0f, 45.0f, 0.0f), F4SP))
				{
					DipTable->SetMobility(EComponentMobility::Movable);
					DipTable->GetStaticMeshComponent()->SetStaticMesh(BasicCube);
					DipTable->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					DipTable->SetActorScale3D(FVector(0.85f, 0.55f, 0.42f));
					if (MatWood)
					{
						DipTable->GetStaticMeshComponent()->SetMaterial(0, MatWood);
					}
					DipTable->Tags.AddUnique(FName("NeighborDiplomacyTable"));
				}
				if (AStaticMeshActor* MemoryHearth = World->SpawnActor<AStaticMeshActor>(FVector(1345.0f, 1790.0f, BaseZ + 18.0f), FRotator::ZeroRotator, F4SP))
				{
					MemoryHearth->SetMobility(EComponentMobility::Movable);
					MemoryHearth->GetStaticMeshComponent()->SetStaticMesh(Rock01 ? Rock01 : BasicCube);
					MemoryHearth->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					MemoryHearth->SetActorScale3D(FVector(0.55f, 0.55f, 0.35f));
					if (MatStone)
					{
						MemoryHearth->GetStaticMeshComponent()->SetMaterial(0, MatStone);
					}
					MemoryHearth->Tags.AddUnique(FName("MemorySanctuaryHearth"));
					MemoryHearth->Tags.AddUnique(FName("ImmortalMemoryNPC"));

					if (UPointLightComponent* HearthGlow = NewObject<UPointLightComponent>(MemoryHearth, TEXT("MemorySanctuaryWarmGlow")))
					{
						HearthGlow->SetupAttachment(MemoryHearth->GetRootComponent());
						HearthGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
						HearthGlow->SetIntensity(6400.0f);
						HearthGlow->SetLightColor(FLinearColor(1.0f, 0.78f, 0.32f));
						HearthGlow->SetAttenuationRadius(520.0f);
						HearthGlow->SetCastShadows(false);
						HearthGlow->RegisterComponent();
					}
				}
			}
		}

		// 3. Aydinlik, Net Gunduz Gokyuzu ve Isiklandirma (Lit modunda karanlik/siyah gokyuzu kalmamasi icin)
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			ADirectionalLight* DirLight = *It;
			if (DirLight && DirLight->GetLightComponent())
			{
				DirLight->SetActorRotation(FRotator(-46.0f, 36.0f, 0.0f));
				DirLight->GetLightComponent()->SetIntensity(8.5f);
				DirLight->GetLightComponent()->SetLightColor(FLinearColor(0.98f, 0.96f, 0.92f));
			}
		}

		// Gercek zamanli Gokyuzu Atmosferi + SkyLight (Siyah gokyuzunu ve karanlik golgeleri tamamen kaldirir)
		if (!UGameplayStatics::GetActorOfClass(World, ASkyAtmosphere::StaticClass()))
		{
			FActorSpawnParameters SkyAtmSP;
			SkyAtmSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SkyAtmSP);
		}
		ASkyLight* SkyLightActor = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(World, ASkyLight::StaticClass()));
		if (!SkyLightActor)
		{
			FActorSpawnParameters SkyLightSP;
			SkyLightSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SkyLightActor = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(1350.0f, 1680.0f, BaseZ + 600.0f), FRotator::ZeroRotator, SkyLightSP);
		}
		if (SkyLightActor && SkyLightActor->GetLightComponent())
		{
			USkyLightComponent* SLC = SkyLightActor->GetLightComponent();
			SLC->SetMobility(EComponentMobility::Movable);
			SLC->SetIntensity(3.6f);
			SLC->SetLightColor(FLinearColor(0.88f, 0.94f, 1.0f));
			SLC->bLowerHemisphereIsBlack = false;
			SLC->LowerHemisphereColor = FLinearColor(0.32f, 0.36f, 0.30f);
			SLC->RecaptureSky();
		}

		// Avlu uzerine yumusak gokyuzu dolgu isigi (Golgede kalan PBR dokularin net gorunmesi icin)
		if (AStaticMeshActor* SkyFillHolder = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(1350.0f, 1680.0f, BaseZ + 520.0f), FRotator::ZeroRotator))
		{
			UPointLightComponent* SkyFill = NewObject<UPointLightComponent>(SkyFillHolder, TEXT("CourtyardSkyFillLight"));
			if (SkyFill)
			{
				SkyFill->SetupAttachment(SkyFillHolder->GetRootComponent());
				SkyFill->SetRelativeLocation(FVector::ZeroVector);
				SkyFill->SetIntensity(28000.0f);
				SkyFill->SetLightColor(FLinearColor(0.84f, 0.90f, 0.98f));
				SkyFill->SetAttenuationRadius(4200.0f);
				SkyFill->SetCastShadows(false);
				SkyFill->RegisterComponent();
			}
		}

		AExponentialHeightFog* FogActor = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(World, AExponentialHeightFog::StaticClass()));
		if (!FogActor)
		{
			FActorSpawnParameters FogParams;
			FogParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FogActor = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.0f, 0.0f, BaseZ + 60.0f), FRotator::ZeroRotator, FogParams);
		}
		if (FogActor && FogActor->GetComponent())
		{
			UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
			FogComp->SetFogDensity(0.006f);
			FogComp->SetFogHeightFalloff(0.20f);
			FogComp->SetFogInscatteringColor(FLinearColor(0.45f, 0.62f, 0.82f, 1.0f));
			FogComp->SetDirectionalInscatteringColor(FLinearColor(0.55f, 0.68f, 0.85f, 1.0f));
			FogComp->SetVolumetricFog(false);
			FogComp->MarkRenderStateDirty();
		}

		APostProcessVolume* PPV = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(World, APostProcessVolume::StaticClass()));
		if (!PPV)
		{
			FActorSpawnParameters PPParams;
			PPParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			PPV = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, PPParams);
		}
		if (PPV)
		{
			PPV->bUnbound = true;
			PPV->Settings.bOverride_AutoExposureMinBrightness = true;
			PPV->Settings.AutoExposureMinBrightness = 1.0f;
			PPV->Settings.bOverride_AutoExposureMaxBrightness = true;
			PPV->Settings.AutoExposureMaxBrightness = 1.0f;
			PPV->Settings.bOverride_AutoExposureBias = true;
			PPV->Settings.AutoExposureBias = 0.85f;
			PPV->Settings.bOverride_ColorSaturation = true;
			PPV->Settings.ColorSaturation = FVector4(0.95f, 0.95f, 0.98f, 1.0f);
			PPV->Settings.bOverride_ColorContrast = true;
			PPV->Settings.ColorContrast = FVector4(1.04f, 1.04f, 1.06f, 1.0f);
			PPV->Settings.bOverride_VignetteIntensity = true;
			PPV->Settings.VignetteIntensity = 0.22f;
		}
	}
}

ACombatGameMode::ACombatGameMode()
{
	// Guven sistemi bileseni (GDD 4) - Score gibi GameMode'da merkezi tutulur
	TrustComponent = CreateDefaultSubobject<UDSTrustComponent>(TEXT("TrustComponent"));
}

void ACombatGameMode::BeginPlay()
{
	Super::BeginPlay();

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (LevelName.Contains(TEXT("Forest")))
	{
		EnrichRealisticForestMap(GetWorld());

		// Eger PlayerStart collision yuzunden oyuncu pawn'i spawn olamadiysa burada garantili olarak spawn et ve possess et
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			const float BaseZ = FindGroundZ(GetWorld(), 1160.0f, 1520.0f, 200.0f);
			const FVector SafeHeroLoc(1160.0f, 1520.0f, BaseZ + 96.0f);
			const FRotator SafeHeroRot(0.0f, 15.0f, 0.0f);
			if (!PC->GetPawn() && DefaultPawnClass)
			{
				FActorSpawnParameters PawnSP;
				PawnSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (APawn* NewHeroPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, SafeHeroLoc, SafeHeroRot, PawnSP))
				{
					PC->Possess(NewHeroPawn);
				}
			}
			else if (APawn* ExistingPawn = PC->GetPawn())
			{
				ExistingPawn->SetActorLocationAndRotation(SafeHeroLoc, SafeHeroRot, false, nullptr, ETeleportType::TeleportPhysics);
			}
			PC->SetControlRotation(FRotator(-6.0f, 15.0f, 0.0f));
		}
	}

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}

	// kazanma kosulu icin DSWaveManager'in dalga-tamamlandi olayina baglan
	if (ADSWaveManager* WaveManager = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ADSWaveManager::StaticClass())))
	{
		WaveManager->OnWaveCompleted.AddDynamic(this, &ACombatGameMode::NotifyWaveCompleted);
		WaveManager->OnWaveCompleted.AddDynamic(this, &ACombatGameMode::NotifyTrustUpdate);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ACombatGameMode: sahnede DSWaveManager bulunamadi, kazanma kosulu tetiklenemeyecek"));
	}
}

bool ACombatGameMode::IsPeacefulShowcaseMode(const UObject* WorldContext)
{
	const UWorld* World = (WorldContext && GEngine) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const ACombatGameMode* CombatGameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
	return CombatGameMode && CombatGameMode->bPeacefulShowcaseMode;
}

void ACombatGameMode::NotifyPlayerDied()
{
	// kazanma sonrasi yeniden baslatma zaten bekliyorsa ikinci "Oyun Bitti" akisini baslatma
	if (bRestartScheduled)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ACombatGameMode: oyuncu oldu"));

	OnPlayerDied.Broadcast();

	// basit "Game Over" ekran mesaji + RestartDelaySeconds sonra otomatik yeniden baslatma (Asama 10)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, RestartDelaySeconds, FColor::Red, TEXT("OYUN BITTI - 5 saniye sonra yeniden basliyor..."));
	}

	ScheduleLevelRestart();
}

void ACombatGameMode::NotifyWaveCompleted(int32 TamamlananWaveNumarasi)
{
	// oyuncu oldukten sonra (yeniden baslatma bekliyorken) tamamlanan dalga kazandirmaz
	if (bOyunKazanildi || bRestartScheduled)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ACombatGameMode: dalga %d tamamlandi (kazanma hedefi: %d)"), TamamlananWaveNumarasi, KazanmaIcinGerekliDalgaSayisi);

	if (TamamlananWaveNumarasi >= KazanmaIcinGerekliDalgaSayisi)
	{
		bOyunKazanildi = true;

		OnGameWon.Broadcast();

		// basit "Kazandin" ekran mesaji + RestartDelaySeconds sonra otomatik yeniden baslatma (Asama 10)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, RestartDelaySeconds, FColor::Green, TEXT("TEBRIKLER! 5 saniye sonra yeniden basliyor..."));
		}

		ScheduleLevelRestart();
	}
}

bool ACombatGameMode::ScheduleLevelRestart()
{
	if (bRestartScheduled)
	{
		return false;
	}

	bRestartScheduled = true;
	GetWorldTimerManager().SetTimer(RestartTimer, this, &ACombatGameMode::RestartCurrentLevel, RestartDelaySeconds, false);
	return true;
}

void ACombatGameMode::NotifyTrustUpdate(int32 TamamlananWaveNumarasi)
{
	if (!TrustComponent)
	{
		return;
	}

	// sahnedeki tum DSVillager'lari say; gercekten olen (GercekOlum'da ragdoll acilan, henuz
	// sahneden kaldirilmamis olan) koyluler hayatta sayilmaz, ama bIsDowned=true olup henuz
	// kurtarma penceresinde olanlar hayatta sayilir (GDD 4: sonuc-bazli guven)
	int32 ToplamKoylu = 0;
	int32 HayattaKalanKoylu = 0;

	for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
	{
		ADSVillager* Villager = *It;

		// olumsuz Ani NPC'leri hic olmedigi icin sayima girerse "tum koyluler oldu" cezasi hic gelmez
		if (!IsValid(Villager) || Villager->ActorHasTag(FName("ImmortalMemoryNPC")))
		{
			continue;
		}

		++ToplamKoylu;

		const bool bGercektenOldu = Villager->GetMesh() && Villager->GetMesh()->IsSimulatingPhysics();

		if (!bGercektenOldu)
		{
			++HayattaKalanKoylu;
		}
	}

	if (ToplamKoylu == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ACombatGameMode: sahnede DSVillager bulunamadi, guven guncellenmeyecek"));
		return;
	}

	if (HayattaKalanKoylu == ToplamKoylu)
	{
		TrustComponent->AddTrust(15.0f, TEXT("Tum koyluler hayatta"));
	}
	else if (HayattaKalanKoylu > 0)
	{
		TrustComponent->AddTrust(5.0f, TEXT("Kismi kayip"));
	}
	else
	{
		TrustComponent->AddTrust(-10.0f, TEXT("Tum koyluler oldu"));
	}

	// M2+: AddTrust cagrilari buraya eklenecek:
	// AddTrust(+10, "Erzak teslim edildi");
	// AddTrust(+8, "Hasta tedavi edildi");
	// AddTrust(+12, "Yardim cagrisina gidildi");
}

void ACombatGameMode::AddScore(int32 Amount)
{
	Score += Amount;

	OnScoreChanged.Broadcast(Score);
}

void ACombatGameMode::RestartCurrentLevel()
{
	// Blueprint'ten dogrudan cagrilirsa bekleyen zamanlayici ikinci bir OpenLevel acmasin
	GetWorldTimerManager().ClearTimer(RestartTimer);
	bRestartScheduled = true;

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);

	UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
}

AActor* ACombatGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// build the current player tag
	FName PlayerTag = FName(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));

	// find all player starts with the matching player tag
	TArray<AActor*> PlayerStarts;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);

	// increment the player start assignment index
	++CurrentPlayerStartAssignment;

	// if no PlayerStarts were found, default to all PlayerStarts instead
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	// have we found at least one PlayerStart?
	if (!PlayerStarts.IsEmpty())
	{
		AActor* ChosenStart = PlayerStarts[ FMath::RandRange(0, PlayerStarts.Num() - 1) ];
		if (ChosenStart && GetWorld())
		{
			const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
			if (LevelName.Contains(TEXT("Forest")))
			{
				const float BaseZ = FindGroundZ(GetWorld(), 1160.0f, 1520.0f, 200.0f);
				ChosenStart->SetActorLocationAndRotation(FVector(1160.0f, 1520.0f, BaseZ + 120.0f), FRotator(0.0f, 15.0f, 0.0f));
			}
		}
		return ChosenStart;
	}

	// no PlayerStarts in the level
	return nullptr;
}
