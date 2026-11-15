// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatDamageable.h"
#include "DSBarikat.generated.h"

class UStaticMeshComponent;
class USoundBase;
class ADSBarikat;

/** Barikat HP=0'a inip yikildiginda yayinlanan delege */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBarikatDestroyed);

/** Barikat saldiri/tehlike altinda oldugunda yayinlanan erken uyari delegesi (docs/BARIKAT_TEHLIKE_TASARIM.md Secenek C) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBarikatTehlikede, ADSBarikat*, Barikat, float, CanYuzdesi);

/**
 *  Koloniyi zombilerden koruyan, tamir edilebilir barikat (GDD Asama 7).
 *  ACombatDamageableBox'in deseninden turer: AActor + ICombatDamageable, TakeDamage'a
 *  gitmeden dogrudan ApplyDamage'de HP isler. Oyuncu yakininda E tusuna bastiginda TamirEt()
 *  cagrilir ve +TamirMiktari kadar can iyilesir (MaxHP'yi asamaz).
 *
 *  CombatCharacter'da henuz bir Enhanced Input "Interact" aksiyonu yok (bkz. DSVillager.cpp
 *  TryKurtar GECICI notu, Asama 6.2). Yeni bir InputAction asset'i veya CombatCharacter
 *  degisikligi gerektirmemek icin E tusu burada dogrudan Tick icinde
 *  PlayerController->WasInputKeyJustPressed(EKeys::E) ile yoklanir.
 */
UCLASS(abstract)
class ADSBarikat : public AActor, public ICombatDamageable
{
	GENERATED_BODY()

	/** Barikat mesh'i */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

public:

	/** Constructor */
	ADSBarikat();

protected:

	/** Barikatin sahip olacagi azami can (Asama 10 denge: 15-30 vurusta yikilmasi hedefiyle 30) */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 1))
	float MaxHP = 30.0f;

	/** Tek bir tamir etkilesiminde eklenecek can miktari */
	UPROPERTY(EditAnywhere, Category="Tamir", meta = (ClampMin = 0))
	float TamirMiktari = 40.0f;

	/** Oyuncunun E tusuyla tamir edebilecegi azami mesafe */
	UPROPERTY(EditAnywhere, Category="Tamir", meta = (ClampMin = 0, Units = "cm"))
	float TamirMesafesi = 200.0f;

	/** Barikat tehlike aldiginda cevresindeki koyluleri uyarma yaricapi (Secenek C Hibrit) */
	UPROPERTY(EditAnywhere, Category="Tehlike", meta = (ClampMin = 0, Units = "cm"))
	float TehlikeYaricapi = 800.0f;

	/** Tehlike uyarisi spam engelleme suresi */
	UPROPERTY(EditAnywhere, Category="Tehlike", meta = (ClampMin = 0, Units = "s"))
	float TehlikeCooldown = 3.0f;

	/** Son tehlike bildirimi zamani */
	float SonTehlikeZamani = -1000.0f;

	/** Tamir sesi */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* RepairSound;

	/** Yikilma sesi */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* BreakSound;

public:

	/** Barikatin mevcut cani; BeginPlay'de MaxHP'den doldurulur */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage")
	float CurrentHP = 0.0f;

	/** Barikat yikildiginda yayinlanan delege */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnBarikatDestroyed OnBarikatDestroyed;

	/** Barikat tehlike/saldiri altindayken yayinlanan delege */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnBarikatTehlikede OnBarikatTehlikede;

public:

	// ~begin ICombatDamageable interface

	/** Hasar olaylarini isler: HP dusurur, 0'a inince HandleDeath cagirir */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Olum olayini isler: barikati sahneden kaldirir */
	virtual void HandleDeath() override;

	/** Iyilesme olaylarini isler: CurrentHP'yi MaxHP'yi asmayacak sekilde artirir */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** stub - barikat tehlike uyarisina tepki vermez */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end ICombatDamageable interface

	/** Oyuncu E tusuna bastiginda ve barikata yeterince yakinsa cagrilir: ApplyHealing ile tamir uygular */
	UFUNCTION(BlueprintCallable, Category="Tamir")
	void TamirEt(AActor* Tamirci);

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Oyuncunun E tusuna basip basmadigini ve mesafeyi kontrol eder */
	virtual void Tick(float DeltaTime) override;
};
