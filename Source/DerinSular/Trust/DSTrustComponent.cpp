// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSTrustComponent.h"

UDSTrustComponent::UDSTrustComponent()
{
	// sadece bir deger tutuyor, tick gerektiren bir davranisi yok
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSTrustComponent::AddTrust(float Miktar, const FString& Sebep)
{
	const float OncekiDeger = TrustValue;

	TrustValue = FMath::Clamp(TrustValue + Miktar, MinTrust, MaxTrust);

	UE_LOG(LogTemp, Log, TEXT("[DSTrust] %+.0f | Sebep: %s | Yeni deger: %.1f (%s)"), Miktar, *Sebep, TrustValue, *GetTrustName());

	if (TrustValue != OncekiDeger)
	{
		OnTrustChanged.Broadcast(TrustValue, GetTrustName());
	}
}

FString UDSTrustComponent::GetTrustName() const
{
	if (TrustValue >= 80.0f)
	{
		return TEXT("Aile");
	}
	if (TrustValue >= 60.0f)
	{
		return TEXT("Muttefik");
	}
	if (TrustValue >= 40.0f)
	{
		return TEXT("Ortak");
	}
	if (TrustValue >= 20.0f)
	{
		return TEXT("Tanidik");
	}
	return TEXT("Yabanci");
}
