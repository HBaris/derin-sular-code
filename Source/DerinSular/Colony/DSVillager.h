// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatDamageable.h"
#include "Engine/TimerHandle.h"
#include "DSVillager.generated.h"

/** Koylu baygin dustugunde yayinlanan delege */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVillagerDowned);

/** Koylu, bayginken kurtarilamayip gercekten oldugunde yayinlanan delege */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVillagerDied);

/** Koylu, baygin durumdayken kurtarildiginda yayinlanan delege */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVillagerRescued);

class UWidgetComponent;
class UCombatLifeBar;
class UAnimInstance;
class UStaticMeshComponent;
class USoundBase;

/**
 *  Sahnede duran, oldurulebilir koylu NPC (GDD 5: Koylu ve NPC Sistemleri).
 *
 *  ACombatCharacter'dan DEGIL, ACombatEnemy'nin deseninden dogrudan ACharacter + ICombatDamageable'dan
 *  turer: ACombatCharacter bir oyuncu sinifi oldugu icin HandleDeath()'i ölümde
 *  GameMode->NotifyPlayerDied() cagirir - miras alinsaydi koylu olumu Game Over tetiklerdi.
 *  Ayrica ACombatCharacter'in 7 InputAction'i, kamerasi ve kombo/sarjli saldiri sistemi
 *  NPC'de gereksizdir. Bkz. docs/YOL_HARITASI_ASAMA6-10.md Asama 6 NOT'u.
 *
 *  Bayginlik akisi (GDD 5.2): HP 0'a inince koylu direkt olmez, once bIsDowned=true olur ve
 *  hareketsiz kalir. KurtarmaSuresi icinde oyuncu TryKurtar() ile kurtarmazsa gercek olum gerceklesir.
 */
UCLASS(abstract)
class ADSVillager : public ACharacter, public ICombatDamageable
{
	GENERATED_BODY()

	/** Can bari widget bileseni */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;

	/** Koylunun sirt cantasi / ekipman gorsel aksesuar mesh'i */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* AccessoryMesh;

public:

	/** Constructor */
	ADSVillager();

	/** Koylunun adi (GDD 5: her NPC'nin adi olmali) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager")
	FString VillagerName;

	/** Koylunun sivil kiyafet rengi (M_Mannequin ZombieTint katmanina uygulanir) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Villager|Gorsel")
	FLinearColor KiyafetRengi = FLinearColor(0.22f, 0.48f, 0.78f, 1.0f);

	/** Kurtarilma aninda calinacak ses efekti */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* RescueSound;

	/** Mesh'e BeginPlay'de atanacak Animation Blueprint sinifi; varsayilan olarak constructor'da
	 *  BP_Villager_01'in Mesh AnimClass'iyla ayni ABP_Unarmed'e isaret eder (bkz. DSVillager.cpp).
	 *  Farkli bir NPC gorunumu icin BP alt sinifinda degistirilebilir */
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TSubclassOf<UAnimInstance> VillagerAnimClass;

protected:

	/** Koylunun sahip olacagi azami can (Asama 10 denge: 10-20 vurusta baygin dusmesi hedefiyle 20) */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 1))
	float MaxHP = 20.0f;

	/** Can bari widget'inin isaretcisi; BeginPlay'de LifeBar bilesenindeki widget'tan cast edilir */
	UPROPERTY(EditAnywhere, Category="Damage")
	UCombatLifeBar* LifeBarWidget;

public:

	/** Koylunun mevcut cani; BeginPlay'de MaxHP'den doldurulur */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage")
	float CurrentHP = 0.0f;

	/** Baygin mi (GDD 5.2: HP 0'a inince direkt olum yerine once bayginlik) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Villager|Baygin")
	bool bIsDowned = false;

protected:

	/** Baygin durumdayken kurtarilmazsa gercek olumun gerceklesecegi sure */
	UPROPERTY(EditAnywhere, Category="Villager|Baygin", meta = (ClampMin = 0, Units = "s"))
	float KurtarmaSuresi = 30.0f;

	/** KurtarmaSuresi dolunca GercekOlum'u tetikleyen zamanlayici */
	FTimerHandle KurtarmaTimer;

	/** Gercek olumden sonra aktoru sahneden kaldirmadan once beklenecek sure (ragdoll'un gorunmesi icin) */
	UPROPERTY(EditAnywhere, Category="Villager|Baygin", meta = (ClampMin = 0, Units = "s"))
	float DeathRemovalTime = 5.0f;

	/** Aktoru sahneden kaldiran zamanlayici */
	FTimerHandle DeathTimer;

	/** Spawn aninda (orn. level acilisinda ust uste binen collision) ani hasara karsi bagisiklik suresi */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, Units = "s"))
	float SpawnKorumaSuresi = 1.0f;

	/** SpawnKorumaSuresi boyunca true; TakeDamage bu sure icinde hasari yok sayar */
	bool bSpawnKorumasiAktif = false;

	/** SpawnKorumaSuresi dolunca bSpawnKorumasiAktif'i kapatan zamanlayici */
	FTimerHandle SpawnKorumaTimer;

public:

	/** Koylu baygin dustugunde yayinlanan delege */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnVillagerDowned OnVillagerDowned;

	/** Koylu gercekten oldugunde yayinlanan delege */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnVillagerDied OnVillagerDied;

	/** Koylu kurtarildiginda yayinlanan delege */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnVillagerRescued OnVillagerRescued;

public:

	/** Oyuncu E tusuyla cagirir: bIsDowned ise koylu ayaga kalkar, KurtarmaTimer iptal olur; basariliysa true doner */
	UFUNCTION(BlueprintCallable, Category="Villager")
	bool TryKurtar(AActor* Kurtaran);

public:

	// ~begin ICombatDamageable interface

	/** Hasar ve itki olaylarini isler */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Olum olayini isler: ilk kez cagrildiginda koyluyu baygin duruma sokar (gercek olum degil) */
	virtual void HandleDeath() override;

	/** Iyilesme olaylarini isler */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Yaklasan tehlikeye (orn. zombi saldirisi) tepki verebilmesi icin koyluyu uyarir */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end ICombatDamageable interface

	/** Varsayilan TakeDamage davranisini override eder */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:

	/** Baygin duruma girer: hareket durur, KurtarmaTimer kurulur, OnVillagerDowned yayinlanir */
	void EnterDownedState();

	/** KurtarmaTimer dolunca cagirilir: gercek olum - ragdoll acilir, OnVillagerDied yayinlanir, sahneden kaldirma zamanlanir */
	void GercekOlum();

	/** Aktoru sahneden kaldirir */
	void RemoveFromLevel();

	/** SpawnKorumaTimer dolunca cagirilir: spawn korumasini kapatir, TakeDamage normal islemeye baslar */
	void BitirSpawnKorumasi();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
};
