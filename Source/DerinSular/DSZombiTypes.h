// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSZombiTypes.generated.h"

/**
 *  Zombi tipi tanim verisi. Zombi cesitleri kod yerine DataTable satirlariyla tanimlanir:
 *  editorde bu struct'i Row Structure olarak kullanan bir DataTable olusturulur
 *  (onerilen ad: DT_ZombiTipleri) ve her tip bir satir olur.
 *
 *  GDD'deki 6 tip icin ornek satirlar (CSV import formati, ilk kolon satir adi):
 *
 *  ---,HP,MoveSpeed,Damage,TintColor,AbilityTag
 *  Aylak,3,300,1,"(R=0.2,G=0.8,B=0.2,A=1)",
 *  Kosucu,2,600,1,"(R=0.9,G=0.6,B=0.1,A=1)",
 *  Siskin,6,200,2,"(R=0.5,G=0.7,B=0.2,A=1)","(TagName=""Zombi.Yetenek.Patlama"")"
 *  Cigirtkan,2,350,0.5,"(R=0.8,G=0.2,B=0.8,A=1)","(TagName=""Zombi.Yetenek.Ciglik"")"
 *  Zirhli,10,250,2,"(R=0.4,G=0.4,B=0.5,A=1)","(TagName=""Zombi.Yetenek.ZirhAzaltma"")"
 *  Drifter,4,450,1.5,"(R=0.2,G=0.5,B=0.9,A=1)","(TagName=""Zombi.Yetenek.FazGecis"")"
 *
 *  Notlar:
 *  - Satir adlarinda ve tag'lerde Turkce karakter kullanilmaz (Kosucu, Siskin, Cigirtkan, Zirhli).
 *  - AbilityTag'lerin import edilebilmesi icin once Config/DefaultGameplayTags.ini
 *    (veya Project Settings > GameplayTags) icinde tanimlanmis olmalari gerekir.
 *  - Aylak'in yetenegi yoktur; AbilityTag bos birakilir.
 */
USTRUCT(BlueprintType)
struct FZombiTipiVerisi : public FTableRowBase
{
	GENERATED_BODY()

	/** Maksimum can puani */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombi Tipi", meta = (ClampMin = 1))
	float HP = 3.0f;

	/** Maksimum yurume hizi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombi Tipi", meta = (ClampMin = 0, Units = "cm/s"))
	float MoveSpeed = 300.0f;

	/** Yakin dovus saldirisinin verdigi hasar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombi Tipi", meta = (ClampMin = 0))
	float Damage = 1.0f;

	/** Gorsel ayirt edicilik icin mesh tint rengi (BP tarafinda dynamic material instance'a uygulanir) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombi Tipi")
	FLinearColor TintColor = FLinearColor::White;

	/** Tipin ozel yetenegini tanimlayan etiket (orn. Zombi.Yetenek.Patlama); yetenegi olmayan tipte (Aylak, Kosucu) bos birakilir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombi Tipi")
	FGameplayTag AbilityTag;
};

/**
 *  DataTable'dan zombi tipi verisi okumak icin yardimci fonksiyonlar.
 *  Hem C++ hem Blueprint tarafindan kullanilabilir; veriyi dusmana uygulama isi
 *  cagirana aittir (bkz. ROADMAP.md - CombatEnemy entegrasyonu sonraki adim).
 */
UCLASS()
class UDSZombiTipiKutuphanesi : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  Verilen tablodan TipAdi satirini okur.
	 *  Tablo gecersizse veya satir bulunamazsa false doner ve uyari loglar;
	 *  OutVeri bu durumda struct varsayilanlarinda kalir.
	 */
	UFUNCTION(BlueprintCallable, Category="Zombi Tipi")
	static bool ZombiTipiVerisiYukle(const UDataTable* ZombiTablosu, FName TipAdi, FZombiTipiVerisi& OutVeri);
};
