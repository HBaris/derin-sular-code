// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DSHUDWidget.generated.h"

class ADSWaveManager;
class UProgressBar;
class UTextBlock;
class UBorder;

/**
 *  Dalga (wave) hayatta kalma HUD'u: dalga numarasi, kalan dusman sayisi,
 *  oyuncu cani, guven (Trust) ve skor icin hem C++ UMG gorsel arayuzu hem de
 *  BlueprintImplementableEvent kancalarini saglar.
 */
UCLASS()
class UDSHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** BP tarafinda gorsel guncelleme icin: dalga numarasi degisince cagrilir */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void OnWaveNumberChanged(int32 YeniDalgaNumarasi);

	/** BP tarafinda gorsel guncelleme icin: kalan dusman sayisi degisince cagrilir */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void OnRemainingEnemiesChanged(int32 KalanDusmanSayisi);

	/** BP tarafinda gorsel guncelleme icin: oyuncu cani degisince cagrilir (0-1 arasi yuzde) */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void OnPlayerHealthChanged(float CanYuzdesi);

	/** BP tarafinda gorsel guncelleme icin: skor degisince cagrilir */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void OnScoreChanged(int32 YeniSkor);

	/** BP tarafinda gorsel guncelleme icin: guven degeri (DSTrustComponent) degisince cagrilir */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void UpdateTrustDisplay(float TrustValue, const FString& TrustName);

	/** BP tarafinda gorsel guncelleme icin: oyun bitince (olum veya kazanma) gosterilecek mesaj (Asama 10) */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD")
	void ShowGameOverMessage(const FString& Mesaj);

protected:

	/** Eger WBP icinde gorsel agac bos ise otomatik C++ UMG HUD bilesenlerini insa eder */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Initialization: DSWaveManager/GameMode delegelerine baglanir, yoklama zamanlayicisini kurar */
	virtual void NativeConstruct() override;

	/** Cleanup: delegate binding'lerini ve zamanlayiciyi kaldirir */
	virtual void NativeDestruct() override;

	/** Surekli guncellenen bir delege olmayan degerleri (oyuncu cani, kalan dusman sayisi) periyodik olarak yoklar */
	void RefreshPolledValues();

	/** DSWaveManager::OnWaveChanged'e baglanir */
	UFUNCTION()
	void HandleWaveChanged(int32 WaveNumber, int32 HedefDusmanSayisiPerSpawner);

	/** ACombatGameMode::OnScoreChanged'e baglanir */
	UFUNCTION()
	void HandleScoreChanged(int32 YeniSkor);

	/** ACombatGameMode::TrustComponent->OnTrustChanged'e baglanir */
	UFUNCTION()
	void HandleTrustChanged(float YeniTrustDegeri, FString TrustIsmi);

	/** ACombatGameMode::OnPlayerDied'e baglanir */
	UFUNCTION()
	void HandlePlayerDied();

	/** ACombatGameMode::OnGameWon'a baglanir */
	UFUNCTION()
	void HandleGameWon();

protected:

	/** Test amacli: GEngine->AddOnScreenDebugMessage ile degerleri ekrana yaz */
	UPROPERTY(EditDefaultsOnly, Category="HUD|Debug")
	bool bShowDebugMessages = false;

	/** Yoklama araligi (oyuncu cani / kalan dusman sayisi icin, bunlarin ozel bir "degisti" delegesi yok) */
	UPROPERTY(EditDefaultsOnly, Category="HUD", meta = (ClampMin = 0.05, Units = "s"))
	float PollAraligi = 0.2f;

	/** RefreshPolledValues'i tetikleyen zamanlayici */
	FTimerHandle PollTimer;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthTextWidget;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> TrustBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrustTextWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveTextWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RemainingTextWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreTextWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> GameOverBannerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GameOverTextWidget;
};
