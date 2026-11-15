// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSVillager.h"
#include "DSVillagerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "CombatLifeBar.h"
#include "CombatGameMode.h"
#include "DSTrustComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "DSCharacterBodyRig.h"

namespace
{
	/** Oyuncunun E tusu InputAction'i (henuz projede yok, bkz. CombatCharacter) olmadan
	 *  bayginlasmis koyluyu kurtarabilecegi azami mesafe (Asama 6.2) */
	constexpr float KurtarmaMesafesi = 150.0f;

	/** Yakinlik kontrolunun tekrarlanma araligi */
	constexpr float KurtarmaKontrolAraligi = 0.2f;
}

ADSVillager::ADSVillager()
{
	// bayginlik/olum zamanlayicilari disinda tick gerektiren bir davranis yok
	PrimaryActorTick.bCanEverTick = false;

	// GECICI FIX (Asama 6.2): spawn aninda hic vurulmadan baygin dusme sorunu icin koruma suresi
	// 1sn'den 2sn'ye cikarildi. GravityScale=0 fix'i tek basina yetmedi; koruma penceresini
	// genisletmek TakeDamage'a ulasan her turlu erken/gecikmeli hasar kaynagini (fall damage,
	// spawn ani collision vb.) daha guvenli sekilde yutar.
	SpawnKorumaSuresi = 2.0f;

	// kacma AI'ini calistiracak controller (ACombatEnemy ile ayni desen); ADSVillagerController
	// abstract oldugu icin BP alt sinifi olmadan concrete bir koylu BP'si spawn edilemez -
	// BP_DSVillager'in Class Defaults'unda somut bir BP_DSVillagerController atanmalidir
	AIControllerClass = ADSVillagerController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// can bari (ACombatEnemy ile ayni desen): WidgetClass'i BP_Villager_01'in Class Defaults'unda
	// (WBP_HUD'daki mevcut can bari widget'i veya basit bir UCombatLifeBar alt sinifi) atanmalidir
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// sivil sirt cantasi / ekipman aksesuari (spine_03 soketine baglanir)
	AccessoryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccessoryMesh"));
	AccessoryMesh->SetupAttachment(GetMesh(), FName("spine_03"));
	AccessoryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// varsayilan Animation Blueprint: BP_Villager_01'in Mesh AnimClass'iyla ayni ABP_Unarmed
	// (SKM_Manny_Simple iskeletiyle uyumlu, BP_ThirdPersonCharacter'da da kullanilan genel yurume/durma AnimBP'si)
	static ConstructorHelpers::FClassFinder<UAnimInstance> DefaultAnimClassFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (DefaultAnimClassFinder.Succeeded())
	{
		VillagerAnimClass = DefaultAnimClassFinder.Class;
	}
}

bool ADSVillager::TryKurtar(AActor* Kurtaran)
{
	// baygin degilse (henuz vurulmamis veya zaten kurtarilmis) kurtarilacak bir sey yok
	if (!bIsDowned)
	{
		return false;
	}

	// bayginlik suresi doldugunda gercek olumu tetikleyecek zamanlayiciyi iptal et
	GetWorld()->GetTimerManager().ClearTimer(KurtarmaTimer);

	bIsDowned = false;
	CurrentHP = MaxHP * 0.5f;
	if (FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(this))
	{
		Rig->bIsDownedPose = false;
	}

	// can barini guncelle
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
	}

	// Animasyon Blueprint'ini geri ac (bayginlik sirasindaki AnimationSingleNode modundan cikar)
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (VillagerAnimClass)
	{
		GetMesh()->SetAnimInstanceClass(VillagerAnimClass);
	}

	// hareketi geri ac
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (RescueSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RescueSound, GetActorLocation());
	}

	if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
	{
		GM->AddScore(50);
		if (GM->TrustComponent)
		{
			GM->TrustComponent->AddTrust(10.0f, TEXT("Baygin koylu kurtarildi"));
		}
	}

	if (FDSColonyAndWorldState* WorldState = FDSColonyAndWorldState::Find(this))
	{
		float& PersonalTrust = WorldState->VillagerPersonalTrustMap.FindOrAdd(this, 65.0f);
		PersonalTrust = FMath::Clamp(PersonalTrust + 15.0f, 0.0f, 100.0f);
		WorldState->DivineFavor = FMath::Clamp(WorldState->DivineFavor + 5.0f, 0.0f, 100.0f);
	}

	OnVillagerRescued.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[DSVillager] kurtarildi!"));

	return true;
}

