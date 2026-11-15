// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "CombatEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "StateTreeAsyncExecutionContext.h"
#include "EngineUtils.h"
#include "DSVillager.h"
#include "DSBarikat.h"

bool FStateTreeCharacterGroundedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// is the character currently grounded?
	bool bCondition = InstanceData.Character->GetMovementComponent()->IsMovingOnGround();

	return InstanceData.bMustBeOnAir ? !bCondition : bCondition;
}

#if WITH_EDITOR
FText FStateTreeCharacterGroundedCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Is Character Grounded</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

bool FStateTreeIsInDangerCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// ensure we have a valid enemy character
	if (InstanceData.Character)
	{
		// is the last detected danger event within the reaction threshold?
		const float ReactionDelta = InstanceData.Character->GetWorld()->GetTimeSeconds() - InstanceData.Character->GetLastDangerTime();

		if (ReactionDelta < InstanceData.MaxReactionTime && ReactionDelta > InstanceData.MinReactionTime)
		{
			// do a dot product check to determine if the danger location is within the character's detection cone
			const FVector DangerDir = (InstanceData.Character->GetLastDangerLocation() - InstanceData.Character->GetActorLocation()).GetSafeNormal2D();

			const float DangerDot = FVector::DotProduct(DangerDir, InstanceData.Character->GetActorForwardVector());
			const float ConeAngleCos = FMath::Cos(FMath::DegreesToRadians(InstanceData.DangerSightConeAngle));

			return DangerDot > ConeAngleCos;
		}
	}

	return false;
}

#if WITH_EDITOR
FText FStateTreeIsInDangerCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Is Character In Danger</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeComboAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);


	// tell the character to do a combo attack
	InstanceData.Character->DoAIComboAttack();

	return EStateTreeRunStatus::Running;
}

void FStateTreeComboAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.Unbind();
}

#if WITH_EDITOR
FText FStateTreeComboAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Do Combo Attack</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeChargedAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);

	// tell the character to do a charged attack
	InstanceData.Character->DoAIChargedAttack();

	return EStateTreeRunStatus::Running;
}

void FStateTreeChargedAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.Unbind();
}

#if WITH_EDITOR
FText FStateTreeChargedAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Do Charged Attack</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeWaitForLandingTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on enemy landed delegate
	InstanceData.Character->OnEnemyLanded.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);

	return EStateTreeRunStatus::Running;
}

void FStateTreeWaitForLandingTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on enemy landed delegate
	InstanceData.Character->OnEnemyLanded.Unbind();
}

#if WITH_EDITOR
FText FStateTreeWaitForLandingTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Wait for Landing</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeFaceActorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the AI Controller's focus
	InstanceData.Controller->SetFocus(InstanceData.ActorToFaceTowards);

	return EStateTreeRunStatus::Running;
}

void FStateTreeFaceActorTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// clear the AI Controller's focus
	InstanceData.Controller->ClearFocus(EAIFocusPriority::Gameplay);
}

#if WITH_EDITOR
FText FStateTreeFaceActorTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Face Towards Actor</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeFaceLocationTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the AI Controller's focus
	InstanceData.Controller->SetFocalPoint(InstanceData.FaceLocation);

	return EStateTreeRunStatus::Running;
}

void FStateTreeFaceLocationTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// clear the AI Controller's focus
	InstanceData.Controller->ClearFocus(EAIFocusPriority::Gameplay);
}

#if WITH_EDITOR
FText FStateTreeFaceLocationTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Face Towards Location</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeSetCharacterSpeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the character's max ground speed
	InstanceData.Character->GetCharacterMovement()->MaxWalkSpeed = InstanceData.Speed;

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeSetCharacterSpeedTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Set Character Speed</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

namespace
{
	/** Zombi bu mesafe icindeki oyuncuyu koylu fallback'inden once dogrudan hedef alir;
	 *  disindaysa (ve zorla aggro aktif degilse) once Oncelik 2'deki koylu adayina bakilir */
	constexpr float OyuncuOncelikMesafesi = 400.0f;
}

