// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimMontage.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "CombatEnemy.generated.h"

class UWidgetComponent;
class UCombatLifeBar;
class UAnimMontage;
class UDataTable;
class UParticleSystem;
class USoundBase;

/** Completed attack animation delegate for StateTree */
DECLARE_DELEGATE(FOnEnemyAttackCompleted);

/** Landed delegate for StateTree */
DECLARE_DELEGATE(FOnEnemyLanded);

/** Enemy died delegate */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyDied);

/** Vurus alindiginda yayinlanan delege; ses/efekt/kamera sarsintisi gibi sistemler buraya baglanabilir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHitReceived, float, Damage, FVector, ImpactPoint, FVector, DamageDirection);

/**
 *  An AI-controlled character with combat capabilities.
 *  Its bundled AI Controller runs logic through StateTree
 */
UCLASS(abstract)
class ACombatEnemy : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;

public:
	
	/** Constructor */
	ACombatEnemy();

protected:

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage")
	float MaxHP = 3.0f;

	/** DataTable satir adi (FZombiTipiVerisi); BeginPlay'de ZombiTipleriTablosu'ndan bu tipin verisi okunup uygulanir */
	UPROPERTY(EditDefaultsOnly, Category="Zombi Tipi")
	FName ZombiTipiSatirAdi;

	/** Row Structure'i FZombiTipiVerisi olan DataTable; bos birakilirsa veya satir bulunamazsa varsayilan degerlerle devam edilir */
	UPROPERTY(EditDefaultsOnly, Category="Zombi Tipi")
	UDataTable* ZombiTipleriTablosu;

	/** ApplyZombiTipiVerisi'de DataTable satirindan okunan yetenek etiketi (orn. Zombi.Yetenek.Patlama); yetenegi olmayan tiplerde bos */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombi Tipi")
	FGameplayTag CurrentAbilityTag;

	/** Zombi.Yetenek.Patlama etiketine sahipse HandleDeath'te uygulanacak alan hasari (orn. Siskin) */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Patlama", meta = (ClampMin = 0))
	float ExplosionDamage = 2.0f;

	/** Patlama hasarinin etki yaricapi */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Patlama", meta = (ClampMin = 0, Units = "cm"))
	float ExplosionRadius = 300.0f;

	/** Patlama aninda oynatilacak gorsel efekt; bos birakilirsa sadece uyari logu basilir (TODO: BP alt sinifinda ata) */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Patlama")
	UParticleSystem* ExplosionEffect;

	/** Zombi.Yetenek.Ciglik etiketine sahipse, oyuncuyu algiladiginda (GetPlayerInfo basarili oldugunda) bu yaricaptaki diger zombiler oyuncuya yonlendirilir */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Ciglik", meta = (ClampMin = 0, Units = "cm"))
	float CiglikYaricapi = 800.0f;

	/** Ciglik yetenegi tekrar tetiklenmeden once beklenmesi gereken sure */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Ciglik", meta = (ClampMin = 0, Units = "s"))
	float CiglikCooldownSuresi = 9.0f;

	/** Ciglik duyan bir zombinin, GetPlayerInfo'nun normal MaxRange kontrolunu atlayip oyuncuyu hedef almaya devam edecegi sure */
	UPROPERTY(EditAnywhere, Category="Yetenekler|Ciglik", meta = (ClampMin = 0, Units = "s"))
	float ZorlaAggroSuresi = 6.0f;

	/** Son ciglik atma zamani (GetWorld time seconds); cooldown kontrolu icin, editorde degistirilmez */
	float LastCiglikZamani = -1000.0f;

	/** Zorla aggro modunun bitecegi oyun zamani (GetWorld time seconds); Ciglik tarafindan tetiklenir, editorde degistirilmez */
	float ZorlaAggroBitisZamani = -1000.0f;

	/** Zombi.Yetenek.ZirhAzaltma etiketine sahipse, TakeDamage'da gelen hasarin yuzde kac azaltilacagi (0.35 = %35) */
	UPROPERTY(EditAnywhere, Category="Yetenekler|ZirhAzaltma", meta = (ClampMin = 0, ClampMax = 1))
	float ZirhAzaltmaOrani = 0.35f;

	/** Zombi.Yetenek.FazGecis etiketine sahipse, bu araliklarla (saniye) duzensiz "faz" hareketi tetiklenir */
	UPROPERTY(EditAnywhere, Category="Yetenekler|FazGecis", meta = (ClampMin = 0.5, Units = "s"))
	float FazAraligi = 5.0f;

	/** Faz sirasinda ne kadar sure hasar alinmayacagi */
	UPROPERTY(EditAnywhere, Category="Yetenekler|FazGecis", meta = (ClampMin = 0, Units = "s"))
	float FazSuresi = 0.6f;

	/** Faz sirasinda uygulanacak yanal (duzensiz hareket) impulse buyuklugu */
	UPROPERTY(EditAnywhere, Category="Yetenekler|FazGecis", meta = (ClampMin = 0, Units = "cm/s"))
	float FazYanalImpulsBuyuklugu = 600.0f;

	/** Faz'in bitecegi oyun zamani (GetWorld time seconds); TakeDamage bu zamana kadar hasari sifirlar, editorde degistirilmez */
	float FazBitisZamani = -1000.0f;

	/** Faz dongusunu tetikleyen zamanlayici; sadece FazGecis etiketli tiplerde BeginPlay'de kurulur */
	FTimerHandle FazDonguTimer;

