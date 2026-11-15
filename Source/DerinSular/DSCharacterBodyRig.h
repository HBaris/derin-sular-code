// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ObjectKey.h"
#include "EngineUtils.h"

/**
 * Silah / Dövüş Stili: Oyuncunun 1-6 veya Q tuşları ile değiştirebildiği silahlar ve kendine has saldırı animasyonları
 */
enum class EDSCombatWeaponStyle : uint8
{
	BattleAxe = 0,   // [1] Çift Yüzlü Savaş Baltası: Geniş yatay/çapraz biçme kombosu
	SurvivalAxe,     // [2] Hayatta Kalma Baltası: Hızlı taktiksel doğrama kombosu
	MedievalAxe,     // [3] Ortaçağ Şövalye Baltası: Uzun menzilli ileri kesme
	WarHammer,       // [4] Ağır Savaş Çekici: Yukarıdan aşağıya ezici balyoz indirme
	UnarmedFists,    // [5] Yumruk / Silahsız: Sol Direk -> Sağ Kroşe -> Aparkat boks kombosu
	LivingDarkSword, // [6] Kadim Orman Kılıcı (Treeguard Canlı Kütük / Living Log ile üretilen yarı-büyülü silah)
	OneHandedAxe = SurvivalAxe,
	LongSword = MedievalAxe
};

/**
 * UE5 Robot Mannequin ("skinsuit") görünümünü tamamen gizleyip yerine gerçek 3B İnsan (Hero, Çiftçi, Yaşlı Sakallı Oduncu-Savaşçı, İşçi),
 * Boss (Ayçiçeği Tarlasının Koruyucusu, Treeguard Orman Muhafızı) ve 3B Zombi parçalı gövde modellerini
 * iskelet kemiklerine gerçek zamanlı bağlayan yardımcı yapı.
 */
/**
 * Koylu Emir Modu: Oyuncunun V tusu ile koylulere (Sakalli Savascilar ve diger koyluler) verdigi anlik emir
 */
enum class EDSVillagerCombatOrder : uint8
{
	FightOrder = 0,       // SAVAS EMRI: Koyluler kacmaz, ellerindeki balta/silahla zombilere ve dusmanlara saldirir
	WorkAndDefendOrder,   // CALIS & KORU: Sakalli savascilar yakin tehdide saldirir, isciler kaynak toplar / korunur
	RetreatOrder          // GERI CEKIL: Tum koyluler guvenli kamp atesine cekilir
};

/**
 * Faz 3 (Denizcilik): 5 Gemi Sinifi Evrimi
 */
enum class EDSShipClass : uint8
{
	Raft        = 0, // Sinif 1: Ahsap Sal (Baslangic Kiyi Kesfi, Kapasite: 8, Govde: 100 HP)
	FishingBoat = 1, // Sinif 2: Balikci Teknesi (Kiyi Balikciligi & Tuz Tasima, Kapasite: 16, Govde: 150 HP)
	Sloop       = 2, // Sinif 3: Korsan Salupasi (Cift Yelken & Pruva Zipkin Topu, Kapasite: 28, Govde: 220 HP)
	CargoVessel = 3, // Sinif 4: Zirhli Nakliye Gemisi (Adalar Arasi Agir Cevher & Tuz Sevkiyati, Kapasite: 45, Govde: 320 HP)
	Flagship    = 4  // Sinif 5: Derin Sular Amiral Gemisi (Firtina & Dev Balina Avcisi Kalyon, Kapasite: 75, Govde: 480 HP)
};

/**
 * V2 Evren, Koloni, Boss, Ilahi Terazi ve Ahlak Durumu (merkezi durum yoneticisi;
 * UPROPERTY / StateTree bellek duzenini degistirmeden tum siniflar arasinda canli baglanti saglar).
 * Ornek UDSWorldStateSubsystem icinde yasar: her level yuklemesinde (restart, yeni PIE) sifirdan olusur.
 * Bariscil vitrin modu artik ACombatGameMode::bPeacefulShowcaseMode'da.
 */
struct FDSColonyAndWorldState
{
	EDSVillagerCombatOrder VillagerOrder = EDSVillagerCombatOrder::RetreatOrder;
	int32 WoodCount = 12;
	int32 SunflowerOilCount = 0;
	int32 ForestWrathCounter = 0;
	int32 YoungTreesCutCount = 0;
	int32 UnplantedTreeCuts = 0;
	int32 LastMentorWaveNumber = 0;
	int32 DivineFavorScore = 0;
	float DivineFavor = 55.0f;

	bool bTreeguardAwakened = false;
	bool bTreeguardDefeated = false;
	bool bLivingLogUnlocked = false;
	bool bSunflowerAllied = false;
	bool bSunflowerBossAllied = false;
	bool bSunflowerProductionUnlocked = false;
	bool bHiddenCityPortalOpened = false;
	bool bMasterCombatUnlocked = false;
	bool bShipyardConstructionStarted = false;
	bool bBoatBuildingStarted = false;
	float BoatBuildProgress = 0.0f;
	int32 FleetShipCount = 1;           // Baslangicta iskelede 1 kesif teknesi hazirdir; [G] ile Gemi Filosuna donusur
	int32 MaritimeExpeditionCount = 0;  // [B] veya iskelede [E] ile yapilan Derin Sular kesif seferi sayisi
	int32 SeaBountyCount = 0;           // Suda yasamdan / kesiflerden toplanan deniz bereketi & erzak
	bool bNomadFleetDoctrine = false;   // Kucuk koloni + Gemi Filosuyla Kesif doktrini aktif mi
	bool bCastleDefenseChosen = false;

	bool bOiledBootsEquipped = false;             // Aycicegi Boss drop: Sivi Yag Surulmus Ayakkabi (+%35 Hareket Hizi)
	bool bSteamAchTabanlariYaglamak = false;      // Basarim: "TABANLARI YAGLAMAK"
	FString ActiveAchievementBannerText = TEXT("");
	float ActiveAchievementBannerTimer = 0.0f;

	bool bSunflowerMinionsSpawned = false;        // Aycicegi Boss faz gecisi: Minik Aycicegi yancilari
	bool bSunflowerFatherlySweepDone = false;     // "HAYIR ONLAR BENIM BANA BIRAKIN" babacan sahnesi

	bool bPirateBossAllied = false;               // Korsan Boss muttefik oldu mu
	bool bPirateSeaMasteryUnlocked = false;       // Daha Guclu Olta & Daha Guclu Balik Agi acildi mi
	bool bTwoHandedSwordMasteryUnlocked = false;  // Korsan Boss'tan Cift Elli Kilic Ustaligi ogrenildi mi (oncesinde Tek Elli Kilic)
	bool bPlayerBlockingWithSword = false;        // [F] Cift Elli Kilic Blok Durusu
	float PlayerParryWindowEndTime = -1.0f;       // Kusursuz Savusturma (Parry) penceresi
	bool bRiposteReady = false;                   // Parry sonrasi 2.5x Kritik Riposte hazir mi
	bool bSwainEasterEggTriggered = false;
	bool bKnightSpawned = false;
	bool bKnightDuelActive = false;
	bool bWanderingKnightDefeated = false;
	bool bWanderingKnightDuelActive = false;
	bool bReturningVillagerPending = false;
	float PlayerOilSlowEndTime = -1.0f;

	// ============================================================================================
	// FAZ 1 (KOLONI TEMELI): FIZIKSEL MADENCILIK, DUNYA LOOT'U, SIRTCANTASI, DEPO & SANTIYE
	// ============================================================================================

	// 1. Oyuncu Sirt Cantasi Envanteri (Yerden [E] ile toplanan fiziksel ganimetler; sandiga/santiyeye tasinir)
	int32 CarriedWood = 0;            // Kesilen agactan yere dusen kutuk
	int32 CarriedIronOre = 0;         // Demir damarindan (OreVein_Iron) cikan demir cevheri
	int32 CarriedStone = 0;           // Kaya ve maden damarindan cikan tas
	int32 CarriedSalt = 0;            // Kiyi tuz kayasindan (OreVein_Salt) cikan tuz kristali
	int32 CarriedLivingLog = 0;       // Treeguard'dan dusen Canli Kutuk (Living Log)
	int32 CarriedGuardianHorn = 0;    // Kadim Labirent Bekcisi'nden dusen Muhafiz Boynuzu
	int32 CarriedMagmaCore = 0;       // Yer Alti / Labirent Bekcisi'nden dusen Magma Cekirdegi
	int32 CarriedSunEssence = 0;      // Aycicegi Boss'tan alinan Gunes Ozu
	int32 CarriedWhaleBlubber = 0;    // Faz 3: Dev Balinadan alinan Balina Yagi
	int32 CarriedAmbergris = 0;       // Faz 3: Dev Balinadan cikan Kadim Ambergris
	int32 MaxCarryCapacity = 24;      // Sirt cantasi tasima kapasitesi

	// ============================================================================================
	// FAZ 2 (KADIM TARIFLER + BOSS FAZLARI): BOSS GANIMETLERI, YENI TARIF AGACI, TEZGAH URETIMI VE KUTUPHANE YEDEGI
	// ============================================================================================
	int32 StoredLivingLog = 0;
	int32 StoredGuardianHorn = 0;
	int32 StoredMagmaCore = 0;
	int32 StoredSunEssence = 0;
	int32 StoredWhaleBlubber = 0;
	int32 StoredAmbergris = 0;

	bool bAncientGuardianDefeated = false;
	bool bRecipeLivingDarkSword = false;  // Kadim Tarif #1: Kadim Karanlik Kilic (Canli Kutuk + Celik -> +%35 Hasar & Can Calma)
	bool bRecipeGuardianArmor = false;    // Kadim Tarif #2: Kadim Muhafiz Zirhi (Muhafiz Boynuzu + Magma Cekirdegi -> +%40 Zirh)
	bool bRecipeSunOilElixir = false;     // Kadim Tarif #3: Gunes Yagi Iksiri (Gunes Ozu + Tuz -> Can Yenileme & +%36 Hiz)
	bool bRecipePirateSalupa = false;     // Kadim Tarif #4: Korsan Salupa & Zipkin Donanimi (Faz 3 Denizcilik Hazirligi)

	bool bCraftedLivingDarkSword = false;
	bool bCraftedGuardianArmor = false;
	bool bCraftedSunOilElixir = false;
	int32 ArchivedRecipesCount = 0;       // GDD Bolum 16: Koloni Kutuphanesinde kitaba yazilip olumden korunan tarif sayisi

	TMap<FObjectKey, int32> BossPhaseMap;               // Boss Faz Takibi (1: %100-65, 2: %65-30, 3: <%30)
	TMap<FObjectKey, float> BossArmorBrokenEndTimeMap;  // Sutuna carpan veya [4] Balyozla zirhi kirilan Boss penceresi

	// ============================================================================================
	// FAZ 3 (DENIZCILIK): 5 GEMI SINIFI, DEV BALINA AVI VE FILO MADENCILIGI
	// ============================================================================================
	EDSShipClass CurrentShipClass = EDSShipClass::Raft; // 0: Sal, 1: Balikci Teknesi, 2: Salupa, 3: Nakliye Gemisi, 4: Amiral Gemisi
	float ShipHullIntegrity = 100.0f;
	float MaxShipHullIntegrity = 100.0f;
	int32 HarpoonPowerLevel = 1;          // Zipkin Firlatma Gucu (1..5)
	int32 FleetCargoCapacity = 8;         // Sefer basina adalardan tasinan maden/tuz kapasitesi
	float WhaleCurrentHP = 300.0f;        // Derin Sular koyundaki Dev Balina (LeviathanWhale) cani
	float WhaleMaxHP = 300.0f;
	int32 WhalesHuntedCount = 0;          // Avlanan Dev Balina sayisi
	int32 ActiveFleetRouteIndex = 0;      // 0: Tuzlu Mercan Adasi, 1: Demir Kayaliklari Adasi, 2: Obsidyen Firtina Adasi
	int32 FleetConvoyCompletedCount = 0;  // Tamamlanan Adalar Arasi Filo Madenciligi Seferi sayisi
	bool bFleetAutoMiningActive = true;   // Filo adalar arasi otomatik maden/tuz sevkiyati yapiyor mu
	float NextFleetConvoyTime = 14.0f;    // Bir sonraki otomatik filo konvoyu varis zamani

	// ============================================================================================
	// FAZ 4 (KARANLIK YOL, VICDAN SECIMLERI, ISIRIK MUAYENESI VE OLUMSUZ ANI NPC'LERI)
	// ============================================================================================
	bool bNeighborAllianceFormed = false;       // [E] Komsu Koy ile Tuz & Silah Ittifaki kuruldu mu
	bool bDarkPathChosen = false;               // [X] Karanlik Yol: Komsu Koyu basip Usta Demirci/Ciftci kacirildi mi
	bool bWeaponsSaleRefused = false;           // [Z] Silah satisi reddedildi -> Caresiz kalan koy karsi saldiriya gecti mi
	bool bNeighborRevengeRaidTriggered = false; // Kacirma veya red sonrasi Komsu Koy Intikam/Yagma Baskini
	int32 KidnappedBlacksmithCount = 0;         // Zorla koloniye getirilen Demirci/Ciftci sayisi
	int32 DarkPathActionsCount = 0;             // Yapilan Karanlik Yol secimlerinin gercek dunya sonucu sayaci
	int32 BiteInspectionsResolvedCount = 0;     // Vicdan tarzi Gizli Isirik Muayenesi cozumu sayisi
	int32 MemoryArchiveCount = 0;               // Yasayan Ani Arsivi (0..3: Akil Hocasi Baba, Dede, Anneanne)
	bool bMemorySanctuaryBlessed = false;       // Korunakli Ani Kosesi Ocagi kutsamasi aktif mi

	// 2. Koloni Depolari & Tuz Ambari (Koydeki sandiklara ve Tuz Ambarina teslim edilen stoklar)
	int32 StoredIronOre = 4;          // Demirci Cevher Sandigi stogu
	int32 StoredSteelIngots = 0;      // 3 Kisilik Demir Ocaginda dovulen Celik Kulcesi
	int32 StoredStone = 6;            // Insaat tasi stogu
	int32 StoredSalt = 10;            // Tuz Ambari (GDD 7: Para birimi + et/balik tuzlama)
	int32 StoredDriedMeat = 8;        // Tuzlanmis kurutulmus erzak

	// 3. Koloni Insaat Santiyesi (Taslak -> Malzeme Tedariki -> Cekicle Insa -> Aktif Istasyon)
	int32 ActiveBuildStage = 0;       // 0: 3 Kisilik Demirci Ocagi, 1: Kiyi Tuzlasi, 2: Koloni Kutuphanesi, 3: Tamami Insa Edildi
	float BuildSiteProgress = 0.0f;   // Mevcut santiyenin insa yuzdesi (0..100)
	int32 BuildSiteDepositedWood = 0;
	int32 BuildSiteDepositedIron = 0;
	int32 BuildSiteDepositedStone = 0;
	bool bBlacksmithForgeBuilt = false; // 3 Kisilik Demir Ocagi tamamlandi mi (+%30 Celik Silah Hasari & Celik Dokumu)
	bool bSalternBuilt = false;         // Kiyi Tuzlasi tamamlandi mi (Duzenli Tuz & Erzak uretimi)
	bool bColonyLibraryBuilt = false;   // Kutuphane tamamlandi mi (GDD 16: Usta ve Boss tarifleri kalici yedeklenir)

