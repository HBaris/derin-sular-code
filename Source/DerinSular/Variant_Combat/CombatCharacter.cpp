// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "CombatLifeBar.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "CombatPlayerController.h"
#include "DSMeleeWeapon.h"
#include "CombatGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "DSBarikat.h"
#include "DSVillager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Engine/OverlapResult.h"
#include "UObject/ConstructorHelpers.h"
#include "DSCharacterBodyRig.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "InputCoreTypes.h"
#include "CombatEnemy.h"
#include "DSTrustComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"

DEFINE_LOG_CATEGORY(LogCombatCharacter);

ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// set default interact action
	static ConstructorHelpers::FObjectFinder<UInputAction> InteractActionFinder(TEXT("/Game/Input/Actions/IA_Interact"));
	if (InteractActionFinder.Succeeded())
	{
		InteractAction = InteractActionFinder.Object;
	}

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatCharacter::AttackMontageEnded);

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// create the camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	CameraBoom->TargetArmLength = DefaultCameraDistance;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;

	// create the orbiting camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// create the life bar widget component
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the player tag
	Tags.Add(FName("Player"));
}

void ACombatCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACombatCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACombatCharacter::ComboAttackPressed()
{
	// route the input
	DoComboAttackStart();
}

void ACombatCharacter::ChargedAttackPressed()
{
	// route the input
	DoChargedAttackStart();
}

void ACombatCharacter::ChargedAttackReleased()
{
	// route the input
	DoChargedAttackEnd();
}

void ACombatCharacter::ToggleCamera()
{
	// call the BP hook
	BP_ToggleCamera();
}

void ACombatCharacter::InteractPressed()
{
	const FVector PlayerLocation = GetActorLocation();
	const float SearchRadius = 320.0f;
	FDSColonyAndWorldState& WorldState = FDSColonyAndWorldState::Get(this);

	// 0a. FAZ 1 (DUNYA GANIMETI): Yakinda yerde duran fiziksel Dunya Loot paketi (WorldLoot) varsa [E] ile topla!
	AActor* NearestLoot = nullptr;
	float NearestLootDistSq = FMath::Square(340.0f);
	TArray<AActor*> NearbyLootBatch;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Act = *It;
		if (IsValid(Act) && !Act->IsHidden() && Act->ActorHasTag(FName("WorldLoot")))
		{
			const float D2 = FVector::DistSquared(PlayerLocation, Act->GetActorLocation());
			if (D2 <= FMath::Square(340.0f))
			{
				NearbyLootBatch.Add(Act);
				if (D2 < NearestLootDistSq)
				{
					NearestLootDistSq = D2;
					NearestLoot = Act;
				}
			}
		}
	}
	if (NearestLoot)
	{
		for (AActor* LAct : NearbyLootBatch)
		{
			WorldState.TryPickupWorldLoot(LAct);
		}
		if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
		{
			GM->AddScore(35 * NearbyLootBatch.Num());
		}
		return;
	}

	// 0b. FAZ 1 (KOLONI DEPO & TUZ AMBARI): Yakinda Koloni Sandigi (ColonyStorageChest) veya Tuz Ambari (SaltGranary) varsa sirt cantasini bosalt!
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Act = *It;
		if (IsValid(Act) && Act->ActorHasTag(FName("ColonyStorageChest")))
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(350.0f))
			{
				if (WorldState.DepositCarriedToColonyStorage())
				{
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						GM->AddScore(100);
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(6.0f, TEXT("Koloni ambarina maden ve kereste teslim edildi"));
						}
					}
				}
				return;
			}
		}
	}

	// 0c. FAZ 1 (KOLONI INSAAT SANTIYESI): Yakinda Santiye (ColonyBuildSite) varsa malzeme yatir ve insa et!
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Act = *It;
		if (IsValid(Act) && Act->ActorHasTag(FName("ColonyBuildSite")))
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(380.0f))
			{
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					OwnerRig->TriggerAttackSwing(2, 0.55f);
				}
				if (WorldState.ContributeToBuildSite(GetWorld()))
				{
					if (UNiagaraSystem* BuildVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BuildVFX, Act->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f), FRotator::ZeroRotator, FVector(1.5f));
					}
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						GM->AddScore(150);
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(10.0f, TEXT("Koloni santiyesi insa edildi"));
						}
					}
				}
				return;
			}
		}
	}

	// 0d. FAZ 2 (KADIM URETIM TEZGAHI & GDD 16 KOLONI KUTUPHANESI): Boss tariflerini uret ve kitaba arsivle!
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Act = *It;
		if (IsValid(Act) && (Act->ActorHasTag(FName("AncientCraftingAltar")) || Act->ActorHasTag(FName("ColonyLibraryLectern"))))
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(360.0f))
			{
				if (WorldState.CraftAndArchiveAtAncientStation(this))
				{
					if (WorldState.bCraftedLivingDarkSword)
					{
						if (FDSCharacterBodyRig* PRig = FDSCharacterBodyRig::GetOrCreateRig(this))
						{
							PRig->WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
							if (EquippedWeapon)
							{
								PRig->ApplyWeaponMeshToComponent(EquippedWeapon->WeaponMesh);
							}
						}
					}
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						GM->AddScore(200);
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(12.0f, TEXT("Boss tarifleri uretildi ve Koloni Kutuphanesine arsivlendi"));
						}
					}
				}
				return;
			}
		}
	}

	// 1. Yakinda baygin koylu varsa kurtar; saglikli koylu varsa Geri Donen Koylu diyalogu veya Gizli Isirik Tetkiki yap
	ADSVillager* NearestHealthyVillager = nullptr;
	float NearestVillagerDistSq = FMath::Square(SearchRadius);

	for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
	{
		ADSVillager* Villager = *It;
		if (!IsValid(Villager))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(PlayerLocation, Villager->GetActorLocation());
		if (DistSq <= FMath::Square(SearchRadius))
		{
			if (Villager->bIsDowned && Villager->TryKurtar(this))
			{
				WorldState.PushNotification(TEXT("BAYGIN KOYLU KURTARILDI! Kisisel Guven +15, Koloni Guveni +10, Ilahi Takdir +5"), 5.5f);
				return;
			}
			else if (!Villager->bIsDowned && DistSq < NearestVillagerDistSq)
			{
				NearestVillagerDistSq = DistSq;
				NearestHealthyVillager = Villager;
			}
		}
	}

	// 2. Yakinda Gezgin Sovalye (WanderingKnight) varsa Onur Duellosunu kabul et
	for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
	{
		ACombatEnemy* Enemy = *It;
		if (IsValid(Enemy) && Enemy->ActorHasTag(FName("WanderingKnight")) && !Enemy->ActorHasTag(FName("WanderingKnight_Hostile")) && !WorldState.bWanderingKnightDefeated)
		{
			if (FVector::DistSquared(PlayerLocation, Enemy->GetActorLocation()) <= FMath::Square(380.0f))
			{
				Enemy->Tags.AddUnique(FName("WanderingKnight_Hostile"));
				WorldState.bWanderingKnightDuelActive = true;
				WorldState.PushNotification(TEXT("GEZGIN SOVALYE ONUR DUELLOSU BASLADI! Teke tek dovus: kazanirsan Usta Dovuscu (+%25 Hasar/Hiz) acilir!"), 7.0f);
				return;
			}
		}
	}

	// 3. Yakinda Supheli Kaya (SuspiciousBoulder) varsa Gizli Kadim Sehir Portalini ac
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Act = *It;
		if (IsValid(Act) && Act->ActorHasTag(FName("SuspiciousBoulder")) && !WorldState.bHiddenCityPortalOpened)
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(440.0f))
			{
				WorldState.bHiddenCityPortalOpened = true;
				WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor + 25.0f, 0.0f, 100.0f);
				Act->SetActorLocation(Act->GetActorLocation() + FVector(0.0f, 0.0f, 240.0f));
				Act->SetActorRotation(Act->GetActorRotation() + FRotator(25.0f, 40.0f, 0.0f));
				MaxHP = 150.0f;
				CurrentHP = MaxHP;
				if (LifeBarWidget)
				{
					LifeBarWidget->SetLifePercentage(1.0f);
				}
				if (UNiagaraSystem* PortalVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail")))
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PortalVFX, Act->GetActorLocation() - FVector(0.0f, 0.0f, 180.0f), FRotator::ZeroRotator, FVector(4.5f));
				}
				if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
				{
					GM->AddScore(250);
					if (GM->TrustComponent)
					{
						GM->TrustComponent->AddTrust(30.0f, TEXT("Sakli Sehir Portali kesfedildi"));
					}
				}
				WorldState.PushNotification(TEXT("GIZLI KADIM SEHIR PORTALI ACILDI! Kadim siginak kutsamasi: Maksimum Can 150 oldu, +30 Guven!"), 8.0f);
				return;
			}
		}

		// 3a. Yakinda Sivi Yag Surulmus Ayakkabi (OiledBootsDrop) varsa kusan ve TABANLARI YAGLAMAK basarimini ac!
		if (IsValid(Act) && Act->ActorHasTag(FName("OiledBootsDrop")))
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(420.0f))
			{
				Act->SetActorHiddenInGame(true);
				WorldState.bOiledBootsEquipped = true;
				if (GetCharacterMovement())
				{
					GetCharacterMovement()->MaxWalkSpeed = 545.0f;
				}
				WorldState.UnlockSteamAchievement(
					TEXT("TABANLARI YAGLAMAK"),
					TEXT("Sivi Yag Surulmus Ayakkabiyi giyip ruzgar gibi kostun! (+%36 Hareket Hizi)")
				);
				WorldState.PushNotification(TEXT("SIVI YAG SURULMUS AYAKKABI ALINDI (+%36 Hareket Hizi)! BASARIM ACILDI: [TABANLARI YAGLAMAK]!"), 7.5f);
				return;
			}
		}

		// 3b. FAZ 3 (DENIZCILIK): Tersane Iskelesi / Pruva Zipkin Topu / Derin Sular Koyunda [E] ile Dev Balinaya Zipkin Atesle & Filo Maden Konvoyunu Baslat!
		if (IsValid(Act) && (Act->ActorHasTag(FName("ShipyardPier")) || Act->ActorHasTag(FName("HarpoonLauncher")) || Act->ActorHasTag(FName("DeepWaterZone")) || Act->ActorHasTag(FName("DeepWaterLife"))))
		{
			if (FVector::DistSquared(PlayerLocation, Act->GetActorLocation()) <= FMath::Square(480.0f))
			{
				if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					OwnerRig->TriggerAttackSwing(1, 0.55f);
				}
				WorldState.LaunchHarpoonAtWhaleAndSail(GetWorld(), this);
				WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor + 7.0f, 0.0f, 100.0f);
				CurrentHP = FMath::Min(MaxHP, CurrentHP + 25.0f);
				if (LifeBarWidget)
				{
					LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
				}
				if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
				{
					GM->AddScore(180);
					if (GM->TrustComponent)
					{
						GM->TrustComponent->AddTrust(12.0f, TEXT("Derin Sular Dev Balina Avi ve Adalar Arasi Filo Madenciligi"));
					}
				}
				return;
			}
		}
	}

	// 4. Oyuncunun yakininda hasarli barikat varsa tamir et
	ADSBarikat* NearestBarikat = nullptr;
	float MinBarikatDistSq = FMath::Square(320.0f);
	for (TActorIterator<ADSBarikat> It(GetWorld()); It; ++It)
	{
		ADSBarikat* Barikat = *It;
		if (IsValid(Barikat))
		{
			const float DistSq = FVector::DistSquared(PlayerLocation, Barikat->GetActorLocation());
			if (DistSq < MinBarikatDistSq)
			{
				MinBarikatDistSq = DistSq;
				NearestBarikat = Barikat;
			}
		}
	}
	if (NearestBarikat)
	{
		NearestBarikat->TamirEt(this);
		WorldState.PushNotification(TEXT("BARIKAT TAMIR EDILDI! Savunma hatti guclendirildi (+5 Guven)."), 4.5f);
		return;
	}

	// 5. Saglikli koylu ile etkilesim: Olumsuz Ani NPC (Baba/Dede/Anneanne), Komsu Koy Elcisi, Geri Donen Koylu veya Gizli Isirik Tetkiki
	if (NearestHealthyVillager)
	{
		if (NearestHealthyVillager->ActorHasTag(FName("ImmortalMemoryNPC")) ||
			NearestHealthyVillager->ActorHasTag(FName("AkilHocasi_Baba")) ||
			NearestHealthyVillager->ActorHasTag(FName("AniNPC_Dede")) ||
			NearestHealthyVillager->ActorHasTag(FName("AniNPC_Anneanne")))
		{
			// tam can + skor + guven her E basisinda verilmesin
			if (!TryConsumeCooldown(NextMemoryNPCBlessingAllowedTime, MemoryNPCBlessingCooldown, TEXT("Ani Kosesi kutsamasi")))
			{
				return;
			}
			WorldState.InteractWithImmortalMemoryNPC(NearestHealthyVillager);
			CurrentHP = MaxHP;
			if (LifeBarWidget)
			{
				LifeBarWidget->SetLifePercentage(1.0f);
			}
			if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
			{
				GM->AddScore(200);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(15.0f, TEXT("Olumsuz Ani NPC ve Yasayan Ani Arsivi etkilesimi"));
				}
			}
			return;
		}

		if (NearestHealthyVillager->ActorHasTag(FName("NeighborVillageEnvoy")) && !WorldState.bNeighborAllianceFormed && !WorldState.bDarkPathChosen && !WorldState.bWeaponsSaleRefused)
		{
			WorldState.ResolveNeighborDiplomacyChoice(GetWorld(), 0);
			if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
			{
				GM->AddScore(150);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(18.0f, TEXT("Komsu Koy ile Tuz ve Silah Ittifaki kuruldu"));
				}
			}
			return;
		}

		if (NearestHealthyVillager->ActorHasTag(FName("ReturningVillager")))
		{
			NearestHealthyVillager->Tags.Remove(FName("ReturningVillager"));
			float& PTrust = WorldState.VillagerPersonalTrustMap.FindOrAdd(NearestHealthyVillager, 65.0f);
			PTrust = FMath::Clamp(PTrust + 22.0f, 0.0f, 100.0f);
			if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
			{
				GM->AddScore(75);
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(12.0f, TEXT("Geri donen koylu koloniye kabul edildi"));
				}
			}
			WorldState.PushNotification(TEXT("GERI DONEN KOYLU: 'Patron nerdeydin ya, biz oldun sandik! Koloniye geri dondum, baltam emrinde!' (+12 Guven)"), 7.0f);
			return;
		}

		bool& bSuspect = WorldState.HiddenBiteSuspectMap.FindOrAdd(NearestHealthyVillager, false);
		float& PTrust = WorldState.VillagerPersonalTrustMap.FindOrAdd(NearestHealthyVillager, 65.0f);
		if (bSuspect)
		{
			bSuspect = false;
			PTrust = FMath::Clamp(PTrust + 16.0f, 0.0f, 100.0f);
			NearestHealthyVillager->CurrentHP = FMath::Max(NearestHealthyVillager->CurrentHP, 55.0f);
			if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
			{
				if (GM->TrustComponent)
				{
					GM->TrustComponent->AddTrust(8.0f, TEXT("Gizli isirik erken tetkik edilip iyilestirildi"));
				}
			}
			WorldState.PushNotification(FString::Printf(TEXT("KOYLU TETKIKI: Gizli zombi isirigi erken fark edildi ve sifali yag ile temizlendi! [Kisisel Guven: %.0f/100]"), PTrust), 6.5f);
		}
		else
		{
			PTrust = FMath::Clamp(PTrust + 6.0f, 0.0f, 100.0f);
			const bool bWarrior = NearestHealthyVillager->ActorHasTag(TEXT("BeardedWarrior"));
			WorldState.PushNotification(FString::Printf(TEXT("KOYLU TETKIKI (%s): Isirik izi yok, savasa hazir! [Kisisel Guven: %.0f/100]"),
				bWarrior ? TEXT("Sakalli Savasci") : TEXT("Koloni Milisi"), PTrust), 5.5f);
		}
	}
}

void ACombatCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACombatCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

namespace
{
	struct FDSDiscreteComboState
	{
		float StepStartTime = -100.0f;
		float StepEndTime = -100.0f;
		int32 LastFinishedStep = -1;
		bool bNextStepBuffered = false;
	};

	FDSDiscreteComboState& GetComboState(const ACombatCharacter* Char)
	{
		static TMap<FObjectKey, FDSDiscreteComboState> Map;
		return Map.FindOrAdd(FObjectKey(Char));
	}
}

void ACombatCharacter::DoComboAttackStart()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FDSDiscreteComboState& CS = GetComboState(this);

	// are we already playing an attack animation?
	if (bIsAttacking)
	{
		// Ayni tiklamada BP + C++ cift tetiklemesini filtrele: mevcut darbe basladiktan en az 0.22 sn sonra
		// yapilan YENI tiklama bir sonraki kombo adimini (1. tik -> 1. vurus, 2. tik -> 2. vurus, 3. tik -> 3. vurus) siraya alir.
		if ((Now - CS.StepStartTime) >= 0.22f)
		{
			CS.bNextStepBuffered = true;
			CachedAttackInputTime = Now;
		}
		return;
	}

	// perform a combo attack
	ComboAttack();
}

void ACombatCharacter::DoComboAttackEnd()
{
	// stub
}

void ACombatCharacter::DoChargedAttackStart()
{
	// raise the charging attack flag
	bIsChargingAttack = true;

	if (bIsAttacking)
	{
		// do not attack if the charge animation hasn't looped at least once
		if (!bHasLoopedChargedAttack)
		{
			bHasReleasedChargedAttack = false;
		}

		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	ChargedAttack();
}

void ACombatCharacter::DoChargedAttackEnd()
{
	// lower the charging attack flag
	bIsChargingAttack = false;

	// have we done the charge loop at least once and haven't released the button yet?
	if (bHasLoopedChargedAttack && !bHasReleasedChargedAttack)
	{
		// release the charge and resolve the attack
		bHasReleasedChargedAttack = true;

		LoopOrResolveChargedAttack();
	}
}

void ACombatCharacter::ResetHP()
{
	// reset the current HP total
	CurrentHP = MaxHP;

	// update the life bar
	LifeBarWidget->SetLifePercentage(1.0f);
}

void ACombatCharacter::ComboAttack()
{
	// raise the attacking flag
	bIsAttacking = true;

	// reset the combo count
	ComboCount = 0;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

	FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (!RigPtr)
	{
		return;
	}
	FDSCharacterBodyRig& Rig = *RigPtr;
	static int32 RollingSwingCounter = 0;

	float PlayRate = 1.25f;
	float SwingDur = 0.45f;
	switch (Rig.WeaponStyle)
	{
	case EDSCombatWeaponStyle::BattleAxe:       PlayRate = 1.25f; SwingDur = 0.46f; break;
	case EDSCombatWeaponStyle::SurvivalAxe:     PlayRate = 1.55f; SwingDur = 0.34f; break;
	case EDSCombatWeaponStyle::MedievalAxe:     PlayRate = 1.20f; SwingDur = 0.48f; break;
	case EDSCombatWeaponStyle::WarHammer:       PlayRate = 0.95f; SwingDur = 0.58f; break;
	case EDSCombatWeaponStyle::UnarmedFists:    PlayRate = 1.55f; SwingDur = 0.34f; break;
	case EDSCombatWeaponStyle::LivingDarkSword:
		if (FDSColonyAndWorldState::Get(this).bTwoHandedSwordMasteryUnlocked)
		{
			PlayRate = 1.32f; SwingDur = 0.44f; // Cift Elli Kilic 3'lu Agir Kombo
		}
		else
		{
			PlayRate = 1.48f; SwingDur = 0.36f; // Tek Elli Kilic (Korsan Boss oncesi)
		}
		break;
	default: break;
	}

	if (FDSColonyAndWorldState::Get(this).bMasterCombatUnlocked)
	{
		PlayRate *= 1.20f;
		SwingDur *= 0.85f;
	}

	Rig.TriggerAttackSwing(RollingSwingCounter++, SwingDur);

	// Silahli stillerde (balta/cekic/kilic) gercek kilic-balta kombo animasyonlari (UAL2) oynatilir;
	// 1. tiklama = 1. vurus (A), kisa sure icinde 2. tiklama = 2. vurus (B), 3. tiklama = 3. vurus (C).
	if (Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		FDSDiscreteComboState& CS = GetComboState(this);
		int32 InitialStep = 0;
		if (CS.LastFinishedStep >= 0 && CS.LastFinishedStep < 2 && (Now - CS.StepEndTime) <= 0.68f)
		{
			InitialStep = CS.LastFinishedStep + 1;
		}
		if (PlayWeaponComboStep(InitialStep))
		{
			return;
		}
	}

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
		}
	}

}

