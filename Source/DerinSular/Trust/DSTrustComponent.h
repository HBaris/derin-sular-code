// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSTrustComponent.generated.h"

/** Guven degeri degistiginde (yeni deger + esik ismiyle birlikte) yayinlanir */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrustChanged, float, YeniTrustDegeri, FString, TrustIsmi);

/**
 *  Koyun oyuncuya duydugu guveni tutan bilesen (GDD 4: Guven Sistemi) - M1 icin sadelestirilmis:
 *  tek sayisal deger (0-100) + 5 isim esigi. ACombatGameMode'a eklenir (Score gibi merkezi durum
 *  tutucu zaten orada - ayri bir DSVillageState aktoru M1'de gereksiz karmasiklik).
 *
 *  Mevcut OnPlayerDied/OnGameWon/OnScoreChanged delege pattern'i (isimlendirme ve stil) birebir
 *  taklit edilir.
 */
UCLASS(ClassGroup=(DerinSular), meta=(BlueprintSpawnableComponent))
class UDSTrustComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UDSTrustComponent();

	/** Guven degistiginde (yeni deger + esik ismiyle) yayinlanir */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnTrustChanged OnTrustChanged;

	/** Guvenin alabilecegi asgari deger */
	UPROPERTY(EditDefaultsOnly, Category="Trust")
	float MinTrust = 0.0f;

	/** Guvenin alabilecegi azami deger */
	UPROPERTY(EditDefaultsOnly, Category="Trust")
	float MaxTrust = 100.0f;

	/** Guveni Miktar kadar degistirir (Min/MaxTrust arasinda clamp'lenir), Sebep'i loglar ve deger degisirse OnTrustChanged'i yayinlar */
	UFUNCTION(BlueprintCallable, Category="Trust")
	void AddTrust(float Miktar, const FString& Sebep);

	/** Mevcut guven degeri */
	UFUNCTION(BlueprintCallable, Category="Trust")
	float GetTrustValue() const { return TrustValue; }

	/** Mevcut guven degerine karsilik gelen esik ismini dondurur (GDD 4: Yabanci/Tanidik/Ortak/Muttefik/Aile) */
	UFUNCTION(BlueprintCallable, Category="Trust")
	FString GetTrustName() const;

protected:

	/** Su anki guven degeri (0-100). Baslangic: 20.0 - "Yabanci" esiginin bir ustu (GDD 4) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trust")
	float TrustValue = 20.0f;
};