EStateTreeRunStatus FStateTreeGetPlayerInfoTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!IsValid(InstanceData.Character))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Same-frame infinite loop protection: if evaluated > 3 times in the same engine frame, throttle and fail safely
	const uint32 CurrentFrame = GFrameNumber;
	if (InstanceData.LastEvalFrame == CurrentFrame)
	{
		InstanceData.SameFrameEvalCount++;
		if (InstanceData.SameFrameEvalCount > 3)
		{
			return EStateTreeRunStatus::Failed;
		}
	}
	else
	{
		InstanceData.LastEvalFrame = CurrentFrame;
		InstanceData.SameFrameEvalCount = 1;
	}
		
	// reset the selected target
	ACharacter* SelectedTarget = nullptr;

	// Ciglik ile zorla aggro tetiklenmisse (Cigirtkan cigligi duyulmussa), normal MaxRange
	// kontrolunu atlayip oyuncuyu her mesafeden hedef al - yeni StateTree node'u gerekmeden
	// mevcut GetPlayerInfo akisini kullanir
	const ACombatEnemy* EnemyCharacter = Cast<ACombatEnemy>(InstanceData.Character.Get());
	const bool bZorlaAggro = EnemyCharacter && EnemyCharacter->IsZorlaAggroActive();
	const float EffectiveMaxRange = bZorlaAggro ? TNumericLimits<float>::Max() : InstanceData.MaxRange;

	const FVector SelfLocation = InstanceData.Character->GetActorLocation();

	// Oncelik 1a: her yerel oyuncuyu dene, EffectiveMaxRange icinde olanlar arasindan bir aday sec
	// (rastgele hedef degistirme davranisi korunuyor); bu asamada henuz SelectedTarget'a atamiyoruz -
	// once asagidaki 400 birimlik oncelik esigini kontrol etmemiz gerekiyor
	ACharacter* DetectedPlayer = nullptr;
	float DetectedPlayerDist = 0.0f;

	const int32 NumPlayers = UGameplayStatics::GetNumLocalPlayerControllers(InstanceData.Character);

	for (int32 i = 0; i < NumPlayers; ++i)
	{
		if (ACharacter* Current = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(InstanceData.Character, i)))
		{
			// compute the distance to the target
			const float TargetDist = (Current->GetActorLocation() - SelfLocation).Size();

			// is this target within range?
			if (TargetDist < EffectiveMaxRange)
			{
				// have we selected a valid candidate already?
				if (DetectedPlayer)
				{
					// randomly switch to the new candidate
					if (FMath::RandBool())
					{
						DetectedPlayer = Current;
						DetectedPlayerDist = TargetDist;
					}
				}
				else
				{
					// no valid candidate yet, so choose this one
					DetectedPlayer = Current;
					DetectedPlayerDist = TargetDist;
				}
			}
		}
	}

	// Oncelik 1b: oyuncu OyuncuOncelikMesafesi (400 birim) icindeyse ya da zorla aggro aktifse
	// (ciglik duyulmus, mesafeden bagimsiz kovalama) dogrudan oyuncuyu hedef al; degilse Oncelik
	// 2'deki koylu fallback'ine firsat tanimak icin oyuncuyu simdilik secmiyoruz
	if (DetectedPlayer && (bZorlaAggro || DetectedPlayerDist <= OyuncuOncelikMesafesi))
	{
		SelectedTarget = DetectedPlayer;
	}

	// Oncelik 2: oyuncu 400 birim icinde degilse (veya hic algilanmadiysa) en yakin (baygin/olu
	// olmayan) DSVillager'i hedef al. ADSVillager bir ACharacter oldugundan current_Target
	// (ACharacter*) slotuna StateTree'ye (current_Target, Move To, Face Towards Actor, Object is
	// Valid) hic dokunmadan atanabilir.
	if (!SelectedTarget)
	{
		float NearestVillagerDist = EffectiveMaxRange;

		for (TActorIterator<ADSVillager> It(InstanceData.Character->GetWorld()); It; ++It)
		{
			ADSVillager* Villager = *It;

			// baygin veya gercekten olmus koyluler hedef alinmaz
			if (Villager->bIsDowned || Villager->CurrentHP <= 0.0f)
			{
				continue;
			}

			const float Dist = (Villager->GetActorLocation() - SelfLocation).Size();

			if (Dist < NearestVillagerDist)
			{
				NearestVillagerDist = Dist;
				SelectedTarget = Villager;
			}
		}
	}

	// Oncelik 1c: koylu bulunamadiysa (ya da yoksa) ama menzil icinde algilanmis, 400 birim disinda
	// kalan bir oyuncu varsa hicbir hedef secmemek yerine yine de o oyuncuya donulur
	if (!SelectedTarget && DetectedPlayer)
	{
		SelectedTarget = DetectedPlayer;
	}

	// set the new target
	InstanceData.TargetPlayerCharacter = SelectedTarget;

	if (SelectedTarget)
	{
		// set the target location and distance
		InstanceData.TargetPlayerLocation = SelectedTarget->GetActorLocation();
		InstanceData.DistanceToTarget = (SelectedTarget->GetActorLocation() - SelfLocation).Size();

		// Cigirtkan: oyuncuyu/koyluyu basariyla algiladik, ciglik yetenegini dene (tag/cooldown kontrolu fonksiyon icinde).
		// Zorla-aggro modunda algilamayi sinirsiz menzil sagladigi icin ciglik atilmaz; aksi halde ciglik
		// duyan bir Cigirtkan cooldown sonrasi kendi konumundan tekrar ciglik atip zincirlemeyi tum haritaya
		// yayabilirdi (runaway cascade). Sadece gercek/organik MaxRange ici algilamada ciglik tetiklenir.
		if (!bZorlaAggro)
		{
			if (ACombatEnemy* EnemyChar = Cast<ACombatEnemy>(InstanceData.Character.Get()))
			{
				EnemyChar->TryCiglikYetenegi(InstanceData.TargetPlayerLocation);
			}
		}

		return EStateTreeRunStatus::Succeeded;
	}

	// Oncelik 3: oyuncu ve koylu da yoksa en yakin DSBarikat'i hedef al. ADSBarikat bir ACharacter
	// DEGIL (bkz. Asama 7 karari), current_Target'a atanamaz - sadece konum/mesafe cikisi doldurulur.
	// StateTree tarafinda bu konum-bazli hedefi tuketecek bir dal (Move To Location / Face Towards
	// Location, mevcut task tipleri) editorde elle eklenmelidir - bkz. Asama 9 notlari.
	ADSBarikat* NearestBarikat = nullptr;
	float NearestBarikatDist = EffectiveMaxRange;

	for (TActorIterator<ADSBarikat> It(InstanceData.Character->GetWorld()); It; ++It)
	{
		ADSBarikat* Barikat = *It;

		if (Barikat->CurrentHP <= 0.0f)
		{
			continue;
		}

		const float Dist = (Barikat->GetActorLocation() - SelfLocation).Size();

		if (Dist < NearestBarikatDist)
		{
			NearestBarikatDist = Dist;
			NearestBarikat = Barikat;
		}
	}

	if (NearestBarikat)
	{
		InstanceData.TargetPlayerLocation = NearestBarikat->GetActorLocation();
		InstanceData.DistanceToTarget = NearestBarikatDist;

		if (NearestBarikatDist <= 300.0f)
		{
			if (ACombatEnemy* EnemyChar = Cast<ACombatEnemy>(InstanceData.Character.Get()))
			{
				FVector DirToBarikat = (NearestBarikat->GetActorLocation() - SelfLocation);
				DirToBarikat.Z = 0.0f;
				if (!DirToBarikat.IsNearlyZero())
				{
					EnemyChar->SetActorRotation(DirToBarikat.Rotation());
				}

				InstanceData.TargetPlayerLocation = SelfLocation;
				InstanceData.DistanceToTarget = 25.0f;

				if (!EnemyChar->IsAttacking())
				{
					EnemyChar->DoAIComboAttack();
				}
			}
		}

		return EStateTreeRunStatus::Succeeded;
	}

	// Hicbir gecerli character veya barikat hedefi yok
	return EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FStateTreeGetPlayerInfoTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Get Player Info</b>");
}
#endif // WITH_EDITOR