	// 4. Koylu Koloni Meslek Modu (True ise koyluler tehlike yokken agac keser, maden kazar ve santiyeye tasir)
	bool bVillagerGatheringJobsActive = true;
	int32 TotalOreMinedCount = 0;
	int32 TotalLootPickedUpCount = 0;

	// Maden damarlari ve agaclarin kalan can haritasi
	TMap<FObjectKey, float> ResourceNodeHPMap;

	/** [6]/[F] kilici kusanilabilir mi: Treeguard'dan Canli Kutuk, uretilmis Kadim Kilic veya Korsan Boss ustaligi */
	bool IsSwordUnlocked() const
	{
		return bLivingLogUnlocked || bCraftedLivingDarkSword || bTwoHandedSwordMasteryUnlocked;
	}

	int32 GetTotalCarriedItems() const
	{
		return CarriedWood + CarriedIronOre + CarriedStone + CarriedSalt + CarriedLivingLog + CarriedGuardianHorn + CarriedMagmaCore + CarriedSunEssence + CarriedWhaleBlubber + CarriedAmbergris;
	}

	FString GetShipClassShortName() const
	{
		switch (CurrentShipClass)
		{
		case EDSShipClass::Raft:        return TEXT("1.Sal");
		case EDSShipClass::FishingBoat: return TEXT("2.BalikciTeknesi");
		case EDSShipClass::Sloop:       return TEXT("3.Salupa");
		case EDSShipClass::CargoVessel: return TEXT("4.NakliyeGemisi");
		case EDSShipClass::Flagship:    return TEXT("5.AmiralGemisi");
		default:                        return TEXT("1.Sal");
		}
	}

	FString GetShipClassDisplayName() const
	{
		switch (CurrentShipClass)
		{
		case EDSShipClass::Raft:        return TEXT("Sinif 1/5: Ahsap Sal (Kapasite:8 | Zipkin:1)");
		case EDSShipClass::FishingBoat: return TEXT("Sinif 2/5: Balikci Teknesi (Kapasite:16 | Zipkin:2)");
		case EDSShipClass::Sloop:       return TEXT("Sinif 3/5: Korsan Salupasi (Kapasite:28 | Zipkin:3)");
		case EDSShipClass::CargoVessel: return TEXT("Sinif 4/5: Zirhli Nakliye Gemisi (Kapasite:45 | Zipkin:4)");
		case EDSShipClass::Flagship:    return TEXT("Sinif 5/5: Derin Sular Amiral Gemisi (Kapasite:75 | Zipkin:5)");
		default:                        return TEXT("Sinif 1/5: Ahsap Sal");
		}
	}

	FString GetActiveFleetRouteName() const
	{
		switch (ActiveFleetRouteIndex % 3)
		{
		case 0:  return TEXT("Tuzlu Mercan Adasi (Kaya Tuzu & Balik)");
		case 1:  return TEXT("Demir Kayaliklari Adasi (Demir Cevheri & Celik)");
		default: return TEXT("Obsidyen Firtina Adasi (Celik, Tuz & Ambergris)");
		}
	}

	int32 GetUnlockedRecipesCount() const
	{
		return (bRecipeLivingDarkSword ? 1 : 0) + (bRecipeGuardianArmor ? 1 : 0) + (bRecipeSunOilElixir ? 1 : 0) + (bRecipePirateSalupa ? 1 : 0);
	}

	int32 GetCraftedBossGearCount() const
	{
		return (bCraftedLivingDarkSword ? 1 : 0) + (bCraftedGuardianArmor ? 1 : 0) + (bCraftedSunOilElixir ? 1 : 0);
	}

	FString GetActiveBuildSiteName() const
	{
		switch (ActiveBuildStage)
		{
		case 0: return TEXT("3 Kisilik Demirci Ocagi (Gereken: 4 Odun, 2 Demir, 2 Tas)");
		case 1: return TEXT("Kiyi Tuzlasi & Tuz Ambari (Gereken: 4 Odun, 2 Tas)");
		case 2: return TEXT("Koloni Kutuphanesi (Gereken: 5 Odun, 3 Tas)");
		default: return TEXT("Tum Koloni Istasyonlari Tamamlandi (Demir Ocagi + Tuzla + Kutuphane Aktif)");
		}
	}

	FString ActiveNotificationText = TEXT("");
	float ActiveNotificationTimer = 0.0f;

	FString GuideTitle = TEXT("AKIL HOCASI (KEHANET REHBERI) — KOLONI & SAVAS KOMUTASI");
	FString GuideBody  = TEXT("[V] Koylu Emri: SAVAS (Sakalli Savascilar & Koyluler Saldirir) | [E] Tetkik / Kurtar / Portal | [C] Fidan Dik | [G/H] Gemi & Kale");
	FLinearColor GuideColor = FLinearColor(1.0f, 0.82f, 0.28f, 1.0f);
	bool bGuideDirty = true;

	TMap<FObjectKey, float> PersonalTrustMap;
	TMap<FObjectKey, float> VillagerPersonalTrustMap;
	TMap<FObjectKey, bool> HiddenBiteSuspectMap;
	TMap<FObjectKey, bool> BiteInspectedMap;

	void PushNotification(const FString& InText, float InDurationSeconds = 5.5f)
	{
		ActiveNotificationText = InText;
		ActiveNotificationTimer = InDurationSeconds;
		GuideBody = InText;
		bGuideDirty = true;
	}

	void UnlockSteamAchievement(const FString& AchCodeName, const FString& AchDescription, float BannerDuration = 9.0f)
	{
		ActiveAchievementBannerText = FString::Printf(TEXT("BASARIM ACILDI: \"%s\" — %s"), *AchCodeName, *AchDescription);
		ActiveAchievementBannerTimer = BannerDuration;
	}

	/** WorldContext'in dunyasindaki durum (UDSWorldStateSubsystem); dunya/subsystem yoksa nullptr. Tanim: DSWorldStateSubsystem.cpp */
	static FDSColonyAndWorldState* Find(const UObject* WorldContext);

	/** Find ile ayni; dunya yoksa her cagrida sifirlanan bos bir durum dondurur (yazilanlar kaybolur, okunanlar varsayilan). Tanim: DSWorldStateSubsystem.cpp */
	static FDSColonyAndWorldState& Get(const UObject* WorldContext);

	void ResetForNewLevel()
	{
		VillagerOrder = EDSVillagerCombatOrder::FightOrder;
		WoodCount = 12;
		SunflowerOilCount = 0;
		ForestWrathCounter = 0;
		DivineFavorScore = 0;
		bTreeguardAwakened = false;
		bLivingLogUnlocked = false;
		bSunflowerAllied = false;
		bSunflowerProductionUnlocked = false;
		bHiddenCityPortalOpened = false;
		bMasterCombatUnlocked = false;
		bShipyardConstructionStarted = false;
		bSwainEasterEggTriggered = false;
		bKnightSpawned = false;
		bKnightDuelActive = false;
		bReturningVillagerPending = false;
		PlayerOilSlowEndTime = -1.0f;
		PersonalTrustMap.Empty();
		HiddenBiteSuspectMap.Empty();
		BiteInspectedMap.Empty();
		ResourceNodeHPMap.Empty();
		StoredLivingLog    += CarriedLivingLog;
		StoredGuardianHorn += CarriedGuardianHorn;
		StoredMagmaCore    += CarriedMagmaCore;
		StoredSunEssence   += CarriedSunEssence;

		CarriedWood = 0;
		CarriedIronOre = 0;
		CarriedStone = 0;
		CarriedSalt = 0;
		CarriedLivingLog = 0;
		CarriedGuardianHorn = 0;
		CarriedMagmaCore = 0;
		CarriedSunEssence = 0;
		CarriedWhaleBlubber = 0;
		CarriedAmbergris = 0;
		StoredIronOre = 4;
		StoredSteelIngots = 0;
		StoredStone = 6;
		StoredSalt = 10;
		StoredDriedMeat = 8;
		ActiveBuildStage = 0;
		BuildSiteProgress = 0.0f;
		bBlacksmithForgeBuilt = false;
		bSalternBuilt = false;
		bColonyLibraryBuilt = false;
		CurrentShipClass = EDSShipClass::Raft;
		ShipHullIntegrity = 100.0f;
		MaxShipHullIntegrity = 100.0f;
		HarpoonPowerLevel = 1;
		FleetCargoCapacity = 8;
		WhaleCurrentHP = 300.0f;
		WhaleMaxHP = 300.0f;
		WhalesHuntedCount = 0;
		ActiveFleetRouteIndex = 0;
		FleetConvoyCompletedCount = 0;
		bFleetAutoMiningActive = true;
		NextFleetConvoyTime = 14.0f;
		bNeighborAllianceFormed = false;
		bDarkPathChosen = false;
		bWeaponsSaleRefused = false;
		bNeighborRevengeRaidTriggered = false;
		KidnappedBlacksmithCount = 0;
		DarkPathActionsCount = 0;
		BiteInspectionsResolvedCount = 0;
		MemoryArchiveCount = 0;
		bMemorySanctuaryBlessed = false;
		SetGuideBanner(
			TEXT("AKIL HOCASI (KEHANET REHBERI) — KOLONI & SAVAS KOMUTASI"),
			TEXT("[V] Koylu Emri | [E] Loot Topla / Depo / Santiye Insa / Kurtar | [4] Cekic ile Maden Kaz | [N] Santiye | [J] Meslek"),
			FLinearColor(1.0f, 0.82f, 0.28f, 1.0f)
		);
	}

	// Yere gercek 3B fiziksel loot paketi (Kutuk, Demir Cevheri, Tuz Kristali, Tas, Canli Kutuk) dusurur (Dunya Ganimeti Sistemi)
	static AStaticMeshActor* SpawnPhysicalWorldLoot(UWorld* World, const FVector& BaseLoc, FName LootTypeTag, int32 ScatterIndex = 0)
	{
		if (!World)
		{
			return nullptr;
		}
		const TCHAR* MeshPath = TEXT("/Game/Variant_Combat/Env/SM_WoodLog.SM_WoodLog");
		const TCHAR* MatPath  = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeatheredWood.M_Env_WeatheredWood");
		FVector LootScale(0.32f, 0.32f, 0.32f);
		FLinearColor GlowColor(0.95f, 0.72f, 0.25f);

		if (LootTypeTag == FName("Loot_IronOre"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_DarkStone.M_Env_DarkStone");
			LootScale = FVector(0.28f, 0.26f, 0.22f);
			GlowColor = FLinearColor(1.0f, 0.45f, 0.15f);
		}
		else if (LootTypeTag == FName("Loot_Salt"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_Moss.SM_ForestRock_Moss");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_VillagePlaster.M_Env_VillagePlaster");
			LootScale = FVector(0.25f, 0.25f, 0.20f);
			GlowColor = FLinearColor(0.75f, 0.95f, 1.0f);
		}
		else if (LootTypeTag == FName("Loot_Stone"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_MossyRock.M_Env_MossyRock");
			LootScale = FVector(0.24f, 0.24f, 0.18f);
			GlowColor = FLinearColor(0.70f, 0.75f, 0.68f);
		}
		else if (LootTypeTag == FName("Loot_LivingLog"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_WoodLog.SM_WoodLog");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Char_BossTreeguard_PBR.M_Char_BossTreeguard_PBR");
			LootScale = FVector(0.42f, 0.38f, 0.38f);
			GlowColor = FLinearColor(0.20f, 1.0f, 0.42f);
		}
		else if (LootTypeTag == FName("Loot_GuardianHorn"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeaponSteel.M_Env_WeaponSteel");
			LootScale = FVector(0.22f, 0.22f, 0.52f);
			GlowColor = FLinearColor(0.62f, 0.28f, 1.0f);
		}
		else if (LootTypeTag == FName("Loot_MagmaCore"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_01.SM_ForestRock_01");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_DarkStone.M_Env_DarkStone");
			LootScale = FVector(0.30f, 0.30f, 0.28f);
			GlowColor = FLinearColor(1.0f, 0.25f, 0.05f);
		}
		else if (LootTypeTag == FName("Loot_SunEssence"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_Sunflower_Crown.SM_Sunflower_Crown");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR");
			LootScale = FVector(0.32f, 0.32f, 0.32f);
			GlowColor = FLinearColor(1.0f, 0.85f, 0.15f);
		}
		else if (LootTypeTag == FName("Loot_WhaleBlubber"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_Prop_OilTin.SM_Prop_OilTin");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Env_VillagePlaster.M_Env_VillagePlaster");
			LootScale = FVector(0.35f, 0.35f, 0.35f);
			GlowColor = FLinearColor(0.15f, 0.88f, 1.0f);
		}
		else if (LootTypeTag == FName("Loot_Ambergris"))
		{
			MeshPath  = TEXT("/Game/Variant_Combat/Env/SM_ForestRock_Moss.SM_ForestRock_Moss");
			MatPath   = TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR");
			LootScale = FVector(0.30f, 0.30f, 0.30f);
			GlowColor = FLinearColor(1.0f, 0.72f, 0.18f);
		}
		else
		{
			// Loot_Wood (Kutuk)
			LootScale = FVector(0.36f, 0.28f, 0.28f);
			GlowColor = FLinearColor(0.95f, 0.68f, 0.22f);
		}

		UStaticMesh* LMesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		if (!LMesh)
		{
			LMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		if (!LMesh)
		{
			return nullptr;
		}

		const bool bIsPierWhaleLoot = (LootTypeTag == FName("Loot_WhaleBlubber") || LootTypeTag == FName("Loot_Ambergris"));
		const float Angle = float(ScatterIndex) * 2.15f + (bIsPierWhaleLoot ? 0.0f : FMath::FRandRange(-0.35f, 0.35f));
		const float Radius = bIsPierWhaleLoot ? (float(ScatterIndex) * 14.0f) : (42.0f + float(ScatterIndex % 3) * 26.0f);
		float GroundZ = 200.0f;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, FVector(BaseLoc.X, BaseLoc.Y, 450.0f), FVector(BaseLoc.X, BaseLoc.Y, -200.0f), ECC_WorldStatic))
		{
			GroundZ = FMath::Clamp(Hit.ImpactPoint.Z, 180.0f, 235.0f);
		}
		const float UseZ = bIsPierWhaleLoot
			? BaseLoc.Z
			: (GroundZ + 14.0f);
		const FVector SpawnLoc(BaseLoc.X + FMath::Cos(Angle) * Radius, BaseLoc.Y + FMath::Sin(Angle) * Radius, UseZ);
		const FRotator SpawnRot(8.0f, Angle * 57.3f, 0.0f);

		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* LootAct = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (LootAct && LootAct->GetStaticMeshComponent())
		{
			UStaticMeshComponent* SMC = LootAct->GetStaticMeshComponent();
			SMC->SetMobility(EComponentMobility::Movable);
			SMC->SetStaticMesh(LMesh);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SMC->SetCastShadow(true);
			const FBoxSphereBounds B = LMesh->GetBounds();
			const float MaxD = FMath::Max(FMath::Max3(B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z) * 2.0f, 1.0f);
			const float TargetCm = (LootTypeTag == FName("Loot_Wood") || LootTypeTag == FName("Loot_LivingLog")) ? 68.0f : (bIsPierWhaleLoot ? 54.0f : 38.0f);
			const float S = FMath::Clamp(TargetCm / MaxD, 0.05f, 10.0f);
			SMC->SetWorldScale3D(FVector(S) * (LootScale / 0.30f));

			if (UMaterialInterface* LMat = LoadObject<UMaterialInterface>(nullptr, MatPath))
			{
				const int32 NumM = FMath::Max(1, SMC->GetNumMaterials());
				for (int32 m = 0; m < NumM; ++m)
				{
					SMC->SetMaterial(m, LMat);
				}
			}

			LootAct->Tags.AddUnique(FName("WorldLoot"));
			LootAct->Tags.AddUnique(LootTypeTag);

			if (UPointLightComponent* LootGlow = NewObject<UPointLightComponent>(LootAct, TEXT("WorldLootBeaconLight")))
			{
				LootGlow->SetupAttachment(SMC);
				LootGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));
				LootGlow->SetIntensity(1400.0f);
				LootGlow->SetLightColor(GlowColor);
				LootGlow->SetAttenuationRadius(145.0f);
				LootGlow->SetCastShadows(false);
				LootGlow->RegisterComponent();
			}
		}
		return LootAct;
	}

