// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSVillagerController.h"
#include "DSVillager.h"
#include "CombatEnemy.h"
#include "DSCharacterBodyRig.h"
#include "CombatGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	/** Kacmiyorken (Calisiyor durumunda) uygulanan taban yurume hizi; KacmaHizCarpani bu
	 *  degeri carpar (KacmaHizCarpani = 1 ise kacis hizi bu taban hizla ayni olur) */
	constexpr float NormalYurumeHizi = 300.0f;

	/** Her koylunun son yakin dovus vurusu zamani (saniye) */
	static TMap<TWeakObjectPtr<ADSVillager>, float> GVillagerLastAttackTime;
	static TMap<TWeakObjectPtr<ADSVillager>, float> GVillagerLastWorkSwingTime;
	static TMap<TWeakObjectPtr<ADSVillager>, int32> GVillagerLastDeliveredCycle;

	void PerformBellwrightProfessionWork(ADSVillager* Villager, float DeltaTime, float NowTime)
	{
		if (!IsValid(Villager) || NowTime < 0.35f)
		{
			return;
		}
		if (Villager->ActorHasTag(TEXT("AkilHocasi_Baba")) || Villager->ActorHasTag(TEXT("ImmortalMemoryNPC")) || Villager->ActorHasTag(TEXT("NeighborVillageEnvoy")))
		{
			return;
		}
		if (Villager->GetCharacterMovement())
		{
			if (Villager->GetCharacterMovement()->MovementMode == MOVE_None)
			{
				Villager->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			Villager->GetCharacterMovement()->MaxWalkSpeed = 165.0f;
		}

		const float VZ = Villager->GetActorLocation().Z;
		const FVector StorageSpot(1065.0f, 1545.0f, VZ);
		FVector WorkSpot(1335.0f, 1525.0f, VZ);
		FVector WorkLookTarget(1335.0f, 1350.0f, VZ);
		bool bSwingsToolAtWork = false;

		if (Villager->ActorHasTag(TEXT("Profession_Miner")))
		{
			WorkSpot = FVector(965.0f, 1360.0f, VZ);
			WorkLookTarget = FVector(910.0f, 1360.0f, VZ); // Demir cevheri damari
			bSwingsToolAtWork = true;
		}
		else if (Villager->ActorHasTag(TEXT("Profession_Builder")))
		{
			WorkSpot = FVector(1125.0f, 1745.0f, VZ);
			WorkLookTarget = FVector(1065.0f, 1745.0f, VZ); // Koloni insaat santiyesi
			bSwingsToolAtWork = true;
		}
		else if (Villager->ActorHasTag(TEXT("Profession_Lumberjack")))
		{
			WorkSpot = FVector(1355.0f, 1445.0f, VZ);
			WorkLookTarget = FVector(1420.0f, 1390.0f, VZ); // Kereste kirma noktasi
			bSwingsToolAtWork = true;
		}
		else
		{
			WorkSpot = FVector(1335.0f, 1525.0f, VZ);
			WorkLookTarget = FVector(1380.0f, 1280.0f, VZ);
		}

		const float PhaseOffset = float(GetTypeHash(Villager) & 0xFF) * 0.04f;
		const float TotalTime = NowTime + PhaseOffset;
		const int32 CycleIdx = FMath::FloorToInt(TotalTime / 14.0f);
		const float CycleTime = FMath::Fmod(TotalTime, 14.0f);

		const bool bGoingToWork = (CycleTime < 9.0f);
		const FVector TargetSpot = bGoingToWork ? WorkSpot : StorageSpot;
		const FVector ToTarget = TargetSpot - Villager->GetActorLocation();

		if (ToTarget.Size2D() > 38.0f)
		{
			const FVector Dir2D = ToTarget.GetSafeNormal2D();
			Villager->SetActorRotation(FMath::RInterpTo(Villager->GetActorRotation(), Dir2D.Rotation(), DeltaTime, 6.0f));
			Villager->AddMovementInput(Dir2D, 0.68f);
		}
		else if (bGoingToWork)
		{
			const FVector LookDir = (WorkLookTarget - Villager->GetActorLocation()).GetSafeNormal2D();
			if (!LookDir.IsNearlyZero())
			{
				Villager->SetActorRotation(FMath::RInterpTo(Villager->GetActorRotation(), LookDir.Rotation(), DeltaTime, 7.0f));
			}
			if (bSwingsToolAtWork)
			{
				float& LastSwing = GVillagerLastWorkSwingTime.FindOrAdd(Villager, -10.0f);
				if (NowTime - LastSwing >= 1.85f)
				{
					LastSwing = NowTime;
					if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(Villager))
					{
						OwnerRig->TriggerWeaponSwing(0.95f);
					}
				}
			}
		}
		else
		{
			// Koy Ambarina (StorageSpot) ulastiginda topladigi cevheri/odunu ambara birakir
			int32& LastCycle = GVillagerLastDeliveredCycle.FindOrAdd(Villager, -1);
			if (LastCycle != CycleIdx)
			{
				LastCycle = CycleIdx;
				FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(Villager);
				if (Villager->ActorHasTag(TEXT("Profession_Miner")))
				{
					WS.StoredIronOre += 1;
				}
				else if (Villager->ActorHasTag(TEXT("Profession_Lumberjack")))
				{
					WS.WoodCount += 1;
				}
			}
		}
	}
}