public:

	/** Current amount of HP the character has */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float CurrentHP = 0.0f;

	/** Spawner tarafindan spawn'dan once atanan dalga bazli HP carpani (1 = carpan yok); BeginPlay'de MaxHP'ye uygulanir */
	UPROPERTY(BlueprintReadWrite, Category="Wave Scaling")
	float WaveHPMultiplier = 1.0f;

	/** Spawner tarafindan spawn'dan once atanan dalga bazli hiz carpani (1 = carpan yok); BeginPlay'de MaxWalkSpeed'e uygulanir */
	UPROPERTY(BlueprintReadWrite, Category="Wave Scaling")
	float WaveSpeedMultiplier = 1.0f;

protected:

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	UCombatLifeBar* LifeBarWidget;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceRadius = 50.0f;

	/** Amount of damage a melee attack will deal */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 150.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 350.0f;

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Target number of attacks in the combo attack string we're playing */
	int32 TargetComboCount = 0;

	/** Index of the current stage of the melee attack combo */
	int32 CurrentComboAttack = 0;

	/** AnimMontage that will play for charged attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Name of the AnimMontage section that corresponds to the charge loop */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Name of the AnimMontage section that corresponds to the attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Minimum number of charge animation loops that will be played by the AI */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged", meta = (ClampMin = 1, ClampMax = 20))
	int32 MinChargeLoops = 2;

	/** Maximum number of charge animation loops that will be played by the AI */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged", meta = (ClampMin = 1, ClampMax = 20))
	int32 MaxChargeLoops = 5;

	/** Target number of charge animation loops to play in this charged attack */
	int32 TargetChargeLoops = 0;

	/** Number of charge animation loop currently playing */
	int32 CurrentChargeLoop = 0;

	/** Time to wait before removing this character from the level after it dies */
	UPROPERTY(EditAnywhere, Category="Death")
	float DeathRemovalTime = 5.0f;

	/** Bu tip oldugunde GameMode'un skoruna eklenecek puan */
	UPROPERTY(EditAnywhere, Category="Death")
	int32 OlumSkoru = 10;

	/** Olum aninda calinacak ses; bos birakilirsa sadece log uyarisi verilir (TODO: BP alt sinifinda ata, Asama 5 ses tasarimi) */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* DeathSound;

	/** Vurus alindiginda calinacak ses; bos birakilirsa sadece log uyarisi verilir (TODO: BP alt sinifinda ata, Asama 5 ses tasarimi) */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* HitSound;

	/** Enemy death timer */
	FTimerHandle DeathTimer;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Last recorded location we're being attacked from */
	FVector LastDangerLocation = FVector::ZeroVector;

	/** Last recorded game time we were attacked */
	float LastDangerTime = -1000.0f;

public:
	/** Attack completed internal delegate to notify StateTree tasks */
	FOnEnemyAttackCompleted OnAttackCompleted;

	/** Landed internal delegate to notify StateTree tasks. We use this instead of the built-in Landed delegate so we can bind to a Lambda in StateTree tasks */
	FOnEnemyLanded OnEnemyLanded;

	/** Enemy died delegate. Allows external subscribers to respond to enemy death */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnEnemyDied OnEnemyDied;

	/** Vurus alindiginda (TakeDamage'da) yayinlanan delege - ses/particle/kamera sarsintisi gibi sistemler buraya baglanabilir */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnHitReceived OnHitReceived;