	bool TryPickupWorldLoot(AActor* LootActor)
	{
		if (!LootActor || LootActor->IsHidden())
		{
			return false;
		}
		FString ItemLabel = TEXT("Kutuk (+2)");
		if (LootActor->ActorHasTag(FName("Loot_IronOre")))
		{
			CarriedIronOre += 2;
			ItemLabel = TEXT("Demir Cevheri (+2)");
		}
		else if (LootActor->ActorHasTag(FName("Loot_Salt")))
		{
			CarriedSalt += 2;
			ItemLabel = TEXT("Tuz Kristali (+2)");
		}
		else if (LootActor->ActorHasTag(FName("Loot_Stone")))
		{
			CarriedStone += 2;
			ItemLabel = TEXT("Insaat Tasi (+2)");
		}
		else if (LootActor->ActorHasTag(FName("Loot_LivingLog")))
		{
			CarriedLivingLog += 1;
			bLivingLogUnlocked = true;
			bRecipeLivingDarkSword = true;
			ItemLabel = TEXT("CANLI KUTUK (Living Log)! [KADIM TARIF ACILDI: Kadim Karanlik Kilic (+%35 Hasar & Can Calma)]");
		}
		else if (LootActor->ActorHasTag(FName("Loot_GuardianHorn")))
		{
			CarriedGuardianHorn += 1;
			bRecipeGuardianArmor = true;
			ItemLabel = TEXT("MUHAFIZ BOYNUZU (Guardian Horn)! [KADIM TARIF ACILDI: Kadim Muhafiz Zirhi (+%40 Hasar Azaltma)]");
		}
		else if (LootActor->ActorHasTag(FName("Loot_MagmaCore")))
		{
			CarriedMagmaCore += 1;
			bRecipeGuardianArmor = true;
			ItemLabel = TEXT("MAGMA CEKIRDEGI (Magma Core)! [KADIM TARIF ACILDI: Kadim Muhafiz Zirhi & Sismik Balyoz]");
		}
		else if (LootActor->ActorHasTag(FName("Loot_SunEssence")))
		{
			CarriedSunEssence += 1;
			bRecipeSunOilElixir = true;
			ItemLabel = TEXT("GUNES OZU (Sun Essence)! [KADIM TARIF ACILDI: Gunes Yagi Iksiri & Bereket Tohumu]");
		}
		else if (LootActor->ActorHasTag(FName("Loot_WhaleBlubber")))
		{
			CarriedWhaleBlubber += 3;
			StoredDriedMeat += 2;
			ItemLabel = TEXT("BALINA YAGI & DENIZ ETI (+3 Balina Yagi, +2 Tuzlu Erzak — Fener Yagi & Gemi Bakimi)!");
		}
		else if (LootActor->ActorHasTag(FName("Loot_Ambergris")))
		{
			CarriedAmbergris += 1;
			bRecipePirateSalupa = true;
			ItemLabel = TEXT("KADIM AMBERGRIS (+1)! [DERIN SULAR HAZINESI: Filo Madenciligi Verimi +%50 & Salupa Tarifi Acildi]");
		}
		else
		{
			CarriedWood += 2;
			ItemLabel = TEXT("Yas Kereste / Kutuk (+2)");
		}

		TotalLootPickedUpCount += 1;
		LootActor->SetActorHiddenInGame(true);
		LootActor->SetActorEnableCollision(false);
		LootActor->Tags.Remove(FName("WorldLoot"));

		PushNotification(FString::Printf(
			TEXT("FIZIKSEL LOOT TOPLANDI: %s | Sirt Cantasi: %d/%d (Odun:%d Demir:%d Tuz:%d BalinaYagi:%d) — Sandiga [E] ile birak!"),
			*ItemLabel, GetTotalCarriedItems(), MaxCarryCapacity, CarriedWood, CarriedIronOre, CarriedSalt, CarriedWhaleBlubber), 5.5f);
		return true;
	}

	bool DepositCarriedToColonyStorage()
	{
		const int32 TotalCarried = GetTotalCarriedItems();
		if (TotalCarried <= 0)
		{
			PushNotification(FString::Printf(
				TEXT("KOLONI AMBARI & TUZ HAZINESI: Odun: %d | Demir: %d | Celik: %d | Tas: %d | Tuz: %d | BalinaYagi: %d (Sirt cantan bos!)"),
				WoodCount, StoredIronOre, StoredSteelIngots, StoredStone, StoredSalt, StoredWhaleBlubber), 5.5f);
			return false;
		}

		const int32 DepWood    = CarriedWood + CarriedLivingLog * 4;
		const int32 DepIron    = CarriedIronOre;
		const int32 DepStone   = CarriedStone;
		const int32 DepSalt    = CarriedSalt + CarriedAmbergris * 3;
		const int32 DepBlubber = CarriedWhaleBlubber;
		const int32 DepAmber   = CarriedAmbergris;

		WoodCount          += DepWood;
		StoredIronOre      += DepIron;
		StoredStone        += DepStone;
		StoredSalt         += DepSalt;
		StoredWhaleBlubber += DepBlubber;
		StoredAmbergris    += DepAmber;
		StoredDriedMeat    += DepBlubber;

		CarriedWood = 0;
		CarriedIronOre = 0;
		CarriedStone = 0;
		CarriedSalt = 0;
		CarriedLivingLog = 0;
		CarriedWhaleBlubber = 0;
		CarriedAmbergris = 0;

		if (bBlacksmithForgeBuilt && StoredIronOre >= 2)
		{
			const int32 Smelted = StoredIronOre / 2;
			StoredIronOre %= 2;
			StoredSteelIngots += Smelted;
		}
		if (bSalternBuilt && StoredSalt >= 2)
		{
			StoredDriedMeat += 2;
		}

		PushNotification(FString::Printf(
			TEXT("KOLONI AMBARINA TESLIM EDILDI (+%d Odun, +%d Demir, +%d Tuz, +%d Balina Yagi)! Ambar -> Odun:%d | Demir:%d | Celik:%d | Tuz:%d | BalinaYagi:%d"),
			DepWood, DepIron, DepSalt, DepBlubber, WoodCount, StoredIronOre, StoredSteelIngots, StoredSalt, StoredWhaleBlubber), 6.5f);
		return true;
	}

	bool ContributeToBuildSite(UWorld* World)
	{
		if (ActiveBuildStage >= 3)
		{
			PushNotification(TEXT("KOLONI SANTIYESI: Tum istasyonlar (3 Kisilik Demir Ocagi + Kiyi Tuzlasi + Koloni Kutuphanesi) insa edildi ve aktif!"), 5.5f);
			return false;
		}

		// Once sirt cantasindaki malzemeleri ambara/santiyeye aktar
		if (GetTotalCarriedItems() > 0)
		{
			WoodCount     += CarriedWood + CarriedLivingLog * 4;
			StoredIronOre += CarriedIronOre;
			StoredStone   += CarriedStone;
			StoredSalt    += CarriedSalt;
			CarriedWood = CarriedIronOre = CarriedStone = CarriedSalt = CarriedLivingLog = 0;
		}

		// Insaat adimi basina 2 Odun + 1 Tas (veya Demir) tuketilir
		if (WoodCount < 2 && StoredStone < 1 && StoredIronOre < 1)
		{
			PushNotification(FString::Printf(
				TEXT("SANTIYE MALZEMESI EKSIK (%s): En az 2 Odun ve 1 Tas/Demir gerekli! Agac kes veya [4] Cekic ile maden damarini kaz."),
				*GetActiveBuildSiteName()), 5.5f);
			return false;
		}

		WoodCount     = FMath::Max(0, WoodCount - 2);
		StoredStone   = FMath::Max(0, StoredStone - 1);
		if (ActiveBuildStage == 0 && StoredIronOre > 0)
		{
			StoredIronOre = FMath::Max(0, StoredIronOre - 1);
		}

		BuildSiteProgress = FMath::Clamp(BuildSiteProgress + 50.0f, 0.0f, 100.0f);
		const FString CurrentStationName = GetActiveBuildSiteName();

		if (BuildSiteProgress >= 100.0f)
		{
			BuildSiteProgress = 0.0f;
			const int32 CompletedStage = ActiveBuildStage;
			ActiveBuildStage += 1;

			if (CompletedStage == 0)
			{
				bBlacksmithForgeBuilt = true;
				StoredSteelIngots += 2;
				PushNotification(TEXT("3 KISILIK DEMIRCI OCAGI INSA EDILDI! (Cok Kisili Koloni Istasyonu: 3 Demirci calisiyor -> Celik Silah Hasari +%30, Celik Dokumu Aktif!)"), 7.5f);
			}
			else if (CompletedStage == 1)
			{
				bSalternBuilt = true;
				StoredSalt += 6;
				StoredDriedMeat += 4;
				PushNotification(TEXT("KIYI TUZLASI & TUZ AMBARI INSA EDILDI! (GDD 7 Tuz Ekonomisi: Deniz suyundan gunluk Tuz ve Tuzlanmis Erzak uretimi basladi!)"), 7.5f);
			}
			else if (CompletedStage == 2)
			{
				bColonyLibraryBuilt = true;
				PushNotification(TEXT("KOLONI KUTUPHANESI INSA EDILDI! (GDD 16: Ustalarin ve Boss'larin ogrettigi tum tarifler kitaba yazildi, usta olse bile bilgi kaybolmaz!)"), 8.0f);
			}
		}
		else
		{
			PushNotification(FString::Printf(
				TEXT("KOLONI SANTIYE INSASI: %s — Insaat Ilerlemesi: %.0f%% (Malzeme yerlestirildi ve cekiclendi, tamamlamak icin [E] veya [4] Cekic vur!)"),
				*CurrentStationName, BuildSiteProgress), 6.0f);
		}
		return true;
	}