ADSVillagerController::ADSVillagerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADSVillagerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledVillager = Cast<ADSVillager>(InPawn);
	CurrentState = EVillagerState::Calisiyor;
}

AActor* ADSVillagerController::FindClosestZombi(float& OutDistance) const
{
	OutDistance = 0.0f;

	if (!IsValid(ControlledVillager))
	{
		return nullptr;
	}

	TArray<AActor*> Zombiler;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemy::StaticClass(), Zombiler);

	const FVector VillagerLocation = ControlledVillager->GetActorLocation();

	AActor* EnYakinZombi = nullptr;
	float EnYakinMesafeKare = TNumericLimits<float>::Max();

	for (AActor* Zombi : Zombiler)
	{
		ACombatEnemy* Enemy = Cast<ACombatEnemy>(Zombi);
		if (!IsValid(Enemy) || Enemy->CurrentHP <= 0.0f)
		{
			continue;
		}

		// Muttefik Aycicegi Muhafizi, Muttefik Korsan Kaptan, pasif Orman Muhafizi veya bariscil Gezgin Sovalye dusman degildir
		if (Enemy->ActorHasTag(TEXT("Boss_Sunflower_Muttefik")) ||
			Enemy->ActorHasTag(TEXT("Boss_Pirate_Muttefik")) ||
			Enemy->ActorHasTag(TEXT("Boss_Treeguard_Passive")) ||
			(Enemy->ActorHasTag(TEXT("WanderingKnight")) && !Enemy->ActorHasTag(TEXT("WanderingKnight_Hostile"))))
		{
			continue;
		}

		const float MesafeKare = FVector::DistSquared(VillagerLocation, Enemy->GetActorLocation());
		if (MesafeKare < EnYakinMesafeKare)
		{
			EnYakinMesafeKare = MesafeKare;
			EnYakinZombi = Enemy;
		}
	}

	if (EnYakinZombi)
	{
		OutDistance = FMath::Sqrt(EnYakinMesafeKare);
	}

	return EnYakinZombi;
}

void ADSVillagerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(ControlledVillager))
	{
		return;
	}

	// baygin durumdayken kacma mantigi devre disi (EnterDownedState hareketi zaten durdurmustur)
	if (ControlledVillager->bIsDowned)
	{
		CurrentState = EVillagerState::Baygin;
		return;
	}

	// Bariscil Dunya & Modelleme Inceleme Modu: Koyluler koloni meslek dongusunu (Madenci, Insaatci, Oduncu) sakin sekilde icra eder
	if (ACombatGameMode::IsPeacefulShowcaseMode(this))
	{
		CurrentState = EVillagerState::Calisiyor;
		const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		PerformBellwrightProfessionWork(ControlledVillager, DeltaTime, NowTime);
		return;
	}

	float ZombiMesafesi = 0.0f;
	AActor* EnYakinZombi = FindClosestZombi(ZombiMesafesi);

	const EDSVillagerCombatOrder CurrentOrder = FDSColonyAndWorldState::Get(this).VillagerOrder;
	const bool bIsBeardedWarrior = ControlledVillager->ActorHasTag(TEXT("BeardedWarrior"));
	const bool bShouldFight = (CurrentOrder == EDSVillagerCombatOrder::FightOrder) ||
		(CurrentOrder == EDSVillagerCombatOrder::WorkAndDefendOrder && bIsBeardedWarrior);

	const float EngageDistance = (CurrentOrder == EDSVillagerCombatOrder::FightOrder) ? 1850.0f : 1100.0f;

	// Eger savas emri aktifse (veya sakalli savasci gorevdeyse) KACMAZ, dusmanin uzerine yurur ve baltayla savasir!
	if (bShouldFight && EnYakinZombi && ZombiMesafesi <= EngageDistance)
	{
		CurrentState = EVillagerState::Calisiyor;
		if (ControlledVillager->GetCharacterMovement())
		{
			if (ControlledVillager->GetCharacterMovement()->MovementMode == MOVE_None)
			{
				ControlledVillager->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			ControlledVillager->GetCharacterMovement()->MaxWalkSpeed = bIsBeardedWarrior ? 425.0f : 390.0f;
		}

		const FVector VillagerLoc = ControlledVillager->GetActorLocation();
		const FVector EnemyLoc = EnYakinZombi->GetActorLocation();
		const FVector ToEnemyDir = (EnemyLoc - VillagerLoc).GetSafeNormal2D();

		if (!ToEnemyDir.IsNearlyZero())
		{
			ControlledVillager->SetActorRotation(FMath::RInterpTo(ControlledVillager->GetActorRotation(), ToEnemyDir.Rotation(), DeltaTime, 10.0f));
		}

		constexpr float MeleeRange = 215.0f;
		if (ZombiMesafesi > MeleeRange)
		{
			const EPathFollowingRequestResult::Type MoveRes = MoveToActor(EnYakinZombi, 130.0f, true, true, false);
			if (MoveRes == EPathFollowingRequestResult::Failed && !ToEnemyDir.IsNearlyZero())
			{
				ControlledVillager->AddMovementInput(ToEnemyDir, 0.90f);
			}
		}
		else
		{
			StopMovement();
			const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			float& LastHitTime = GVillagerLastAttackTime.FindOrAdd(ControlledVillager, -10.0f);
			const float AttackCooldown = bIsBeardedWarrior ? 1.15f : 1.35f;
			if (NowTime - LastHitTime >= AttackCooldown)
			{
				LastHitTime = NowTime;
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(ControlledVillager))
				{
					OwnerRig->TriggerWeaponSwing(bIsBeardedWarrior ? 0.95f : 1.15f);
				}

				if (ACombatEnemy* TargetEnemy = Cast<ACombatEnemy>(EnYakinZombi))
				{
					// Gezgin sovalye teke tek onur duellosunu tercih eder (koylu mudahalesinde uyarir)
					const float BaseDmg = bIsBeardedWarrior ? 18.0f : 11.0f;
					const FVector HitImpulse = (ToEnemyDir * 320.0f) + FVector(0.0f, 0.0f, 120.0f);
					TargetEnemy->ApplyDamage(BaseDmg, ControlledVillager, TargetEnemy->GetActorLocation(), HitImpulse);
				}
			}
		}
		return;
	}

	// Geri cekilme emri verildiyse kampa cekil
	if (CurrentOrder == EDSVillagerCombatOrder::RetreatOrder)
	{
		const FVector VillagerLoc = ControlledVillager->GetActorLocation();
		const FVector SafeCampCenter(1260.0f, 1780.0f, VillagerLoc.Z);
		const FVector ToSafeCamp = SafeCampCenter - VillagerLoc;
		if (ToSafeCamp.Size2D() > 260.0f)
		{
			ControlledVillager->GetCharacterMovement()->MaxWalkSpeed = NormalYurumeHizi * 1.25f;
			const FVector Dir = ToSafeCamp.GetSafeNormal2D();
			ControlledVillager->SetActorRotation(FMath::RInterpTo(ControlledVillager->GetActorRotation(), Dir.Rotation(), DeltaTime, 7.0f));
			ControlledVillager->AddMovementInput(Dir, 0.80f);
		}
		return;
	}

	if (CurrentState == EVillagerState::Kaciyor)
	{
		// zombi uzaklasti veya ortadan kalkti: calismaya geri don
		if (!EnYakinZombi || ZombiMesafesi > GuvenliMesafe)
		{
			CurrentState = EVillagerState::Calisiyor;
			ControlledVillager->GetCharacterMovement()->MaxWalkSpeed = NormalYurumeHizi;
		}
	}
	else if (EnYakinZombi && ZombiMesafesi <= KacmaTetikMesafesi)
	{
		CurrentState = EVillagerState::Kaciyor;
		ControlledVillager->GetCharacterMovement()->MaxWalkSpeed = NormalYurumeHizi * KacmaHizCarpani;
	}

	if (CurrentState != EVillagerState::Kaciyor || !EnYakinZombi)
	{
		const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		PerformBellwrightProfessionWork(ControlledVillager, DeltaTime, NowTime);
		return;
	}

	// halihazirda bir kacma hedefine dogru pathfinding ile yol alirken her tick yeni istek gonderme
	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	bool bNavMoveStarted = false;
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation KacisNavLocation;
		if (NavSys->GetRandomReachablePointInRadius(ControlledVillager->GetActorLocation(), GuvenliMesafe, KacisNavLocation))
		{
			const EPathFollowingRequestResult::Type Sonuc = MoveToLocation(KacisNavLocation.Location, 50.0f, true, true, /*bProjectDestinationToNavigation=*/true, false);
			bNavMoveStarted = (Sonuc == EPathFollowingRequestResult::RequestSuccessful);
		}
	}

	// NavMesh bulunmayan arazi kesimlerinde (orn. Lvl_Forest dogal zemin) koyluyu dogrudan guvenli kamp/ates hattina yonlendir
	if (!bNavMoveStarted)
	{
		if (ControlledVillager->GetCharacterMovement() && ControlledVillager->GetCharacterMovement()->MovementMode == MOVE_None)
		{
			ControlledVillager->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}

		const FVector VillagerLoc = ControlledVillager->GetActorLocation();
		const FVector SafeCampCenter(1260.0f, 1780.0f, VillagerLoc.Z);
		const FVector AwayFromZombie = (VillagerLoc - EnYakinZombi->GetActorLocation()).GetSafeNormal2D();
		const FVector ToSafeCamp = SafeCampCenter - VillagerLoc;
		const FVector SteerDir = (ToSafeCamp.Size2D() > 360.0f)
			? (AwayFromZombie * 0.45f + ToSafeCamp.GetSafeNormal2D() * 0.55f).GetSafeNormal2D()
			: AwayFromZombie;

		if (!SteerDir.IsNearlyZero() && ZombiMesafesi < 420.0f)
		{
			ControlledVillager->SetActorRotation(FMath::RInterpTo(ControlledVillager->GetActorRotation(), SteerDir.Rotation(), DeltaTime, 7.0f));
			ControlledVillager->AddMovementInput(SteerDir, 0.70f);
		}
	}
}

void ADSVillagerController::ForceFleeFromDanger(const FVector& DangerLocation)
{
	if (!IsValid(ControlledVillager) || ControlledVillager->bIsDowned)
	{
		return;
	}

	CurrentState = EVillagerState::Kaciyor;
	ControlledVillager->GetCharacterMovement()->MaxWalkSpeed = NormalYurumeHizi * KacmaHizCarpani;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		return;
	}

	const FVector VillagerLoc = ControlledVillager->GetActorLocation();
	const FVector EscapeDir = (VillagerLoc - DangerLocation).GetSafeNormal2D();
	const FVector DesiredTarget = VillagerLoc + (EscapeDir.IsNearlyZero() ? FVector(600.0f, 0.0f, 0.0f) : EscapeDir * 700.0f);

	FNavLocation KacisNavLocation;
	if (NavSys->ProjectPointToNavigation(DesiredTarget, KacisNavLocation, FVector(500.0f, 500.0f, 500.0f)) ||
		NavSys->GetRandomReachablePointInRadius(VillagerLoc, GuvenliMesafe, KacisNavLocation))
	{
		MoveToLocation(KacisNavLocation.Location, 50.0f, true, true, true, false);
	}
}