void ADSVillager::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// hasari TakeDamage'a devret (ACombatEnemy ile ayni desen)
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// sadece gercekten hasar aldiysak knockback uygula (ACombatEnemy::ApplyDamage ile ayni desen)
	if (ActualDamage > 0.0f)
	{
		GetCharacterMovement()->AddImpulse(DamageImpulse, true);

		// ragdoll'daysa fiziksel govdeye de impulse uygula
		if (GetMesh()->IsSimulatingPhysics())
		{
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}
	}
}

void ADSVillager::HandleDeath()
{
	// zaten bayginsa (veya gercekten olmusse) tekrar isleme alma
	if (bIsDowned)
	{
		return;
	}

	// HP ilk kez 0'a indi: direkt olum yerine once bayginlik (GDD 5.2)
	EnterDownedState();
}

void ADSVillager::ApplyHealing(float Healing, AActor* Healer)
{
	// stub - M1'de koylu icin heal akisi yok, kurtarma TryKurtar() ile yapiliyor
}

void ADSVillager::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// Savas emri verilmisse veya sakalli savasci gorevdeyse tehlikeden kacmaz, savasa girer!
	const FDSColonyAndWorldState* WorldState = FDSColonyAndWorldState::Find(this);
	const EDSVillagerCombatOrder CurrentOrder = WorldState ? WorldState->VillagerOrder : EDSVillagerCombatOrder::RetreatOrder;
	if (CurrentOrder == EDSVillagerCombatOrder::FightOrder ||
		(CurrentOrder == EDSVillagerCombatOrder::WorkAndDefendOrder && ActorHasTag(TEXT("BeardedWarrior"))))
	{
		return;
	}

	// Sadece geri cekilme / sivil korunma modunda kacisi tetikle
	if (ADSVillagerController* VillagerCtrl = Cast<ADSVillagerController>(GetController()))
	{
		VillagerCtrl->ForceFleeFromDanger(DangerLocation);
	}
}

float ADSVillager::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (ActorHasTag(TEXT("ImmortalMemoryNPC")) || ActorHasTag(TEXT("AkilHocasi_Baba")) || ActorHasTag(TEXT("AniNPC_Dede")) || ActorHasTag(TEXT("AniNPC_Anneanne"))) { CurrentHP = MaxHP; return 0.0f; }
	// spawn korumasi aktifken (ilk SpawnKorumaSuresi saniye) hasar yok sayilir
	if (bSpawnKorumasiAktif)
	{
		return 0.0f;
	}

	// baygin veya zaten olu koyluye hasar islenmez
	if (bIsDowned || CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// hasari veren "Player" veya muttefik ise yok say (dostane ates yok)
	if (DamageCauser && (DamageCauser->ActorHasTag(TEXT("Player")) || DamageCauser->ActorHasTag(TEXT("Boss_Sunflower_Muttefik")) || DamageCauser->ActorHasTag(TEXT("Boss_Pirate_Muttefik")) || DamageCauser->IsA<ADSVillager>()))
	{
		return 0.0f;
	}

	CurrentHP -= Damage;

	// Zombi saldirisi aldiginda gizli isirik suphesi olusabilir (Vicdan / Tetkik mekanigi)
	if (FDSColonyAndWorldState* WorldState = FDSColonyAndWorldState::Find(this))
	{
		if (FMath::FRand() < 0.35f)
		{
			WorldState->HiddenBiteSuspectMap.FindOrAdd(this) = true;
		}
		float& PersonalTrust = WorldState->VillagerPersonalTrustMap.FindOrAdd(this, 65.0f);
		PersonalTrust = FMath::Clamp(PersonalTrust - 4.0f, 0.0f, 100.0f);
	}

	if (CurrentHP <= 0.0f)
	{
		CurrentHP = 0.0f;
		HandleDeath();
	}

	// can barini guncelle (bayginken de gorunur kalsin diye HandleDeath'ten sonra da calisir)
	if (LifeBar)
	{
		LifeBar->SetVisibility(true);
		LifeBar->SetHiddenInGame(false);
	}
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
	}

	return Damage;
}

