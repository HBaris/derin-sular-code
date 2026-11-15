// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CombatAIController.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "CombatLifeBar.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "DSZombiTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "CombatGameMode.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "DSVillager.h"
#include "DSBarikat.h"
#include "DSTrustComponent.h"
#include "DSCharacterBodyRig.h"
#include "Engine/StaticMeshActor.h"

ACombatEnemy::ACombatEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatEnemy::AttackMontageEnded);

	// set the AI Controller class by default
	AIControllerClass = ACombatAIController::StaticClass();

	// use an AI Controller regardless of whether we're placed or spawned
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// ignore the controller's yaw rotation
	bUseControllerRotationYaw = false;

	// create the life bar
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the collision capsule size
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// set the character movement properties
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	// reset HP to maximum
	CurrentHP = MaxHP;
}

void ACombatEnemy::DoAIComboAttack()
{
	// ignore if we're already playing an attack animation
	if (bIsAttacking)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// choose how many times we're going to attack
	TargetComboCount = FMath::RandRange(1, ComboSectionNames.Num() - 1);

	// reset the attack counter
	CurrentComboAttack = 0;

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
		}
	}
}

void ACombatEnemy::DoAIChargedAttack()
{
	// ignore if we're already playing an attack animation
	if (bIsAttacking)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// choose how many loops are we going to charge for
	TargetChargeLoops = FMath::RandRange(MinChargeLoops, MaxChargeLoops);

	// reset the charge loop counter
	CurrentChargeLoop = 0;

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ChargedAttackMontage);
		}
	}
}

void ACombatEnemy::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;

	// call the attack completed delegate so the StateTree can continue execution
	OnAttackCompleted.ExecuteIfBound();
}

const FVector& ACombatEnemy::GetLastDangerLocation() const
{
	return LastDangerLocation;
}

float ACombatEnemy::GetLastDangerTime() const
{
	return LastDangerTime;
}

void ACombatEnemy::DoAttackTrace(FName DamageSourceBone)
{
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the provided socket location, sweep forward
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	// enemies affect both Pawn (player/villager) and WorldDynamic (ADSBarikat) collision objects
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(MeleeTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHitAnyBarikat = false;

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			AActor* HitActor = CurrentHit.GetActor();
			if (!HitActor)
			{
				continue;
			}

			// Asama 9: gecerli hedef - oyuncu (Player tag), koylu (DSVillager) veya barikat (DSBarikat)
			const bool bIsBarikat = Cast<ADSBarikat>(HitActor) != nullptr;
			const bool bIsValidTarget = HitActor->ActorHasTag(FName("Player"))
				|| Cast<ADSVillager>(HitActor) != nullptr
				|| bIsBarikat;

			if (bIsValidTarget)
			{
				// check if the actor is damageable
				ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor);

				if (Damageable)
				{
					if (bIsBarikat)
					{
						bHitAnyBarikat = true;
					}

					// knock upwards and away from the impact normal
					const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse) + (FVector::UpVector * MeleeLaunchImpulse);

					// pass the damage event to the actor
					Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);
				}
			}
		}
	}

	// Barikat fallback guvencesi: barikat genis bir statik yapi oldugu icin el soketi sweep'i pivot/bounds
	// farkindan dolayi isabet ettiremezse, 320cm menzil icindeki en yakin canli barikata dogrudan hasar uygula
	if (!bHitAnyBarikat && GetWorld())
	{
		for (TActorIterator<ADSBarikat> It(GetWorld()); It; ++It)
		{
			ADSBarikat* Barikat = *It;
			if (IsValid(Barikat) && Barikat->CurrentHP > 0.0f)
			{
				const float Dist = FVector::Dist(GetActorLocation(), Barikat->GetActorLocation());
				if (Dist <= 320.0f)
				{
					Barikat->ApplyDamage(MeleeDamage, this, Barikat->GetActorLocation(), GetActorForwardVector() * MeleeKnockbackImpulse);
					break;
				}
			}
		}
	}
}

void ACombatEnemy::CheckCombo()
{
	// increase the combo counter
	++CurrentComboAttack;

	// do we still have attacks to play in this string?
	if (CurrentComboAttack < TargetComboCount)
	{
		// jump to the next attack section
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_JumpToSection(ComboSectionNames[CurrentComboAttack], ComboAttackMontage);
		}
	}
}

void ACombatEnemy::CheckChargedAttack()
{
	// increase the charge loop counter
	++CurrentChargeLoop;

	// jump to either the loop or attack section of the montage depending on whether we hit the loop target
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(CurrentChargeLoop >= TargetChargeLoops ? ChargeAttackSection : ChargeLoopSection, ChargedAttackMontage);
	}
}

void ACombatEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		const bool bIsBoss = ActorHasTag(FName("Boss_Sunflower"))
			|| ActorHasTag(FName("Boss_Treeguard"))
			|| ActorHasTag(FName("Boss_Pirate"))
			|| ActorHasTag(FName("Boss_AncientGuardian"))
			|| ActorHasTag(FName("WanderingKnight"));

		// Devasa Boss'lar (8-10 metre) darbe aldiginda knockback yemez, hoplayip ziplamaz ve saldirisi yarida kesilmez (Super-Armor)
		if (!bIsBoss)
		{
			// apply the knockback impulse
			GetCharacterMovement()->AddImpulse(DamageImpulse, true);

			// is the character ragdolling?
			if (GetMesh()->IsSimulatingPhysics())
			{
				// apply an impulse to the ragdoll
				GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
			}

			// stop the attack montages to interrupt the attack
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.1f, ComboAttackMontage);
				AnimInstance->Montage_Stop(0.1f, ChargedAttackMontage);
			}
		}

		// pass control to BP to play effects, etc.
		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}
}

void ACombatEnemy::HandleDeath()
{
	// olum aninda hit-stop uygulaniyor olabilir; ragdoll'un yavaslamis gorunmemesi icin sifirla
	CustomTimeDilation = 1.0f;

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// disable the collision capsule to avoid being hit again while dead
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable character movement
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	// Siskin: Zombi.Yetenek.Patlama etiketi varsa olum aninda alan hasari + gorsel efekt + patlama sesi
	if (CurrentAbilityTag == FGameplayTag::RequestGameplayTag(FName("Zombi.Yetenek.Patlama")))
	{
		const FVector ExplosionLocation = GetActorLocation();

		UGameplayStatics::ApplyRadialDamage(this, ExplosionDamage, ExplosionLocation, ExplosionRadius,
			UDamageType::StaticClass(), TArray<AActor*>{ this }, this, nullptr, /*bDoFullDamage=*/true);

		if (USoundBase* ExpSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Zombie_Explosion.SFX_Zombie_Explosion")))
		{
			UGameplayStatics::PlaySoundAtLocation(this, ExpSound, ExplosionLocation);
		}

		if (ExplosionEffect)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, ExplosionLocation);
		}
		else if (UNiagaraSystem* NiagaraBurst = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraBurst, ExplosionLocation, FRotator::ZeroRotator, FVector(3.5f));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: patlama tetiklendi ama ExplosionEffect atanmamis, gorsel efekt atlaniyor"), *GetName());
		}
	}

	// skor: GameMode'a bildir (basit puan sistemi, Asama 5)
	if (ACombatGameMode* GameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		GameMode->AddScore(OlumSkoru);
	}

	// ses: olum sesi
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s: DeathSound atanmamis, olum sesi calinamadi"), *GetName());
	}

	// call the died delegate to notify any subscribers
	OnEnemyDied.Broadcast();

	// set up the death timer
	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &ACombatEnemy::RemoveFromLevel, DeathRemovalTime);
}

void ACombatEnemy::ApplyHealing(float Healing, AActor* Healer)
{
	// stub
}

void ACombatEnemy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// ensure we're being attacked by the player
	if (DangerSource && DangerSource->ActorHasTag(FName("Player")))
	{
		// save the danger location and game time
		LastDangerLocation = DangerLocation;
		LastDangerTime = GetWorld()->GetTimeSeconds();
	}
}