UAnimSequence* ACombatCharacter::LoadUALAnim(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Characters/Anims_UAL/Manny/MM_UAL_%s.MM_UAL_%s"), Name, Name);
	return LoadObject<UAnimSequence>(nullptr, *Path);
}

void ACombatCharacter::PerformCombatJump()
{
	if (!GetCharacterMovement() || CurrentHP <= 0.0f)
	{
		return;
	}
	if (GetCharacterMovement()->IsFalling())
	{
		return;
	}
	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(WeaponChainTimer);
	TM.ClearTimer(WeaponRecoverTimer);
	TM.ClearTimer(WeaponHitTimer);
	TM.ClearTimer(WeaponHitTimer2);
	bIsAttacking = false;
	WeaponComboStep = 0;
	ComboCount = 0;

	Jump();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	GetCharacterMovement()->Velocity.Z = 600.0f;

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimSequence* JumpSeq = LoadUALAnim(TEXT("Jump_Start")))
		{
			ActiveWeaponMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(JumpSeq, FName("DefaultSlot"), 0.05f, 0.25f, 1.0f, 1, -1.0f, 0.0f);
		}
	}
}

namespace
{
	float GetWeaponAnimPlayRate(EDSCombatWeaponStyle Style, const UObject* WorldContext)
	{
		const FDSColonyAndWorldState& WorldState = FDSColonyAndWorldState::Get(WorldContext);
		float Rate = 1.0f;
		switch (Style)
		{
		case EDSCombatWeaponStyle::BattleAxe:       Rate = 0.95f; break;
		case EDSCombatWeaponStyle::SurvivalAxe:     Rate = 1.20f; break;
		case EDSCombatWeaponStyle::MedievalAxe:     Rate = 0.92f; break;
		case EDSCombatWeaponStyle::WarHammer:       Rate = 0.78f; break;
		case EDSCombatWeaponStyle::LivingDarkSword: Rate = WorldState.bTwoHandedSwordMasteryUnlocked ? 0.90f : 1.10f; break;
		default: break;
		}
		if (WorldState.bMasterCombatUnlocked)
		{
			Rate *= 1.18f;
		}
		return Rate;
	}
}

bool ACombatCharacter::PlayWeaponComboStep(int32 Step)
{
	static const TCHAR* StepAnims[] = { TEXT("Sword_Regular_A"), TEXT("Sword_Regular_B"), TEXT("Sword_Regular_C") };
	// vurusun bicagin hedefe degdigi an (animasyon uzunlugunun orani)
	static const float StepHitFrac[]  = { 0.50f, 0.50f, 0.32f };
	static const float StepHit2Frac[] = { -1.0f, -1.0f, 0.58f };

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimSequence* Seq = LoadUALAnim(StepAnims[FMath::Clamp(Step, 0, 2)]);
	if (!AnimInstance || !Seq)
	{
		return false;
	}

	FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (!RigPtr)
	{
		return false;
	}
	FDSCharacterBodyRig& Rig = *RigPtr;
	const float Rate = GetWeaponAnimPlayRate(Rig.WeaponStyle, this) * (Step == 2 ? 1.15f : 1.0f);
	const float Duration = Seq->GetPlayLength() / FMath::Max(Rate, 0.05f);

	bIsAttacking = true;
	WeaponComboStep = Step;
	ComboCount = Step;
	CachedAttackInputTime = -100.0f;
	if (GetWorld())
	{
		FDSDiscreteComboState& CS = GetComboState(this);
		CS.StepStartTime = GetWorld()->GetTimeSeconds();
		CS.bNextStepBuffered = false;
		CS.LastFinishedStep = -1;
	}
	NotifyEnemiesOfIncomingAttack();

	ActiveWeaponMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Seq, FName("DefaultSlot"), 0.06f, 0.18f, Rate, 1, -1.0f, 0.0f);

	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(WeaponHitTimer);
	TM.ClearTimer(WeaponHitTimer2);
	TM.ClearTimer(WeaponChainTimer);
	TM.ClearTimer(WeaponRecoverTimer);

	const int32 SafeStep = FMath::Clamp(Step, 0, 2);
	TM.SetTimer(WeaponHitTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { DoAttackTrace(FName("hand_r")); }), FMath::Max(Duration * StepHitFrac[SafeStep], 0.01f), false);
	if (StepHit2Frac[SafeStep] > 0.0f)
	{
		TM.SetTimer(WeaponHitTimer2, FTimerDelegate::CreateWeakLambda(this, [this]() { DoAttackTrace(FName("hand_r")); }), FMath::Max(Duration * StepHit2Frac[SafeStep], 0.01f), false);
	}

	// kombo penceresi: sadece bu darbe sirasinda YENI bir tiklama yapildiysa sonraki darbeye gec
	TM.SetTimer(WeaponChainTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ResolveWeaponComboChain(); }), FMath::Max(Duration * (SafeStep == 2 ? 0.82f : 0.86f), 0.02f), false);
	return true;
}

void ACombatCharacter::ResolveWeaponComboChain()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FDSDiscreteComboState& CS = GetComboState(this);
	const bool bHasBufferedInput = CS.bNextStepBuffered;

	if (bHasBufferedInput && WeaponComboStep < 2 && CurrentHP > 0.0f)
	{
		CS.bNextStepBuffered = false;
		CachedAttackInputTime = -100.0f;
		PlayWeaponComboStep(WeaponComboStep + 1);
		return;
	}

	// Yeni tiklama yapilmadiysa tek vurusta durur; 0.68 sn icinde tekrar tiklanirsa bir sonraki kombo adimindan devam eder
	const int32 FinishedStep = WeaponComboStep;
	bIsAttacking = false;
	CS.bNextStepBuffered = false;
	CS.StepEndTime = Now;
	CS.LastFinishedStep = (FinishedStep < 2) ? FinishedStep : -1;
	WeaponComboStep = 0;
	ComboCount = 0;

	if (FinishedStep < 2)
	{
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			const TCHAR* RecName = (FinishedStep == 0) ? TEXT("Sword_Regular_A_Rec") : TEXT("Sword_Regular_B_Rec");
			if (UAnimSequence* Rec = LoadUALAnim(RecName))
			{
				ActiveWeaponMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Rec, FName("DefaultSlot"), 0.10f, 0.25f, 1.25f, 1, -1.0f, 0.0f);
			}
		}
	}
}

bool ACombatCharacter::PlayWeaponHeavyAttack()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimSequence* Seq = LoadUALAnim(TEXT("Sword_Heavy_Combo"));
	if (!AnimInstance || !Seq)
	{
		return false;
	}

	FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (!RigPtr)
	{
		return false;
	}
	FDSCharacterBodyRig& Rig = *RigPtr;
	const float Rate = GetWeaponAnimPlayRate(Rig.WeaponStyle, this) * 1.25f;
	const float Duration = Seq->GetPlayLength() / FMath::Max(Rate, 0.05f);

	bIsAttacking = true;
	NotifyEnemiesOfIncomingAttack();
	ActiveWeaponMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Seq, FName("DefaultSlot"), 0.08f, 0.25f, Rate, 1, -1.0f, 0.0f);

	FTimerManager& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(WeaponChainTimer);
	TM.ClearTimer(WeaponHitTimer2);

	// agir kombo boyunca 4 darbe (her biri ayri vurus izi)
	TSharedRef<int32> HitsLeft = MakeShared<int32>(4);
	TM.SetTimer(WeaponHitTimer, FTimerDelegate::CreateWeakLambda(this, [this, HitsLeft]()
	{
		if (*HitsLeft > 0 && CurrentHP > 0.0f)
		{
			--(*HitsLeft);
			DoAttackTrace(FName("hand_r"));
		}
	}), Duration * 0.19f, true, Duration * 0.16f);

	TM.SetTimer(WeaponRecoverTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		GetWorld()->GetTimerManager().ClearTimer(WeaponHitTimer);
		bIsAttacking = false;
		bHasLoopedChargedAttack = false;
		bHasReleasedChargedAttack = false;
	}), Duration * 0.95f, false);
	return true;
}