	// 1.2sn Gorsel Alan Saldirisi Telgraf Halkasi (BossTelegraphRing)
	static AStaticMeshActor* SpawnRemnantTelegraphRing(UWorld* World, const FVector& CenterLoc, float Radius, const FLinearColor& RingColor)
	{
		if (!World)
		{
			return nullptr;
		}
		UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!CylMesh)
		{
			return nullptr;
		}
		float GroundZ = CenterLoc.Z;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, FVector(CenterLoc.X, CenterLoc.Y, 450.0f), FVector(CenterLoc.X, CenterLoc.Y, -200.0f), ECC_WorldStatic))
		{
			GroundZ = FMath::Clamp(Hit.ImpactPoint.Z, 185.0f, 235.0f);
		}
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* RingAct = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(CenterLoc.X, CenterLoc.Y, GroundZ + 3.0f), FRotator::ZeroRotator, SP);
		if (RingAct && RingAct->GetStaticMeshComponent())
		{
			UStaticMeshComponent* SMC = RingAct->GetStaticMeshComponent();
			SMC->SetMobility(EComponentMobility::Movable);
			SMC->SetStaticMesh(CylMesh);
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SMC->SetCastShadow(false);
			const float ScaleXY = FMath::Clamp(Radius / 50.0f, 1.5f, 12.0f);
			SMC->SetWorldScale3D(FVector(ScaleXY, ScaleXY, 0.24f));
			if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Boss_Sunflower_PBR.M_Boss_Sunflower_PBR")))
			{
				SMC->SetMaterial(0, BaseMat);
			}
			RingAct->Tags.AddUnique(FName("BossTelegraphRing"));
			RingAct->SetLifeSpan(1.30f);

			if (UPointLightComponent* TelLight = NewObject<UPointLightComponent>(RingAct, TEXT("RemnantTelegraphLight")))
			{
				TelLight->SetupAttachment(SMC);
				TelLight->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
				TelLight->SetIntensity(6800.0f);
				TelLight->SetLightColor(RingColor);
				TelLight->SetAttenuationRadius(Radius * 1.35f);
				TelLight->SetCastShadows(false);
				TelLight->RegisterComponent();
			}
		}
		return RingAct;
	}

	// Kadim Uretim Tezgahi & GDD 16 Koloni Kutuphanesi Tarif Arsivleme ([E] veya [K])
	bool CraftAndArchiveAtAncientStation(AActor* PlayerActor)
	{
		StoredLivingLog    += CarriedLivingLog;
		StoredGuardianHorn += CarriedGuardianHorn;
		StoredMagmaCore    += CarriedMagmaCore;
		StoredSunEssence   += CarriedSunEssence;
		CarriedLivingLog = CarriedGuardianHorn = CarriedMagmaCore = CarriedSunEssence = 0;

		bool bDidAnything = false;
		FString CraftedList;

		if ((bRecipeLivingDarkSword || StoredLivingLog > 0) && !bCraftedLivingDarkSword)
		{
			bRecipeLivingDarkSword = true;
			bLivingLogUnlocked = true;
			bCraftedLivingDarkSword = true;
			StoredLivingLog = FMath::Max(0, StoredLivingLog - 1);
			StoredSteelIngots = FMath::Max(0, StoredSteelIngots - 1);
			CraftedList += TEXT("[Kadim Karanlik Kilic: +%35 Hasar & +6 HP Can Calma] ");
			bDidAnything = true;
		}
		if ((bRecipeGuardianArmor || StoredGuardianHorn > 0 || StoredMagmaCore > 0) && !bCraftedGuardianArmor)
		{
			bRecipeGuardianArmor = true;
			bCraftedGuardianArmor = true;
			StoredGuardianHorn = FMath::Max(0, StoredGuardianHorn - 1);
			StoredMagmaCore = FMath::Max(0, StoredMagmaCore - 1);
			CraftedList += TEXT("[Kadim Muhafiz Zirhi: +%40 Zirh Korumasi] ");
			bDidAnything = true;
		}
		if ((bRecipeSunOilElixir || StoredSunEssence > 0 || bSunflowerBossAllied) && !bCraftedSunOilElixir)
		{
			bRecipeSunOilElixir = true;
			bCraftedSunOilElixir = true;
			bOiledBootsEquipped = true;
			StoredSunEssence = FMath::Max(0, StoredSunEssence - 1);
			CraftedList += TEXT("[Gunes Yagi Iksiri: +%36 Hiz & Can Yenileme] ");
			bDidAnything = true;
		}

		const int32 UnlockedCount = GetUnlockedRecipesCount();
		if (UnlockedCount > ArchivedRecipesCount)
		{
			ArchivedRecipesCount = UnlockedCount;
			bColonyLibraryBuilt = true;
			bDidAnything = true;
		}

		if (bDidAnything)
		{
			PushNotification(FString::Printf(
				TEXT("KADIM TEZGAH & KUTUPHANE [E/K]: Uretilen Techizat: %s| Kutuphane Arsivi: %d/4 Tarif Kalici Yedeklendi (GDD 16: Olumde Kaybolmaz!)"),
				CraftedList.IsEmpty() ? TEXT("(Mevcut Techizatlar Aktif) ") : *CraftedList,
				ArchivedRecipesCount), 7.5f);
			return true;
		}

		PushNotification(FString::Printf(
			TEXT("KADIM TEZGAH & KUTUPHANE [E/K]: Acilan Tarif: %d/4 | Kutuphanede Yedeklenen: %d/4 | Boss'lari (Treeguard, Labirent Bekcisi, Aycicegi) yenip ganimetlerini getir!"),
			UnlockedCount, ArchivedRecipesCount), 6.0f);
		return false;
	}

	// FAZ 3 (C1 - DENIZCILIK): 5 Gemi Sinifi Evrimi (Sal -> Balikci Teknesi -> Salupa -> Nakliye Gemisi -> Amiral Gemisi)
	bool UpgradeShipClassAndFleet(UWorld* World, int32 WoodCost, int32 IronCost)
	{
		// Sirt cantasinda tasinan kereste/cevher varsa once tersaneye aktar
		if (CarriedWood > 0 || CarriedIronOre > 0)
		{
			WoodCount     += CarriedWood;
			StoredIronOre += CarriedIronOre;
			CarriedWood = CarriedIronOre = 0;
		}

		// Sal'dan sonraki kademeler demir de ister; kaynak yetmezse yukseltme reddedilir
		const int32 RequiredIron = (static_cast<uint8>(CurrentShipClass) >= 1) ? IronCost : 0;
		if (WoodCount < WoodCost || StoredIronOre < RequiredIron)
		{
			PushNotification(FString::Printf(
				TEXT("TERSANE [G]: Yetersiz kaynak! Gereken: %d Odun, %d Demir | Ambar: %d Odun, %d Demir"),
				WoodCost, RequiredIron, WoodCount, StoredIronOre), 5.0f);
			return false;
		}

		bBoatBuildingStarted = true;
		bNomadFleetDoctrine  = true;
		bFleetAutoMiningActive = true;

		WoodCount     -= WoodCost;
		StoredIronOre -= RequiredIron;

		// [G] basildiginda Gemi Sinifi 1 kademe yukselir (Amiral Gemisine ulasildiginda yeni refakatci gemiler eklenir)
		const uint8 CurTier = static_cast<uint8>(CurrentShipClass);
		if (CurTier < 4)
		{
			CurrentShipClass = static_cast<EDSShipClass>(CurTier + 1);
		}
		else
		{
			FleetShipCount = FMath::Min(6, FleetShipCount + 1);
		}

		const uint8 NewTier = static_cast<uint8>(CurrentShipClass);
		if (NewTier + 1 > FleetShipCount)
		{
			FleetShipCount = NewTier + 1;
		}

		switch (CurrentShipClass)
		{
		case EDSShipClass::Raft:
			MaxShipHullIntegrity = 100.0f; HarpoonPowerLevel = 1; FleetCargoCapacity = 8; break;
		case EDSShipClass::FishingBoat:
			MaxShipHullIntegrity = 150.0f; HarpoonPowerLevel = 2; FleetCargoCapacity = 16; break;
		case EDSShipClass::Sloop:
			MaxShipHullIntegrity = 220.0f; HarpoonPowerLevel = 3; FleetCargoCapacity = 28; bRecipePirateSalupa = true; break;
		case EDSShipClass::CargoVessel:
			MaxShipHullIntegrity = 320.0f; HarpoonPowerLevel = 4; FleetCargoCapacity = 45; break;
		case EDSShipClass::Flagship:
			MaxShipHullIntegrity = 480.0f; HarpoonPowerLevel = 5; FleetCargoCapacity = 75; break;
		}
		ShipHullIntegrity = MaxShipHullIntegrity;

		// Sahnedeki 3B Sancak Gemisi (FlagshipHull), Direk & Yelken (ShipMastAndSail) ve Zipkin Topu (HarpoonLauncher) boyutlarini evrimlestir
		if (World)
		{
			const float TierMult = 1.0f + float(NewTier) * 0.22f;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Act = *It;
				if (!IsValid(Act)) continue;
				if (Act->ActorHasTag(FName("PrimaryFlagshipHull")))
				{
					Act->SetActorScale3D(FVector(1.85f * TierMult, 0.72f * TierMult, 0.34f * TierMult));
				}
				else if (Act->ActorHasTag(FName("ShipMastPole")))
				{
					Act->SetActorHiddenInGame(NewTier == 0);
					Act->SetActorScale3D(FVector(0.14f * TierMult, 0.14f * TierMult, 1.30f * TierMult));
				}
				else if (Act->ActorHasTag(FName("ShipMastAndSail")))
				{
					Act->SetActorHiddenInGame(NewTier == 0);
					Act->SetActorScale3D(FVector(0.10f, 1.35f * TierMult, 1.05f * TierMult));
				}
				else if (Act->ActorHasTag(FName("HarpoonLauncher")))
				{
					Act->SetActorScale3D(FVector(1.0f + float(NewTier) * 0.14f));
				}
			}
		}

		PushNotification(FString::Printf(
			TEXT("TERSANE GEMI SINIFI YUKSELTILDI [G]: %s | Filo: %d Gemi | Govde: %.0f HP | Pruva Zipkin Gucu: Seviye %d ([B] ile Dev Balina Avla & Filo Seferi Baslat!)"),
			*GetShipClassDisplayName(), FleetShipCount, ShipHullIntegrity, HarpoonPowerLevel), 7.5f);
		return true;
	}

	// FAZ 3 (C3 - ADALAR ARASI FILO MADENCILIGI VE OTOMATIK TUZ/CEVHER SEVKIYATI)
	void ExecuteFleetMiningConvoy(UWorld* World, bool bManualTrigger = false)
	{
		FleetConvoyCompletedCount += 1;
		MaritimeExpeditionCount   += 1;

		const int32 TierBonus = static_cast<int32>(CurrentShipClass) + 1;
		const int32 AmberMult = (StoredAmbergris > 0 || CarriedAmbergris > 0) ? 2 : 1;
		const int32 RouteIdx  = ActiveFleetRouteIndex % 3;
		ActiveFleetRouteIndex = (ActiveFleetRouteIndex + 1) % 3;

		int32 ConvoyIron  = 2 * TierBonus * AmberMult;
		int32 ConvoySalt  = 3 * TierBonus * AmberMult;
		int32 ConvoyWood  = 3 * TierBonus;
		int32 ConvoySteel = (TierBonus >= 3 || bBlacksmithForgeBuilt) ? (1 + TierBonus / 2) : 0;

		if (RouteIdx == 0)
		{
			// Tuzlu Mercan Adasi: Yuksek Kaya Tuzu & Kurutulmus Balik
			ConvoySalt += 4;
			StoredDriedMeat += 3 * TierBonus;
		}
		else if (RouteIdx == 1)
		{
			// Demir Kayaliklari Adasi: Yuksek Demir Cevheri & Celik Kulce
			ConvoyIron  += 4;
			ConvoySteel += 2;
		}
		else
		{
			// Obsidyen Firtina Adasi: Firtina hasari riski + Yuksek Celik/Tuz/Ambergris
			const float StormDmg = FMath::Max(4.0f, 18.0f - float(TierBonus) * 3.0f);
			ShipHullIntegrity = FMath::Max(25.0f, ShipHullIntegrity - StormDmg);
			ConvoySteel += 3;
			ConvoySalt  += 3;
			StoredAmbergris += 1;
		}

		StoredIronOre     += ConvoyIron;
		StoredSalt        += ConvoySalt;
		WoodCount         += ConvoyWood;
		StoredSteelIngots += ConvoySteel;
		SeaBountyCount    += (ConvoySalt + ConvoyIron);

		if (bManualTrigger)
		{
			PushNotification(FString::Printf(
				TEXT("ADALAR ARASI FILO MADENCILIGI (#%d - %s | %s): Ambara +%d Demir, +%d Celik, +%d Kaya Tuzu, +%d Odun sevkiyati ulasti! (Gemi Govdesi: %.0f/%.0f HP)"),
				FleetConvoyCompletedCount, *GetShipClassShortName(), *GetActiveFleetRouteName(),
				ConvoyIron, ConvoySteel, ConvoySalt, ConvoyWood, ShipHullIntegrity, MaxShipHullIntegrity), 7.5f);
		}
	}

	// FAZ 3 (C2 - DENIZCILIK: DEV BALINA AVI & 3B ZIPKIN FIRLATMA)
	bool LaunchHarpoonAtWhaleAndSail(UWorld* World, AActor* PlayerActor)
	{
		if (!World)
		{
			return false;
		}
		bNomadFleetDoctrine    = true;
		bFleetAutoMiningActive = true;

		AActor* WhaleActor = nullptr;
		FVector LauncherLoc(965.0f, 1965.0f, 232.0f);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Act = *It;
			if (!IsValid(Act)) continue;
			if (Act->ActorHasTag(FName("LeviathanWhale")))
			{
				WhaleActor = Act;
			}
			else if (Act->ActorHasTag(FName("HarpoonLauncher")))
			{
				LauncherLoc = Act->GetActorLocation() + FVector(0.0f, 0.0f, 24.0f);
			}
		}

		const FVector WhaleTargetLoc = WhaleActor ? WhaleActor->GetActorLocation() : FVector(715.0f, 2295.0f, 222.0f);
		const FVector ToWhaleDir = (WhaleTargetLoc - LauncherLoc).GetSafeNormal();

		// 3B Celik Zipkin Mermisi (HarpoonProjectile) firlat — Balinaya saplanan parlayan zipkin mizragi
		if (UStaticMesh* HarpoonMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Variant_Combat/Weapons/SM_Pirate_Greatsword.SM_Pirate_Greatsword")))
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector MidFlightLoc = FMath::Lerp(LauncherLoc, WhaleTargetLoc, 0.72f) + FVector(0.0f, 0.0f, 22.0f);
			const FRotator HarpoonRot = ToWhaleDir.Rotation() + FRotator(-72.0f, 0.0f, 0.0f);
			if (AStaticMeshActor* HarpoonProj = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), MidFlightLoc, HarpoonRot, SP))
			{
				HarpoonProj->SetMobility(EComponentMobility::Movable);
				HarpoonProj->GetStaticMeshComponent()->SetStaticMesh(HarpoonMesh);
				HarpoonProj->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				HarpoonProj->SetActorScale3D(FVector(0.75f, 0.75f, 1.35f));
				HarpoonProj->Tags.AddUnique(FName("HarpoonProjectile"));
				HarpoonProj->SetLifeSpan(2.2f);

				if (UPointLightComponent* HLight = NewObject<UPointLightComponent>(HarpoonProj, TEXT("HarpoonTrailGlow")))
				{
					HLight->SetupAttachment(HarpoonProj->GetStaticMeshComponent());
					HLight->SetIntensity(5400.0f);
					HLight->SetLightColor(FLinearColor(1.0f, 0.78f, 0.20f));
					HLight->SetAttenuationRadius(320.0f);
					HLight->SetCastShadows(false);
					HLight->RegisterComponent();
				}
			}
		}

		// Balina etrafinda su halkasi / dalga telgrafi olustur (Su yuzeyinin tam ustunde Z=218, yaricap=85cm)
		SpawnRemnantTelegraphRing(World, FVector(WhaleTargetLoc.X, WhaleTargetLoc.Y, 218.0f), 85.0f, FLinearColor(0.12f, 0.88f, 1.0f));

		const float HarpoonDmg = 85.0f + float(HarpoonPowerLevel) * 35.0f + (bPirateSeaMasteryUnlocked ? 45.0f : 0.0f);
		WhaleCurrentHP = FMath::Max(0.0f, WhaleCurrentHP - HarpoonDmg);

		// Her zipkin isabetinde veya balina avlandiginda iskele guvertesinin sol ve sag yanina fiziksel 3B Balina Yagi ve Ambergris ganimeti duser!
		const FVector LeftPierDrop(985.0f, 2035.0f, 234.0f);
		const FVector RightPierDrop(942.0f, 1995.0f, 234.0f);
		if (WhaleCurrentHP <= 0.0f)
		{
			WhalesHuntedCount += 1;
			WhaleMaxHP += 80.0f;
			WhaleCurrentHP = WhaleMaxHP;

			SpawnPhysicalWorldLoot(World, LeftPierDrop, FName("Loot_WhaleBlubber"), 0);
			SpawnPhysicalWorldLoot(World, LeftPierDrop + FVector(-22.0f, 18.0f, 0.0f), FName("Loot_WhaleBlubber"), 0);
			SpawnPhysicalWorldLoot(World, RightPierDrop, FName("Loot_Ambergris"), 0);

			ExecuteFleetMiningConvoy(World, false);

			PushNotification(FString::Printf(
				TEXT("DERIN SULAR — DEV BALINA AVLANDI (#%d)! Kiyiya 3B Balina Yagi (Loot_WhaleBlubber) ve Kadim Ambergris (Loot_Ambergris) dustu ([E] ile Topla) + Filo Maden Konvoyu Ambara Ulasti!"),
				WhalesHuntedCount), 8.0f);
		}
		else
		{
			SpawnPhysicalWorldLoot(World, LeftPierDrop, FName("Loot_WhaleBlubber"), 0);
			SpawnPhysicalWorldLoot(World, RightPierDrop, FName("Loot_Ambergris"), 0);
			ExecuteFleetMiningConvoy(World, false);

			PushNotification(FString::Printf(
				TEXT("PRUVA ZIPKIN TOPU ATESLENDI (Hasar: %.0f | Dev Balina HP: %.0f/%.0f)! Kiyiya Balina Yagi dustu & %s Adalar Arasi Maden/Tuz Sevkiyatini Tamamladi!"),
				HarpoonDmg, WhaleCurrentHP, WhaleMaxHP, *GetShipClassShortName()), 7.0f);
		}
		return true;
	}

	// ============================================================================================
	// FAZ 4 (D1): KOMSU KOY DIPLOMASISI, KARANLIK YOL (DEMIRCI KACIRMA) VE GERCEK DUNYA SONUCLARI
	// ============================================================================================
	void ResolveNeighborDiplomacyChoice(UWorld* World, int32 ChoiceMode, int32 DarkPathSteelReward = 6, int32 DarkPathIronReward = 8, int32 DarkPathSaltReward = 5)
	{
		if (ChoiceMode == 0)
		{
			// [E] ADIL TUZ & SILAH ITTIFAKI: Komsu koye 2 Tuz + 1 Celik/Demir ver -> Muttefik Ciftci/Demirci gonullu katilir!
			bNeighborAllianceFormed = true;
			StoredSalt = FMath::Max(0, StoredSalt - 2);
			StoredDriedMeat += 6;
			StoredSteelIngots += 2;
			DivineFavor = FMath::Clamp(DivineFavor + 15.0f, 0.0f, 100.0f);
			PushNotification(
				TEXT("VICDAN SECIMI [E — TUZ & SILAH ITTIFAKI]: Komsu Koye silah ve tuz destegi verildi! Komsu Elci koloniye gonullu Usta Zanaatkar olarak katildi (+6 Tuzlu Erzak, +2 Celik, +15 Guven)!"),
				8.0f);
		}
		else if (ChoiceMode == 1)
		{
			// [X] KARANLIK YOL — KOMSU KOYU BASIP USTA DEMIRCI & CIFTCI KACIRMA:
			// Ahlak puani yerine gercek dunya sonucu: Zorla getirilen Demirci celik kulce ve demir uretir ama Kisisel Guveni 15'e duser ve Komsu Koy Intikam Baskini baslatir!
			bDarkPathChosen = true;
			bNeighborRevengeRaidTriggered = true;
			KidnappedBlacksmithCount += 1;
			DarkPathActionsCount += 1;
			StoredSteelIngots += DarkPathSteelReward;
			StoredIronOre += DarkPathIronReward;
			StoredSalt += DarkPathSaltReward;
			DivineFavor = FMath::Clamp(DivineFavor - 18.0f, 0.0f, 100.0f);
			if (World)
			{
				SpawnRemnantTelegraphRing(World, FVector(1225.0f, 1585.0f, 212.0f), 115.0f, FLinearColor(0.95f, 0.15f, 0.12f));
			}
			PushNotification(FString::Printf(
				TEXT("KARANLIK YOL SECIMI (#%d) [X — DEMIRCI KACIRMA]: Komsu koy basildi, Usta Demirci zorla ocaga getirildi (+%d Celik, +%d Demir)! SONUC: Kisisel Guven 15/100'e dustu & Komsu Koy Intikam Baskini basladi!"),
				KidnappedBlacksmithCount, DarkPathSteelReward, DarkPathIronReward), 8.5f);
		}
		else
		{
			// [Z] SILAH SATISINI REDDETME: Caresiz kalan komsu koy acliktan senin ambarina saldirir!
			bWeaponsSaleRefused = true;
			bNeighborRevengeRaidTriggered = true;
			DarkPathActionsCount += 1;
			DivineFavor = FMath::Clamp(DivineFavor - 10.0f, 0.0f, 100.0f);
			PushNotification(
				TEXT("VICDAN SECIMI [Z — SILAH SATISINI REDDET]: Silah satisi reddedildi! Gercek Dunya Sonucu: Caresiz kalan komsu koy hayatta kalmak icin ambarina karsi baskin baslatti!"),
				8.0f);
		}
	}

	// FAZ 4 (D2): VICDAN TARZI GIZLI ZOMBI ISIRIGI MUAYENESI VE KARANTINA / SURGUN IKILEMI
	void ResolveBiteInspectionDilemma(AActor* VillagerActor, bool bMercyQuarantineCure)
	{
		BiteInspectionsResolvedCount += 1;
		if (VillagerActor)
		{
			HiddenBiteSuspectMap.FindOrAdd(FObjectKey(VillagerActor)) = false;
			BiteInspectedMap.FindOrAdd(FObjectKey(VillagerActor)) = true;
		}
		if (bMercyQuarantineCure)
		{
			StoredSalt = FMath::Max(0, StoredSalt - 1);
			if (VillagerActor)
			{
				VillagerPersonalTrustMap.FindOrAdd(FObjectKey(VillagerActor)) = 96.0f;
			}
			DivineFavor = FMath::Clamp(DivineFavor + 12.0f, 0.0f, 100.0f);
			PushNotification(FString::Printf(
				TEXT("GIZLI ISIRIK MUAYENESI (#%d — KARANTINA & MERHEM TEDAVISI): Koyludeki gizli isirik Tuzlu Gunes Merhemi ile temizlendi! Koylu kurtuldu (Kisisel Guven: 96/100, +12 Koloni Guveni)!"),
				BiteInspectionsResolvedCount), 8.0f);
		}
		else
		{
			bDarkPathChosen = true;
			DarkPathActionsCount += 1;
			PushNotification(FString::Printf(
				TEXT("GIZLI ISIRIK MUAYENESI (#%d — KARANLIK KARAR: SURGUN): Enfeksiyon riski sifirlandi ancak sahit olan koylulerin guveni sarsildi (-15 Koloni Guveni)!"),
				BiteInspectionsResolvedCount), 8.0f);
		}
	}

	// FAZ 4 (D3): DUYGUSAL CEKIRDEK — KORUNAKLI ANI KOYU KOSESI VE OLUMSUZ ANI NPC'LERI (BABA, DEDE, ANNEANNE)
	void InteractWithImmortalMemoryNPC(AActor* MemoryNPC)
	{
		bMemorySanctuaryBlessed = true;
		MemoryArchiveCount = FMath::Clamp(MemoryArchiveCount + 1, 1, 3);
		ArchivedRecipesCount = 4;
		DivineFavor = FMath::Clamp(DivineFavor + 20.0f, 0.0f, 100.0f);

		FString SpeakerTitle = TEXT("AKIL HOCASI BABA");
		FString MemoryQuote  = TEXT("\"Evlat, huzur kaybedilebilir oldugu icin huzurdur. Aileni ve ocagini korudugun surece bu ates hic sonmeyecek.\"");
		if (MemoryNPC && MemoryNPC->ActorHasTag(FName("AniNPC_Dede")))
		{
			SpeakerTitle = TEXT("BILGE DEDE (OLUMSUZ ANI NPC)");
			MemoryQuote  = TEXT("\"Dedemin elleriyle yonttugu bu iskele ve ocak, yillar gecse de bizim yasayan hatiramizdir. Yuregini ferah tut evlat!\"");
		}
		else if (MemoryNPC && MemoryNPC->ActorHasTag(FName("AniNPC_Anneanne")))
		{
			SpeakerTitle = TEXT("SEFKATLI ANNEANNE (OLUMSUZ ANI NPC)");
			MemoryQuote  = TEXT("\"Anneannenin ocaginda her zaman sicak as ve huzur var yavrum. Disarida firtina kopsa da bu kose dokunulmazdir.\"");
		}

		PushNotification(FString::Printf(
			TEXT("YASAYAN ANI ARSIVI (%d/3) — %s: %s [Olumsuz Ani Kosesi Kutsamasi: Tam Can & +20 Guven]"),
			MemoryArchiveCount, *SpeakerTitle, *MemoryQuote), 8.5f);
	}

	void LegacyBannerInit()
	{
		SetGuideBanner(
			TEXT("AKIL HOCASI (KEHANET REHBERI) — KOLONI & SAVAS KOMUTASI"),
			TEXT("[V] Koylu Emri: SAVAS (Sakalli Savascilar & Koyluler Saldirir) | [E] Tetkik / Kurtar / Portal | [C] Fidan Dik | [G/H] Gemi & Kale"),
			FLinearColor(1.0f, 0.82f, 0.28f, 1.0f)
		);
	}

	void SetGuideBanner(const FString& InTitle, const FString& InBody, const FLinearColor& InColor)
	{
		GuideTitle = InTitle;
		GuideBody = InBody;
		GuideColor = InColor;
		bGuideDirty = true;
	}

	static FString GetOrderDisplayName(EDSVillagerCombatOrder Order)
	{
		switch (Order)
		{
		case EDSVillagerCombatOrder::FightOrder:         return TEXT("SAVAS EMRI (Hucum)");
		case EDSVillagerCombatOrder::WorkAndDefendOrder: return TEXT("CALIS & SAVUN");
		case EDSVillagerCombatOrder::RetreatOrder:       return TEXT("GERI CEKIL (Kamp)");
		default:                                         return TEXT("SAVAS EMRI");
		}
	}

	float GetPersonalTrust(AActor* Villager)
	{
		if (!Villager)
		{
			return 55.0f;
		}
		if (float* Found = PersonalTrustMap.Find(FObjectKey(Villager)))
		{
			return *Found;
		}
		PersonalTrustMap.Add(FObjectKey(Villager), 55.0f);
		return 55.0f;
	}

	float AddPersonalTrust(AActor* Villager, float Delta)
	{
		const float Cur = GetPersonalTrust(Villager);
		const float Next = FMath::Clamp(Cur + Delta, 0.0f, 100.0f);
		if (Villager)
		{
			PersonalTrustMap.Add(FObjectKey(Villager), Next);
		}
		return Next;
	}
};

