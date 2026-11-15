// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DSVillagerController.generated.h"

class ADSVillager;

/** Koylunun basit kacma davranisini tanimlayan durum makinesi (StateTree degil, GDD Asama 6.3) */
UENUM(BlueprintType)
enum class EVillagerState : uint8
{
	Calisiyor,
	Kaciyor,
	Baygin
};

/**
 *  ADSVillager icin StateTree kullanmayan, basit Tick tabanli kacma AI'i (Asama 6.3).
 *  Her tick'te en yakin ACombatEnemy'yi arar: KacmaTetikMesafesi icine girince Kaciyor
 *  durumuna gecer ve RequestDirectMove ile zombiden ters yone kacar; zombi GuvenliMesafe
 *  disina cikinca Calisiyor'a doner. Koylu bIsDowned oldugunda (bkz. ADSVillager bayginlik
 *  akisi) Baygin durumuna gecilir ve kacma mantigi devre disi kalir.
 */
UCLASS()
class ADSVillagerController : public AAIController
{
	GENERATED_BODY()

public:

	/** Constructor */
	ADSVillagerController();

protected:

	/** Zombi bu mesafenin icine girince kacis tetiklenir */
	UPROPERTY(EditAnywhere, Category="Kacma", meta = (ClampMin = 0, Units = "cm"))
	float KacmaTetikMesafesi = 1500.0f;

	/** Kacistan cikip Calisiyor'a donmek icin zombinin bu mesafenin disina cikmasi gerekir
	 *  (KacmaTetikMesafesi'nden buyuk tutulur, aksi halde sinirda titreme/flip-flop olusur) */
	UPROPERTY(EditAnywhere, Category="Kacma", meta = (ClampMin = 0, Units = "cm"))
	float GuvenliMesafe = 1800.0f;

	/** Kacma yonunde her tick'te istenen hareket hizi carpani (1 = MaxWalkSpeed) */
	UPROPERTY(EditAnywhere, Category="Kacma", meta = (ClampMin = 0))
	float KacmaHizCarpani = 1.0f;

	/** Possess edilen koylu (OnPossess'te atanir) */
	UPROPERTY(Transient)
	ADSVillager* ControlledVillager;

	/** Mevcut kacma durumu */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Kacma")
	EVillagerState CurrentState = EVillagerState::Calisiyor;

	/** Sahnedeki tum ACombatEnemy'ler arasindan koyluye en yakin olani bulur; hicbiri yoksa nullptr ve OutDistance=0 doner */
	AActor* FindClosestZombi(float& OutDistance) const;

public:

	/** Pawn Initialization */
	virtual void OnPossess(APawn* InPawn) override;

	/** Zombi yakinligini kontrol edip kacma hareketini uygular */
	virtual void Tick(float DeltaTime) override;

	/** Disaridan (orn. DSBarikat::NotifyDanger veya yakin tehlike) gelen tehlike uyarisiyla derhal kacis baslatir */
	void ForceFleeFromDanger(const FVector& DangerLocation);
};
