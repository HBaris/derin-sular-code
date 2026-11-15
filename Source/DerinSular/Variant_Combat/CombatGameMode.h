// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameMode.generated.h"

class UDSTrustComponent;

/** Oyuncu oldugunde yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

/** Kazanma kosulu (KazanmaIcinGerekliDalgaSayisi) saglaninca yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameWon);

/** Skor degistiginde (yeni toplam skorla birlikte) yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, YeniSkor);

/**
 *  Simple GameMode for a third person combat game
 */
UCLASS(abstract)
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	ACombatGameMode();

	/** Oyuncu oldugunde yayinlanir; ACombatCharacter::HandleDeath'ten NotifyPlayerDied araciligiyla tetiklenir */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnPlayerDied OnPlayerDied;

	/** Kazanma kosulu saglaninca yayinlanir */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnGameWon OnGameWon;

	/** Skor degistiginde yayinlanir */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnScoreChanged OnScoreChanged;

	/** Su anki toplam skor */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 Score = 0;

	/** Koyun oyuncuya duydugu guveni tutan bilesen (GDD 4: Guven Sistemi) - Score gibi merkezi burada tutulur */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trust")
	TObjectPtr<UDSTrustComponent> TrustComponent;

	/** Bu sayida dalga tamamlaninca OnGameWon yayinlanir */
	UPROPERTY(EditDefaultsOnly, Category="Win Condition", meta = (ClampMin = 1))
	int32 KazanmaIcinGerekliDalgaSayisi = 10;

	/** Bariscil vitrin modu (model inceleme): acikken zombi StateTree'si baslamaz, ilk dalga otomatik baslamaz, koyluler savasmaz */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bPeacefulShowcaseMode = false;

	/** WorldContext'in dunyasindaki ACombatGameMode'un bPeacefulShowcaseMode degeri; CombatGameMode yoksa false */
	static bool IsPeacefulShowcaseMode(const UObject* WorldContext);

	/** Oyuncu oldugunde cagirilir (orn. ACombatCharacter::HandleDeath'ten); OnPlayerDied'i yayinlar */
	UFUNCTION(BlueprintCallable, Category="Events")
	void NotifyPlayerDied();

	/** DSWaveManager::OnWaveCompleted'e baglanir; kazanma kosulunu kontrol eder */
	UFUNCTION(BlueprintCallable, Category="Events")
	void NotifyWaveCompleted(int32 TamamlananWaveNumarasi);

	/** DSWaveManager::OnWaveCompleted'e NotifyWaveCompleted ile birlikte baglanir; hayatta kalan DSVillager sayisina gore TrustComponent'i gunceller (GDD 4, sonuc-bazli guven) */
	UFUNCTION(BlueprintCallable, Category="Events")
	void NotifyTrustUpdate(int32 TamamlananWaveNumarasi);

	/** Skoru Amount kadar artirir ve OnScoreChanged'i yayinlar (orn. zombi olumunde ACombatEnemy'den cagirilir) */
	UFUNCTION(BlueprintCallable, Category="Score")
	void AddScore(int32 Amount);

	/** UGameplayStatics::OpenLevel ile su anki level'i yeniden yukler (basit restart, PIE prefix'i dogru sekilde ele alinir) */
	UFUNCTION(BlueprintCallable, Category="Events")
	void RestartCurrentLevel();

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;

	/** OnGameWon'un yalnizca bir kez tetiklenmesini saglar */
	bool bOyunKazanildi = false;

	/** Olum + kazanma ust uste gelirse ikinci OpenLevel zamanlanmasin diye */
	bool bRestartScheduled = false;

	/** RestartDelaySeconds sonra RestartCurrentLevel'i cagiran zamanlayici */
	FTimerHandle RestartTimer;

	/** Yeniden baslatmayi bir kez zamanlar; zaten zamanlanmissa false doner */
	bool ScheduleLevelRestart();
};