void ACombatEnemy::RemoveFromLevel()
{
	// destroy this actor
	Destroy();
}

void ACombatEnemy::TryCiglikYetenegi(const FVector& OyuncuKonumu)
{
	// sadece Cigirtkan tipi (Zombi.Yetenek.Ciglik) icin calisir, digerlerinde no-op
	if (CurrentAbilityTag != FGameplayTag::RequestGameplayTag(FName("Zombi.Yetenek.Ciglik")))
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	if (Now - LastCiglikZamani < CiglikCooldownSuresi)
	{
		return;
	}

	LastCiglikZamani = Now;

	if (USoundBase* ScreamSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Zombie_Scream.SFX_Zombie_Scream")))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ScreamSound, GetActorLocation());
	}

	UE_LOG(LogTemp, Log, TEXT("%s: ciglik atildi, yaricap=%.0f"), *GetName(), CiglikYaricapi);

	// yaricap icindeki diger zombileri bul ve oyuncuya yonlendir
	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionShape Sphere = FCollisionShape::MakeSphere(CiglikYaricapi);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams, Sphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ACombatEnemy* OtherZombie = Cast<ACombatEnemy>(Overlap.GetActor()))
			{
				OtherZombie->NotifyCiglikDuyuldu(OyuncuKonumu);
			}
		}
	}
}

void ACombatEnemy::NotifyCiglikDuyuldu(const FVector& OyuncuKonumu)
{
	ZorlaAggroBitisZamani = GetWorld()->GetTimeSeconds() + ZorlaAggroSuresi;

	UE_LOG(LogTemp, Log, TEXT("%s: ciglik duydu, %.1f saniye zorla aggro modunda"), *GetName(), ZorlaAggroSuresi);
}

bool ACombatEnemy::IsZorlaAggroActive() const
{
	return GetWorld()->GetTimeSeconds() < ZorlaAggroBitisZamani;
}

void ACombatEnemy::TriggerFazGecisi()
{
	// olu ya da ragdoll'daysa faz tetikleme
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	FazBitisZamani = GetWorld()->GetTimeSeconds() + FazSuresi;

	// Faz gecisi gorsel efekti
	if (UNiagaraSystem* PhaseVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PhaseVFX, GetActorLocation());
	}

	// duzensiz hareket: rastgele yatay yonde kisa bir itki
	const FVector RandomYanalYon = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal();
	LaunchCharacter(RandomYanalYon * FazYanalImpulsBuyuklugu, true, false);

	UE_LOG(LogTemp, Log, TEXT("%s: Drifter faz gecisi baslatildi, %.2f sn hasarsiz"), *GetName(), FazSuresi);

	// bitis anini ayri loglayabilmek icin kisa omurlu bir lambda timer'i (yeni UFUNCTION gerekmez)
	const FString ActorName = GetName();
	const float LoggedFazSuresi = FazSuresi;
	FTimerHandle FazBitisLogTimer;
	GetWorld()->GetTimerManager().SetTimer(FazBitisLogTimer, [ActorName, LoggedFazSuresi]()
	{
		UE_LOG(LogTemp, Log, TEXT("%s: Drifter faz gecisi bitti, %.2f sn hasarsizlik penceresi kapandi"), *ActorName, LoggedFazSuresi);
	}, FazSuresi, false);
}

void ACombatEnemy::BaslatHitStop()
{
	CustomTimeDilation = HitStopTimeDilation;

	UE_LOG(LogTemp, Log, TEXT("HitStop: tetiklendi aktor=%s sure=%.2f dilation=%.2f"), *GetName(), HitStopSuresi, HitStopTimeDilation);

	GetWorld()->GetTimerManager().SetTimer(HitStopTimer, this, &ACombatEnemy::BitirHitStop, HitStopSuresi, false);
}

void ACombatEnemy::BitirHitStop()
{
	CustomTimeDilation = 1.0f;

	UE_LOG(LogTemp, Log, TEXT("HitStop: bitti aktor=%s"), *GetName());
}

void ACombatEnemy::BitirStagger()
{
	GetMesh()->SetPhysicsBlendWeight(0.0f);

	UE_LOG(LogTemp, Log, TEXT("Stagger: zamanlayici ile bitti aktor=%s"), *GetName());
}