protected:

	/** Hit-stop sirasinda uygulanacak zaman yavaslamasi carpani (1 = yavaslama yok) */
	UPROPERTY(EditAnywhere, Category="Hit Stop", meta = (ClampMin = 0.01, ClampMax = 1))
	float HitStopTimeDilation = 0.15f;

	/** Hit-stop suresi (gercek/dunya saniyesi) */
	UPROPERTY(EditAnywhere, Category="Hit Stop", meta = (ClampMin = 0, Units = "s"))
	float HitStopSuresi = 0.08f;

	/** Hit-stop'u bitirip CustomTimeDilation'i normale dondurmek icin kullanilan zamanlayici */
	FTimerHandle HitStopTimer;

	/** Kismi ragdoll ("stagger") gorunumunun en fazla ne kadar surebilecegi; Landed() sadece knockback
	 *  impulse ile havalanildiginda tetiklenir, RadialDamage gibi impulse uygulamayan hasarlarda hic
	 *  tetiklenmeyebilir - bu yuzden suresiz kalmayi onlemek icin ayri bir zamanlayici kullanilir */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, Units = "s"))
	float StaggerSuresi = 4.0f;

	/** Stagger'i bitirip kismi ragdoll blend weight'ini sifirlamak icin kullanilan zamanlayici */
	FTimerHandle StaggerTimer;

public:

	/** Performs an AI-initiated combo attack. Number of hits will be decided by this character */
	void DoAIComboAttack();

	/** True if currently playing an attack montage */
	bool IsAttacking() const { return bIsAttacking; }

	/** Performs an AI-initiated charged attack. Charge time will be decided by this character */
	void DoAIChargedAttack();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Returns the last recorded location we were attacked from */
	const FVector& GetLastDangerLocation() const;

	/** Returns the last game time we were attacked */
	float GetLastDangerTime() const;

public:

	// ~begin ICombatAttacker interface

	/** Performs an attack's collision check */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs a combo attack's check to continue the string */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckCombo() override;

	/** Performs a charged attack's check to loop the charge animation */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckChargedAttack() override;

	// ~end ICombatAttacker interface

	// ~begin ICombatDamageable interface

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows the enemy to react to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end ICombatDamageable interface

	/** Zombi.Yetenek.Ciglik etiketine sahipse ve cooldown doluysa, CiglikYaricapi icindeki diger zombileri oyuncuya yonlendiren ciglik yetenegini tetikler. FStateTreeGetPlayerInfoTask basarili oldugunda cagrilir (mevcut oyuncu-algilama sistemi kullanilir, yeni StateTree node'u gerekmez) */
	void TryCiglikYetenegi(const FVector& OyuncuKonumu);

	/** Baska bir zombinin ciglik yetenegi tarafindan cagrilir: bu zombiyi ZorlaAggroSuresi boyunca oyuncuya yonlendirir (GetPlayerInfo'nun MaxRange kontrolunu gecici olarak atlatir) */
	void NotifyCiglikDuyuldu(const FVector& OyuncuKonumu);

	/** GetPlayerInfo StateTree task'i icin: zorla aggro modu aktifse (ciglik ile tetiklenmisse) true doner */
	bool IsZorlaAggroActive() const;

protected:

	/** Drifter: FazDonguTimer tarafindan periyodik olarak cagrilir - rastgele yanal itki uygular ve kisa bir hasarsizlik penceresi acar */
	void TriggerFazGecisi();

	/** Vurus geri bildirimi: TakeDamage'da cagrilir, CustomTimeDilation'i kisa sureligine dusurur (hit-stop) */
	void BaslatHitStop();

	/** HitStopTimer tarafindan cagrilir: CustomTimeDilation'i normale dondurur */
	void BitirHitStop();

	/** StaggerTimer tarafindan cagrilir: kismi ragdoll blend weight'ini sifirlar (Landed() tetiklenmese bile) */
	void BitirStagger();

protected:

	/** Removes this character from the level after it dies */
	void RemoveFromLevel();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

protected:

	/** Tops HP before Super spawns the AI Controller, so the StateTree never starts while CurrentHP is 0 */
	virtual void PostInitializeComponents() override;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** ZombiTipleriTablosu'ndan ZombiTipiSatirAdi satirini okuyup MaxHP/CurrentHP, MaxWalkSpeed ve MeleeDamage'a uygular; satir bulunamazsa mevcut varsayilanlarla devam eder */
	void ApplyZombiTipiVerisi();

	/** EndPlay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
};