void ACombatCharacter::ChargedAttack()
{
	// raise the attacking flag
	bIsAttacking = true;

	// reset the charge loop flag
	bHasLoopedChargedAttack = false;

	// reset the charge release flag
	bHasReleasedChargedAttack = false;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

	FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (!RigPtr)
	{
		return;
	}
	FDSCharacterBodyRig& Rig = *RigPtr;
	Rig.TriggerAttackSwing(2, 0.65f);

	// Silahli stillerde agir kombo (UAL2 Sword_Heavy_Combo, 4 darbe)
	if (Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists && PlayWeaponHeavyAttack())
	{
		return;
	}

	// play the charged attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ChargedAttackMontage, 1.2f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ChargedAttackMontage);
		}
	}
}

void ACombatCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;

	// check if we have a non-stale cached input
	if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance)
	{
		// are we holding the charged attack button?
		if (bIsChargingAttack)
		{
			// do a charged attack
			ChargedAttack();
		}
		else
		{
			// do a regular attack
			ComboAttack();
		}
	}
}

void ACombatCharacter::DoAttackTrace(FName DamageSourceBone)
{
	// EquippedWeapon sadece gorsel amaclidir; hasar/vurus tespiti burada, mevcut kemik-sweep akisiyla degismeden devam eder
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (!RigPtr)
	{
		return;
	}
	FDSCharacterBodyRig& Rig = *RigPtr;

	float EffectiveDistance = MeleeTraceDistance;
	float EffectiveRadius = MeleeTraceRadius;
	float EffectiveDamage = MeleeDamage;
	float EffectiveKnockback = MeleeKnockbackImpulse;

	switch (Rig.WeaponStyle)
	{
	case EDSCombatWeaponStyle::BattleAxe:
		EffectiveDistance = MeleeTraceDistance * 1.45f;
		EffectiveRadius = MeleeTraceRadius * 1.35f;
		EffectiveDamage = MeleeDamage * 1.5f;
		EffectiveKnockback = MeleeKnockbackImpulse * 1.3f;
		break;
	case EDSCombatWeaponStyle::SurvivalAxe:
		EffectiveDistance = MeleeTraceDistance * 1.25f;
		EffectiveRadius = MeleeTraceRadius * 1.2f;
		EffectiveDamage = MeleeDamage * 1.15f;
		break;
	case EDSCombatWeaponStyle::MedievalAxe:
		EffectiveDistance = MeleeTraceDistance * 1.65f;
		EffectiveRadius = MeleeTraceRadius * 1.35f;
		EffectiveDamage = MeleeDamage * 1.4f;
		break;
	case EDSCombatWeaponStyle::WarHammer:
		EffectiveDistance = MeleeTraceDistance * 1.55f;
		EffectiveRadius = MeleeTraceRadius * 1.75f;
		EffectiveDamage = MeleeDamage * 2.25f;
		EffectiveKnockback = MeleeKnockbackImpulse * 2.0f;
		break;
	case EDSCombatWeaponStyle::UnarmedFists:
		EffectiveDistance = MeleeTraceDistance * 1.1f;
		EffectiveRadius = MeleeTraceRadius * 1.1f;
		EffectiveDamage = MeleeDamage * 0.9f;
		break;
	case EDSCombatWeaponStyle::LivingDarkSword:
		if (FDSColonyAndWorldState::Get(this).bTwoHandedSwordMasteryUnlocked)
		{
			// Korsan Boss'tan ogrenilen Cift Elli Kilic Ustaligi (Genis Menzil + Yuksek Hasar)
			EffectiveDistance = MeleeTraceDistance * 1.95f;
			EffectiveRadius = MeleeTraceRadius * 1.75f;
			EffectiveDamage = MeleeDamage * 2.35f;
			EffectiveKnockback = MeleeKnockbackImpulse * 1.85f;
		}
		else
		{
			// Korsan Boss ogretmeden once Tek Elli Kilic
			EffectiveDistance = MeleeTraceDistance * 1.45f;
			EffectiveRadius = MeleeTraceRadius * 1.25f;
			EffectiveDamage = MeleeDamage * 1.55f;
			EffectiveKnockback = MeleeKnockbackImpulse * 1.35f;
		}
		break;
	default:
		break;
	}

	FDSColonyAndWorldState& WorldState = FDSColonyAndWorldState::Get(this);




	if (WorldState.bRiposteReady)
	{
		WorldState.bRiposteReady = false;
		EffectiveDamage *= 2.5f;
		EffectiveKnockback *= 2.2f;
		WorldState.PushNotification(TEXT("CIFT ELLI KILIC RIPOSTE DARBESI! Kusursuz Parry sonrasi 2.5x Kritik Hasar vuruldu!"), 5.0f);
	}
	if (WorldState.bMasterCombatUnlocked)
	{
		EffectiveDamage *= 1.25f;
	}
	if (WorldState.bSunflowerBossAllied)
	{
		EffectiveDamage *= 1.12f; // Aycicegi Yagi / Tarim kutsamasi bonusu
	}
	if (WorldState.bBlacksmithForgeBuilt)
	{
		EffectiveDamage *= 1.30f; // 3 Kisilik Demir Ocagi Celik Silah Kalitesi Bonusu (+%30 Hasar)
	}
	if (WorldState.bCraftedLivingDarkSword && Rig.WeaponStyle == EDSCombatWeaponStyle::LivingDarkSword)
	{
		EffectiveDamage *= 1.35f; // Kadim Karanlik Kilic Bonusu (+%35 Hasar & Vurus Basina Can Calma)
	}

	// Orman Agaci Kesme (BattleAxe / SurvivalAxe / MedievalAxe / LivingDarkSword) ve Iblis Kuzgun Easter Egg kontrolu
	if (GetWorld())
	{
		const FVector MyLoc = GetActorLocation();
		const FVector MyFwd = GetActorForwardVector();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Act = *It;
			if (!IsValid(Act) || Act == this)
			{
				continue;
			}

			const FVector ToAct = Act->GetActorLocation() - MyLoc;
			const float Dist2D = ToAct.Size2D();
			if (Dist2D > 310.0f || FVector::DotProduct(ToAct.GetSafeNormal2D(), MyFwd) < 0.25f)
			{
				continue;
			}

			// Iblis Kuzgun Easter Egg
			if (Act->ActorHasTag(FName("ColonyBuildSite")) && Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists)
			{
				if (WorldState.ContributeToBuildSite(GetWorld()))
				{
					if (UNiagaraSystem* BuildVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BuildVFX, Act->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f), FRotator::ZeroRotator, FVector(1.5f));
					}
				}
				break;
			}

			// FAZ 1 (KOLONI MADENCILIGI): Demir Damari (OreVein_Iron), Tuz Kayasi (OreVein_Salt) ve Kaya (OreVein_Stone) Kazma
			if (Act->ActorHasTag(FName("OreVein")) && !Act->IsHidden() && Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists)
			{
				const bool bIsHammer = (Rig.WeaponStyle == EDSCombatWeaponStyle::WarHammer);
				const float VeinDmg = bIsHammer ? 25.0f : 10.0f;
				float& VeinHP = WorldState.ResourceNodeHPMap.FindOrAdd(FObjectKey(Act), 50.0f);
				VeinHP -= VeinDmg;
				WorldState.TotalOreMinedCount += 1;

				// Vurus sarsintisi ve kivilcim efekti
				Act->SetActorRotation(Act->GetActorRotation() + FRotator(FMath::FRandRange(-3.5f, 3.5f), FMath::FRandRange(-5.0f, 5.0f), 0.0f));
				if (!FParse::Param(FCommandLine::Get(), TEXT("AutoCapturePreview")))
				{
					if (UNiagaraSystem* OreVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), OreVFX, Act->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(1.35f));
					}
				}

				const FName PrimaryLootTag = Act->ActorHasTag(FName("OreVein_Iron"))
					? FName("Loot_IronOre")
					: (Act->ActorHasTag(FName("OreVein_Salt")) ? FName("Loot_Salt") : FName("Loot_Stone"));
				const TCHAR* VeinTypeName = Act->ActorHasTag(FName("OreVein_Iron"))
					? TEXT("DEMIR MADENI DAMARI")
					: (Act->ActorHasTag(FName("OreVein_Salt")) ? TEXT("KIYI TUZ KAYASI") : TEXT("KAYA DAMARI"));

				// Her vurusta yere fiziksel 3B cevher/tas parcasi (WorldLoot) firlar!
				FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), Act->GetActorLocation(), PrimaryLootTag, WorldState.TotalOreMinedCount);

				if (VeinHP <= 0.0f)
				{
					// Damar kirildi: 2 ekstra fiziksel loot parcasi daha sacilir ve kaya ufalanir
					FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), Act->GetActorLocation(), PrimaryLootTag, WorldState.TotalOreMinedCount + 1);
					FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), Act->GetActorLocation(), FName("Loot_Stone"), WorldState.TotalOreMinedCount + 2);
					Act->SetActorScale3D(Act->GetActorScale3D() * FVector(0.65f, 0.65f, 0.35f));
					Act->Tags.Remove(FName("OreVein"));
					WorldState.PushNotification(FString::Printf(
						TEXT("%s KIRILDI! Yere fiziksel 3B Cevher ve Tas parcalari sacildi — [E] ile toplayip Sandiga/Santiyeye tasi!"),
						VeinTypeName), 6.5f);
				}
				else
				{
					WorldState.PushNotification(FString::Printf(
						TEXT("%s KAZILIYOR (Kalan Damar: %.0f HP%s) — Yere 3B Cevher dustu ([E] ile Topla)!"),
						VeinTypeName,
						VeinHP,
						bIsHammer ? TEXT(" | [4] Balyoz Tam Hasar!") : TEXT(" | Ipucu: [4] Savas Cekici 2.5x hizli kazar!")), 5.0f);
				}
				break;
			}
			if (Act->ActorHasTag(FName("SwainCrow")) && !WorldState.bSwainEasterEggTriggered)
			{
				WorldState.bSwainEasterEggTriggered = true;
				WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor + 10.0f, 0.0f, 100.0f);
				Act->SetActorScale3D(Act->GetActorScale3D() * 2.85f);
				Act->SetActorLocation(Act->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f));
				if (UNiagaraSystem* CrowVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")))
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CrowVFX, Act->GetActorLocation(), FRotator::ZeroRotator, FVector(3.2f));
				}
				WorldState.PushNotification(TEXT("IBLIS KUZGUN SIRRI: 'Kuzgunlar her seyi gorur...' Karga dev iblis-kuzgun formuna burundu! (+10 Ilahi Takdir)"), 7.5f);
				break;
			}

			// Agac kesme (Yasli Ulu Agac vs. Yamacindaki Genc/Kisa Agac) ve Ormanin Gazabi (Treeguard uyanisi)
			if (Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists &&
				(Act->ActorHasTag(FName("ChopTree")) || Act->GetName().Contains(TEXT("PineTree"))))
			{
				const bool bIsYoungTree = Act->ActorHasTag(FName("YoungTree"));
				if (bIsYoungTree)
				{
					// Yasli agacin yamacindaki henuz genc ve kisa agaci kesmek Treeguard'i dogrudan kizdirir!
					Act->SetActorHiddenInGame(true);
					Act->SetActorEnableCollision(false);
					Act->Tags.Remove(FName("ChopTree"));

					WorldState.WoodCount += 2;
					WorldState.YoungTreesCutCount += 1;
					WorldState.UnplantedTreeCuts += 1;
					WorldState.ForestWrathCounter += 2;
					WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor - 8.0f, 0.0f, 100.0f);

					// Eger oyuncu kestigi genc agacin yerine 10 saniye icinde [C] ile tohum ekmezse Treeguard yine de uyanir!
					TSharedRef<FTimerHandle> SeedCheckTimer = MakeShared<FTimerHandle>();
					GetWorld()->GetTimerManager().SetTimer(*SeedCheckTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
						if (WS.UnplantedTreeCuts > 0 && !WS.bTreeguardAwakened && GetWorld())
						{
							WS.bTreeguardAwakened = true;
							WS.ForestWrathCounter = 3;
							for (TActorIterator<ACombatEnemy> BossIt(GetWorld()); BossIt; ++BossIt)
							{
								ACombatEnemy* Boss = *BossIt;
								if (IsValid(Boss) && Boss->ActorHasTag(FName("Boss_Treeguard")))
								{
									Boss->Tags.Remove(FName("Boss_Treeguard_Passive"));
									Boss->SetActorLocation(GetActorLocation() + GetActorForwardVector() * 480.0f + FVector(0.0f, 0.0f, 40.0f));
									break;
								}
							}
							WS.PushNotification(TEXT("Akil Hocasi: 'Kestigin genc agacin yerine tohum [C] ekmedin evlat... Treeguard (Orman Muhafizi) ofkeyle uyandi!'"), 8.0f);
						}
					}), 10.0f, false);
				}
				else
				{
					// Heybetli Yasli Agac (ElderTree): Bol odun verir (+6), ama hic tohum ekmeden art arda kesilirse yine kizdirir
					WorldState.WoodCount += 4;
					FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), Act->GetActorLocation(), FName("Loot_Wood"), WorldState.ForestWrathCounter);
					FDSColonyAndWorldState::SpawnPhysicalWorldLoot(GetWorld(), Act->GetActorLocation(), FName("Loot_Wood"), WorldState.ForestWrathCounter + 1);
					WorldState.UnplantedTreeCuts += 1;
					WorldState.ForestWrathCounter += 1;
					WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor - 3.0f, 0.0f, 100.0f);
				}

				if (WorldState.ForestWrathCounter >= 3 && !WorldState.bTreeguardAwakened)
				{
					WorldState.bTreeguardAwakened = true;
					for (TActorIterator<ACombatEnemy> BossIt(GetWorld()); BossIt; ++BossIt)
					{
						ACombatEnemy* Boss = *BossIt;
						if (IsValid(Boss) && Boss->ActorHasTag(FName("Boss_Treeguard")))
						{
							Boss->Tags.Remove(FName("Boss_Treeguard_Passive"));
							Boss->SetActorLocation(MyLoc + MyFwd * 480.0f + FVector(0.0f, 0.0f, 40.0f));
							break;
						}
					}
					WorldState.PushNotification(
						bIsYoungTree
							? TEXT("TREEGUARD UYANDI! Yasli agaclarin yamacindaki genc fidanlara kiydigin icin Orman Muhafizi uzerine yuruyor! ([C] ile tohum dikerek yatistir)")
							: TEXT("TREEGUARD UYANDI! Kestigin agaclarin yerine tohum [C] ekmedigin icin Orman Muhafizi uyandi!"),
						8.0f);
				}
				else if (bIsYoungTree)
				{
					WorldState.PushNotification(FString::Printf(
						TEXT("GENC AGAC KESILDI (+2 Odun)! Akil Hocasi: 'Evlat, yasli agacin yamacindaki korpe fidana kiydin; hemen [C] ile tohum ekmezsen Treeguard uyanacak!' (Gazap: %d/3)"),
						WorldState.ForestWrathCounter), 6.5f);
				}
				else
				{
					WorldState.PushNotification(FString::Printf(
						TEXT("YASLI ULU AGAC (+6 Odun, Toplam: %d). Akil Hocasi: 'Topraktan aldigini unutma, [C] ile tohum ek ki orman kusmesin.' (Gazap: %d/3)"),
						WorldState.WoodCount, WorldState.ForestWrathCounter), 5.0f);
				}
				break;
			}
		}
	}

	// start at the provided socket location, sweep forward
	const FName ActiveBone = (Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists) ? FName("hand_r") : DamageSourceBone;
	const FVector TraceStart = GetMesh()->DoesSocketExist(ActiveBone) ? GetMesh()->GetSocketLocation(ActiveBone) : GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * EffectiveDistance);

	// check for pawn and world dynamic collision object types
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(EffectiveRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		UNiagaraSystem* HitVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage"));

		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor());

			if (Damageable)
			{
				// knock upwards and away from the impact normal
				const FVector Impulse = (CurrentHit.ImpactNormal * -EffectiveKnockback) + (FVector::UpVector * MeleeLaunchImpulse);

				// pass the damage event to the actor
				Damageable->ApplyDamage(EffectiveDamage, this, CurrentHit.ImpactPoint, Impulse);
				if (WorldState.bCraftedLivingDarkSword && Rig.WeaponStyle == EDSCombatWeaponStyle::LivingDarkSword && CurrentHP > 0.0f)
				{
					CurrentHP = FMath::Min(MaxHP, CurrentHP + 6.0f);
					if (LifeBarWidget)
					{
						LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
					}
				}

				if (!FParse::Param(FCommandLine::Get(), TEXT("AutoCapturePreview")))
				{
					if (HitVFX)
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitVFX, CurrentHit.ImpactPoint, FRotator::ZeroRotator, FVector(0.22f));
					}

					// call the BP handler to play effects, etc.
					DealtDamage(EffectiveDamage, CurrentHit.ImpactPoint);
				}
			}
		}
	}
}

