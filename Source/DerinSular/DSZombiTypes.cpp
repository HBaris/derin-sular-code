// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSZombiTypes.h"

bool UDSZombiTipiKutuphanesi::ZombiTipiVerisiYukle(const UDataTable* ZombiTablosu, FName TipAdi, FZombiTipiVerisi& OutVeri)
{
	// varsayilanlardan basla; satir bulunamazsa cagiran gecerli degerlerle kalir
	OutVeri = FZombiTipiVerisi();

	if (!IsValid(ZombiTablosu))
	{
		UE_LOG(LogTemp, Warning, TEXT("ZombiTipiVerisiYukle: zombi tablosu atanmamis"));
		return false;
	}

	// tablonun dogru row struct'ini kullandigini dogrula
	if (ZombiTablosu->GetRowStruct() != FZombiTipiVerisi::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("ZombiTipiVerisiYukle: %s tablosunun Row Structure'i ZombiTipiVerisi degil"), *ZombiTablosu->GetName());
		return false;
	}

	const FZombiTipiVerisi* Satir = ZombiTablosu->FindRow<FZombiTipiVerisi>(TipAdi, TEXT("ZombiTipiVerisiYukle"));

	if (!Satir)
	{
		UE_LOG(LogTemp, Warning, TEXT("ZombiTipiVerisiYukle: '%s' satiri %s tablosunda bulunamadi"), *TipAdi.ToString(), *ZombiTablosu->GetName());
		return false;
	}

	OutVeri = *Satir;
	return true;
}
