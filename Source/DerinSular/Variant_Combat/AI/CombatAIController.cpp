// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatAIController.h"
#include "Components/StateTreeAIComponent.h"
#if WITH_EDITOR
#include "StateTree.h"
#include "UObject/UObjectIterator.h"
#endif

ACombatAIController::ACombatAIController()
{
	// create the StateTree AI Component
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	// StateTree'yi otomatik baslatma; baslatma zamanini kendimiz kontrol ediyoruz
	// (born-dead sorunu icin asagida OnPossess'teki aciklamaya bakin)
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);

	// ensure we're attached to the possessed character.
	// this is necessary for EnvQueries to work correctly
	bAttachToPawn = true;
}

void ACombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("ZDBG OnPossess pawn=%s BegunPlay=%d"), *GetNameSafe(InPawn), InPawn ? (int32)InPawn->HasActorBegunPlay() : -1);

	// --- BORN-DEAD SORUNU VE COZUMU ---
	//
	// Sorun: ACombatEnemy, AutoPossessAI = PlacedInWorldOrSpawned kullandigi icin
	// possession, pawn'un BeginPlay'inden ONCE gerceklesir
	// (APawn::PostInitializeComponents -> SpawnDefaultController -> OnPossess).
	// Eskiden burada dogrudan StateTreeAI->StartLogic() cagriliyordu; StateTree,
	// ACombatEnemy::BeginPlay icindeki "CurrentHP = MaxHP" atamasindan once
	// calismaya baslayip CurrentHP'yi 0 olarak okuyabiliyordu. Agacin Dead
	// gecis kosulu (CurrentHP <= 0) ilk secimde tutuyor, dusman daha dogarken
	// Dead state'ine dusuyor ve agac orada kalici olarak bitiyordu.
	//
	// YAKLASIM A (burada uygulanan): StartLogic'i HP init sonrasina ertele.
	// Pawn BeginPlay'ini henuz tamamlamadiysa burada hicbir sey yapmayiz;
	// ACombatEnemy::BeginPlay, HP'yi doldurduktan sonra StartStateTreeLogic()
	// cagirir. Boylece agac ilk degerlendirmesini her zaman gecerli HP ile yapar
	// ve StateTree asset'ine dokunmak gerekmez. Pawn zaten BeginPlay'ini
	// tamamlamissa (orn. sonradan repossess) hemen baslatmak guvenlidir.
	//
	// YAKLASIM B (alternatif): Dead kosuluna init guard'i eklemek.
	// CombatStateTreeUtility'de "Character->HasActorBegunPlay() && CurrentHP <= 0"
	// dondururen ozel bir FStateTreeCondition yazip StateTree asset'indeki Dead
	// gecisine baglamak da ayni belirtiyi giderirdi. Ancak bu, asil nedeni
	// (agacin yanlis siralamayla erken baslamasi) yerinde birakir, ayni guard'in
	// HP okuyan diger tum kosullara da eklenmesini gerektirir ve StateTree
	// asset'inde elle duzenleme ister. Bu yuzden A tercih edildi.
	if (InPawn && InPawn->HasActorBegunPlay())
	{
		StartStateTreeLogic();
	}
}

void ACombatAIController::StartStateTreeLogic()
{
#if WITH_EDITOR
	for (TObjectIterator<UStateTree> It; It; ++It)
	{
		if (UStateTree* ST = *It)
		{
			if (!ST->HasAnyFlags(RF_ClassDefaultObject) && !ST->IsReadyToRun())
			{
				ST->CompileIfChanged();
			}
		}
	}
#endif

	UE_LOG(LogTemp, Warning, TEXT("ZDBG StartLogic oncesi HasBegunPlay=%d IsRegistered=%d"),
		StateTreeAI ? (int32)StateTreeAI->HasBegunPlay() : -1,
		StateTreeAI ? (int32)StateTreeAI->IsRegistered() : -1);

	// StartLogic zaten calisan bir agacta yeniden baslatma yapmaz, tekrarli cagri guvenlidir
	StateTreeAI->StartLogic();

	// enum sirasi (StateTreeExecutionTypes.h): 0=Running 1=Stopped 2=Succeeded 3=Failed 4=Unset
	UE_LOG(LogTemp, Warning, TEXT("ZDBG StartLogic sonrasi durum=%d (0=Running 1=Stopped 2=Succeeded 3=Failed 4=Unset)"), (int32)StateTreeAI->GetStateTreeRunStatus());

	// bir tick sonra tekrar olc: pending start var mi, yoksa gercekten olu mu?
	// WeakObjectPtr kullaniliyor: level restart sirasinda controller destroy edilirse
	// raw "this" capture'i freed memory'e erisip crash'e yol acabilirdi.
	TWeakObjectPtr<ACombatAIController> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			ACombatAIController* Self = WeakThis.Get();
			if (!Self || !Self->StateTreeAI)
			{
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("ZDBG [1 tick sonra] %s durum=%d HasBegunPlay=%d"),
				*GetNameSafe(Self->GetPawn()), (int32)Self->StateTreeAI->GetStateTreeRunStatus(), (int32)Self->StateTreeAI->HasBegunPlay());

			if (Self->StateTreeAI->GetStateTreeRunStatus() != EStateTreeRunStatus::Running)
			{
				UE_LOG(LogTemp, Warning, TEXT("ZDBG [1 tick sonra] durum Running degil, StartLogic yeniden deneniyor"));
				Self->StateTreeAI->StartLogic();
				UE_LOG(LogTemp, Warning, TEXT("ZDBG [retry sonrasi] durum=%d"), (int32)Self->StateTreeAI->GetStateTreeRunStatus());
			}
		});
}