float ACombatEnemy::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// only process damage if the character is still alive
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Drifter: faz gecisi aktifse hasar alinmaz; digerlerinde FazBitisZamani hicbir zaman ayarlanmadigindan bu her zaman false'dur
	if (GetWorld()->GetTimeSeconds() < FazBitisZamani)
	{
		return 0.0f;
	}

	// Zirhli: Zombi.Yetenek.ZirhAzaltma etiketi varsa gelen hasari azalt; diger tiplerde EffectiveDamage == Damage (davranis degismez)
	float EffectiveDamage = Damage;
	if (CurrentAbilityTag == FGameplayTag::RequestGameplayTag(FName("Zombi.Yetenek.ZirhAzaltma")))
	{
		EffectiveDamage = Damage * (1.0f - ZirhAzaltmaOrani);

		UE_LOG(LogTemp, Log, TEXT("%s: Zirhli hasar azaltildi, orijinal=%.2f azaltilmis=%.2f"), *GetName(), Damage, EffectiveDamage);
	}

	// vurus geri bildirimi: delege + kisa hit-stop + Niagara NS_Damage efekti
	FVector ImpactPointGuess = GetActorLocation();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		ImpactPointGuess = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		ImpactPointGuess = static_cast<const FRadialDamageEvent&>(DamageEvent).Origin;
	}

	const FVector DamageDirection = ImpactPointGuess.Equals(GetActorLocation())
		? GetActorForwardVector()
		: (GetActorLocation() - ImpactPointGuess).GetSafeNormal();

	OnHitReceived.Broadcast(EffectiveDamage, ImpactPointGuess, DamageDirection);
	BaslatHitStop();

	if (UNiagaraSystem* HitVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitVFX, ImpactPointGuess, DamageDirection.Rotation());
	}

	// ses: vurulma sesi
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, ImpactPointGuess);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s: HitSound atanmamis, vurulma sesi calinamadi"), *GetName());
	}

	// reduce the current HP
	if (ActorHasTag(FName("Boss_Sunflower_Muttefik")) || ActorHasTag(FName("Boss_Pirate_Muttefik")))
	{
		return 0.0f;
	}
	if (ActorHasTag(FName("WanderingKnight")) && !ActorHasTag(FName("WanderingKnight_Hostile")))
	{
		return 0.0f;
	}
	if (ActorHasTag(FName("Boss_Treeguard_Passive")))
	{
		Tags.Remove(FName("Boss_Treeguard_Passive"));
		FDSColonyAndWorldState::Get(this).bTreeguardAwakened = true;
		FDSColonyAndWorldState::Get(this).PushNotification(TEXT("ORMAN MUHAFIZI (TREEGUARD) UYANDI! Ormanin gazabina dikkat et!"), 6.0f);
	}

	// FAZ 2 (KADIM TARIFLER + BOSS FAZLARI): Kadim Labirent Bekcisi Obsidyen Zirhi vs [4] Savas Cekici / Tas Sutun Stagger
	if (ActorHasTag(FName("Boss_AncientGuardian")) && GetWorld())
	{
		FDSColonyAndWorldState& GWS = FDSColonyAndWorldState::Get(this);
		const float NowSec = GetWorld()->GetTimeSeconds();
		const FDSCharacterBodyRig* CauserRig = DamageCauser ? FDSCharacterBodyRig::GetOrCreateRig(DamageCauser) : nullptr;
		const bool bHitWithHammer = CauserRig && (CauserRig->WeaponStyle == EDSCombatWeaponStyle::WarHammer);
		const bool bArmorAlreadyBroken = (NowSec < GWS.BossArmorBrokenEndTimeMap.FindRef(FObjectKey(this)));

		if (bHitWithHammer)
		{
			GWS.BossArmorBrokenEndTimeMap.FindOrAdd(FObjectKey(this)) = NowSec + 8.0f;
			EffectiveDamage *= 1.65f;
			// Yakindaki Kadim Labirent Tas Sutunu varsa parcala ve ekstra sarsinti uygula
			for (TActorIterator<AActor> PIt(GetWorld()); PIt; ++PIt)
			{
				AActor* PAct = *PIt;
				if (IsValid(PAct) && !PAct->IsHidden() && PAct->ActorHasTag(FName("AncientLabyrinthPillar")) && FVector::Dist2D(GetActorLocation(), PAct->GetActorLocation()) <= 360.0f)
				{
					PAct->SetActorScale3D(PAct->GetActorScale3D() * FVector(1.0f, 1.0f, 0.28f));
					PAct->Tags.Remove(FName("AncientLabyrinthPillar"));
					FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), PAct->GetActorLocation(), FName("Loot_Stone"), 0);
					EffectiveDamage += 45.0f;
					break;
				}
			}
			GWS.PushNotification(TEXT("ZIRH KIRMA ([4] Savas Cekici): Kadim Labirent Bekcisi'nin obsidyen zirhi catladi ve tas sutun yikildi! (1.65x Hasar)"), 5.5f);
		}
		else if (bArmorAlreadyBroken)
		{
			EffectiveDamage *= 1.35f;
		}
		else
		{
			EffectiveDamage *= 0.40f;
			GWS.PushNotification(TEXT("KADIM LABIRENT BEKCISI OBSIDYEN ZIRHI (%60 Hasar Azaltma)! Zirhini kirmak icin [4] Savas Cekici/Balyoz kullan veya Tas Sutuna carpdir!"), 5.0f);
		}
	}

	CurrentHP -= EffectiveDamage;

	// FAZ 2 (BOSS FAZLARI): Boss Faz Gecisleri (Phase 1 -> Phase 2 [%65 HP] -> Phase 3 [%30 HP]) ve 1.2sn Gorsel Telgraf Halkasi
	if ((ActorHasTag(FName("Boss_Sunflower")) || ActorHasTag(FName("Boss_Treeguard")) || ActorHasTag(FName("Boss_AncientGuardian")) || ActorHasTag(FName("Boss_Pirate"))) && CurrentHP > 0.0f && GetWorld())
	{
		FDSColonyAndWorldState& PWS = FDSColonyAndWorldState::Get(this);
		int32& BossPhase = PWS.BossPhaseMap.FindOrAdd(FObjectKey(this), 1);
		const float HPRatio = CurrentHP / FMath::Max(MaxHP, 1.0f);
		const int32 DesiredPhase = (HPRatio <= 0.35f) ? 3 : ((HPRatio <= 0.70f) ? 2 : 1);
		if (DesiredPhase > BossPhase)
		{
			BossPhase = DesiredPhase;
			const FLinearColor TelColor = ActorHasTag(FName("Boss_AncientGuardian"))
				? FLinearColor(0.75f, 0.20f, 1.0f)
				: (ActorHasTag(FName("Boss_Treeguard")) ? FLinearColor(0.20f, 1.0f, 0.35f) : FLinearColor(1.0f, 0.32f, 0.10f));
			const FVector TelCenter = GetActorLocation() + GetActorForwardVector() * 195.0f;
			FDSColonyAndWorldState::SpawnRemnantTelegraphRing(GetWorld(), TelCenter, 260.0f, TelColor);
			if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
			{
				OwnerRig->TriggerWeaponSwing(1.25f);
			}
			PWS.PushNotification(FString::Printf(
				TEXT("BOSS FAZ %d TELGRAFI! Yerde parlayan alan saldirisi halkasindan 1.2 sn icinde cik veya [F] ile Parry yap!"),
				BossPhase), 6.0f);
		}
	}

	// Boss 1 (Aycicegi Koruyucusu) Faz Gecisi (%78 HP alti): Insandan ziyade aycicegini andiran Minik Aycicegi Yancilari (SunflowerMinion) sahneye cikar!
	if (ActorHasTag(FName("Boss_Sunflower")) && CurrentHP > 0.0f && CurrentHP <= MaxHP * 0.78f && !FDSColonyAndWorldState::Get(this).bSunflowerMinionsSpawned)
	{
		FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
		WS.bSunflowerMinionsSpawned = true;
		if (UStaticMesh* CrownMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Sunflower_Crown.SM_Sunflower_Crown")))
		{
			UMaterialInterface* SunMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR"));
			const FBoxSphereBounds Bounds = CrownMesh->GetBounds();
			const float MaxExt = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
			const float BaseS = (MaxExt > 0.01f) ? FMath::Clamp(52.0f / MaxExt, 0.1f, 5.0f) : 0.6f;
			for (int32 MIdx = 0; MIdx < 3; ++MIdx)
			{
				const float Ang = float(MIdx) * 2.09f;
				const FVector MinionLoc = GetActorLocation() + FVector(FMath::Cos(Ang) * 145.0f, FMath::Sin(Ang) * 145.0f, -45.0f);
				FActorSpawnParameters MSP;
				MSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* MinionAct = GetWorld()->SpawnActor<AStaticMeshActor>(MinionLoc, FRotator(25.0f, Ang * 57.3f, 0.0f), MSP))
				{
					MinionAct->SetMobility(EComponentMobility::Movable);
					MinionAct->GetStaticMeshComponent()->SetStaticMesh(CrownMesh);
					MinionAct->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					if (SunMat)
					{
						MinionAct->GetStaticMeshComponent()->SetMaterial(0, SunMat);
					}
					MinionAct->SetActorScale3D(FVector(BaseS, BaseS, BaseS * 1.15f));
					MinionAct->Tags.AddUnique(FName("SunflowerMinion"));
					MinionAct->Tags.AddUnique(FName("SunflowerMinion_Fighting"));
				}
			}
		}
		WS.PushNotification(TEXT("AYCICEGI BOSS FAZ GECISI: Insandan ziyade aycicegini andiran Minik Aycicegi Yancilari firlayip seninle savasmaya basladi!"), 5.5f);

		// Minik yancilar oyuncuyla savasmaya basladiktan kisa sure sonra Boss babacan tavriyla onlari kenara uzaklastirir
		TSharedRef<FTimerHandle> FatherlyTimer = MakeShared<FTimerHandle>();
		GetWorld()->GetTimerManager().SetTimer(*FatherlyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			FDSColonyAndWorldState& InnerWS = FDSColonyAndWorldState::Get(this);
			if (!InnerWS.bSunflowerFatherlySweepDone && GetWorld())
			{
				InnerWS.bSunflowerFatherlySweepDone = true;
				int32 SafeIdx = 0;
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* Act = *It;
					if (IsValid(Act) && Act->ActorHasTag(FName("SunflowerMinion")))
					{
						Act->Tags.Remove(FName("SunflowerMinion_Fighting"));
						Act->Tags.AddUnique(FName("SunflowerMinion_SafeAside"));
						const FVector SafeSpot = GetActorLocation() + FVector(-220.0f - float(SafeIdx) * 75.0f, 260.0f + float(SafeIdx) * 65.0f, -45.0f);
						Act->SetActorLocation(SafeSpot);
						++SafeIdx;
					}
				}
				InnerWS.PushNotification(TEXT("AYCICEGI BOSS: 'HAYIR ONLAR BENIM, BANA BIRAKIN! Cekilin bakayim ufakliklar!' (Babacan yapisiyla minik ayciceklerini ezmeden kenara uzaklastirdi!)"), 7.5f);
			}
		}), 3.2f, false);
	}

	// Boss 1 (Aycicegi Koruyucusu) %55 HP altina indiginde (veya faz bittiginde) minik yancilarini babacan sekilde uzaklastirip Faz 3 "Tanrica Gucu" acar
	if (ActorHasTag(FName("Boss_Sunflower")) && CurrentHP > 0.0f && CurrentHP <= MaxHP * 0.55f && !ActorHasTag(FName("Boss_Sunflower_GoddessPhase")))
	{
		Tags.AddUnique(FName("Boss_Sunflower_GoddessPhase"));
		MeleeDamage = 25.0f;
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = 460.0f;
		}
		if (UNiagaraSystem* GoldVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail")))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), GoldVFX, GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(2.8f));
		}
		FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
		WS.bSunflowerFatherlySweepDone = true;
		int32 SafeIdx = 0;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Act = *It;
			if (IsValid(Act) && Act->ActorHasTag(FName("SunflowerMinion")))
			{
				Act->Tags.Remove(FName("SunflowerMinion_Fighting"));
				Act->Tags.AddUnique(FName("SunflowerMinion_SafeAside"));
				Act->SetActorLocation(GetActorLocation() + FVector(-220.0f - float(SafeIdx) * 75.0f, 260.0f + float(SafeIdx) * 65.0f, -45.0f));
				++SafeIdx;
			}
		}
		WS.PushNotification(TEXT("AYCICEGI BOSS: 'HAYIR ONLAR BENIM, BANA BIRAKIN!' Minik aycicegi yancilarini babacan sekilde kenara uzaklastirdi ve Tanrica Gucu acti!"), 7.5f);
	}

	// have we run out of HP?
	if (CurrentHP <= 0.0f)
	{
		// Aycicegi Tarlasinin Koruyucusu (Boss 1) yenildiginde oldurulmez; muttefik olur, Tarimcilik sirlarini ogretir
		// VE yere "Sivi Yag Surulmus Ayakkabi" (+Hareket Hizi) dusurup "TABANLARI YAGLAMAK" basarimini acar!
		if (ActorHasTag(FName("Boss_Sunflower")))
		{
			CurrentHP = MaxHP * 0.85f;
			Tags.AddUnique(FName("Boss_Sunflower_Muttefik"));
			bIsAttacking = false;
			GetMesh()->SetPhysicsBlendWeight(0.0f);
			if (LifeBarWidget)
			{
				LifeBarWidget->SetBarColor(FLinearColor(0.18f, 0.92f, 0.36f, 1.0f));
				LifeBarWidget->SetLifePercentage(1.0f);
			}
			FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
			WS.bSunflowerBossAllied = true;
			WS.bSunflowerFatherlySweepDone = true;
			WS.bOiledBootsEquipped = true;
			WS.bRecipeSunOilElixir = true;
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), GetActorLocation() + GetActorRightVector() * 95.0f, FName("Loot_SunEssence"), 0);
			WS.DivineFavor = FMath::Clamp(WS.DivineFavor + 20.0f, 0.0f, 100.0f);

			// Yere 3B Sivi Yag Surulmus Ayakkabi (SM_Boots_Oiled) drop'u birak ve oyuncunun hareket hizini artir
			if (UStaticMesh* BootsMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Env/SM_Boots_Oiled.SM_Boots_Oiled")))
			{
				FActorSpawnParameters BSP;
				BSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FVector DropLoc = GetActorLocation() + GetActorForwardVector() * 115.0f - FVector(0.0f, 0.0f, 75.0f);
				if (AStaticMeshActor* BootsDrop = GetWorld()->SpawnActor<AStaticMeshActor>(DropLoc, FRotator(0.0f, 25.0f, 0.0f), BSP))
				{
					BootsDrop->SetMobility(EComponentMobility::Movable);
					BootsDrop->GetStaticMeshComponent()->SetStaticMesh(BootsMesh);
					BootsDrop->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					if (UMaterialInterface* OilMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR")))
					{
						BootsDrop->GetStaticMeshComponent()->SetMaterial(0, OilMat);
					}
					const FBoxSphereBounds Bounds = BootsMesh->GetBounds();
					const float MaxExt = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
					const float BootS = (MaxExt > 0.01f) ? FMath::Clamp(38.0f / MaxExt, 0.1f, 5.0f) : 0.75f;
					BootsDrop->SetActorScale3D(FVector(BootS));
					BootsDrop->Tags.AddUnique(FName("OiledBootsDrop"));
				}
			}

			if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
			{
				if (PlayerChar->GetCharacterMovement())
				{
					PlayerChar->GetCharacterMovement()->MaxWalkSpeed = 545.0f; // +%36 Hareket Hizi (400 -> 545)
				}
			}

			WS.UnlockSteamAchievement(
				TEXT("TABANLARI YAGLAMAK"),
				TEXT("Aycicegi Boss'tan dusen Sivi Yag Surulmus Ayakkabiyi aldin! (+%36 Hareket Hizi & Tarimcilik Sirlari)")
			);
			WS.PushNotification(TEXT("BOSS DROP: 'SIVI YAG SURULMUS AYAKKABI' (+%36 Hareket Hizi) kusanildi! BASARIM ACILDI: [TABANLARI YAGLAMAK]!"), 8.5f);

			if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
			{
				GM->AddScore(200);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(25.0f, TEXT("Aycicegi Koruyucusu muttefik oldu ve Sivi Yag Surulmus Ayakkabi verdi"));
				}
			}
			return EffectiveDamage;
		}
		else if (ActorHasTag(FName("Boss_Pirate")))
		{
			// Korsan Boss (Boss_Pirate) yenildiginde oldurulmez; muttefik olur, Denizcilik Sirlarini (Guclu Olta & Balik Agi)
			// ve Cift Elli Kilic Ustaligini (3'lu Kombo + [F] Blok Durusu & Parry) ogretir!
			CurrentHP = MaxHP * 0.85f;
			Tags.AddUnique(FName("Boss_Pirate_Muttefik"));
			bIsAttacking = false;
			GetMesh()->SetPhysicsBlendWeight(0.0f);
			if (LifeBarWidget)
			{
				LifeBarWidget->SetBarColor(FLinearColor(0.15f, 0.90f, 0.78f, 1.0f));
				LifeBarWidget->SetLifePercentage(1.0f);
			}
			FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
			WS.bPirateBossAllied = true;
			WS.bPirateSeaMasteryUnlocked = true;
			WS.bTwoHandedSwordMasteryUnlocked = true;
			WS.bRecipePirateSalupa = true;
			WS.SeaBountyCount += 6;
			WS.DivineFavor = FMath::Clamp(WS.DivineFavor + 20.0f, 0.0f, 100.0f);

			// Oyuncu kilic tasiyorsa aninda Cift Elli Korsan Kilici (SM_Pirate_Greatsword) gorunumune yukselt
			if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				if (FDSCharacterBodyRig* PRig = FDSCharacterBodyRig::GetOrCreateRig(PlayerPawn))
				{
					PRig->WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					PRig->ApplyWeaponMeshToComponent();
				}
			}

			WS.PushNotification(TEXT("KORSAN KAPTAN MUTTEFIK OLDU! 'Daha Guclu Olta & Balik Agi' (Deniz Bereketi x2.5) + '[6] CIFT ELLI KILIC USTALIGI, KOMBO ve [F] BLOK/PARRY' ogrenildi!"), 9.0f);

			if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
			{
				GM->AddScore(250);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(25.0f, TEXT("Korsan Boss muttefik oldu; Denizcilik ve Cift Elli Kilic Ustaligi ogretti"));
				}
			}
			return EffectiveDamage;
		}
		else if (ActorHasTag(FName("WanderingKnight")))
		{
			CurrentHP = MaxHP * 0.75f;
			Tags.Remove(FName("WanderingKnight_Hostile"));
			bIsAttacking = false;
			GetMesh()->SetPhysicsBlendWeight(0.0f);
			if (LifeBarWidget)
			{
				LifeBarWidget->SetBarColor(FLinearColor(0.20f, 0.85f, 0.95f, 1.0f));
				LifeBarWidget->SetLifePercentage(1.0f);
			}
			FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
			WS.bWanderingKnightDefeated = true;
			WS.bWanderingKnightDuelActive = false;
			WS.bMasterCombatUnlocked = true;
			WS.DivineFavor = FMath::Clamp(WS.DivineFavor + 15.0f, 0.0f, 100.0f);
			WS.PushNotification(TEXT("GEZGIN SOVALYE: 'Onurlu bir duelloydu! Usta Dovuscu teknigini sana ogretiyorum (+%25 Hasar & Saldiri Hizi)!'"), 8.0f);

			if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
			{
				GM->AddScore(250);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(20.0f, TEXT("Gezgin Sovalye onur duellosunda maglup edildi"));
				}
			}
			return EffectiveDamage;
		}
		else if (ActorHasTag(FName("Boss_Treeguard")))
		{
			FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
			WS.bTreeguardDefeated = true;
			WS.bLivingLogUnlocked = true;
			WS.bRecipeLivingDarkSword = true;
			WS.WoodCount += 12;
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), GetActorLocation(), FName("Loot_LivingLog"), 0);
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), GetActorLocation(), FName("Loot_LivingLog"), 1);
			WS.PushNotification(TEXT("ORMAN MUHAFIZI (TREEGUARD) DEVRILDI! Yere fiziksel 3B 'Canli Kutuk (Living Log)' dustu -> [KADIM TARIF ACILDI: Kadim Karanlik Kilic]!"), 8.5f);

			if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
			{
				GM->AddScore(150);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(15.0f, TEXT("Treeguard devrildi, Canli Kutuk (Living Log) elde edildi"));
				}
			}
		}
		else if (ActorHasTag(FName("Boss_AncientGuardian")))
		{
			FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
			WS.bAncientGuardianDefeated = true;
			WS.bRecipeGuardianArmor = true;
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), GetActorLocation(), FName("Loot_GuardianHorn"), 0);
			FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), GetActorLocation(), FName("Loot_MagmaCore"), 1);
			WS.PushNotification(TEXT("KADIM LABIRENT BEKCISI YENILDI! Yere 3B 'Muhafiz Boynuzu' ve 'Magma Cekirdegi' dustu -> [KADIM TARIF ACILDI: Kadim Muhafiz Zirhi (+%40 Zirh)]!"), 9.0f);

			if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
			{
				GM->AddScore(250);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(20.0f, TEXT("Kadim Labirent Bekcisi yenildi ve Muhafiz Boynuzu + Magma Cekirdegi kazanildi"));
				}
			}
		}

		// die
		HandleDeath();
	}
	else
	{
		const bool bIsBoss = ActorHasTag(FName("Boss_Sunflower"))
			|| ActorHasTag(FName("Boss_Treeguard"))
			|| ActorHasTag(FName("Boss_Pirate"))
			|| ActorHasTag(FName("Boss_AncientGuardian"))
			|| ActorHasTag(FName("WanderingKnight"));

		// update the life bar (show overhead bar for damaged regular enemies; bosses use top-center HUD bar)
		if (LifeBar && !bIsBoss)
		{
			LifeBar->SetVisibility(true);
			LifeBar->SetHiddenInGame(false);
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
		}

		// Devasa Boss'lar (Super-Armor) kismi ragdoll / stagger yasamaz, dimdik durur
		if (!bIsBoss)
		{
			// enable partial ragdoll physics, but keep the pelvis vertical
			GetMesh()->SetPhysicsBlendWeight(0.5f);
			GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);

			// stagger'in suresiz kalmamasi icin zamanlayici kur/yenile
			GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &ACombatEnemy::BitirStagger, StaggerSuresi, false);
		}
	}

	// return the received (post-reduction) damage amount
	return EffectiveDamage;
}

void ACombatEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (CurrentHP >= 0.0f)
	{
		// disable ragdoll physics
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}

	// call the landed Delegate for StateTree
	OnEnemyLanded.ExecuteIfBound();
}

void ACombatEnemy::PostInitializeComponents()
{
	// top the HP before Super spawns and possesses the AI Controller (AutoPossessAI):
	// the controller starts the StateTree on possess, and if CurrentHP is still 0 at
	// that point the tree enters its "Dead" state and finishes permanently
	CurrentHP = MaxHP;

	UE_LOG(LogTemp, Warning, TEXT("ZDBG PostInit %s HP=%.1f (Super oncesi)"), *GetName(), CurrentHP);

	Super::PostInitializeComponents();

	UE_LOG(LogTemp, Warning, TEXT("ZDBG PostInit %s tamam, Controller=%s"), *GetName(), *GetNameSafe(GetController()));
}

void ACombatEnemy::BeginPlay()
{
	// reset HP to maximum
	CurrentHP = MaxHP;

	UE_LOG(LogTemp, Warning, TEXT("ZDBG BeginPlay %s HP=%.1f"), *GetName(), CurrentHP);

	Super::BeginPlay();

	if (!HitSound)
	{
		HitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Zombie_Hit.SFX_Zombie_Hit"));
	}
	if (!DeathSound)
	{
		DeathSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Zombie_Death.SFX_Zombie_Death"));
	}

	// get the life bar widget from the widget comp
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// fill the life bar and hide until damaged
	LifeBarWidget->SetLifePercentage(1.0f);
	if (LifeBar)
	{
		LifeBar->SetVisibility(false);
		LifeBar->SetHiddenInGame(true);
	}

	// zombi tipi verisini tablodan uygula (bulunamazsa mevcut varsayilanlarla devam edilir)
	ApplyZombiTipiVerisi();

	// dalga bazli stat rampasi: spawner spawn'dan once WaveHPMultiplier/WaveSpeedMultiplier'i atamis olabilir
	MaxHP *= WaveHPMultiplier;
	CurrentHP = MaxHP;
	GetCharacterMovement()->MaxWalkSpeed *= WaveSpeedMultiplier;

	// HP artik dolu; StateTree'yi ancak simdi baslatiyoruz (Bariscil model inceleme modunda savas AI baslatilmaz).
	if (!ACombatGameMode::IsPeacefulShowcaseMode(this))
	{
		if (ACombatAIController* AIController = GetController<ACombatAIController>())
		{
			AIController->StartStateTreeLogic();
		}

		// Drifter: FazGecis etiketi varsa duzensiz faz dongusunu baslat
		if (CurrentAbilityTag == FGameplayTag::RequestGameplayTag(FName("Zombi.Yetenek.FazGecis")))
		{
			GetWorld()->GetTimerManager().SetTimer(FazDonguTimer, this, &ACombatEnemy::TriggerFazGecisi, FazAraligi, true);

			UE_LOG(LogTemp, Log, TEXT("%s: Drifter faz dongu timer'i kuruldu, aralik=%.2f"), *GetName(), FazAraligi);
		}
	}

	// Robot Mannequin skinsuit'i gizleyip Boss veya Zombi tipine ozel 6 parcali gercek 3B govdeyi bagla
	const FString CombinedType = ZombiTipiSatirAdi.ToString() + TEXT("_") + GetClass()->GetName();
	FString ZombieModelPrefix = TEXT("ZomWalker");
	FVector ZombieRigScale(1.0f, 1.0f, 1.0f);

	if (ActorHasTag(FName("Boss_Sunflower")))
	{
		ZombieModelPrefix = TEXT("BossSunflower");
		// 8-10 metre (~8.75m / 875 cm) devasa Boss boyutu!
		ZombieRigScale = FVector(4.85f, 4.85f, 4.85f);
		MaxHP = 850.0f;
		CurrentHP = MaxHP;
		MeleeDamage = 28.0f;
		MeleeTraceDistance = 420.0f;
		MeleeTraceRadius = 180.0f;
		if (GetCapsuleComponent())
		{
			GetCapsuleComponent()->SetCapsuleRadius(88.0f);
		}
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->Mass = 8500.0f;
			GetCharacterMovement()->bPushForceUsingZOffset = false;
		}
		if (LifeBar)
		{
			LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 835.0f));
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetBarColor(FLinearColor(1.0f, 0.78f, 0.12f, 1.0f));
		}
	}
	else if (ActorHasTag(FName("Boss_Treeguard")))
	{
		ZombieModelPrefix = TEXT("BossTreeguard");
		// 8-10 metre (~8.8m / 880 cm) devasa Orman Muhafizi boyutu!
		ZombieRigScale = FVector(4.90f, 4.90f, 4.90f);
		MaxHP = 800.0f;
		CurrentHP = MaxHP;
		MeleeDamage = 30.0f;
		MeleeTraceDistance = 420.0f;
		MeleeTraceRadius = 180.0f;
		if (GetCapsuleComponent())
		{
			GetCapsuleComponent()->SetCapsuleRadius(88.0f);
		}
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->Mass = 9000.0f;
			GetCharacterMovement()->bPushForceUsingZOffset = false;
		}
		if (LifeBar)
		{
			LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 845.0f));
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetBarColor(FLinearColor(0.95f, 0.42f, 0.10f, 1.0f));
		}
	}
	else if (ActorHasTag(FName("Boss_Pirate")))
	{
		ZombieModelPrefix = TEXT("BossPirate");
		ZombieRigScale = FVector(1.18f, 1.18f, 1.18f);
		MaxHP = 175.0f;
		CurrentHP = MaxHP;
		MeleeDamage = 19.0f;
		if (LifeBar)
		{
			LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 208.0f));
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetBarColor(FLinearColor(0.95f, 0.28f, 0.22f, 1.0f));
		}
	}
	else if (ActorHasTag(FName("Boss_AncientGuardian")))
	{
		ZombieModelPrefix = TEXT("ZomTank");
		ZombieRigScale = FVector(2.15f, 2.15f, 2.10f);
		MaxHP = 480.0f;
		CurrentHP = MaxHP;
		MeleeDamage = 26.0f;
		MeleeTraceDistance = 320.0f;
		MeleeTraceRadius = 140.0f;
		if (LifeBar)
		{
			LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 390.0f));
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetBarColor(FLinearColor(0.68f, 0.28f, 1.0f, 1.0f));
		}
	}
	else if (ActorHasTag(FName("WanderingKnight")))
	{
		ZombieModelPrefix = TEXT("Hero");
		ZombieRigScale = FVector(1.12f, 1.12f, 1.10f);
		MaxHP = 140.0f;
		CurrentHP = MaxHP;
		MeleeDamage = 16.0f;
		if (LifeBar)
		{
			LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 195.0f));
		}
		if (LifeBarWidget)
		{
			LifeBarWidget->SetBarColor(FLinearColor(0.25f, 0.78f, 1.0f, 1.0f));
		}
	}
	else if (CombinedType.Contains(TEXT("Kosucu")) || CombinedType.Contains(TEXT("Hizli")))
	{
		ZombieModelPrefix = TEXT("ZomRunner");
		ZombieRigScale = FVector(0.94f, 0.94f, 1.04f);
	}
	else if (CombinedType.Contains(TEXT("Siskin")))
	{
		ZombieModelPrefix = TEXT("ZomBloater");
		ZombieRigScale = FVector(1.22f, 1.22f, 1.12f);
	}
	else if (CombinedType.Contains(TEXT("Cigirtkan")))
	{
		ZombieModelPrefix = TEXT("ZomScreamer");
		ZombieRigScale = FVector(0.92f, 0.92f, 1.12f);
	}
	else if (CombinedType.Contains(TEXT("Zirhli")) || CombinedType.Contains(TEXT("Dayanikli")))
	{
		ZombieModelPrefix = TEXT("ZomTank");
		ZombieRigScale = FVector(1.18f, 1.18f, 1.08f);
	}

	FDSCharacterBodyRig* ZombieRig = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (ZombieRig)
	{
		ZombieRig->InitializeRig(this, GetMesh(), ZombieModelPrefix, !ActorHasTag(FName("WanderingKnight")) && !ActorHasTag(FName("Boss_Pirate")), ZombieRigScale);
	}

	if (ActorHasTag(FName("WanderingKnight")))
	{
		if (UStaticMesh* KnightSword = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_Weapon_LivingDarkSword.SM_Weapon_LivingDarkSword")))
		{
			if (UStaticMeshComponent* SwordComp = NewObject<UStaticMeshComponent>(this, TEXT("WanderingKnightSword")))
			{
				SwordComp->SetStaticMesh(KnightSword);
				SwordComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SwordComp->SetCastShadow(true);
				SwordComp->RegisterComponent();
				SwordComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				if (ZombieRig)
				{
					ZombieRig->WeaponStyle = EDSCombatWeaponStyle::LongSword;
					ZombieRig->ApplyWeaponMeshToComponent(SwordComp);
				}
			}
		}
	}
	else if (ActorHasTag(FName("Boss_Pirate")))
	{
		if (UStaticMesh* PirateSword = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_Pirate_Greatsword.SM_Pirate_Greatsword")))
		{
			if (UStaticMeshComponent* SwordComp = NewObject<UStaticMeshComponent>(this, TEXT("BossPirateGreatsword")))
			{
				SwordComp->SetStaticMesh(PirateSword);
				SwordComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SwordComp->SetCastShadow(true);
				if (UMaterialInterface* SwordMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeaponSteel.M_Env_WeaponSteel")))
				{
					SwordComp->SetMaterial(0, SwordMat);
				}
				SwordComp->RegisterComponent();
				SwordComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				SwordComp->SetWorldScale3D(FVector(ZombieRigScale.Z));
				if (ZombieRig)
				{
					ZombieRig->WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					ZombieRig->CachedWeaponMesh = SwordComp;
				}
			}
		}
	}
	else if (ActorHasTag(FName("Boss_AncientGuardian")))
	{
		if (UStaticMesh* GuardianHammer = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_WarHammer.SM_WarHammer")))
		{
			if (UStaticMeshComponent* HammerComp = NewObject<UStaticMeshComponent>(this, TEXT("BossGuardianWarHammer")))
			{
				HammerComp->SetStaticMesh(GuardianHammer);
				HammerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				HammerComp->SetCastShadow(true);
				if (UMaterialInterface* HammerMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeaponSteel.M_Env_WeaponSteel")))
				{
					HammerComp->SetMaterial(0, HammerMat);
				}
				HammerComp->RegisterComponent();
				HammerComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				HammerComp->SetWorldScale3D(FVector(ZombieRigScale.Z));
				if (ZombieRig)
				{
					ZombieRig->WeaponStyle = EDSCombatWeaponStyle::WarHammer;
					ZombieRig->CachedWeaponMesh = HammerComp;
				}
			}
		}
	}

	TSharedRef<FTimerHandle> ZombiePoseTimer = MakeShared<FTimerHandle>();
	TSharedRef<float> LastSpecialActionTime = MakeShared<float>(0.0f);
	TSharedRef<FVector> PeacefulHomeLoc = MakeShared<FVector>(FVector::ZeroVector);
	TSharedRef<int32> ActiveZomAnimState = MakeShared<int32>(-1); // 0=Idle, 1=Walk
	GetWorld()->GetTimerManager().SetTimer(*ZombiePoseTimer, FTimerDelegate::CreateWeakLambda(this, [this, LastSpecialActionTime, PeacefulHomeLoc, ActiveZomAnimState]()
	{
		if (CurrentHP > 0.0f && GetMesh() && !GetMesh()->IsSimulatingPhysics() && GetWorld())
		{
			if (GetCharacterMovement() && GetCharacterMovement()->MovementMode == MOVE_None)
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}

			const bool bIsBossOrKnight = ActorHasTag(FName("Boss_Sunflower"))
				|| ActorHasTag(FName("Boss_Treeguard"))
				|| ActorHasTag(FName("Boss_Pirate"))
				|| ActorHasTag(FName("WanderingKnight"));

			// Zombiler insan gibi yurumez; gercek UAL zombi animasyonlarini (MM_UAL_Zombie_Walk_Fwd_Loop / MM_UAL_Zombie_Idle_Loop) oynatir
			if (!bIsBossOrKnight)
			{
				if (UAnimInstance* ZAnimInst = GetMesh()->GetAnimInstance())
				{
					const float CurSpeed2D = GetVelocity().Size2D();
					const int32 DesiredZState = (CurSpeed2D > 12.0f) ? 1 : 0;
					if (*ActiveZomAnimState != DesiredZState && !bIsAttacking)
					{
						*ActiveZomAnimState = DesiredZState;
						const TCHAR* ZSeqPath = (DesiredZState == 1)
							? TEXT("/Game/Characters/Anims_UAL/Manny/MM_UAL_Zombie_Walk_Fwd_Loop.MM_UAL_Zombie_Walk_Fwd_Loop")
							: TEXT("/Game/Characters/Anims_UAL/Manny/MM_UAL_Zombie_Idle_Loop.MM_UAL_Zombie_Idle_Loop");
						if (UAnimSequence* ZSeq = LoadObject<UAnimSequence>(nullptr, ZSeqPath))
						{
							ZAnimInst->PlaySlotAnimationAsDynamicMontage(ZSeq, FName("DefaultSlot"), 0.20f, 0.20f, (DesiredZState == 1) ? 0.92f : 0.85f, 9999, -1.0f, 0.0f);
						}
					}
				}
			}

			const float NowTime = GetWorld()->GetTimeSeconds();

			// Bariscil Dunya & Modelleme Inceleme Modu: Tum yaratiklar ve boss'lar saldirmadan kendi bolgelerinde dolasir (Iskeledeki Boss_Pirate ve Sovalye sabit nobet tutar)
			if (ACombatGameMode::IsPeacefulShowcaseMode(this))
			{
				if (NowTime >= 0.35f && !ActorHasTag(FName("Boss_Pirate")) && !ActorHasTag(FName("WanderingKnight")))
				{
					if (PeacefulHomeLoc->IsNearlyZero())
					{
						*PeacefulHomeLoc = GetActorLocation();
					}
					if (GetCharacterMovement())
					{
						const FString ZType = ZombiTipiSatirAdi.ToString();
						const float TargetShamblingSpeed = bIsBossOrKnight ? 145.0f : (ZType.Contains(TEXT("Kosucu")) ? 165.0f : (ZType.Contains(TEXT("Siskin")) ? 82.0f : 98.0f));
						GetCharacterMovement()->MaxWalkSpeed = TargetShamblingSpeed;
					}
					const float PhaseOffset = float(GetTypeHash(this) & 0xFF) * 0.04f;
					const float CycleTime = FMath::Fmod(NowTime + PhaseOffset, 8.5f);
					if (CycleTime < 4.2f)
					{
						const float WalkAngle = ((CycleTime < 2.1f) ? 0.0f : PI) + PhaseOffset + FMath::Sin(NowTime * 1.8f + PhaseOffset) * (bIsBossOrKnight ? 0.05f : 0.38f);
						const float PatrolDist = ActorHasTag(FName("Boss_Sunflower")) ? 220.0f : 115.0f;
						const FVector TargetSpot = *PeacefulHomeLoc + FVector(FMath::Cos(WalkAngle) * PatrolDist, FMath::Sin(WalkAngle) * PatrolDist, 0.0f);
						const FVector ToTarget = TargetSpot - GetActorLocation();
						if (ToTarget.Size2D() > 20.0f)
						{
							const FVector Dir2D = ToTarget.GetSafeNormal2D();
							SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir2D.Rotation(), 0.016f, bIsBossOrKnight ? 4.0f : 2.8f));
							AddMovementInput(Dir2D, bIsBossOrKnight ? 0.62f : 0.48f);
						}
					}
				}
				if (FDSCharacterBodyRig* MyRig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					MyRig->UpdatePose(this, GetMesh(), 0.016f, false, MyRig->CachedWeaponMesh.Get());
				}
				return;
			}

			// 1) Muttefik Boss'lar (Boss_Sunflower_Muttefik veya Boss_Pirate_Muttefik): Zombileri avlar ve koloniyi korur!
			if (ActorHasTag(FName("Boss_Sunflower_Muttefik")) || ActorHasTag(FName("Boss_Pirate_Muttefik")))
			{
				ACombatEnemy* ClosestEnemyZombie = nullptr;
				float ClosestDistSq = FMath::Square(2200.0f);
				const FVector MyLoc = GetActorLocation();

				for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
				{
					ACombatEnemy* Candidate = *It;
					if (!IsValid(Candidate) || Candidate == this || Candidate->CurrentHP <= 0.0f)
					{
						continue;
					}
					if (Candidate->ActorHasTag(FName("Boss_Sunflower_Muttefik")) ||
						Candidate->ActorHasTag(FName("Boss_Pirate_Muttefik")) ||
						Candidate->ActorHasTag(FName("Boss_Treeguard_Passive")) ||
						Candidate->ActorHasTag(FName("WanderingKnight")))
					{
						continue;
					}
					const float DistSq = FVector::DistSquared2D(MyLoc, Candidate->GetActorLocation());
					if (DistSq < ClosestDistSq)
					{
						ClosestDistSq = DistSq;
						ClosestEnemyZombie = Candidate;
					}
				}

				if (ClosestEnemyZombie)
				{
					const FVector ToZom = ClosestEnemyZombie->GetActorLocation() - MyLoc;
					const float Dist2D = ToZom.Size2D();
					const FVector Dir2D = ToZom.GetSafeNormal2D();
					if (!Dir2D.IsNearlyZero())
					{
						SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir2D.Rotation(), 0.016f, 8.0f));
					}
					if (Dist2D > 210.0f)
					{
						AddMovementInput(Dir2D, 0.88f);
					}
					else if (NowTime - *LastSpecialActionTime >= 1.25f)
					{
						*LastSpecialActionTime = NowTime;
						if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
						{
							OwnerRig->TriggerWeaponSwing(1.2f);
						}
						ClosestEnemyZombie->ApplyDamage(28.0f, this, ClosestEnemyZombie->GetActorLocation(), Dir2D * 450.0f + FVector(0.0f, 0.0f, 180.0f));
					}
				}
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					OwnerRig->UpdatePose(this, GetMesh(), 0.016f, bIsAttacking, OwnerRig->CachedWeaponMesh.Get());
				}
				return;
			}

			// 2) Pasif Treeguard veya Duello bekleyen Gezgin Sovalye saldirmaz, nobet tutar
			if (ActorHasTag(FName("Boss_Treeguard_Passive")) ||
				(ActorHasTag(FName("WanderingKnight")) && !ActorHasTag(FName("WanderingKnight_Hostile"))))
			{
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					OwnerRig->UpdatePose(this, GetMesh(), 0.016f, false, OwnerRig->CachedWeaponMesh.Get());
				}
				return;
			}

			// 3) Boss_Sunflower (Dusmanken) Faz 1: Orta mesafede oyuncuya Patlayici Yag Tenekesi (SM_Prop_OilTin) firlatir!
			if (ActorHasTag(FName("Boss_Sunflower")) && !ActorHasTag(FName("Boss_Sunflower_Muttefik")))
			{
				if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
				{
					const float DistToPlayer = FVector::Dist2D(GetActorLocation(), PlayerPawn->GetActorLocation());
					if (DistToPlayer > 280.0f && DistToPlayer < 1050.0f && (NowTime - *LastSpecialActionTime >= 4.2f))
					{
						*LastSpecialActionTime = NowTime;
						if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
						{
							OwnerRig->TriggerWeaponSwing(1.1f);
						}
						const FVector TargetGroundLoc = PlayerPawn->GetActorLocation();
						if (ICombatDamageable* PlayerDmg = Cast<ICombatDamageable>(PlayerPawn))
						{
							if (FVector::Dist2D(PlayerPawn->GetActorLocation(), TargetGroundLoc) <= 260.0f)
							{
								const FVector ThrowDir = (TargetGroundLoc - GetActorLocation()).GetSafeNormal2D();
								PlayerDmg->ApplyDamage(12.0f, this, TargetGroundLoc, ThrowDir * 300.0f + FVector(0.0f, 0.0f, 140.0f));
							}
						}
						if (UNiagaraSystem* OilSplashVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
						{
							UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OilSplashVFX, TargetGroundLoc, FRotator::ZeroRotator, FVector(2.2f));
						}
					}
				}
			}

			// StateTree / NavMesh hareketi devreye girmediyse dogrudan en yakin barikat, koylu veya oyuncuya yonel ve saldir
			if (GetVelocity().Size2D() < 15.0f || ActorHasTag(FName("WanderingKnight_Hostile")))
			{
				const FVector MyLoc = GetActorLocation();
				AActor* BestTarget = nullptr;
				float BestScoreDistSq = TNumericLimits<float>::Max();

				// Gezgin sovalye duelloda yalnizca oyuncuya odaklanir
				if (!ActorHasTag(FName("WanderingKnight_Hostile")))
				{
					for (TActorIterator<ADSBarikat> It(GetWorld()); It; ++It)
					{
						ADSBarikat* Barikat = *It;
						if (IsValid(Barikat) && Barikat->CurrentHP > 0.0f)
						{
							const float DistSq = FVector::DistSquared2D(MyLoc, Barikat->GetActorLocation());
							if (DistSq < FMath::Square(520.0f) && DistSq < BestScoreDistSq)
							{
								BestScoreDistSq = DistSq;
								BestTarget = Barikat;
							}
						}
					}

					if (!BestTarget)
					{
						for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
						{
							ADSVillager* Villager = *It;
							if (IsValid(Villager) && !Villager->bIsDowned && Villager->GetMesh() && !Villager->GetMesh()->IsSimulatingPhysics())
							{
								const float DistSq = FVector::DistSquared2D(MyLoc, Villager->GetActorLocation());
								if (DistSq < FMath::Square(750.0f) && DistSq < BestScoreDistSq)
								{
									BestScoreDistSq = DistSq;
									BestTarget = Villager;
								}
							}
						}
					}
				}

				if (!BestTarget)
				{
					if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
					{
						BestTarget = PlayerPawn;
						BestScoreDistSq = FVector::DistSquared2D(MyLoc, PlayerPawn->GetActorLocation());
					}
				}

				if (BestTarget)
				{
					const FVector ToTarget = BestTarget->GetActorLocation() - MyLoc;
					const float Dist2D = ToTarget.Size2D();
					const FVector Dir2D = ToTarget.GetSafeNormal2D();
					if (!Dir2D.IsNearlyZero())
					{
						SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir2D.Rotation(), 0.016f, 6.5f));
					}

					if (Dist2D > 148.0f && Dist2D < 2400.0f && !bIsAttacking)
					{
						AddMovementInput(Dir2D, 0.78f);
					}
					else if (Dist2D <= 155.0f && !bIsAttacking)
					{
						if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
						{
							OwnerRig->TriggerWeaponSwing(1.05f);
						}
						DoAIComboAttack();
					}
				}
			}
		}

		if (FDSCharacterBodyRig* MyRig = FDSCharacterBodyRig::GetOrCreateRig(this))
		{
			MyRig->UpdatePose(this, GetMesh(), 0.016f, bIsAttacking, MyRig->CachedWeaponMesh.Get());
		}
	}), 0.016f, true);
}