void ADSVillager::EnterDownedState()
{
	bIsDowned = true;
	if (ActorHasTag(TEXT("ImmortalMemoryNPC")) || ActorHasTag(TEXT("AkilHocasi_Baba")) || ActorHasTag(TEXT("AniNPC_Dede")) || ActorHasTag(TEXT("AniNPC_Anneanne"))) { bIsDowned = false; CurrentHP = MaxHP; return; }
	if (FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(this))
	{
		Rig->bIsDownedPose = true;
	}

	// hareketi durdur
	GetCharacterMovement()->DisableMovement();
	UAnimSequence* KnockedAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, TEXT("/Game/Colony/Animations/Knocked_Out")));
	if (KnockedAnim)
	{
		GetMesh()->PlayAnimation(KnockedAnim, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Knocked_Out animasyonu yuklenemedi!"));
	}

	// KurtarmaSuresi icinde TryKurtar() cagirilmazsa gercek olum tetiklenir
	GetWorld()->GetTimerManager().SetTimer(KurtarmaTimer, this, &ADSVillager::GercekOlum, KurtarmaSuresi, false);

	OnVillagerDowned.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("%s: baygin dustu, %.0f sn icinde kurtarilmazsa olecek"), *GetName(), KurtarmaSuresi);
}

void ADSVillager::GercekOlum()
{
	if (FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(this))
	{
		Rig->bIsDownedPose = false;
	}
	LifeBar->SetHiddenInGame(true);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->DisableMovement();

	// Ragdoll fizik profilini ve carpismasini aktif et
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->WakeAllRigidBodies();
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	if (ACombatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACombatGameMode>() : nullptr)
	{
		if (GM->TrustComponent)
		{
			GM->TrustComponent->AddTrust(-12.0f, TEXT("Koylu kurtarilamadi"));
		}
	}

	OnVillagerDied.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("%s: kurtarilamadi, gercekten oldu"), *GetName());

	// bir sure sonra aktoru sahneden kaldir
	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &ADSVillager::RemoveFromLevel, DeathRemovalTime, false);
}

void ADSVillager::RemoveFromLevel()
{
	Destroy();
}

void ADSVillager::BitirSpawnKorumasi()
{
	bSpawnKorumasiAktif = false;

	// spawn aninda kapatilan yercekimini geri ac
	GetCharacterMovement()->GravityScale = 1.0f;
}

