// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Variant_Combat/AI/CombatEnemySpawner.h"
#include "DSWaveManager.generated.h"

/** HUD icin: yeni bir dalga basladiginda yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWaveChanged, int32, WaveNumber, int32, HedefDusmanSayisiPerSpawner);

/** HUD/GameMode icin: bir dalga tamamen bittiginde (tum spawner'lar tukendiginde) yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCompleted, int32, TamamlananWaveNumarasi);

/**
 *  Belirli bir dalga numarasindan itibaren gecerli olan dusman tipi karisimi.
 *  DalgaTipiKarisimlari listesinde birden fazla giris varsa, o an oynanan dalga
 *  icin BaslangicDalgasi'si dalga numarasina esit veya kucuk olan en yuksek
 *  girdi kullanilir (orn. Wave 1-2: sadece Aylak, Wave 3+: Aylak+Kosucu karisir).
 */
USTRUCT(BlueprintType)
struct FDSWaveTipiKarisimi
{
	GENERATED_BODY()

	/** Bu karisimin gecerli oldugu ilk dalga numarasi (dahil) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager", meta = (ClampMin = 1))
	int32 BaslangicDalgasi = 1;

	/** Bu araliktaki dalgalarda spawn edilebilecek dusman siniflari; spawner her spawn'da bunlardan birini rastgele secer */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	TArray<TSubclassOf<ACombatEnemy>> DusmanSiniflari;
};

/**
 *  Manages combat wave progression: tracks the current wave, times the gap
 *  between waves, and exposes hooks to start/end a wave.
 */
UCLASS()
class ADSWaveManager : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	ADSWaveManager();

	/** Index of the wave currently in progress (or most recently started) */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Manager")
	int32 CurrentWaveNumber;

	/** Time to wait between waves, in seconds (8 gunde bir dalga dengesine referans) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	float TimeBetweenWaves = 8.0f;

	/** True ise ilk dalga BeginPlay'den FirstWaveDelay saniye sonra otomatik baslar */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	bool bAutoStartFirstWave = true;

	/** Oyun basindaki hazirlik suresi: ilk dalga baslamadan once beklenecek sure (M0: ilk dalga 15. gunde, TimeBetweenWaves'teki 1 gun = 1 saniye dengesiyle 15.0f) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager", meta = (ClampMin = 0, Units = "s", EditCondition = "bAutoStartFirstWave"))
	float FirstWaveDelay = 15.0f;

	/** Number of enemies targeted for the first wave */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	int32 BaseEnemiesPerWave = 6;

	/** Additional enemies targeted per wave beyond the first (simple linear increase) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	int32 EnemiesPerWaveIncrement = 2;

	/** Wave basina HP carpani artisi (orn. 0.10 = Wave N'de HP x(1+(N-1)*0.10)) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager", meta = (ClampMin = 0))
	float HPCarpaniPerWave = 0.10f;

	/** Wave basina hiz carpani artisi (orn. 0.05 = Wave N'de hiz x(1+(N-1)*0.05)) */
	UPROPERTY(EditAnywhere, Category = "Wave Manager", meta = (ClampMin = 0))
	float HizCarpaniPerWave = 0.05f;

	/** True while a wave is currently active */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Manager")
	bool bWaveActive;

	/** Spawners activated when a wave starts. Assigned manually in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Manager")
	TArray<AActor*> ManagedSpawners;

	/**
	 *  Dalga numarasina gore hangi dusman siniflarinin spawn edilecegini belirleyen
	 *  karisim listesi. Bos birakilirsa (varsayilan) her spawner kendi EnemyClass'ini
	 *  kullanmaya devam eder - eski tek-tip davranis bozulmaz.
	 */
	UPROPERTY(EditAnywhere, Category = "Wave Manager")
	TArray<FDSWaveTipiKarisimi> DalgaTipiKarisimlari;

	/** HUD icin: yeni bir dalga basladiginda yayinlanir (dalga numarasi, spawner basina hedef dusman sayisi) */
	UPROPERTY(BlueprintAssignable, Category = "Wave Manager|Events")
	FOnWaveChanged OnWaveChanged;

	/** HUD/GameMode icin: bir dalga tamamen bittiginde yayinlanir */
	UPROPERTY(BlueprintAssignable, Category = "Wave Manager|Events")
	FOnWaveCompleted OnWaveCompleted;

	/** Adds a spawner to ManagedSpawners */
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void RegisterSpawner(AActor* Spawner);

	/** HUD icin: ManagedSpawners icindeki tum ACombatEnemySpawner'larin kalan spawn sayilarinin toplami (kaba bir "kalan dusman" tahmini) */
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	int32 GetTotalRemainingEnemies() const;

	/** Advances to the next wave, marks it active, and activates all managed spawners */
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void StartNextWave();

	/** Marks the current wave as no longer active */
	UFUNCTION(BlueprintCallable, Category = "Wave Manager")
	void EndWave();

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Called when one of the ManagedSpawners broadcasts OnSpawnerDepleted */
	UFUNCTION()
	void HandleSpawnerDepleted(AActor* DepletedSpawner);

private:

	/** DalgaTipiKarisimlari icinde WaveNumber icin gecerli girdiyi bulur (BaslangicDalgasi <= WaveNumber olan en yuksek); yoksa nullptr doner */
	const FDSWaveTipiKarisimi* GetKarisimForWave(int32 WaveNumber) const;

	/** Spawners that have been depleted during the current wave */
	TSet<AActor*> DepletedSpawnersThisWave;

	/** Timer used to automatically start the next wave after TimeBetweenWaves */
	FTimerHandle NextWaveTimerHandle;
};