struct FDSCharacterBodyRig
{
	TWeakObjectPtr<UStaticMeshComponent> HeadComp;
	TWeakObjectPtr<UStaticMeshComponent> TorsoComp;
	TWeakObjectPtr<UStaticMeshComponent> ArmLComp;
	TWeakObjectPtr<UStaticMeshComponent> ArmRComp;
	TWeakObjectPtr<UStaticMeshComponent> LegLComp;
	TWeakObjectPtr<UStaticMeshComponent> LegRComp;
	TWeakObjectPtr<UStaticMeshComponent> CachedWeaponMesh;
	TWeakObjectPtr<UStaticMeshComponent> ExtraCrownComp;
	TWeakObjectPtr<USkeletalMeshComponent> OwnerSkelMesh;

	bool bInitialized = false;
	bool bUsingArgonSkeletalMesh = false;
	bool bIsZombieRig = false;
	bool bIsDownedPose = false;
	bool bIsBlockingPose = false;
	float ActiveParryFlashTime = -1.0f;
	FVector BaseRigScale = FVector(1.0f);
	float WalkCyclePhase = 0.0f;
	float ActiveSwingTime = -1.0f;
	float ActiveSwingDuration = 0.48f;
	int32 ActiveComboIndex = 0;
	EDSCombatWeaponStyle WeaponStyle = EDSCombatWeaponStyle::BattleAxe;

	/** Owner'in rig kaydi (UDSWorldStateSubsystem); Owner yok edilince kayit silinir. Owner/dunya yoksa nullptr. Tanim: DSWorldStateSubsystem.cpp */
	static FDSCharacterBodyRig* GetOrCreateRig(AActor* Owner);

	static FString GetWeaponDisplayName(EDSCombatWeaponStyle Style, const UObject* WorldContext)
	{
		const FDSColonyAndWorldState* WS = FDSColonyAndWorldState::Find(WorldContext);
		const bool bCraftedDarkSword = WS && WS->bCraftedLivingDarkSword;
		const bool bTwoHandedMastery = WS && WS->bTwoHandedSwordMasteryUnlocked;

		switch (Style)
		{
		case EDSCombatWeaponStyle::BattleAxe:       return TEXT("[1] Cift Yuzlu Balta (Genis Bicme)");
		case EDSCombatWeaponStyle::SurvivalAxe:     return TEXT("[2] Hayatta Kalma Baltasi (Hizli Dograma)");
		case EDSCombatWeaponStyle::MedievalAxe:     return TEXT("[3] Ortacag Baltasi (Uzun Kesme)");
		case EDSCombatWeaponStyle::WarHammer:       return TEXT("[4] Savas Cekici (Ezici Balyoz)");
		case EDSCombatWeaponStyle::UnarmedFists:    return TEXT("[5] Yumruk / Boks Kombosu");
		case EDSCombatWeaponStyle::LivingDarkSword:
			if (bCraftedDarkSword)
			{
				return TEXT("[6] Kadim Karanlik Kilic (+%35 Hasar & Can Calma)");
			}
			return bTwoHandedMastery
				? TEXT("[6] Cift Elli Korsan Kilici (3'lu Kombo + [F] Blok/Parry)")
				: TEXT("[6] Tek Elli Kilic (Korsan Boss -> Cift El Ustaligi)");
		default:                                    return TEXT("Cift Yuzlu Balta");
		}
	}

