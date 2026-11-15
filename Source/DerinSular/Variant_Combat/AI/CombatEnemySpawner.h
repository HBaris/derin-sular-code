// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatActivatable.h"
#include "CombatEnemySpawner.generated.h"

class UCapsuleComponent;
class UArrowComponent;
class ACombatEnemy;

/** Broadcast when a spawner has spawned and lost all of its enemies and will not spawn any more */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpawnerDepleted, AActor*, DepletedSpawner);

/**
 *  A basic Actor in charge of spawning Enemy Characters and monitoring their deaths.
 *  Enemies will be spawned one by one, and the spawner will wait until the enemy dies before spawning a new one.
 *  The spawner can be remotely activated through the ICombatActivatable interface
 *  When the last spawned enemy dies, the spawner can also activate other ICombatActivatables
 */
UCLASS(abstract)
class ACombatEnemySpawner : public AActor, public ICombatActivatable
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* SpawnCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnDirection;

protected:

	/** Type of enemy to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	TSubclassOf<ACombatEnemy> EnemyClass;

	/** If true, the first enemy will be spawned as soon as the game starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	bool bShouldSpawnEnemiesImmediately = true;

	/** Time to wait before spawning the first enemy on game start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float InitialSpawnDelay = 5.0f;

	/** Number of enemies to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 100))
	int32 SpawnCount = 1;

	/** Time to wait before spawning the next enemy after the current one dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float RespawnDelay = 5.0f;

	/** Time to wait after this spawner is depleted before activating the actor list */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Activation", meta = (ClampMin = 0, ClampMax = 10))
	float ActivationDelay = 1.0f;

	/** List of actors to activate after the last enemy dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Activation")
	TArray<AActor*> ActorsToActivateWhenDepleted;

	/** Flag to ensure this is only activated once */
	bool bHasBeenActivated = false;

	/** Timer to spawn enemies after a delay */
	FTimerHandle SpawnTimer;

	/** SpawnCount value cached at BeginPlay, used to restore SpawnCount when the spawner is reset */
	UPROPERTY()
	int32 InitialSpawnCount = 0;

	/** Su anki dalga numarasi; SetWaveScaling ile DSWaveManager tarafindan atanir, spawn edilen dusmanlara uygulanacak carpanlari hesaplamak icin kullanilir */
	int32 CurrentWaveNumber = 1;

	/** Dalga basina HP carpani artisi (orn. 0.10 = %10); SetWaveScaling ile atanir */
	float HPMultiplierPerWave = 0.0f;

	/** Dalga basina hiz carpani artisi (orn. 0.05 = %5); SetWaveScaling ile atanir */
	float SpeedMultiplierPerWave = 0.0f;

	/** DSWaveManager tarafindan atanirsa, SpawnEnemy her cagrildiginda EnemyClass yerine bu listeden rastgele bir sinif secer. Bos ise (varsayilan) eski tek-tip EnemyClass davranisi degismeden devam eder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Spawner")
	TArray<TSubclassOf<ACombatEnemy>> AllowedEnemyClasses;

public:	
	
	/** Constructor */
	ACombatEnemySpawner();

	/** Broadcast when this spawner has spawned and lost all of its enemies and will not spawn any more */
	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnSpawnerDepleted OnSpawnerDepleted;

public:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	/** Spawn an enemy and subscribe to its death event */
	void SpawnEnemy();

	/** Called when the spawned enemy has died */
	UFUNCTION()
	void OnEnemyDied();

	/** Called after the last spawned enemy has died */
	void SpawnerDepleted();

public:

	/** Restores this spawner to its pre-activation state so it can be used again (e.g. for a new wave) */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void ResetSpawner();

	/** Sets how many enemies this spawner will spawn (clamped to at least 1); also becomes the value ResetSpawner restores */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SetSpawnCount(int32 NewCount);

	/** DSWaveManager tarafindan her dalga basinda cagrilir; bundan sonra spawn edilecek dusmanlara uygulanacak dalga numarasini ve stat carpanlarini ayarlar */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SetWaveScaling(int32 WaveNumber, float InHPMultiplierPerWave, float InSpeedMultiplierPerWave);

	/** DSWaveManager tarafindan her dalga basinda cagrilir; bu dalgada spawn edilebilecek dusman siniflarini ayarlar. Bos liste = eski tek-tip EnemyClass davranisina don. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SetAllowedEnemyClasses(const TArray<TSubclassOf<ACombatEnemy>>& NewAllowedClasses);

	/** Returns the enemy class this spawner will spawn */
	TSubclassOf<ACombatEnemy> GetEnemyClass() const { return EnemyClass; }

	/** HUD icin: bu spawner'in su an spawn etmesi gereken/hala hayatta olan tahmini dusman sayisi */
	int32 GetRemainingSpawnCount() const { return SpawnCount; }

public:

	// ~begin ICombatActivatable interface

	/** Toggles the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ToggleInteraction(AActor* ActivationInstigator) override;

	/** Activates the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ActivateInteraction(AActor* ActivationInstigator) override;

	/** Deactivates the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void DeactivateInteraction(AActor* ActivationInstigator) override;

	// ~end IActivatable interface
};
