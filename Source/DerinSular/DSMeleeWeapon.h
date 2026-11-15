// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DSMeleeWeapon.generated.h"

class UStaticMeshComponent;
class UAnimMontage;

/**
 *  Basit, tek parca yakin dovus silahi. Karakterin elinde (soket) tasinir ve
 *  DoAttackTrace cagrildiginda onunden kisa menzilli bir SphereTrace yapar.
 *  Hasar, yeni bir hasar yolu acmadan mevcut ICombatDamageable::ApplyDamage
 *  arayuzu uzerinden uygulanir; bu da hedefin kendi TakeDamage'ine gider
 *  (orn. ACombatEnemy::TakeDamage).
 */
UCLASS()
class ADSMeleeWeapon : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	ADSMeleeWeapon();

	/** Silahin gorsel mesh'i. Simdilik placeholder kup; sonra balta mesh'i ile degistirilecek */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;

	/** Bir vurusun verdigi hasar */
	UPROPERTY(EditAnywhere, Category="Melee Weapon", meta = (ClampMin = 0))
	float DamageAmount = 1.0f;

	/** Saldiri animasyonu; opsiyoneldir, atanmazsa silah animasyonsuz da calisir */
	UPROPERTY(EditAnywhere, Category="Melee Weapon")
	UAnimMontage* AttackMontage;

	/** SphereTrace'in silahin onunden ne kadar ileri uzanacagi */
	UPROPERTY(EditAnywhere, Category="Melee Weapon|Trace", meta = (ClampMin = 0, Units = "cm"))
	float TraceRange = 150.0f;

	/** SphereTrace'in yaricapi */
	UPROPERTY(EditAnywhere, Category="Melee Weapon|Trace", meta = (ClampMin = 0, Units = "cm"))
	float TraceRadius = 40.0f;

	/**
	 *  Silahin onunden SphereTrace ile vurus kontrolu yapar; isabet eden her
	 *  ICombatDamageable'a ApplyDamage ile hasar uygular. Wielder, kendine
	 *  vurmayi engellemek icin trace disi birakilir (silahi kusanan karakter).
	 */
	UFUNCTION(BlueprintCallable, Category="Melee Weapon")
	void DoAttackTrace(AActor* Wielder);

protected:
	virtual void BeginPlay() override;
};