	static UStaticMeshComponent* SpawnBodyPartComponent(AActor* Owner, USceneComponent* AttachParent, FName CompName, const FString& AssetPath, const FVector& Scale3D, UMaterialInterface* OverridePBRMat = nullptr)
	{
		if (!Owner || !AttachParent)
		{
			return nullptr;
		}

		UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, *AssetPath);
		if (!MeshAsset)
		{
			return nullptr;
		}

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner, CompName);
		if (!Comp)
		{
			return nullptr;
		}

		Comp->SetStaticMesh(MeshAsset);
		if (OverridePBRMat)
		{
			const int32 NumMats = FMath::Max(1, Comp->GetNumMaterials());
			for (int32 i = 0; i < NumMats; ++i)
			{
				Comp->SetMaterial(i, OverridePBRMat);
			}
		}
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(true);
		Comp->RegisterComponent();
		Comp->AttachToComponent(AttachParent, FAttachmentTransformRules::KeepWorldTransform);

		FVector EffectiveScale = Scale3D;
		const FBoxSphereBounds Bounds = MeshAsset->GetBounds();
		const float MaxExt = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z) * 2.0f;
		if (MaxExt > 0.01f)
		{
			if (AssetPath.Contains(TEXT("Sunflower_Crown")))
			{
				EffectiveScale = Scale3D * FMath::Clamp(34.0f / MaxExt, 0.05f, 10.0f);
			}
			else if (AssetPath.Contains(TEXT("FatherHead")))
			{
				EffectiveScale = Scale3D * FMath::Clamp(27.0f / MaxExt, 0.05f, 10.0f);
			}
		}

		Comp->SetWorldScale3D(EffectiveScale);
		return Comp;
	}

	/**
	 * true: karakterler gercek iskelet (Manny/Quinn) mesh'i + AnimBlueprint + montajlarla animasyon oynatir
	 * (dirsek/diz bukulur, ziplama/dusme/yurume animasyonlari calisir). false: eski 6 parcali statik mesh
	 * gorunumu (kemiklere yapistirilmis kaskati parcalar) — sadece geri donus icin birakildi.
	 */
	static bool UseRealSkeletalAnimation() { return true; }

	/**
	 * Statik silah mesh'ini karakterin sag el kemigine (hand_r) gercek bir kavrayis acisiyla baglar.
	 * Kavrayis yonu el/parmak kemiklerinin dunya konumlarindan hesaplanir (sap avuc icinden gecer,
	 * agiz/bicak parmaklarin baktigi yone bakar), sonra KeepWorldTransform ile kemige yapistirilir;
	 * boylece silah animasyonla birlikte elin icinde hareket eder.
	 */
	static void AttachWeaponToHand(USkeletalMeshComponent* SkelMesh, UStaticMeshComponent* WeaponComp, float DesiredLength)
	{
		if (!SkelMesh || !WeaponComp || !WeaponComp->GetStaticMesh() || !SkelMesh->DoesSocketExist(TEXT("hand_r")))
		{
			return;
		}

		const FBoxSphereBounds B = WeaponComp->GetStaticMesh()->GetBounds();
		const FVector Ext = B.BoxExtent;

		// sap ekseni = en uzun bounds ekseni, agiz ekseni = ikinci en uzun eksen
		int32 ShaftAxis = 2;
		if (Ext.X >= Ext.Y && Ext.X >= Ext.Z) ShaftAxis = 0;
		else if (Ext.Y >= Ext.X && Ext.Y >= Ext.Z) ShaftAxis = 1;
		int32 BladeAxis = (ShaftAxis == 0) ? ((Ext.Y >= Ext.Z) ? 1 : 2) : ((ShaftAxis == 1) ? ((Ext.X >= Ext.Z) ? 0 : 2) : ((Ext.X >= Ext.Y) ? 0 : 1));

		const float MaxDim = Ext[ShaftAxis] * 2.0f;
		const float WScale = (MaxDim > 0.001f) ? FMath::Clamp(DesiredLength / MaxDim, 0.005f, 50.0f) : 1.0f;

		// el kemiklerinden dunya uzayinda kavrayis cercevesi
		const FVector HandLoc   = SkelMesh->GetSocketLocation(TEXT("hand_r"));
		const FVector MiddleLoc = SkelMesh->DoesSocketExist(TEXT("middle_01_r")) ? SkelMesh->GetSocketLocation(TEXT("middle_01_r")) : HandLoc + SkelMesh->GetSocketRotation(TEXT("hand_r")).Vector() * 8.0f;
		const FVector IndexLoc  = SkelMesh->DoesSocketExist(TEXT("index_01_r")) ? SkelMesh->GetSocketLocation(TEXT("index_01_r")) : MiddleLoc;
		const FVector PinkyLoc  = SkelMesh->DoesSocketExist(TEXT("pinky_01_r")) ? SkelMesh->GetSocketLocation(TEXT("pinky_01_r")) : MiddleLoc;

		const FVector FingerDir = (MiddleLoc - HandLoc).GetSafeNormal(0.001f, FVector::ForwardVector);
		FVector ShaftDir = (IndexLoc - PinkyLoc);
		ShaftDir = (ShaftDir - FingerDir * FVector::DotProduct(ShaftDir, FingerDir)).GetSafeNormal(0.001f, FVector::UpVector);
		const FVector PalmDir = FVector::CrossProduct(FingerDir, ShaftDir).GetSafeNormal();

		// mesh yerel eksenlerini dunya yonlerine esle
		FVector LocalShaft = FVector::ZeroVector; LocalShaft[ShaftAxis] = 1.0f;
		FVector LocalBlade = FVector::ZeroVector; LocalBlade[BladeAxis] = 1.0f;
		const FMatrix LocalBasis = FRotationMatrix::MakeFromZX(LocalShaft, LocalBlade);
		const FMatrix WorldBasis = FRotationMatrix::MakeFromZX(ShaftDir, FingerDir);
		const FQuat WorldRot = FQuat(WorldBasis) * FQuat(LocalBasis).Inverse();

		// kavrayis noktasi: sapin alt ucundan %18 yukari (yerel uzayda), avucun ortasina gelir
		FVector GripLocal = B.Origin;
		GripLocal[ShaftAxis] = B.Origin[ShaftAxis] - Ext[ShaftAxis] + (Ext[ShaftAxis] * 2.0f) * 0.18f;
		const FVector PalmCenter = HandLoc + (MiddleLoc - HandLoc) * 0.55f + PalmDir * 2.5f;
		const FVector WorldLoc = PalmCenter - WorldRot.RotateVector(GripLocal * WScale);

		WeaponComp->AttachToComponent(SkelMesh, FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
		WeaponComp->SetWorldScale3D(FVector(WScale));
		WeaponComp->SetWorldLocationAndRotation(WorldLoc, WorldRot.Rotator());
	}

	static float GetWeaponDesiredLength(EDSCombatWeaponStyle Style, const UObject* WorldContext)
	{
		const FDSColonyAndWorldState* WS = FDSColonyAndWorldState::Find(WorldContext);
		const bool bTwoHandedMastery = WS && WS->bTwoHandedSwordMasteryUnlocked;

		switch (Style)
		{
		case EDSCombatWeaponStyle::WarHammer:       return 84.0f;
		case EDSCombatWeaponStyle::SurvivalAxe:     return 62.0f;
		case EDSCombatWeaponStyle::MedievalAxe:     return 88.0f;
		case EDSCombatWeaponStyle::LivingDarkSword: return bTwoHandedMastery ? 120.0f : 92.0f;
		default:                                    return 80.0f;
		}
	}

	void InitializeRig(AActor* Owner, USkeletalMeshComponent* SkelMesh, const FString& CharacterPrefix, bool bInIsZombie, const FVector& RigScale = FVector(1.0f))
	{
		if (!Owner || !SkelMesh)
		{
			return;
		}

		const FString EffectivePrefix = (CharacterPrefix == TEXT("Villager04")) ? TEXT("Villager03") : ((CharacterPrefix == TEXT("BossPirate")) ? TEXT("Villager02") : CharacterPrefix);

		bIsZombieRig = bInIsZombie;
		BaseRigScale = RigScale;
		OwnerSkelMesh = SkelMesh;

		if (EffectivePrefix == TEXT("BossSunflower") || Owner->ActorHasTag(TEXT("Boss_Sunflower")))
		{
			if (USkeletalMesh* BossSunSKM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/BossSunflower/SKM_BossSunflower.SKM_BossSunflower")))
			{
				SkelMesh->SetSkeletalMeshAsset(BossSunSKM);
				SkelMesh->EmptyOverrideMaterials();
			}
			if (UStaticMesh* FlagMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/BossSunflower/SM_BossSunflower_Flag/StaticMeshes/SM_BossSunflower_Flag.SM_BossSunflower_Flag")))
			{
				if (UStaticMeshComponent* FlagComp = NewObject<UStaticMeshComponent>(Owner, TEXT("DSBossSunflower_FlagComp")))
				{
					FlagComp->SetMobility(EComponentMobility::Movable);
					FlagComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					FlagComp->SetCastShadow(true);
					FlagComp->SetStaticMesh(FlagMesh);
					FlagComp->SetupAttachment(SkelMesh, FName("hand_r"));
					FlagComp->RegisterComponent();
					FlagComp->SetRelativeLocation(FVector(0.0f, 0.0f, -32.0f));
					FlagComp->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
					FlagComp->SetRelativeScale3D(FVector(0.45f));
					ExtraCrownComp = FlagComp;
				}
			}

			SkelMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
			SkelMesh->SetRelativeScale3D(RigScale);
			SkelMesh->SetBoundsScale(8.0f);
			SkelMesh->SetHiddenInGame(false, false);
			SkelMesh->SetVisibility(true, false);
			SkelMesh->SetCastShadow(true);
			bInitialized = false;
			return;
		}

		// Eski robot Manny mankeni tamamen gizlenir; iskelet kemikleri UAL animasyonlarini calistirirken
		// gercek 3B karakter/zombi/boss parcalari kemik soketlerini canli takip eder.
		SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		SkelMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		SkelMesh->SetRelativeScale3D(FVector(1.0f));
		SkelMesh->SetBoundsScale(FMath::Max(2.0f, RigScale.Z * 2.0f));
		SkelMesh->SetCastShadow(false);
		SkelMesh->SetVisibility(false, false);
		SkelMesh->SetHiddenInGame(true, false);

		auto DestroyOldPart = [](TWeakObjectPtr<UStaticMeshComponent>& PartPtr)
		{
			if (PartPtr.IsValid())
			{
				PartPtr->DestroyComponent();
				PartPtr.Reset();
			}
		};
		DestroyOldPart(HeadComp);
		DestroyOldPart(TorsoComp);
		DestroyOldPart(ArmLComp);
		DestroyOldPart(ArmRComp);
		DestroyOldPart(LegLComp);
		DestroyOldPart(LegRComp);
		DestroyOldPart(ExtraCrownComp);

		USceneComponent* RootComp = Owner->GetRootComponent() ? Owner->GetRootComponent() : SkelMesh;
		const FString ArgonPrefix = Owner->ActorHasTag(TEXT("AkilHocasi_Baba")) ? TEXT("Villager02") : EffectivePrefix;
		const FString BasePath = FString::Printf(TEXT("/Game/Characters/BodyParts/SM_%s_"), *EffectivePrefix);
		const FString MatPath = FString::Printf(TEXT("/Game/Variant_Combat/Env/Materials/M_Char_%s_PBR.M_Char_%s_PBR"), *ArgonPrefix, *ArgonPrefix);
		UMaterialInterface* CharPBRMat = LoadObject<UMaterialInterface>(nullptr, *MatPath);

		const bool bIsDeferredBoss = (CharacterPrefix == TEXT("BossPirate") || CharacterPrefix == TEXT("BossTreeguard"));
		if (!bIsDeferredBoss)
		{
			const FString ArgonSKMPath = FString::Printf(TEXT("/Game/Characters/ArgonRig/SKM_%s.SKM_%s"), *ArgonPrefix, *ArgonPrefix);
			if (USkeletalMesh* ArgonSKM = LoadObject<USkeletalMesh>(nullptr, *ArgonSKMPath))
			{
				SkelMesh->SetSkeletalMeshAsset(ArgonSKM);
				SkelMesh->EmptyOverrideMaterials();
				if (CharPBRMat)
				{
					SkelMesh->SetMaterial(0, CharPBRMat);
				}
				SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				SkelMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
				SkelMesh->SetRelativeScale3D(RigScale);
				SkelMesh->SetBoundsScale(FMath::Max(4.0f, RigScale.Z * 4.0f));
				SkelMesh->SetHiddenInGame(false, false);
				SkelMesh->SetVisibility(true, false);
				SkelMesh->SetCastShadow(true);
				bUsingArgonSkeletalMesh = true;
				bInitialized = true;
				UpdatePose(Owner, SkelMesh, 0.016f, false, nullptr);
				return;
			}
		}
		bUsingArgonSkeletalMesh = false;

		// Akil Hocasi (Baba Yuzu Entegrasyon Kancasi): /Game/Colony/Mentor/SM_FatherHead mevcutsa dogrudan onu yukler,
		// henuz eklenmemisse bilge sakalli bas modelini (Villager02_Head) kullanir.
		FString HeadAssetPath = BasePath + TEXT("Head.SM_") + EffectivePrefix + TEXT("_Head");
		UMaterialInterface* HeadOverrideMat = CharPBRMat;
		if (Owner->ActorHasTag(TEXT("AkilHocasi_Baba")))
		{
			HeadAssetPath = TEXT("/Game/Characters/BodyParts/SM_Villager02_Head.SM_Villager02_Head");
		}

		HeadComp  = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_Head"), *EffectivePrefix)),  HeadAssetPath,  RigScale, HeadOverrideMat);
		TorsoComp = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_Torso"), *EffectivePrefix)), BasePath + TEXT("Torso.SM_") + EffectivePrefix + TEXT("_Torso"), RigScale, CharPBRMat);
		ArmLComp  = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_ArmL"), *EffectivePrefix)),  BasePath + TEXT("ArmL.SM_")  + EffectivePrefix + TEXT("_ArmL"),  RigScale, CharPBRMat);
		ArmRComp  = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_ArmR"), *EffectivePrefix)),  BasePath + TEXT("ArmR.SM_")  + EffectivePrefix + TEXT("_ArmR"),  RigScale, CharPBRMat);
		LegLComp  = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_LegL"), *EffectivePrefix)),  BasePath + TEXT("LegL.SM_")  + EffectivePrefix + TEXT("_LegL"),  RigScale, CharPBRMat);
		LegRComp  = SpawnBodyPartComponent(Owner, RootComp, FName(*FString::Printf(TEXT("DSBody_%s_LegR"), *EffectivePrefix)),  BasePath + TEXT("LegR.SM_")  + EffectivePrefix + TEXT("_LegR"),  RigScale, CharPBRMat);

		bInitialized = (HeadComp.IsValid() && TorsoComp.IsValid());
		UpdatePose(Owner, SkelMesh, 0.016f, false, nullptr);
	}

	void ApplyWeaponMeshToComponent(UStaticMeshComponent* WeaponMeshComp = nullptr)
	{
		if (WeaponMeshComp)
		{
			CachedWeaponMesh = WeaponMeshComp;
		}
		else
		{
			WeaponMeshComp = CachedWeaponMesh.Get();
		}

		if (!WeaponMeshComp)
		{
			return;
		}

		if (WeaponStyle == EDSCombatWeaponStyle::UnarmedFists)
		{
			WeaponMeshComp->SetVisibility(false, true);
			WeaponMeshComp->SetHiddenInGame(true, true);
			return;
		}

		FString WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_BattleAxe.SM_BattleAxe");
		float TargetSize = 86.0f;

		switch (WeaponStyle)
		{
		case EDSCombatWeaponStyle::BattleAxe:
			WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_BattleAxe.SM_BattleAxe");
			TargetSize = 88.0f;
			break;
		case EDSCombatWeaponStyle::SurvivalAxe:
			WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_SurvivalAxe.SM_SurvivalAxe");
			TargetSize = 68.0f;
			break;
		case EDSCombatWeaponStyle::MedievalAxe:
			WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_MedievalAxe.SM_MedievalAxe");
			TargetSize = 92.0f;
			break;
		case EDSCombatWeaponStyle::WarHammer:
			WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_WarHammer.SM_WarHammer");
			TargetSize = 96.0f;
			break;
		case EDSCombatWeaponStyle::LivingDarkSword:
			if (const FDSColonyAndWorldState* WS = FDSColonyAndWorldState::Find(WeaponMeshComp); WS && WS->bTwoHandedSwordMasteryUnlocked)
			{
				WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_Pirate_Greatsword.SM_Pirate_Greatsword");
				TargetSize = 114.0f;
				if (!LoadObject<UStaticMesh>(nullptr, *WeaponAssetPath))
				{
					WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_Weapon_LivingDarkSword.SM_Weapon_LivingDarkSword");
				}
			}
			else
			{
				WeaponAssetPath = TEXT("/Game/Variant_Combat/Weapons/SM_Weapon_LivingDarkSword.SM_Weapon_LivingDarkSword");
				TargetSize = 82.0f;
			}
			break;
		default:
			break;
		}

		if (UStaticMesh* NewMesh = LoadObject<UStaticMesh>(nullptr, *WeaponAssetPath))
		{
			WeaponMeshComp->SetStaticMesh(NewMesh);
			if (UMaterialInterface* SteelMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Variant_Combat/Env/Materials/M_Env_WeaponSteel.M_Env_WeaponSteel")))
			{
				for (int32 mIdx = 0; mIdx < FMath::Max(1, WeaponMeshComp->GetNumMaterials()); ++mIdx)
				{
					WeaponMeshComp->SetMaterial(mIdx, SteelMat);
				}
			}
			WeaponMeshComp->SetHiddenInGame(false, true);
			WeaponMeshComp->SetVisibility(true, true);
			WeaponMeshComp->SetWorldScale3D(FVector(BaseRigScale.Z));
		}
	}

	void TriggerAttackSwing(int32 ComboIdx, float Duration = 0.46f)
	{
		ActiveComboIndex = ComboIdx;
		ActiveSwingDuration = FMath::Max(Duration, 0.20f);
		ActiveSwingTime = 0.0f;
	}

	void TriggerWeaponSwing(float InSpeed = 1.0f)
	{
		TriggerAttackSwing(0, 0.46f / FMath::Max(0.25f, InSpeed));
	}

	static FRotator MakePartRotation(const FVector& UpZ, const FVector& LeftX)
	{
		const FVector SafeZ = UpZ.GetSafeNormal(0.0001f, FVector::UpVector);
		const FVector SafeX = LeftX.GetSafeNormal(0.0001f, FVector::LeftVector);
		return FRotationMatrix::MakeFromZX(SafeZ, SafeX).Rotator();
	}

	void UpdatePose(AActor* Owner, USkeletalMeshComponent* SkelMesh, float DeltaTime, bool bIsAttacking, UStaticMeshComponent* EquippedWeaponMesh = nullptr)
	{
		if (!bInitialized || !Owner || !SkelMesh)
		{
			return;
		}

		if (ActiveSwingTime >= 0.0f)
		{
			ActiveSwingTime += DeltaTime;
			if (ActiveSwingTime > ActiveSwingDuration)
			{
				ActiveSwingTime = -1.0f;
			}
		}
		else if (bIsAttacking && bIsZombieRig)
		{
			ActiveSwingTime = 0.0f;
			ActiveSwingDuration = 0.52f;
		}

		if (ActiveParryFlashTime >= 0.0f)
		{
			ActiveParryFlashTime += DeltaTime;
			if (ActiveParryFlashTime > 0.42f)
			{
				ActiveParryFlashTime = -1.0f;
			}
		}

		const FVector Velocity = Owner->GetVelocity();
		const float Speed2D = Velocity.Size2D();
		if (Speed2D > 15.0f)
		{
			WalkCyclePhase += DeltaTime * FMath::Clamp(Speed2D / 38.0f, 3.5f, 14.0f);
		}

		FVector PelvisLoc = SkelMesh->GetSocketLocation(TEXT("pelvis"));
		FVector NeckLoc   = SkelMesh->GetSocketLocation(TEXT("neck_01"));
		FVector HeadLoc   = SkelMesh->GetSocketLocation(TEXT("head"));
		FVector ShldrLLoc = SkelMesh->GetSocketLocation(TEXT("upperarm_l"));
		FVector HandLLoc  = SkelMesh->GetSocketLocation(TEXT("hand_l"));
		FVector ShldrRLoc = SkelMesh->GetSocketLocation(TEXT("upperarm_r"));
		FVector HandRLoc  = SkelMesh->GetSocketLocation(TEXT("hand_r"));
		FVector HipLLoc   = SkelMesh->GetSocketLocation(TEXT("thigh_l"));
		FVector FootLLoc  = SkelMesh->GetSocketLocation(TEXT("foot_l"));
		FVector HipRLoc   = SkelMesh->GetSocketLocation(TEXT("thigh_r"));
		FVector FootRLoc  = SkelMesh->GetSocketLocation(TEXT("foot_r"));

		const FVector SkelArmLVec = HandLLoc - ShldrLLoc;
		const FVector SkelArmRVec = HandRLoc - ShldrRLoc;
		const FVector SkelLegLVec = FootLLoc - HipLLoc;
		const FVector SkelLegRVec = FootRLoc - HipRLoc;

		const bool bRagdoll = SkelMesh->IsSimulatingPhysics();

		// Boss ve uzun karakterlerde (BaseRigScale.Z > 1.05f) pelvis yuksekligini zemine gore olcekle ki bacaklar yere tam bassin
		if (!bRagdoll && !bIsDownedPose && BaseRigScale.Z > 1.05f)
		{
			PelvisLoc.Z += (BaseRigScale.Z - 1.0f) * 76.0f;
			NeckLoc.Z   += (BaseRigScale.Z - 1.0f) * 112.0f;
			HeadLoc.Z   += (BaseRigScale.Z - 1.0f) * 124.0f;
		}

		FVector FwdDir = Owner->GetActorForwardVector();
		FVector LeftDir = -Owner->GetActorRightVector();
		if (bRagdoll)
		{
			LeftDir = (ShldrLLoc - ShldrRLoc).GetSafeNormal(0.001f, LeftDir);
			const FVector UpSpine = (NeckLoc - PelvisLoc).GetSafeNormal(0.001f, FVector::UpVector);
			FwdDir = FVector::CrossProduct(UpSpine, LeftDir).GetSafeNormal(0.001f, FwdDir);
		}
		else if (!bIsDownedPose)
		{
			const FVector RawSpineUp = (NeckLoc - PelvisLoc).GetSafeNormal(0.001f, FVector::UpVector);
			const FVector StandTorsoUp = bIsZombieRig
				? RawSpineUp
				: (FVector::UpVector * 0.88f + RawSpineUp * 0.12f).GetSafeNormal();
			const float ShldrHeight = bIsZombieRig ? 39.0f : 35.5f;
			const float ShldrHalfWidth = bIsZombieRig ? 14.5f : 15.4f;
			ShldrLLoc = PelvisLoc + StandTorsoUp * (ShldrHeight * BaseRigScale.Z) + LeftDir * (ShldrHalfWidth * BaseRigScale.X);
			ShldrRLoc = PelvisLoc + StandTorsoUp * (ShldrHeight * BaseRigScale.Z) - LeftDir * (ShldrHalfWidth * BaseRigScale.X);
			HipLLoc   = PelvisLoc - StandTorsoUp * (6.0f * BaseRigScale.Z)  + LeftDir * (9.2f * BaseRigScale.X);
			HipRLoc   = PelvisLoc - StandTorsoUp * (6.0f * BaseRigScale.Z)  - LeftDir * (9.2f * BaseRigScale.X);
			if (!bIsZombieRig)
			{
				HandLLoc = ShldrLLoc + SkelArmLVec * BaseRigScale.Z;
				HandRLoc = ShldrRLoc + SkelArmRVec * BaseRigScale.Z;
				FootLLoc = HipLLoc   + SkelLegLVec * BaseRigScale.Z;
				FootRLoc = HipRLoc   + SkelLegRVec * BaseRigScale.Z;
			}
			else if (BaseRigScale.Z > 1.25f)
			{
				FootLLoc = HipLLoc - FVector(0.0f, 0.0f, 80.0f * BaseRigScale.Z) + FwdDir * (FMath::Sin(WalkCyclePhase) * 18.0f);
				FootRLoc = HipRLoc - FVector(0.0f, 0.0f, 80.0f * BaseRigScale.Z) - FwdDir * (FMath::Sin(WalkCyclePhase) * 18.0f);
			}
		}

		// 0. KÖYLÜ BAYGINLIK (DOWNED) YERDE YATMA / ÇÖKME POZU
		if (bIsDownedPose && !bRagdoll)
		{
			const float GroundZ = Owner->GetActorLocation().Z - 72.0f;
			const float Breathe = FMath::Sin(Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() * 3.0f : 0.0f) * 1.5f;
			PelvisLoc = FVector(PelvisLoc.X, PelvisLoc.Y, GroundZ + 12.0f);
			NeckLoc   = PelvisLoc + FwdDir * 42.0f + FVector(0.0f, 0.0f, 8.0f + Breathe);
			HeadLoc   = NeckLoc   + FwdDir * 20.0f + FVector(0.0f, 0.0f, 5.0f + Breathe);
			ShldrLLoc = NeckLoc   - FwdDir * 6.0f  + LeftDir * 14.0f;
			ShldrRLoc = NeckLoc   - FwdDir * 6.0f  - LeftDir * 14.0f;
			HandLLoc  = ShldrLLoc + FwdDir * 24.0f + LeftDir * 16.0f + FVector(0.0f, 0.0f, -6.0f);
			HandRLoc  = ShldrRLoc + FwdDir * 18.0f - LeftDir * 18.0f + FVector(0.0f, 0.0f, -6.0f);
			HipLLoc   = PelvisLoc + LeftDir * 9.0f;
			HipRLoc   = PelvisLoc - LeftDir * 9.0f;
			FootLLoc  = HipLLoc   - FwdDir * 48.0f + LeftDir * 6.0f  + FVector(0.0f, 0.0f, -4.0f);
			FootRLoc  = HipRLoc   - FwdDir * 46.0f - LeftDir * 8.0f  + FVector(0.0f, 0.0f, -4.0f);
		}

		// 1. ZOMBİ VE BOSS ÖZEL POSTÜRÜ VE SALDIRI ANİMASYONU
		if (bIsZombieRig && !bRagdoll)
		{
			const float ScaleM = BaseRigScale.Z;
			const float ShamblingSway = FMath::Sin(WalkCyclePhase * 0.85f) * 9.5f;
			const float LimpDip = FMath::Abs(FMath::Sin(WalkCyclePhase * 0.85f)) * 4.5f;
			PelvisLoc.Z -= LimpDip;
			NeckLoc += FwdDir * (14.0f * ScaleM) + LeftDir * (ShamblingSway * 0.55f) - FVector(0.0f, 0.0f, 4.0f * ScaleM + LimpDip);
			HeadLoc += FwdDir * (22.0f * ScaleM) + LeftDir * (ShamblingSway * 1.15f) - FVector(0.0f, 0.0f, 7.0f * ScaleM + LimpDip);

			if (ActiveSwingTime >= 0.0f)
			{
				const float Alpha = FMath::Clamp(ActiveSwingTime / ActiveSwingDuration, 0.0f, 1.0f);
				const float SlashArc = FMath::Sin(Alpha * PI);
				const float ChopHeight = FMath::Cos(Alpha * PI) * 40.0f * ScaleM;
				HandLLoc = ShldrLLoc + FwdDir * ((44.0f + SlashArc * 24.0f) * ScaleM) + FVector(0.0f, 0.0f, ChopHeight) + LeftDir * (6.0f * ScaleM);
				HandRLoc = ShldrRLoc + FwdDir * ((46.0f + SlashArc * 26.0f) * ScaleM) + FVector(0.0f, 0.0f, ChopHeight) - LeftDir * (6.0f * ScaleM);
			}
			else if (ScaleM > 1.25f)
			{
				// Boss durusu: Güçlü pehlivan / Treeguard omuz ve yumruk duruşu
				const float ArmBob = FMath::Sin(WalkCyclePhase) * 12.0f;
				HandLLoc = ShldrLLoc + FwdDir * (26.0f * ScaleM) + LeftDir * (12.0f * ScaleM) + FVector(0.0f, 0.0f, -32.0f * ScaleM + ArmBob);
				HandRLoc = ShldrRLoc + FwdDir * (30.0f * ScaleM) - LeftDir * (12.0f * ScaleM) + FVector(0.0f, 0.0f, -28.0f * ScaleM - ArmBob);
			}
			else
			{
				// Zombi pence / uzanma posturu (asimetrik one uzanan kollar ve topallayan adim)
				const float ArmBobL = FMath::Sin(WalkCyclePhase * 0.85f) * 11.0f;
				const float ArmBobR = FMath::Cos(WalkCyclePhase * 0.85f) * 9.0f;
				HandLLoc = ShldrLLoc + FwdDir * 48.0f + FVector(0.0f, 0.0f, -4.0f + ArmBobL) + LeftDir * 6.0f;
				HandRLoc = ShldrRLoc + FwdDir * 44.0f + FVector(0.0f, 0.0f, -14.0f + ArmBobR) - LeftDir * 5.0f;
				if (Speed2D > 12.0f)
				{
					const float LimpStride = FMath::Sin(WalkCyclePhase * 0.85f);
					FootLLoc = HipLLoc - FVector(0.0f, 0.0f, 76.0f * ScaleM) + FwdDir * (LimpStride * 22.0f) + LeftDir * 4.0f;
					FootRLoc = HipRLoc - FVector(0.0f, 0.0f, 78.0f * ScaleM) - FwdDir * (LimpStride * 14.0f) - LeftDir * 5.0f;
				}
			}
		}

		FVector WeaponShaftUp = (FwdDir * 0.78f + FVector::UpVector * 0.62f).GetSafeNormal();
		FVector WeaponBladeLeft = LeftDir;
		const FDSColonyAndWorldState* OwnerWS = FDSColonyAndWorldState::Find(Owner);
		const bool bUsingTwoHandedSword = (WeaponStyle == EDSCombatWeaponStyle::LivingDarkSword && OwnerWS && OwnerWS->bTwoHandedSwordMasteryUnlocked);

		// 1b. ÇİFT ELLİ KILIÇ BLOK DURUŞU ([F] Basılı) VE PARRY (SAVUŞTURMA) ANİMASYONU
		if (!bIsZombieRig && !bRagdoll && (bIsBlockingPose || ActiveParryFlashTime >= 0.0f) && ActiveSwingTime < 0.0f)
		{
			if (ActiveParryFlashTime >= 0.0f)
			{
				// Kusursuz Parry (Savusturma) Animasyonu: Cift elli kilici yukari-disari sertce savurarak darbeyi celme!
				const float PAlpha = FMath::Clamp(ActiveParryFlashTime / 0.42f, 0.0f, 1.0f);
				const float PArc = FMath::Sin(PAlpha * PI);
				HandRLoc = ShldrRLoc + FwdDir * (34.0f + PArc * 26.0f) - LeftDir * (14.0f * PArc) + FVector(0.0f, 0.0f, 22.0f + PArc * 28.0f);
				HandLLoc = HandRLoc + LeftDir * 12.0f - FVector(0.0f, 0.0f, 6.0f);
				WeaponShaftUp = (FVector::UpVector * 0.65f + LeftDir * (0.75f - PArc * 1.1f) + FwdDir * 0.35f).GetSafeNormal();
				WeaponBladeLeft = FwdDir;
			}
			else
			{
				// Cift Elli Kilic Blok Durusu (Guard Stance): Kilic gogus/bas onunde capraz siper edilir, iki el kabzayi kavrar
				HandRLoc = ShldrRLoc + FwdDir * 32.0f + LeftDir * 8.0f + FVector(0.0f, 0.0f, 14.0f);
				HandLLoc = ShldrLLoc + FwdDir * 30.0f - LeftDir * 4.0f + FVector(0.0f, 0.0f, 22.0f);
				WeaponShaftUp = (FVector::UpVector * 0.62f + LeftDir * 0.72f + FwdDir * 0.25f).GetSafeNormal();
				WeaponBladeLeft = FwdDir;
			}
		}

		// 2. OYUNCU BALTA SWING, BALYOZ, TEK/ÇİFT ELLİ KILIÇ VE YUMRUK KOMBO ANİMASYONLARI
		const bool bHasUALWeaponAnim = UseRealSkeletalAnimation() && WeaponStyle != EDSCombatWeaponStyle::UnarmedFists && !bIsZombieRig && !bRagdoll;
		if (!bIsZombieRig && !bRagdoll && ActiveSwingTime >= 0.0f && !bHasUALWeaponAnim)
		{
			const float Alpha = FMath::Clamp(ActiveSwingTime / ActiveSwingDuration, 0.0f, 1.0f);
			const float SwingT = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.1f);
			const float ArcSin = FMath::Sin(Alpha * PI);

			if (WeaponStyle == EDSCombatWeaponStyle::UnarmedFists)
			{
				const int32 ModCombo = ActiveComboIndex % 3;
				if (ModCombo == 0)
				{
					HandLLoc = ShldrLLoc + FwdDir * (22.0f + ArcSin * 50.0f) - LeftDir * (ArcSin * 12.0f) + FVector(0.0f, 0.0f, 5.0f);
					HandRLoc = ShldrRLoc + FwdDir * 24.0f + FVector(0.0f, 0.0f, 14.0f) + LeftDir * 8.0f;
					LeftDir = (LeftDir - FwdDir * (ArcSin * 0.45f)).GetSafeNormal();
				}
				else if (ModCombo == 1)
				{
					const float HookSide = FMath::Lerp(-36.0f, 24.0f, SwingT);
					HandRLoc = ShldrRLoc + FwdDir * (20.0f + ArcSin * 52.0f) + LeftDir * HookSide + FVector(0.0f, 0.0f, 8.0f);
					HandLLoc = ShldrLLoc + FwdDir * 22.0f + FVector(0.0f, 0.0f, 10.0f);
					LeftDir = (LeftDir + FwdDir * (ArcSin * 0.58f)).GetSafeNormal();
				}
				else
				{
					const float UpperZ = FMath::Lerp(-30.0f, 46.0f, SwingT);
					HandRLoc = ShldrRLoc + FwdDir * (24.0f + ArcSin * 42.0f) + FVector(0.0f, 0.0f, UpperZ) + LeftDir * 10.0f;
					HandLLoc = ShldrLLoc + FwdDir * 20.0f + FVector(0.0f, 0.0f, -6.0f);
					LeftDir = (LeftDir + FwdDir * (ArcSin * 0.50f)).GetSafeNormal();
				}
			}
			else if (WeaponStyle == EDSCombatWeaponStyle::WarHammer)
			{
				const float HammerZ = FMath::Lerp(58.0f, -46.0f, SwingT);
				const float HammerFwd = 22.0f + ArcSin * 46.0f;
				HandRLoc = ShldrRLoc + FwdDir * HammerFwd + LeftDir * 14.0f + FVector(0.0f, 0.0f, HammerZ);
				HandLLoc = HandRLoc + LeftDir * 8.0f;
				NeckLoc += FwdDir * (ArcSin * 12.0f);

				WeaponShaftUp = (FwdDir * (0.45f + ArcSin * 0.75f) + FVector(0.0f, 0.0f, FMath::Lerp(0.95f, -0.85f, SwingT))).GetSafeNormal();
				WeaponBladeLeft = LeftDir;
			}
			else if (bUsingTwoHandedSword)
			{
				// Korsan Boss'tan ogrenilen CIFT ELLI KILIC 3'LU KOMBO ANIMASYONU (Iki el kabzada kenetli!)
				const int32 ModCombo = ActiveComboIndex % 3;
				if (ModCombo == 0)
				{
					// Kombo 1: Genis Cift Elli Yatay Bicme (Great Cleave)
					const float SideOffset = FMath::Lerp(-62.0f, 56.0f, SwingT);
					const float HeightOffset = FMath::Lerp(24.0f, -12.0f, SwingT);
					const float ForwardReach = 26.0f + ArcSin * 54.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * SideOffset + FVector(0.0f, 0.0f, HeightOffset);
					HandLLoc = HandRLoc + LeftDir * 7.0f - FVector(0.0f, 0.0f, 5.0f);
					LeftDir = (LeftDir + FwdDir * FMath::Lerp(-0.60f, 0.65f, SwingT)).GetSafeNormal();
					WeaponShaftUp = (FwdDir * (0.45f + ArcSin * 0.90f) + LeftDir * FMath::Lerp(-0.92f, 0.92f, SwingT)).GetSafeNormal();
					WeaponBladeLeft = FVector::UpVector;
				}
				else if (ModCombo == 1)
				{
					// Kombo 2: Alttan Yukari Capraz Cift Elli Yukselen Kesme (Rising Gale Slash)
					const float SideOffset = FMath::Lerp(54.0f, -58.0f, SwingT);
					const float HeightOffset = FMath::Lerp(-32.0f, 44.0f, SwingT);
					const float ForwardReach = 28.0f + ArcSin * 56.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * SideOffset + FVector(0.0f, 0.0f, HeightOffset);
					HandLLoc = HandRLoc - LeftDir * 7.0f - FVector(0.0f, 0.0f, 5.0f);
					LeftDir = (LeftDir + FwdDir * FMath::Lerp(0.58f, -0.62f, SwingT)).GetSafeNormal();
					WeaponShaftUp = (FwdDir * (0.45f + ArcSin * 0.90f) + LeftDir * FMath::Lerp(0.88f, -0.88f, SwingT) + FVector(0.0f, 0.0f, FMath::Lerp(-0.45f, 0.55f, SwingT))).GetSafeNormal();
					WeaponBladeLeft = -FVector::UpVector;
				}
				else
				{
					// Kombo 3: Bas Ustu Cift Elli Korsan Infaz Indirmesi (Executioner Slam)
					const float ChopZ = FMath::Lerp(68.0f, -48.0f, SwingT);
					const float ForwardReach = 28.0f + ArcSin * 58.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * 12.0f + FVector(0.0f, 0.0f, ChopZ);
					HandLLoc = HandRLoc + LeftDir * 6.0f - FVector(0.0f, 0.0f, 6.0f);
					NeckLoc += FwdDir * (ArcSin * 14.0f);
					WeaponShaftUp = (FwdDir * (0.45f + ArcSin * 0.85f) + FVector(0.0f, 0.0f, FMath::Lerp(0.98f, -0.90f, SwingT))).GetSafeNormal();
					WeaponBladeLeft = LeftDir;
				}
			}
			else
			{
				// Tek Elli Kilic (Korsan Boss oncesi) ve Baltalar: Sag el savurur, sol el denge koludur
				const int32 ModCombo = ActiveComboIndex % 3;
				if (ModCombo == 0)
				{
					const float SideOffset = FMath::Lerp(-52.0f, 44.0f, SwingT);
					const float HeightOffset = FMath::Lerp(30.0f, -18.0f, SwingT);
					const float ForwardReach = 20.0f + ArcSin * 48.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * SideOffset + FVector(0.0f, 0.0f, HeightOffset);
					HandLLoc = ShldrLLoc + FwdDir * (14.0f - ArcSin * 8.0f) + LeftDir * 18.0f + FVector(0.0f, 0.0f, -12.0f);
					LeftDir = (LeftDir + FwdDir * FMath::Lerp(-0.48f, 0.58f, SwingT)).GetSafeNormal();

					WeaponShaftUp = (FwdDir * (0.40f + ArcSin * 0.85f) + LeftDir * FMath::Lerp(-0.85f, 0.85f, SwingT) + FVector(0.0f, 0.0f, FMath::Lerp(0.35f, -0.25f, SwingT))).GetSafeNormal();
					WeaponBladeLeft = FVector::UpVector;
				}
				else if (ModCombo == 1)
				{
					const float SideOffset = FMath::Lerp(42.0f, -52.0f, SwingT);
					const float HeightOffset = FMath::Lerp(-14.0f, 24.0f, SwingT);
					const float ForwardReach = 22.0f + ArcSin * 48.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * SideOffset + FVector(0.0f, 0.0f, HeightOffset);
					HandLLoc = ShldrLLoc + FwdDir * 12.0f + LeftDir * 20.0f + FVector(0.0f, 0.0f, -14.0f);
					LeftDir = (LeftDir + FwdDir * FMath::Lerp(0.48f, -0.52f, SwingT)).GetSafeNormal();

					WeaponShaftUp = (FwdDir * (0.40f + ArcSin * 0.85f) + LeftDir * FMath::Lerp(0.85f, -0.85f, SwingT) + FVector(0.0f, 0.0f, FMath::Lerp(-0.20f, 0.30f, SwingT))).GetSafeNormal();
					WeaponBladeLeft = -FVector::UpVector;
				}
				else
				{
					const float ChopZ = FMath::Lerp(56.0f, -42.0f, SwingT);
					const float ForwardReach = 24.0f + ArcSin * 50.0f;
					HandRLoc = ShldrRLoc + FwdDir * ForwardReach + LeftDir * 8.0f + FVector(0.0f, 0.0f, ChopZ);
					HandLLoc = (WeaponStyle == EDSCombatWeaponStyle::LivingDarkSword)
						? (ShldrLLoc + FwdDir * 14.0f + LeftDir * 18.0f + FVector(0.0f, 0.0f, -10.0f))
						: (HandRLoc + LeftDir * 10.0f + FVector(0.0f, 0.0f, 4.0f));
					LeftDir = (LeftDir + FwdDir * (ArcSin * 0.40f)).GetSafeNormal();

					WeaponShaftUp = (FwdDir * (0.40f + ArcSin * 0.80f) + FVector(0.0f, 0.0f, FMath::Lerp(0.95f, -0.85f, SwingT))).GetSafeNormal();
					WeaponBladeLeft = LeftDir;
				}
			}
		}

		UStaticMeshComponent* ActiveWMeshComp = EquippedWeaponMesh ? EquippedWeaponMesh : CachedWeaponMesh.Get();

		if (bUsingArgonSkeletalMesh)
		{
			if (ActiveWMeshComp && WeaponStyle != EDSCombatWeaponStyle::UnarmedFists)
			{
				const FVector WristRLoc = SkelMesh->GetSocketLocation(TEXT("hand_r"));
				const FVector ArmRUp = (ShldrRLoc - WristRLoc).GetSafeNormal(0.001f, FVector::UpVector);
				const FVector ArmRFwd = FVector::CrossProduct(ArmRUp, LeftDir).GetSafeNormal(0.001f, FwdDir);
				const FVector VisualPalm = WristRLoc + ArmRUp * (4.2f * BaseRigScale.Z) - ArmRFwd * (2.0f * BaseRigScale.Z) + LeftDir * (2.2f * BaseRigScale.X);
				const bool bUseCustomWeaponDir = (!bIsZombieRig && ((bIsBlockingPose || ActiveParryFlashTime >= 0.0f) || (ActiveSwingTime >= 0.0f && !bHasUALWeaponAnim)));
				const FVector ShaftDir = bUseCustomWeaponDir
					? WeaponShaftUp
					: (ArmRFwd * 0.82f + ArmRUp * 0.57f).GetSafeNormal(0.001f, FwdDir);
				const FVector FlatLeft = bUseCustomWeaponDir
					? WeaponBladeLeft
					: LeftDir;
				ActiveWMeshComp->SetWorldScale3D(FVector(BaseRigScale.Z));
				ActiveWMeshComp->SetWorldLocationAndRotation(VisualPalm, MakePartRotation(ShaftDir, FlatLeft));
			}
			return;
		}

		// 3. 6 PARÇALI İNSAN / BOSS / ZOMBİ GÖVDESİNİ KEMİK KONUMLARINA HİZALA
		const FVector RawTorsoUp = (NeckLoc - PelvisLoc).GetSafeNormal(0.001f, FVector::UpVector);
		const FVector TorsoUp = (!bIsZombieRig && !bRagdoll && !bIsDownedPose)
			? FVector::UpVector
			: RawTorsoUp;
		if (TorsoComp.IsValid())
		{
			TorsoComp->SetWorldLocationAndRotation(PelvisLoc, MakePartRotation(TorsoUp, LeftDir));
		}

		if (HeadComp.IsValid())
		{
			const FVector HeadUp = (bRagdoll || bIsDownedPose)
				? (HeadLoc - NeckLoc).GetSafeNormal(0.001f, TorsoUp)
				: (bIsZombieRig ? (TorsoUp * 0.82f + FwdDir * 0.18f).GetSafeNormal() : TorsoUp);
			const float HeadHeightOffset = bIsZombieRig ? 46.0f : 40.0f;
			const FVector HeadOrigin = (!bRagdoll && !bIsDownedPose)
				? (PelvisLoc + TorsoUp * (HeadHeightOffset * BaseRigScale.Z))
				: (NeckLoc - HeadUp * 4.0f);
			HeadComp->SetWorldLocationAndRotation(HeadOrigin, MakePartRotation(HeadUp, LeftDir));

			// Aycicegi Boss'un ustundeki aycicekleri sabah/gunduz gunes yonunu (Heliotropism) takip eder!
			if (ExtraCrownComp.IsValid())
			{
				const float TimeSec = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0.0f;
				// Sabah/oglen gunes acisi (dogu-guneydogu gokyuzu yonu + hafif gunes salinimi)
				const FVector MorningSunDir = FVector(-0.62f, -0.45f + FMath::Sin(TimeSec * 0.35f) * 0.12f, 0.64f).GetSafeNormal();
				const FVector CrownPos = HeadOrigin + FVector(0.0f, 0.0f, 18.0f * BaseRigScale.Z) - FwdDir * 6.0f;
				ExtraCrownComp->SetWorldLocationAndRotation(CrownPos, MorningSunDir.Rotation());
			}
		}

		if (ArmLComp.IsValid())
		{
			const FVector ArmLUp = (ShldrLLoc - HandLLoc).GetSafeNormal();
			ArmLComp->SetWorldLocationAndRotation(ShldrLLoc, MakePartRotation(ArmLUp, LeftDir));
		}

		if (ArmRComp.IsValid())
		{
			const FVector ArmRUp = (ShldrRLoc - HandRLoc).GetSafeNormal(0.001f, FVector::UpVector);
			ArmRComp->SetWorldLocationAndRotation(ShldrRLoc, MakePartRotation(ArmRUp, LeftDir));
			if (ActiveWMeshComp && WeaponStyle != EDSCombatWeaponStyle::UnarmedFists)
			{
				// Tum silah modellerinde (0,0,0) noktasi dogrudan sag el kabza kavrayis merkezidir,
				// +Z ekseni sap/namlu uzunlugu, +Y ekseni kesici agiz yonudur.
				const FVector VisualPalm = ArmRComp->GetComponentTransform().TransformPosition(FVector(0.15f, 3.25f, -52.0f));
				const FVector ArmRFwd = FVector::CrossProduct(ArmRUp, LeftDir).GetSafeNormal(0.001f, FwdDir);
				const FVector ShaftDir = (!bIsZombieRig && (bIsBlockingPose || ActiveParryFlashTime >= 0.0f) && ActiveSwingTime < 0.0f)
					? WeaponShaftUp
					: (ArmRFwd * 0.82f + ArmRUp * 0.57f).GetSafeNormal(0.001f, FwdDir);
				const FVector FlatLeft = (!bIsZombieRig && (bIsBlockingPose || ActiveParryFlashTime >= 0.0f) && ActiveSwingTime < 0.0f)
					? WeaponBladeLeft
					: LeftDir;
				ActiveWMeshComp->SetWorldScale3D(FVector(BaseRigScale.Z));
				ActiveWMeshComp->SetWorldLocationAndRotation(VisualPalm, MakePartRotation(ShaftDir, FlatLeft));
			}
		}

		if (LegLComp.IsValid())
		{
			const FVector LegLUp = (HipLLoc - FootLLoc).GetSafeNormal();
			LegLComp->SetWorldLocationAndRotation(HipLLoc, MakePartRotation(LegLUp, LeftDir));
		}

		if (LegRComp.IsValid())
		{
			const FVector LegRUp = (HipRLoc - FootRLoc).GetSafeNormal();
			LegRComp->SetWorldLocationAndRotation(HipRLoc, MakePartRotation(LegRUp, LeftDir));
		}
	}
};