void ADSVillager::BeginPlay()
{
	Super::BeginPlay();

	// GECICI DEBUG (kaykay/kayma animasyon sorununu teshis etmek icin): capsule boyutlarini logla -
	// inceleme bitince kaldirilmali
	UE_LOG(LogTemp, Warning, TEXT("[DSVillager] CapsuleHalfHeight=%.1f CapsuleRadius=%.1f"),
		GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), GetCapsuleComponent()->GetScaledCapsuleRadius());

	// can barini widget bileseninden al ve doldur (ACombatEnemy ile ayni desen)
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(1.0f);
		LifeBarWidget->SetBarColor(FLinearColor(0.15f, 0.75f, 0.95f, 1.0f));
	}
	// Tam saglikli koylulerin bas ustunde surekli bar asili durmasin; yalnizca hasar alinca gorunsun
	if (LifeBar)
	{
		LifeBar->SetVisibility(false);
		LifeBar->SetHiddenInGame(true);
	}

	// bind pose'da kalma ihtimaline karsi Animation Blueprint'i kod tarafinda da ata (BP_Villager_01'in
	// Mesh AnimClass'i zaten ayniysa bu no-op'tur; farkli/bos kalmissa burasi kurtarir)
	if (VillagerAnimClass)
	{
		GetMesh()->SetAnimInstanceClass(VillagerAnimClass);
	}

	// Sivil kiyafet rengi ve 6 parcali gercek 3B Insan Koylu govdesi uygula
	static int32 GVillagerSpawnCounter = 0;
	const int32 SpawnMod = (GVillagerSpawnCounter++) % 3;
	const FString MyName = GetClass()->GetName();
	FString VillagerModelPrefix = (SpawnMod == 0) ? TEXT("Villager02") : ((SpawnMod == 1) ? TEXT("Villager01") : TEXT("Villager03"));
	if (MyName.Contains(TEXT("02")))
	{
		VillagerModelPrefix = TEXT("Villager02");
	}
	else if (MyName.Contains(TEXT("03")) && SpawnMod != 0)
	{
		VillagerModelPrefix = TEXT("Villager03");
	}

	FDSCharacterBodyRig* VillagerRig = FDSCharacterBodyRig::GetOrCreateRig(this);
	if (VillagerRig)
	{
		VillagerRig->InitializeRig(this, GetMesh(), VillagerModelPrefix, false, FVector(1.0f));
	}

	// Yasli uzun sakalli oduncu-savasci (Villager02) elinde cift elli savas baltasi tasir;
	// diger koyluler de savas emri verildiginde savasabilmeleri icin oduncu/kamp baltasi tasir.
	Tags.AddUnique(TEXT("Villager"));
	if (VillagerModelPrefix == TEXT("Villager02"))
	{
		Tags.AddUnique(TEXT("BeardedWarrior"));
		if (UStaticMesh* AxeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_BattleAxe.SM_BattleAxe")))
		{
			if (UStaticMeshComponent* VillagerAxeComp = NewObject<UStaticMeshComponent>(this, TEXT("ElderLumberjackAxe")))
			{
				VillagerAxeComp->SetStaticMesh(AxeMesh);
				VillagerAxeComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				VillagerAxeComp->SetCastShadow(true);
				VillagerAxeComp->RegisterComponent();
				VillagerAxeComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				if (VillagerRig)
				{
					VillagerRig->WeaponStyle = EDSCombatWeaponStyle::BattleAxe;
					VillagerRig->ApplyWeaponMeshToComponent(VillagerAxeComp);
				}
			}
		}
	}
	else
	{
		const bool bIsV01 = (VillagerModelPrefix == TEXT("Villager01"));
		const TCHAR* ToolPath = bIsV01
			? TEXT("/Game/Variant_Combat/Weapons/SM_SurvivalAxe.SM_SurvivalAxe")
			: TEXT("/Game/Variant_Combat/Weapons/SM_MedievalAxe.SM_MedievalAxe");
		if (UStaticMesh* ToolMesh = LoadObject<UStaticMesh>(nullptr, ToolPath))
		{
			if (UStaticMeshComponent* VillagerToolComp = NewObject<UStaticMeshComponent>(this, TEXT("VillagerMilitiaAxe")))
			{
				VillagerToolComp->SetStaticMesh(ToolMesh);
				VillagerToolComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				VillagerToolComp->SetCastShadow(true);
				VillagerToolComp->RegisterComponent();
				VillagerToolComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				if (VillagerRig)
				{
					VillagerRig->WeaponStyle = bIsV01 ? EDSCombatWeaponStyle::SurvivalAxe : EDSCombatWeaponStyle::MedievalAxe;
					VillagerRig->ApplyWeaponMeshToComponent(VillagerToolComp);
				}
			}
		}
		else if (VillagerRig)
		{
			VillagerRig->WeaponStyle = EDSCombatWeaponStyle::UnarmedFists;
		}
	}

	if (FDSColonyAndWorldState* WorldState = FDSColonyAndWorldState::Find(this))
	{
		WorldState->VillagerPersonalTrustMap.FindOrAdd(this) = (VillagerModelPrefix == TEXT("Villager02")) ? 78.0f : 65.0f;
		WorldState->HiddenBiteSuspectMap.FindOrAdd(this) = false;
	}
	if (SpawnMod == 2)
	{
		Tags.AddUnique(TEXT("ReturningVillager"));
	}

	TSharedRef<FTimerHandle> VillagerPoseTimer = MakeShared<FTimerHandle>();
	GetWorld()->GetTimerManager().SetTimer(*VillagerPoseTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(this))
		{
			Rig->UpdatePose(this, GetMesh(), 0.016f, false, Rig->CachedWeaponMesh.Get());
		}
	}), 0.016f, true);

	if (AccessoryMesh)
	{
		if (FDSCharacterBodyRig::UseRealSkeletalAnimation())
		{
			AccessoryMesh->SetHiddenInGame(true);
			AccessoryMesh->SetVisibility(false);
		}
		else if (!AccessoryMesh->GetStaticMesh())
		{
			if (UStaticMeshComponent* TorsoMC = VillagerRig ? VillagerRig->TorsoComp.Get() : nullptr)
			{
				if (UStaticMesh* BackpackMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Colony/Meshes/SM_Backpack.SM_Backpack")))
				{
					AccessoryMesh->SetStaticMesh(BackpackMesh);
					AccessoryMesh->AttachToComponent(TorsoMC, FAttachmentTransformRules::KeepRelativeTransform);
					const FBoxSphereBounds Bounds = BackpackMesh->GetBounds();
					const float MaxExt = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
					const float Scale = (MaxExt > 0.01f) ? FMath::Clamp(26.0f / MaxExt, 0.1f, 60.0f) : 1.0f;
					AccessoryMesh->SetRelativeScale3D(FVector(Scale));
					AccessoryMesh->SetRelativeLocation(FVector(0.0f, -13.0f, 12.0f));
					AccessoryMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
				}
			}
		}
	}

	if (!RescueSound)
	{
		RescueSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Villager_Rescue.SFX_Villager_Rescue"));
	}

	// Sakalli savascilar daha dayanikli (MaxHP=75), diger koyluler (MaxHP=55)
	if (MaxHP <= 45.0f)
	{
		MaxHP = ActorHasTag(TEXT("BeardedWarrior")) ? 75.0f : 55.0f;
	}
	CurrentHP = MaxHP;

	// spawn aninda ani hasara ve dusme hasarina karsi kisa sureli bagisiklik
	bSpawnKorumasiAktif = true;
	GetCharacterMovement()->GravityScale = 0.0f;
	GetWorld()->GetTimerManager().SetTimer(SpawnKorumaTimer, this, &ADSVillager::BitirSpawnKorumasi, SpawnKorumaSuresi, false);

	// Yakinlik tabanli otomatik kurtarma + E tusu yedegi
	TSharedRef<FTimerHandle> KurtarmaKontrolTimer = MakeShared<FTimerHandle>();
	GetWorld()->GetTimerManager().SetTimer(*KurtarmaKontrolTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bIsDowned || !GetWorld()->GetTimerManager().IsTimerActive(KurtarmaTimer))
		{
			return;
		}

		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!PlayerPawn)
		{
			return;
		}

		const float Distance = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
		if (Distance <= KurtarmaMesafesi)
		{
			TryKurtar(PlayerPawn);
		}
	}), KurtarmaKontrolAraligi, true);
}

void ADSVillager::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(KurtarmaTimer);
	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
	GetWorld()->GetTimerManager().ClearTimer(SpawnKorumaTimer);
}
