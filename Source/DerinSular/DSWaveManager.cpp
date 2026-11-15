// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSWaveManager.h"
#include "CombatActivatable.h"
#include "CombatEnemy.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

ADSWaveManager::ADSWaveManager()
{
	CurrentWaveNumber = 0;
	bWaveActive = false;

	// Asama 1.4: varsayilan dalga/tip eslemesi. EditAnywhere oldugu icin editorde
	// bu listeler istenirse elle degistirilebilir/genisletilebilir - burasi sadece
	// baslangic degeri.
	ConstructorHelpers::FClassFinder<ACombatEnemy> AylakBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Aylak"));
	ConstructorHelpers::FClassFinder<ACombatEnemy> KosucuBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Kosucu"));
	ConstructorHelpers::FClassFinder<ACombatEnemy> SiskinBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Siskin"));
	ConstructorHelpers::FClassFinder<ACombatEnemy> ZirhliBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Zirhli"));
	ConstructorHelpers::FClassFinder<ACombatEnemy> CigirtkanBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Cigirtkan"));
	ConstructorHelpers::FClassFinder<ACombatEnemy> DrifterBP(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Drifter"));

	const TSubclassOf<ACombatEnemy> Aylak = AylakBP.Class;
	const TSubclassOf<ACombatEnemy> Kosucu = KosucuBP.Class;
	const TSubclassOf<ACombatEnemy> Siskin = SiskinBP.Class;
	const TSubclassOf<ACombatEnemy> Zirhli = ZirhliBP.Class;
	const TSubclassOf<ACombatEnemy> Cigirtkan = CigirtkanBP.Class;
	const TSubclassOf<ACombatEnemy> Drifter = DrifterBP.Class;

	if (!Aylak || !Kosucu || !Siskin || !Zirhli || !Cigirtkan || !Drifter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ADSWaveManager: bir veya daha fazla BP_Zombie_* sinifi yuklenemedi, DalgaTipiKarisimlari varsayilani eksik kalabilir (Aylak=%d Kosucu=%d Siskin=%d Zirhli=%d Cigirtkan=%d Drifter=%d)"),
			Aylak != nullptr, Kosucu != nullptr, Siskin != nullptr, Zirhli != nullptr, Cigirtkan != nullptr, Drifter != nullptr);
	}

	FDSWaveTipiKarisimi Karisim1To2;
	Karisim1To2.BaslangicDalgasi = 1;
	Karisim1To2.DusmanSiniflari = { Aylak };

	FDSWaveTipiKarisimi Karisim3To4;
	Karisim3To4.BaslangicDalgasi = 3;
	Karisim3To4.DusmanSiniflari = { Aylak, Kosucu };

	FDSWaveTipiKarisimi Karisim5To6;
	Karisim5To6.BaslangicDalgasi = 5;
	Karisim5To6.DusmanSiniflari = { Aylak, Kosucu, Siskin, Zirhli };

	FDSWaveTipiKarisimi Karisim7Plus;
	Karisim7Plus.BaslangicDalgasi = 7;
	Karisim7Plus.DusmanSiniflari = { Aylak, Kosucu, Siskin, Zirhli, Cigirtkan, Drifter };

	DalgaTipiKarisimlari = { Karisim1To2, Karisim3To4, Karisim5To6, Karisim7Plus };
}