void ACombatEnemy::ApplyZombiTipiVerisi()
{
	FZombiTipiVerisi Veri;

	if (!UDSZombiTipiKutuphanesi::ZombiTipiVerisiYukle(ZombiTipleriTablosu, ZombiTipiSatirAdi, Veri))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: zombi tipi verisi ('%s') yuklenemedi, mevcut varsayilanlarla devam ediliyor"), *GetName(), *ZombiTipiSatirAdi.ToString());
		return;
	}

	MaxHP = Veri.HP;
	CurrentHP = MaxHP;
	GetCharacterMovement()->MaxWalkSpeed = Veri.MoveSpeed;
	MeleeDamage = Veri.Damage;
	CurrentAbilityTag = Veri.AbilityTag;

	// Gorsel ayirt edicilik: mesh'in TUM materyal slotlarina (0: Torso, 1: Head/Limbs) dynamic material instance
	// uygulayip TintColor'i "ZombieTint" ve "PaintTint" parametrelerine yaz.
	const int32 NumMats = GetMesh()->GetNumMaterials();
	UMaterialInstanceDynamic* FirstMID = nullptr;
	for (int32 i = 0; i < NumMats; ++i)
	{
		if (UMaterialInstanceDynamic* SlotMID = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			if (i == 0)
			{
				FirstMID = SlotMID;
			}
			SlotMID->SetVectorParameterValue(FName("ZombieTint"), Veri.TintColor);
			SlotMID->SetVectorParameterValue(FName("PaintTint"), Veri.TintColor);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ZDBG ZombieTint oncesi: satir=%s TintColor=(R=%.2f,G=%.2f,B=%.2f,A=%.2f) mesh=%s MID_gecerli=%d"),
		*ZombiTipiSatirAdi.ToString(), Veri.TintColor.R, Veri.TintColor.G, Veri.TintColor.B, Veri.TintColor.A,
		*GetMesh()->GetName(), FirstMID != nullptr);

	// Zombi tipine gore siluet olceklendirmesi (3D zombi parcalari ZombieRig uzerinden olusturulur)
	const FString TipStr = ZombiTipiSatirAdi.ToString();
	FVector BaseMeshScale(1.0f, 1.0f, 1.0f);

	if (TipStr.Equals(TEXT("Kosucu"), ESearchCase::IgnoreCase))
	{
		BaseMeshScale = FVector(0.94f, 0.94f, 1.04f);
	}
	else if (TipStr.Equals(TEXT("Siskin"), ESearchCase::IgnoreCase))
	{
		BaseMeshScale = FVector(1.18f, 1.18f, 1.08f);
	}
	else if (TipStr.Equals(TEXT("Cigirtkan"), ESearchCase::IgnoreCase))
	{
		BaseMeshScale = FVector(0.90f, 0.90f, 1.10f);
	}
	else if (TipStr.Equals(TEXT("Zirhli"), ESearchCase::IgnoreCase))
	{
		BaseMeshScale = FVector(1.15f, 1.15f, 1.08f);
		FDSCharacterBodyRig* ZRig = FDSCharacterBodyRig::GetOrCreateRig(this);
		if (UStaticMeshComponent* HeadMC = ZRig ? ZRig->HeadComp.Get() : nullptr)
		{
			if (UStaticMesh* HelmetSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/Meshes/SM_Helmet_Knight.SM_Helmet_Knight")))
			{
				UStaticMeshComponent* GearComp = NewObject<UStaticMeshComponent>(this);
				if (GearComp)
				{
					GearComp->SetStaticMesh(HelmetSM);
					GearComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					GearComp->AttachToComponent(HeadMC, FAttachmentTransformRules::KeepRelativeTransform);
					GearComp->RegisterComponent();
					const FBoxSphereBounds Bounds = HelmetSM->GetBounds();
					const float MaxExt = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
					const float AutoScale = (MaxExt > 0.01f) ? FMath::Clamp(28.0f / MaxExt, 0.1f, 80.0f) : 1.0f;
					GearComp->SetRelativeScale3D(FVector(AutoScale));
					GearComp->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f));
				}
			}
		}
	}

	GetMesh()->SetRelativeScale3D(BaseMeshScale);

	UE_LOG(LogTemp, Warning, TEXT("ZDBG ZombieTint sonrasi: satir=%s TintColor=(R=%.2f,G=%.2f,B=%.2f,A=%.2f) mesh=%s MID_gecerli=%d"),
		*ZombiTipiSatirAdi.ToString(), Veri.TintColor.R, Veri.TintColor.G, Veri.TintColor.B, Veri.TintColor.A,
		*GetMesh()->GetName(), FirstMID != nullptr);

	UE_LOG(LogTemp, Log, TEXT("%s: zombi tipi '%s' uygulandi (HP=%.1f Hiz=%.0f Hasar=%.1f Tint=%s)"),
		*GetName(), *ZombiTipiSatirAdi.ToString(), MaxHP, Veri.MoveSpeed, MeleeDamage, *Veri.TintColor.ToString());
}

void ACombatEnemy::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the death timer
	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);

	// clear the Drifter faz dongusu timer (hicbir zaman kurulmadiysa no-op)
	GetWorld()->GetTimerManager().ClearTimer(FazDonguTimer);

	// clear the hit-stop timer
	GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);

	// clear the stagger timer
	GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
}
