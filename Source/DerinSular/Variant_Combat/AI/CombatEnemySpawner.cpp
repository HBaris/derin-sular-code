// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatEnemySpawner.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "CombatEnemy.h"
#include "UObject/ConstructorHelpers.h"

ACombatEnemySpawner::ACombatEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// default to the Aylak zombie so newly created spawners spawn zombies out of the box.
	// note: serialized values (BP defaults / level instances) still override this.
	static ConstructorHelpers::FClassFinder<ACombatEnemy> ZombieClassFinder(TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Aylak.BP_Zombie_Aylak_C"));
	if (ZombieClassFinder.Succeeded())
	{
		EnemyClass = ZombieClassFinder.Class;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ACombatEnemySpawner: BP_Zombie_Aylak bulunamadi, varsayilan EnemyClass atanamadi"));
	}

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

void ACombatEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	// cache the starting SpawnCount so ResetSpawner() can restore it later
	InitialSpawnCount = SpawnCount;

	// should we spawn an enemy right away?
	if (bShouldSpawnEnemiesImmediately)
	{
		// schedule the first enemy spawn
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnEnemy, InitialSpawnDelay);
	}

}

void ACombatEnemySpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the spawn timer
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
}

void ACombatEnemySpawner::SpawnEnemy()
{
	// her spawn tick'inde 1 yerine 3 dusman ayni anda cikar
	for (int32 i = 0; i < 3; ++i)
	{
		// AllowedEnemyClasses atanmissa (DSWaveManager'in dalga/tip karisimi) bunlardan rastgele
		// birini sec; bos ise eski tek-tip EnemyClass davranisina don
		TSubclassOf<ACombatEnemy> ClassToSpawn = EnemyClass;
		if (AllowedEnemyClasses.Num() > 0)
		{
			ClassToSpawn = AllowedEnemyClasses[FMath::RandRange(0, AllowedEnemyClasses.Num() - 1)];
		}

		// ensure the enemy class is valid
		if (IsValid(ClassToSpawn))
		{
			const FTransform SpawnTransform = SpawnCapsule->GetComponentTransform();

			// deferred spawn: dalga carpanlarini BeginPlay calismadan once atayabilmek icin
			// FinishSpawning'e kadar BeginPlay ertelenir
			ACombatEnemy* SpawnedEnemy = GetWorld()->SpawnActorDeferred<ACombatEnemy>(ClassToSpawn, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

			// was the enemy successfully created?
			if (SpawnedEnemy)
			{
				// dalga bazli stat rampasi: BeginPlay bu carpanlari MaxHP/MaxWalkSpeed'e uygulayacak
				SpawnedEnemy->WaveHPMultiplier = 1.0f + (CurrentWaveNumber - 1) * HPMultiplierPerWave;
				SpawnedEnemy->WaveSpeedMultiplier = 1.0f + (CurrentWaveNumber - 1) * SpeedMultiplierPerWave;

				SpawnedEnemy->FinishSpawning(SpawnTransform);

				// subscribe to the death delegate
				SpawnedEnemy->OnEnemyDied.AddDynamic(this, &ACombatEnemySpawner::OnEnemyDied);
			}
		}
	}
}

void ACombatEnemySpawner::OnEnemyDied()
{
	// decrease the spawn counter
	--SpawnCount;

	// is this the last enemy we should spawn?
	if (SpawnCount <= 0)
	{
		// schedule the activation on depleted message
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnerDepleted, ActivationDelay);
		return;
	}

	// schedule the next enemy spawn
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnEnemy, RespawnDelay);
}

void ACombatEnemySpawner::SpawnerDepleted()
{
	// process the actors to activate list
	for (AActor* CurrentActor : ActorsToActivateWhenDepleted)
	{
		// check if the actor is activatable
		if (ICombatActivatable* CombatActivatable = Cast<ICombatActivatable>(CurrentActor))
		{
			// activate the actor
			CombatActivatable->ActivateInteraction(this);
		}
	}

	// notify any external listeners (e.g. a wave manager) that this spawner is depleted
	OnSpawnerDepleted.Broadcast(this);
}

void ACombatEnemySpawner::ResetSpawner()
{
	// clear any pending spawn/depletion timer
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);

	// restore the enemy counter and allow ActivateInteraction to fire again
	bShouldSpawnEnemiesImmediately = false;
	SpawnCount = InitialSpawnCount;
	bHasBeenActivated = false;

	UE_LOG(LogTemp, Log, TEXT("Spawner resetlendi"));
}

void ACombatEnemySpawner::SetSpawnCount(int32 NewCount)
{
	// clamp to at least one enemy, and update the reset baseline so ResetSpawner() restores this value
	SpawnCount = InitialSpawnCount = FMath::Max(1, NewCount);

	UE_LOG(LogTemp, Log, TEXT("SpawnCount %d olarak ayarlandi"), SpawnCount);
}

void ACombatEnemySpawner::SetWaveScaling(int32 WaveNumber, float InHPMultiplierPerWave, float InSpeedMultiplierPerWave)
{
	CurrentWaveNumber = WaveNumber;
	HPMultiplierPerWave = InHPMultiplierPerWave;
	SpeedMultiplierPerWave = InSpeedMultiplierPerWave;
}

void ACombatEnemySpawner::SetAllowedEnemyClasses(const TArray<TSubclassOf<ACombatEnemy>>& NewAllowedClasses)
{
	AllowedEnemyClasses = NewAllowedClasses;
}

void ACombatEnemySpawner::ToggleInteraction(AActor* ActivationInstigator)
{
	// stub
}

void ACombatEnemySpawner::ActivateInteraction(AActor* ActivationInstigator)
{
	// ensure we're only activated once, and only if we've deferred enemy spawning
	if (bHasBeenActivated || bShouldSpawnEnemiesImmediately)
	{
		return;
	}

	// raise the activation flag
	bHasBeenActivated = true;

	// spawn the first enemy
	SpawnEnemy();
}

void ACombatEnemySpawner::DeactivateInteraction(AActor* ActivationInstigator)
{
	// stub
}