void ADSWaveManager::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager hazir"));

	ManagedSpawners.RemoveAll([](AActor* A) { return !IsValid(A); });
	if (ManagedSpawners.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatEnemySpawner::StaticClass(), ManagedSpawners);
	}

	int32 BoundSpawnerCount = 0;

	for (AActor* Spawner : ManagedSpawners)
	{
		if (ACombatEnemySpawner* EnemySpawner = Cast<ACombatEnemySpawner>(Spawner))
		{
			EnemySpawner->ResetSpawner();
			EnemySpawner->OnSpawnerDepleted.AddUniqueDynamic(this, &ADSWaveManager::HandleSpawnerDepleted);
			++BoundSpawnerCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: %d/%d spawner'a basariyla bind olundu"), BoundSpawnerCount, ManagedSpawners.Num());

	// kalici oyun akisi: ilk dalga hazirlik suresinin ardindan otomatik baslar.
	// T tusu testi yedek olarak calismaya devam eder; oyuncu erken tetiklerse
	// StartNextWave bekleyen zamanlayiciyi iptal eder ve cift baslatma olmaz.
	// Bariscil vitrin modu (ACombatGameMode::bPeacefulShowcaseMode) acikken GameMode bAutoStartFirstWave'i kapatir.
	if (bAutoStartFirstWave)
	{
		if (FirstWaveDelay > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(NextWaveTimerHandle, this, &ADSWaveManager::StartNextWave, FirstWaveDelay, false);

			UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Ilk dalga %.1f saniye sonra otomatik baslayacak"), FirstWaveDelay);
		}
		else
		{
			// gecikme sifirsa dogrudan basla (SetTimer <= 0 surelerde calismaz)
			StartNextWave();
		}
	}
}

void ADSWaveManager::RegisterSpawner(AActor* Spawner)
{
	if (!IsValid(Spawner))
	{
		return;
	}

	ManagedSpawners.AddUnique(Spawner);
	if (ACombatEnemySpawner* EnemySpawner = Cast<ACombatEnemySpawner>(Spawner))
	{
		EnemySpawner->ResetSpawner();
		EnemySpawner->OnSpawnerDepleted.AddUniqueDynamic(this, &ADSWaveManager::HandleSpawnerDepleted);
	}

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Spawner kaydedildi - %s"), *Spawner->GetName());
}

void ADSWaveManager::StartNextWave()
{
	// bekleyen otomatik baslatma zamanlayicisini iptal et; boylece dalga elle
	// (T tusu) baslatildiginda zamanlayici sonradan ates edip fazladan dalga acamaz
	GetWorld()->GetTimerManager().ClearTimer(NextWaveTimerHandle);

	DepletedSpawnersThisWave.Empty();

	++CurrentWaveNumber;
	bWaveActive = true;

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Wave %d basladi"), CurrentWaveNumber);

	// simple linear difficulty ramp: how many enemies each spawner should spawn this wave
	const int32 TargetEnemiesThisWave = BaseEnemiesPerWave + (CurrentWaveNumber - 1) * EnemiesPerWaveIncrement;
	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Wave %d hedef dusman sayisi: %d"), CurrentWaveNumber, TargetEnemiesThisWave);

	// HUD icin: dalga degisti
	OnWaveChanged.Broadcast(CurrentWaveNumber, TargetEnemiesThisWave);

	for (AActor* Spawner : ManagedSpawners)
	{
		if (ACombatEnemySpawner* EnemySpawner = Cast<ACombatEnemySpawner>(Spawner))
		{
			// bu dalga icin gecerli dusman tipi karisimi varsa (DalgaTipiKarisimlari), spawner
			// EnemyClass yerine bu listeden sececek - o zaman EnemyClass'in bos olmasi sorun degil
			const FDSWaveTipiKarisimi* Karisim = GetKarisimForWave(CurrentWaveNumber);
			const bool bHasTypeMix = Karisim && Karisim->DusmanSiniflari.Num() > 0;

			// a spawner without an enemy class (and without a type mix to fall back on) silently
			// spawns nothing and never depletes, which would stall the wave loop - surface it loudly instead
			if (!EnemySpawner->GetEnemyClass() && !bHasTypeMix)
			{
				UE_LOG(LogTemp, Warning, TEXT("DSWaveManager: %s icin EnemyClass atanmamis, bu spawner dusman spawn edemeyecek"), *EnemySpawner->GetName());
			}

			// spawners are single-use (ActivateInteraction no-ops after the first activation),
			// so always reset them into wave-managed mode before activating
			EnemySpawner->ResetSpawner();

			// apply this wave's difficulty target
			EnemySpawner->SetSpawnCount(TargetEnemiesThisWave);

			// apply this wave's stat ramp to enemies spawned from now on
			EnemySpawner->SetWaveScaling(CurrentWaveNumber, HPCarpaniPerWave, HizCarpaniPerWave);

			// bu dalga icin gecerli dusman tipi karisimini uygula (bos ise spawner kendi EnemyClass'ini kullanmaya devam eder)
			if (bHasTypeMix)
			{
				EnemySpawner->SetAllowedEnemyClasses(Karisim->DusmanSiniflari);
			}
		}

		if (ICombatActivatable* Activatable = Cast<ICombatActivatable>(Spawner))
		{
			Activatable->ActivateInteraction(this);

			UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Spawner aktive edildi - %s"), *Spawner->GetName());
		}
	}
}

void ADSWaveManager::EndWave()
{
	bWaveActive = false;

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Dalga %d tamamlandi"), CurrentWaveNumber);

	// HUD/GameMode icin: dalga tamamlandi (kazanma kosulu kontrolu GameMode tarafinda yapilir)
	OnWaveCompleted.Broadcast(CurrentWaveNumber);
}

int32 ADSWaveManager::GetTotalRemainingEnemies() const
{
	int32 Total = 0;

	for (AActor* Spawner : ManagedSpawners)
	{
		if (const ACombatEnemySpawner* EnemySpawner = Cast<ACombatEnemySpawner>(Spawner))
		{
			Total += EnemySpawner->GetRemainingSpawnCount();
		}
	}

	return Total;
}

const FDSWaveTipiKarisimi* ADSWaveManager::GetKarisimForWave(int32 WaveNumber) const
{
	const FDSWaveTipiKarisimi* BestMatch = nullptr;

	for (const FDSWaveTipiKarisimi& Karisim : DalgaTipiKarisimlari)
	{
		// WaveNumber icin gecerli olan (BaslangicDalgasi <= WaveNumber), aday adaylar arasindan en yuksek BaslangicDalgasi'na sahip olani sec
		if (Karisim.BaslangicDalgasi <= WaveNumber && (!BestMatch || Karisim.BaslangicDalgasi > BestMatch->BaslangicDalgasi))
		{
			BestMatch = &Karisim;
		}
	}

	return BestMatch;
}

void ADSWaveManager::HandleSpawnerDepleted(AActor* DepletedSpawner)
{
	DepletedSpawnersThisWave.Add(DepletedSpawner);

	UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Spawner tukendi: %s (%d/%d)"),
		*GetNameSafe(DepletedSpawner), DepletedSpawnersThisWave.Num(), ManagedSpawners.Num());

	if (DepletedSpawnersThisWave.Num() >= ManagedSpawners.Num())
	{
		EndWave();

		GetWorld()->GetTimerManager().SetTimer(NextWaveTimerHandle, this, &ADSWaveManager::StartNextWave, TimeBetweenWaves, false);

		UE_LOG(LogTemp, Log, TEXT("DSWaveManager: Sonraki dalga %.1f saniye sonra baslayacak"), TimeBetweenWaves);
	}
}