void ACombatCharacter::CheckCombo()
{
	// are we playing a non-charge attack animation?
	if (bIsAttacking && !bIsChargingAttack)
	{
		// is the last attack input not stale?
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			// consume the attack input so we don't accidentally trigger it twice
			CachedAttackInputTime = 0.0f;

			// increase the combo counter
			++ComboCount;

			// do we still have a combo section to play?
			if (ComboCount < ComboSectionNames.Num())
			{
				// notify enemies they are about to be attacked
				NotifyEnemiesOfIncomingAttack();

				if (FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(this))
				{
					Rig->TriggerAttackSwing(ComboCount, 0.44f);
				}

				// jump to the next combo section
				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
				}
			}
		}
	}
}

void ACombatCharacter::CheckChargedAttack()
{
	// raise the looped charged attack flag
	bHasLoopedChargedAttack = true;

	// set the input release flag from the input. This will determine if we loop or resolve
	bHasReleasedChargedAttack = !bIsChargingAttack;

	// resolve the charge loop
	LoopOrResolveChargedAttack();
}

void ACombatCharacter::LoopOrResolveChargedAttack()
{
	// jump to either the loop or the attack section depending on whether we've released the charge
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(bHasReleasedChargedAttack ? ChargeAttackSection : ChargeLoopSection , ChargedAttackMontage);
	}
}

void ACombatCharacter::NotifyEnemiesOfIncomingAttack()
{
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the actor location, sweep forward
	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * DangerTraceDistance);

	// check for pawn object types only
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(DangerTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor());

			if (Damageable)
			{
				// notify the enemy
				Damageable->NotifyDanger(GetActorLocation(), this);
			}
		}
	}
}

void ACombatCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		// apply the knockback impulse
		GetCharacterMovement()->AddImpulse(DamageImpulse, true);

		// is the character ragdolling?
		if (GetMesh()->IsSimulatingPhysics())
		{
			// apply an impulse to the ragdoll
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		// pass control to BP to play effects, etc.
		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}

}

void ACombatCharacter::HandleDeath()
{
	// disable movement while we're dead
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// pull back the camera
	GetCameraBoom()->TargetArmLength = DeathCameraDistance;

	// GameMode'a bildir (Game Over / kazanma altyapisi, Asama 5)
	if (ACombatGameMode* GameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		GameMode->NotifyPlayerDied();
	}

	// ses: oyuncu olum sesi (Asama 5 - ses tasarimi hook'u)
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
	else
	{
		UE_LOG(LogCombatCharacter, Verbose, TEXT("%s: DeathSound atanmamis, oyuncu olum sesi calinamadi"), *GetName());
	}

	// schedule respawning
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ACombatCharacter::RespawnCharacter, RespawnTime, false);
}

void ACombatCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	// stub
}

void ACombatCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// stub
}

void ACombatCharacter::RespawnCharacter()
{
	// destroy the character and let it be respawned by the Player Controller
	Destroy();
}

float ACombatCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// only process damage if the character is still alive
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Cift Elli Kilic Blok Durusu ([F]) ve Kusursuz Parry (Savusturma) kontrolu
	FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
	if (WS.bPlayerBlockingWithSword && GetWorld())
	{
		const float NowSec = GetWorld()->GetTimeSeconds();
		if (NowSec <= WS.PlayerParryWindowEndTime)
		{
			// KUSURSUZ PARRY! Hasar tamamen savusturulur, kıvılcım cikar ve 2.5x Riposte darbesi hazirlanir
			if (FDSCharacterBodyRig* OwnerRig = FDSCharacterBodyRig::GetOrCreateRig(this))
			{
				OwnerRig->ActiveParryFlashTime = 0.0f;
			}
			WS.bRiposteReady = true;
			if (ACombatEnemy* Attacker = Cast<ACombatEnemy>(DamageCauser))
			{
				const FVector PushDir = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				Attacker->ApplyDamage(16.0f, this, Attacker->GetActorLocation(), PushDir * 680.0f + FVector(0.0f, 0.0f, 180.0f));
			}
			if (UNiagaraSystem* ParrySparks = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail")))
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ParrySparks, GetActorLocation() + GetActorForwardVector() * 75.0f + FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator, FVector(2.2f));
			}
			WS.PushNotification(TEXT("KUSURSUZ PARRY (SAVUSTURMA)! Darbe cift elli kilicla savusturuldu, dusman sarsildi ve 2.5x Kritik Riposte hazir!"), 6.0f);
			return 0.0f;
		}
		else
		{
			// Cift Elli Kilic Blok Durusu: Gelen hasarin %75'i engellenir
			Damage *= 0.25f;
			WS.PushNotification(TEXT("CIFT ELLI KILIC BLOK DURUSU [F]: Gelen hasarin %75'i kilic siperiyle engellendi!"), 3.5f);
		}
	}

	// ses: vurulma sesi (Asama 5 - ses tasarimi hook'u)
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
	}
	else
	{
		UE_LOG(LogCombatCharacter, Verbose, TEXT("%s: HitSound atanmamis, oyuncu vurulma sesi calinamadi"), *GetName());
	}

	// FAZ 2 (KADIM MUHAFIZ ZIRHI): Muhafiz Boynuzu + Magma Cekirdegi ile uretilen zirh gelen hasari %40 azaltir!
	if (WS.bCraftedGuardianArmor)
	{
		Damage *= 0.60f;
	}

	// reduce the current HP
	CurrentHP -= Damage;

	// have we run out of HP?
	if (CurrentHP <= 0.0f)
	{
		// die
		HandleDeath();
	}
	else
	{
		// update the life bar
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);

		// enable partial ragdoll physics, but keep the pelvis vertical
		GetMesh()->SetPhysicsBlendWeight(0.5f);
		GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);

		// stagger'in suresiz kalmamasi icin zamanlayici kur/yenile: Landed() sadece knockback impulse ile
		// havalanildiginda tetiklenir, RadialDamage (orn. Siskin patlamasi) ApplyDamage'i atlayip dogrudan
		// TakeDamage'i cagirdigindan impulse hic uygulanmaz ve Landed() hic tetiklenmeyebilir
		GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &ACombatCharacter::BitirStagger, StaggerSuresi, false);
	}

	// return the received damage amount
	return Damage;
}

void ACombatCharacter::BitirStagger()
{
	GetMesh()->SetPhysicsBlendWeight(0.0f);

	UE_LOG(LogCombatCharacter, Log, TEXT("Stagger: zamanlayici ile bitti aktor=%s"), *GetName());
}

void ACombatCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (CurrentHP >= 0.0f)
	{
		// disable ragdoll physics
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetActorLocation().Z < 200.0f || GetActorLocation().X < 0.0f)
	{
		SetActorLocationAndRotation(FVector(1200.0f, 1620.0f, 305.0f), FRotator(0.0f, 12.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* PC = GetController())
		{
			PC->SetControlRotation(FRotator(-8.0f, 12.0f, 0.0f));
		}
	}

	if (!HitSound)
	{
		HitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Player_Hit.SFX_Player_Hit"));
	}
	if (!DeathSound)
	{
		DeathSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Player_Death.SFX_Player_Death"));
	}

	// Oyuncu karakteri icin koyu taktiksel kurtarici zirh/kiyafet tonu
	const FLinearColor HeroTint(0.14f, 0.22f, 0.36f, 1.0f);
	const int32 NumMats = GetMesh()->GetNumMaterials();
	for (int32 i = 0; i < NumMats; ++i)
	{
		if (UMaterialInstanceDynamic* MID = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			MID->SetVectorParameterValue(FName("ZombieTint"), HeroTint);
			MID->SetVectorParameterValue(FName("PaintTint"), HeroTint);
		}
	}

	// get the life bar from the widget component
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// initialize the camera (disable spring-arm collision pullback against forest props/volumes so 3rd-person view stays clear)
	GetCameraBoom()->bDoCollisionTest = false;
	GetCameraBoom()->TargetArmLength = FMath::Max(DefaultCameraDistance, 310.0f);

	// save the relative transform for the mesh so we can reset the ragdoll later
	MeshStartingTransform = GetMesh()->GetRelativeTransform();

	// set the life bar color
	LifeBarWidget->SetBarColor(LifeBarColor);

	// Oyuncunun can bilgisi sol ust HUD'da gosterildigi icin sirttaki 3B dunya can barini gizle
	LifeBar->SetVisibility(false);
	LifeBar->SetHiddenInGame(true);

	// reset HP to maximum
	ResetHP();

	if (!WeaponClass)
	{
		WeaponClass = ADSMeleeWeapon::StaticClass();
	}

	// baslangic silahini kusan
	UE_LOG(LogTemp, Warning, TEXT("ZDBG CombatCharacter BeginPlay %s WeaponClass=%s"), *GetName(), *GetNameSafe(WeaponClass.Get()));

	if (WeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		EquippedWeapon = GetWorld()->SpawnActor<ADSMeleeWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (EquippedWeapon)
		{
			const bool bSocketExists = GetMesh()->DoesSocketExist(WeaponAttachSocketName);

			UE_LOG(LogTemp, Warning, TEXT("ZDBG EquippedWeapon spawn edildi (%s), soket '%s' mevcut=%d"),
				*EquippedWeapon->GetName(), *WeaponAttachSocketName.ToString(), bSocketExists);

			EquippedWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ZDBG EquippedWeapon spawn edilemedi (WeaponClass=%s)"), *GetNameSafe(WeaponClass.Get()));
		}
	}

	// Robot Mannequin skinsuit'i gizleyip 6 parcali gercek 3B Insan (Hero Adventurer) govdesini bagla
	FDSCharacterBodyRig* HeroRig = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (HeroRig)
	{
		HeroRig->InitializeRig(this, GetMesh(), TEXT("Hero"), false, FVector(1.0f));
	}
	if (EquippedWeapon && HeroRig)
	{
		HeroRig->ApplyWeaponMeshToComponent(EquippedWeapon->WeaponMesh);
	}

	FTimerHandle HeroRigTimer;
	GetWorld()->GetTimerManager().SetTimer(HeroRigTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		FDSCharacterBodyRig* RigPtr = FDSCharacterBodyRig::GetOrCreateRig(this);
		if (!RigPtr)
		{
			return;
		}
		FDSCharacterBodyRig& Rig = *RigPtr;
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// [Space] Ziplama (IMC_Combat'ta IA_Jump eslemesi olmadigi icin dogrudan tus okunur)
			if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
			{
				PerformCombatJump();
			}
			else if (PC->WasInputKeyJustReleased(EKeys::SpaceBar))
			{
				StopJumping();
			}

			bool bWeaponChanged = false;
			if (PC->WasInputKeyJustPressed(EKeys::One))
			{
				Rig.WeaponStyle = EDSCombatWeaponStyle::BattleAxe;
				bWeaponChanged = true;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Two))
			{
				Rig.WeaponStyle = EDSCombatWeaponStyle::SurvivalAxe;
				bWeaponChanged = true;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Three))
			{
				Rig.WeaponStyle = EDSCombatWeaponStyle::MedievalAxe;
				bWeaponChanged = true;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Four))
			{
				Rig.WeaponStyle = EDSCombatWeaponStyle::WarHammer;
				bWeaponChanged = true;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Five))
			{
				Rig.WeaponStyle = EDSCombatWeaponStyle::UnarmedFists;
				bWeaponChanged = true;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Six))
			{
				// [F] ile ayni kural: kilidi acilmamis kilic kusanilmaz
				if (FDSColonyAndWorldState::Get(this).IsSwordUnlocked())
				{
					Rig.WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					bWeaponChanged = true;
				}
				else
				{
					FDSColonyAndWorldState::Get(this).PushNotification(TEXT("KILIC KILITLI [6]: Orman Muhafizi'ni (Treeguard) veya Korsan Boss'u yenerek kilici ac."), 4.0f);
				}
			}
			else if (PC->WasInputKeyJustPressed(EKeys::Q))
			{
				uint8 NextIdx = (static_cast<uint8>(Rig.WeaponStyle) + 1) % 6;
				// kilit acilmadiysa dongu kilici atlar
				if (static_cast<EDSCombatWeaponStyle>(NextIdx) == EDSCombatWeaponStyle::LivingDarkSword && !FDSColonyAndWorldState::Get(this).IsSwordUnlocked())
				{
					NextIdx = (NextIdx + 1) % 6;
				}
				Rig.WeaponStyle = static_cast<EDSCombatWeaponStyle>(NextIdx);
				bWeaponChanged = true;
			}

			if (bWeaponChanged && EquippedWeapon)
			{
				Rig.ApplyWeaponMeshToComponent(EquippedWeapon->WeaponMesh);
			}

			FDSColonyAndWorldState& WorldState = FDSColonyAndWorldState::Get(this);





			// [V] Tusu: Koylu ve Sakalli Savascilar icin Savas Emri gecisi (Hucum / Calis & Savun / Geri Cekil)
			if (PC->WasInputKeyJustPressed(EKeys::V))
			{
				if (WorldState.VillagerOrder == EDSVillagerCombatOrder::FightOrder)
				{
					WorldState.VillagerOrder = EDSVillagerCombatOrder::WorkAndDefendOrder;
					WorldState.PushNotification(TEXT("KOYLU EMRI: CALIS & SAVUN (Sakalli savascilar zombileri avlar, siviller tehlikede korunur)"), 5.0f);
				}
				else if (WorldState.VillagerOrder == EDSVillagerCombatOrder::WorkAndDefendOrder)
				{
					WorldState.VillagerOrder = EDSVillagerCombatOrder::RetreatOrder;
					WorldState.PushNotification(TEXT("KOYLU EMRI: GERI CEKIL (Tum koyluler kamp atesine cekiliyor)"), 5.0f);
				}
				else
				{
					WorldState.VillagerOrder = EDSVillagerCombatOrder::FightOrder;
					WorldState.PushNotification(TEXT("KOYLU EMRI: SAVAS EMRI (HUCUM)! Sakalli savascilar ve tum koyluler baltalariyla zombilere saldiriyor!"), 5.5f);
				}
			}

			// [C] Tusu: Tohum / Fidan Dik (1 Odun harcar, Genc Agac/Tohum borcunu sifirlar, Orman Gazabini yatistirir)
			if (PC->WasInputKeyJustPressed(EKeys::C))
			{
				if (WorldState.WoodCount >= 1)
				{
					WorldState.WoodCount -= 1;
					WorldState.UnplantedTreeCuts = 0;
					WorldState.ForestWrathCounter = FMath::Max(0, WorldState.ForestWrathCounter - 2);
					WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor + 8.0f, 0.0f, 100.0f);

					if (UStaticMesh* SaplingMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Coldblood_Forest/Models/PineTrees/SM_PineTree_06.SM_PineTree_06")))
					{
						FActorSpawnParameters SP;
						SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						const FVector SaplingLoc = GetActorLocation() + GetActorForwardVector() * 140.0f - FVector(0.0f, 0.0f, 85.0f);
						if (AStaticMeshActor* Sapling = GetWorld()->SpawnActor<AStaticMeshActor>(SaplingLoc, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), SP))
						{
							Sapling->SetMobility(EComponentMobility::Movable);
							Sapling->GetStaticMeshComponent()->SetStaticMesh(SaplingMesh);
							Sapling->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							Sapling->SetActorScale3D(FVector(0.26f));
							Sapling->Tags.AddUnique(FName("ChopTree"));
							Sapling->Tags.AddUnique(FName("YoungTree"));
						}
					}
					WorldState.PushNotification(FString::Printf(TEXT("TOHUM / FIDAN DIKILDI! Akil Hocasi: 'Topraga verdigin her tohum ormanin ofkesini dindirir evlat.' (Gazap: %d/3)"),
						WorldState.ForestWrathCounter), 5.0f);
				}
				else
				{
					WorldState.PushNotification(TEXT("Tohum dikmek icin 1 Odun gerekli! (Yasli ulu agaclara vurarak odun toplayabilirsin)"), 4.0f);
				}
			}

			// [G] Tusu (FAZ 3 - DENIZCILIK): 5 Gemi Sinifi Evrimi (Sal -> Balikci Teknesi -> Salupa -> Nakliye -> Amiral Gemisi)
			if (PC->WasInputKeyJustPressed(EKeys::G))
			{
				// yetersiz kaynakta UpgradeShipClassAndFleet reddeder; skor/guven yalnizca basarili yukseltmede
				if (WorldState.UpgradeShipClassAndFleet(GetWorld(), ShipUpgradeWoodCost, ShipUpgradeIronCost))
				{
					Rig.TriggerAttackSwing(2, 0.50f);
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						GM->AddScore(220);
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(14.0f, TEXT("Tersanede gemi sinifi ve filo yukseltildi"));
						}
					}
				}
			}

			// [B] Tusu (FAZ 3 - DENIZCILIK): Dev Balinaya 3B Zipkin Firlat, Balina Yagi/Ambergris Dusur & Adalar Arasi Filo Maden Konvoyunu Calistir
			if (PC->WasInputKeyJustPressed(EKeys::B) && TryConsumeCooldown(NextHarpoonAllowedTime, HarpoonCooldown, TEXT("Zipkin [B]")))
			{
				Rig.TriggerAttackSwing(1, 0.55f);
				WorldState.LaunchHarpoonAtWhaleAndSail(GetWorld(), this);
				WorldState.DivineFavor = FMath::Clamp(WorldState.DivineFavor + 6.0f, 0.0f, 100.0f);
				CurrentHP = FMath::Min(MaxHP, CurrentHP + HarpoonHealAmount);
				if (LifeBarWidget)
				{
					LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
				}

				if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
				{
					GM->AddScore(160 * WorldState.FleetShipCount);
					if (GM->TrustComponent)
					{
						GM->TrustComponent->AddTrust(12.0f, TEXT("Derin Sular Dev Balina Avi ve Adalar Arasi Filo Madenciligi"));
					}
				}
			}

			// Otomatik / Yari-Otomatik Adalar Arasi Filo Madenciligi Dongusu (12 saniyede bir ambara Demir/Tuz/Celik sevkiyati)
			if (WorldState.bFleetAutoMiningActive && GetWorld())
			{
				const float NowT = GetWorld()->GetTimeSeconds();
				if (NowT >= WorldState.NextFleetConvoyTime)
				{
					WorldState.NextFleetConvoyTime = NowT + 12.0f;
					WorldState.ExecuteFleetMiningConvoy(GetWorld(), false);
				}
			}

			// [H] Tusu: Her Zaman Eskiye Donus! Gemiciligi Birakip Karaya Yerles <-> Kaleyi Tahkim Et & Gemiyi Onar
			if (PC->WasInputKeyJustPressed(EKeys::H))
			{
				const bool bWasSeafaring = WorldState.bNomadFleetDoctrine;
				WorldState.bNomadFleetDoctrine = false;
				WorldState.bCastleDefenseChosen = true;
				WorldState.ShipHullIntegrity = WorldState.MaxShipHullIntegrity;
				WorldState.VillagerOrder = EDSVillagerCombatOrder::FightOrder;
				if (WorldState.SeaBountyCount > 0)
				{
					WorldState.WoodCount += WorldState.SeaBountyCount * 2;
				}
				for (TActorIterator<ADSBarikat> It(GetWorld()); It; ++It)
				{
					ADSBarikat* Barikat = *It;
					if (IsValid(Barikat))
					{
						Barikat->TamirEt(this);
					}
				}
				if (bWasSeafaring)
				{
					WorldState.PushNotification(TEXT("ESKIYE DONUS — GEMICILIGI BIRAKIP KARAYA YERLESTIN! Filo demir atti, Gemi Govdesi %100 onarildi ve barikatlar tahkim edildi! ([G]/[B] ile tekrar denize acilabilirsin)"), 7.5f);
				}
				else
				{
					WorldState.PushNotification(TEXT("KARA YERLESIMI & TERSANE ONARIMI [H]: Tum barikatlar ve Gemi Govdesi onarildi, koylulere Savas Emri verildi!"), 6.5f);
				}
			}

			// [X] Tusu (FAZ 4 - KARANLIK YOL & VICDAN SECIMI): Komsu Koyu Basip Usta Demirci/Ciftci Kacir!
			if (PC->WasInputKeyJustPressed(EKeys::X))
			{
				// baskin yalnizca Komsu Koy Elcisi'nin yaninda ve cooldown dolunca yapilabilir
				bool bEnvoyInRange = false;
				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(FName("NeighborVillageEnvoy"))
						&& FVector::DistSquared(GetActorLocation(), It->GetActorLocation()) <= FMath::Square(DarkPathRaidRange))
					{
						bEnvoyInRange = true;
						break;
					}
				}

				if (!bEnvoyInRange)
				{
					WorldState.PushNotification(TEXT("KARANLIK YOL [X]: Baskin icin Komsu Koy Elcisi'nin yanina git."), 4.0f);
				}
				else if (TryConsumeCooldown(NextDarkPathRaidAllowedTime, DarkPathRaidCooldown, TEXT("Karanlik Yol baskini [X]")))
				{
					Rig.TriggerAttackSwing(2, 0.55f);
					WorldState.ResolveNeighborDiplomacyChoice(GetWorld(), 1, DarkPathSteelReward, DarkPathIronReward, DarkPathSaltReward);
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(-10.0f, TEXT("Karanlik Yol: Komsu Koyden Demirci kacirildi"));
						}
					}
				}
			}

			// [Z] Tusu (FAZ 4 - VICDAN SECIMI): Komsu Koye Silah Satisini Reddet -> Caresiz Koyun Karsi Baskini!
			if (PC->WasInputKeyJustPressed(EKeys::Z))
			{
				WorldState.ResolveNeighborDiplomacyChoice(GetWorld(), 2);
			}

			// [M] Tusu (FAZ 4 - GIZLI ISIRIK MUAYENESI): Şüpheli Köylüyü Muayene Et & Tuzlu Güneş Merhemi ile Karantina Tedavisi Uygula
			if (PC->WasInputKeyJustPressed(EKeys::M))
			{
				// dunyadaki ilk koylu degil, BiteInspectionRange icindeki en yakin koylu muayene edilir
				ADSVillager* SuspectV = nullptr;
				float MinSuspectDistSq = FMath::Square(BiteInspectionRange);
				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It))
					{
						const float DistSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
						if (DistSq <= MinSuspectDistSq)
						{
							MinSuspectDistSq = DistSq;
							SuspectV = *It;
						}
					}
				}

				if (!SuspectV)
				{
					WorldState.PushNotification(TEXT("ISIRIK MUAYENESI [M]: Muayene icin bir koylunun yanina git."), 4.0f);
				}
				else
				{
					WorldState.ResolveBiteInspectionDilemma(SuspectV, true);
					if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
					{
						if (GM->TrustComponent)
						{
							GM->TrustComponent->AddTrust(12.0f, TEXT("Gizli Isirik Muayenesi ve Karantina Tedavisi"));
						}
					}
				}
			}

			// [F] Tusu: Cift Elli Kilic Blok Durusu ve Kusursuz Parry (Savusturma)
			const float NowTimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const bool bSwordUnlocked = WorldState.IsSwordUnlocked();
			if (PC->WasInputKeyJustPressed(EKeys::F) && !bSwordUnlocked)
			{
				WorldState.PushNotification(TEXT("KILIC KILITLI [F]: Orman Muhafizi'ni (Treeguard) veya Korsan Boss'u yenerek kilici ac."), 4.0f);
			}
			else if (PC->WasInputKeyJustPressed(EKeys::F))
			{
				if (Rig.WeaponStyle != EDSCombatWeaponStyle::LivingDarkSword)
				{
					Rig.WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					if (EquippedWeapon)
					{
						Rig.ApplyWeaponMeshToComponent(EquippedWeapon->WeaponMesh);
					}
				}
				WorldState.PlayerParryWindowEndTime = NowTimeSec + (WorldState.bTwoHandedSwordMasteryUnlocked ? 0.55f : 0.35f);
				Rig.ActiveParryFlashTime = 0.0f;
				if (WorldState.bTwoHandedSwordMasteryUnlocked)
				{
					WorldState.PushNotification(TEXT("CIFT ELLI KILIC PARRY & BLOK DURUSU [F]: Kusursuz Savusturma penceresi acildi! (Basili tutarak Blok Durusunda kalabilirsin)"), 4.0f);
				}
				else
				{
					WorldState.PushNotification(TEXT("TEK ELLI KILIC SIPERI [F]: Kiyidaki Korsan Boss'u yenerek Cift Elli Kilic Ustaligi, Kombo ve Kusursuz Parry ogrenebilirsin!"), 5.0f);
				}
			}

			const bool bHoldingBlock = bSwordUnlocked && (PC->IsInputKeyDown(EKeys::F) || (NowTimeSec <= WorldState.PlayerParryWindowEndTime));
			Rig.bIsBlockingPose = bHoldingBlock;
			WorldState.bPlayerBlockingWithSword = bHoldingBlock;

			// Gercek blok durusu animasyonu (UAL2 Sword_Block): kalkan/kilic siper pozuna gelince orada dondurulur
			if (UAnimInstance* BlockAnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				const bool bWantBlockAnim = bHoldingBlock && !bIsAttacking && Rig.WeaponStyle != EDSCombatWeaponStyle::UnarmedFists;
				if (bWantBlockAnim && !bPlayingBlockAnim)
				{
					if (UAnimSequence* BlockSeq = LoadUALAnim(TEXT("Sword_Block")))
					{
						ActiveWeaponMontage = BlockAnimInst->PlaySlotAnimationAsDynamicMontage(BlockSeq, FName("DefaultSlot"), 0.08f, 0.2f, 1.4f, 1, -1.0f, 0.0f);
						bPlayingBlockAnim = true;
						TWeakObjectPtr<UAnimMontage> WeakBlock = ActiveWeaponMontage.Get();
						FTimerHandle FreezeHandle;
						GetWorld()->GetTimerManager().SetTimer(FreezeHandle, FTimerDelegate::CreateWeakLambda(this, [this, WeakBlock]()
						{
							if (bPlayingBlockAnim && WeakBlock.IsValid() && GetMesh() && GetMesh()->GetAnimInstance())
							{
								GetMesh()->GetAnimInstance()->Montage_SetPlayRate(WeakBlock.Get(), 0.0f);
							}
						}), 0.30f, false);
					}
				}
				else if (!bWantBlockAnim && bPlayingBlockAnim)
				{
					bPlayingBlockAnim = false;
					if (ActiveWeaponMontage && !bIsAttacking)
					{
						BlockAnimInst->Montage_Stop(0.2f, ActiveWeaponMontage);
					}
				}
			}

			if (GetCharacterMovement())
			{
				const float BaseTargetSpeed = WorldState.bOiledBootsEquipped ? 545.0f : 400.0f;
				GetCharacterMovement()->MaxWalkSpeed = bHoldingBlock ? (BaseTargetSpeed * 0.72f) : BaseTargetSpeed;
			}

			// [R] Tusu: Gezgin Sovalye Duellosunu Ertele veya Geri Donen Koyluyu Reddet
			// Yerdeki fiziksel loot'un tam uzerinden gecildiginde (145 cm) otomatik sirt cantasina alma
			if (GetWorld())
			{
				const FVector PPos = GetActorLocation();
				for (TActorIterator<AActor> LIt(GetWorld()); LIt; ++LIt)
				{
					AActor* LAct = *LIt;
					if (IsValid(LAct) && !LAct->IsHidden() && LAct->ActorHasTag(FName("WorldLoot")))
					{
						if (FVector::DistSquared(PPos, LAct->GetActorLocation()) <= FMath::Square(145.0f))
						{
							WorldState.TryPickupWorldLoot(LAct);
						}
					}
				}
			}

			// [N] Tusu: Koloni Insaat Santiyesi (3 Kisilik Demir Ocagi -> Kiyi Tuzlasi -> Kutuphane)
			if (PC->WasInputKeyJustPressed(EKeys::N))
			{
				Rig.TriggerAttackSwing(2, 0.55f);
				WorldState.ContributeToBuildSite(GetWorld());
			}

			// [K] Tusu: FAZ 2 Kadim Tezgah Uretimi & GDD 16 Koloni Kutuphanesi Tarif Yedekleme
			if (PC->WasInputKeyJustPressed(EKeys::K))
			{
				if (WorldState.CraftAndArchiveAtAncientStation(this) && WorldState.bCraftedLivingDarkSword)
				{
					Rig.WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					if (EquippedWeapon)
					{
						Rig.ApplyWeaponMeshToComponent(EquippedWeapon->WeaponMesh);
					}
				}
			}

			// [J] Tusu: Koylu Meslek Vardiyasi (Oduncu / Madenci / Insaatci & Muhafiz Usta)
			if (PC->WasInputKeyJustPressed(EKeys::J) && TryConsumeCooldown(NextVillagerShiftAllowedTime, VillagerShiftCooldown, TEXT("Meslek vardiyasi [J]")))
			{
				WorldState.bVillagerGatheringJobsActive = true;
				WorldState.VillagerOrder = EDSVillagerCombatOrder::WorkAndDefendOrder;
				WorldState.WoodCount += VillagerShiftWoodReward;
				WorldState.StoredIronOre += VillagerShiftIronReward;
				WorldState.StoredSalt += VillagerShiftSaltReward;
				WorldState.PushNotification(FString::Printf(
					TEXT("KOYLU MESLEK VARDIYASI [J]: Oduncu (+%d Odun), Madenci (+%d Demir Cevheri), Tuzcu (+%d Tuz) ambara teslim etti! (Ambar -> Odun:%d Demir:%d Tuz:%d)"),
					VillagerShiftWoodReward, VillagerShiftIronReward, VillagerShiftSaltReward,
					WorldState.WoodCount, WorldState.StoredIronOre, WorldState.StoredSalt), 6.5f);
			}

			if (PC->WasInputKeyJustPressed(EKeys::R))
			{
				WorldState.PushNotification(TEXT("TEKLIF ERTELENDI: 'Henuz hazir degilim, daha sonra goruselim.'"), 4.5f);
			}
		}

		Rig.UpdatePose(this, GetMesh(), 0.016f, bIsAttacking, EquippedWeapon ? EquippedWeapon->WeaponMesh : nullptr);
	}), 0.016f, true);
}

bool ACombatCharacter::TryConsumeCooldown(float& NextAllowedTime, float Cooldown, const TCHAR* ActionName)
{
	const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (NowTime < NextAllowedTime)
	{
		FDSColonyAndWorldState::Get(this).PushNotification(
			FString::Printf(TEXT("%s hazir degil: %.0f sn sonra tekrar dene."), ActionName, NextAllowedTime - NowTime), 3.0f);
		return false;
	}

	NextAllowedTime = NowTime + Cooldown;
	return true;
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

	// clear the stagger timer
	GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);

		// Combo Attack
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ComboAttackPressed);

		// Charged Attack
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ChargedAttackPressed);
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &ACombatCharacter::ChargedAttackReleased);

		// Camera Side Toggle
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ACombatCharacter::ToggleCamera);

		// Interact
		if (!InteractAction)
		{
			InteractAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Interact.IA_Interact"));
		}
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ACombatCharacter::InteractPressed);
		}
	}
}

void ACombatCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// update the respawn transform on the Player Controller
	if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController()))
	{
		PC->SetRespawnTransform(GetActorTransform());
	}
}

