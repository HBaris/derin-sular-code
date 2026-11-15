// Copyright Epic Games, Inc. All Rights Reserved.


#include "DSHUDWidget.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "DSWaveManager.h"
#include "CombatGameMode.h"
#include "CombatCharacter.h"
#include "DSTrustComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "DSVillager.h"
#include "DSBarikat.h"
#include "CombatEnemy.h"
#include "DSCharacterBodyRig.h"
#include "DSMeleeWeapon.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"

namespace
{
	constexpr int32 HUDDebugKey_OyuncuCani = 1;
	constexpr int32 HUDDebugKey_KalanDusman = 2;
	constexpr int32 HUDDebugKey_Dalga = 3;
	constexpr int32 HUDDebugKey_Skor = 4;
	constexpr int32 HUDDebugKey_Guven = 5;

	TWeakObjectPtr<UTextBlock> GColonyStatusText;
	TWeakObjectPtr<UTextBlock> GLeftTacticalBadgeText;
	TWeakObjectPtr<UTextBlock> GResWoodText;
	TWeakObjectPtr<UTextBlock> GResIronText;
	TWeakObjectPtr<UTextBlock> GResSteelText;
	TWeakObjectPtr<UTextBlock> GResSaltText;
	TWeakObjectPtr<UTextBlock> GResBagText;
	TWeakObjectPtr<UTextBlock> GActiveWeaponTitleText;
	TWeakObjectPtr<UTextBlock> GActiveWeaponSubText;
	TWeakObjectPtr<UBorder> GBossBannerBorder;
	TWeakObjectPtr<UTextBlock> GBossNameText;
	TWeakObjectPtr<UTextBlock> GBossHPText;
	TWeakObjectPtr<UProgressBar> GBossHealthBar;
	TWeakObjectPtr<UBorder> GGuideBanner;
	TWeakObjectPtr<UTextBlock> GGuideTitleText;
	TWeakObjectPtr<UTextBlock> GGuideBodyText;
	TWeakObjectPtr<UBorder> GSteamAchBanner;
	TWeakObjectPtr<UTextBlock> GSteamAchTitleText;
	TWeakObjectPtr<UTextBlock> GSteamAchDescText;

	void StyleHudText(UTextBlock* Txt, int32 FontSize, const FLinearColor& Color, bool bBold = true)
	{
		if (!Txt)
		{
			return;
		}
		Txt->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo FontInfo = Txt->GetFont();
		FontInfo.Size = FontSize;
		FontInfo.TypefaceFontName = bBold ? FName("Bold") : FName("Regular");
		Txt->SetFont(FontInfo);
		Txt->SetShadowOffset(FVector2D(1.25f, 1.25f));
		Txt->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));
	}

	void ApplyCustomProgressBarStyle(UProgressBar* Bar, const FLinearColor& TroughColor, const FLinearColor& FillColor)
	{
		if (!Bar)
		{
			return;
		}
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
		Style.BackgroundImage.TintColor = FSlateColor(TroughColor);
		Style.FillImage.DrawAs = ESlateBrushDrawType::Box;
		Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(FillColor);
		Bar->SetBorderPadding(FVector2D(1.0f, 1.0f));
	}

	UBorder* CreateFramedPanel(
		UWidgetTree* WT,
		UCanvasPanel* Root,
		const FName& OuterName,
		const FName& InnerName,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FVector2D& Pos,
		const FVector2D& Size,
		bool bAutoSize,
		const FLinearColor& TrimColor,
		const FLinearColor& InnerBgColor,
		const FMargin& InnerPadding,
		UBorder*& OutOuterBorder)
	{
		UBorder* Outer = WT->ConstructWidget<UBorder>(UBorder::StaticClass(), OuterName);
		Outer->SetBrushColor(TrimColor);
		Outer->SetPadding(FMargin(1.5f));
		if (UCanvasPanelSlot* CSlot = Root->AddChildToCanvas(Outer))
		{
			CSlot->SetAnchors(Anchors);
			CSlot->SetAlignment(Alignment);
			CSlot->SetPosition(Pos);
			if (bAutoSize)
			{
				CSlot->SetAutoSize(true);
			}
			else
			{
				CSlot->SetSize(Size);
			}
		}
		UBorder* Inner = WT->ConstructWidget<UBorder>(UBorder::StaticClass(), InnerName);
		Inner->SetBrushColor(InnerBgColor);
		Inner->SetPadding(InnerPadding);
		Outer->SetContent(Inner);
		OutOuterBorder = Outer;
		return Inner;
	}

	UTextBlock* CreateResourcePill(
		UWidgetTree* WT,
		UHorizontalBox* ParentHBox,
		const FName& PillName,
		const FString& InitialText,
		const FLinearColor& TrimAccent,
		const FLinearColor& TextColor)
	{
		UBorder* PillTrim = WT->ConstructWidget<UBorder>(UBorder::StaticClass(), *(PillName.ToString() + TEXT("_Trim")));
		PillTrim->SetBrushColor(TrimAccent);
		PillTrim->SetPadding(FMargin(1.0f));
		if (UHorizontalBoxSlot* HSlot = ParentHBox->AddChildToHorizontalBox(PillTrim))
		{
			HSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* PillBg = WT->ConstructWidget<UBorder>(UBorder::StaticClass(), *(PillName.ToString() + TEXT("_Bg")));
		PillBg->SetBrushColor(FLinearColor(0.015f, 0.022f, 0.035f, 0.94f));
		PillBg->SetPadding(FMargin(7.0f, 3.0f));
		PillTrim->SetContent(PillBg);

		UTextBlock* PillTxt = WT->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), PillName);
		PillTxt->SetText(FText::FromString(InitialText));
		StyleHudText(PillTxt, 9, TextColor, true);
		PillBg->SetContent(PillTxt);
		return PillTxt;
	}

	void SetGuideMessageBox(const FString& Title, const FString& Body, const FLinearColor& AccentColor)
	{
		if (GGuideBanner.IsValid())
		{
			GGuideBanner->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (GGuideTitleText.IsValid())
		{
			GGuideTitleText->SetText(FText::FromString(Title));
			GGuideTitleText->SetColorAndOpacity(FSlateColor(AccentColor));
		}
		if (GGuideBodyText.IsValid())
		{
			GGuideBodyText->SetText(FText::FromString(Body));
		}
	}
}

TSharedRef<SWidget> UDSHUDWidget::RebuildWidget()
{
	if (WidgetTree && !HealthBarWidget)
	{
		UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!RootCanvas)
		{
			RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WidgetTree->RootWidget = RootCanvas;
		}
		else
		{
			for (UWidget* ExistingChild : RootCanvas->GetAllChildren())
			{
				if (ExistingChild)
				{
					ExistingChild->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}

		const FLinearColor BronzeTrim(0.58f, 0.45f, 0.22f, 0.82f);
		const FLinearColor ObsidianBg(0.014f, 0.020f, 0.032f, 0.90f);
		const FLinearColor DarkTrough(0.04f, 0.05f, 0.08f, 0.96f);

		// =========================================================================================
		// 1. SOL UST PANEL: Karakter Durumu (Can Barı, Koloni Güveni ve Taktik Rozet Şeridi)
		// =========================================================================================
		UBorder* LeftOuter = nullptr;
		UBorder* LeftInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("LeftStatusOuter"), TEXT("LeftStatusInner"),
			FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f),
			FVector2D(20.0f, 18.0f), FVector2D(316.0f, 106.0f), /*bAutoSize=*/false,
			BronzeTrim, ObsidianBg, FMargin(12.0f, 9.0f), LeftOuter);

		UVerticalBox* LeftBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftStatusVBox"));
		LeftInner->SetContent(LeftBox);

		// Can Satiri (Sol: Etiket, Sag: Deger)
		UHorizontalBox* HealthHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HealthHeaderRow"));
		if (UVerticalBoxSlot* VSlot = LeftBox->AddChildToVerticalBox(HealthHeaderRow))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}
		UTextBlock* HealthLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthLabel"));
		HealthLabel->SetText(FText::FromString(TEXT("YAŞAM GÜCÜ")));
		StyleHudText(HealthLabel, 10, FLinearColor(0.92f, 0.90f, 0.84f, 0.95f), true);
		if (UHorizontalBoxSlot* HSlot = HealthHeaderRow->AddChildToHorizontalBox(HealthLabel))
		{
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetHorizontalAlignment(HAlign_Left);
		}

		HealthTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
		HealthTextWidget->SetText(FText::FromString(TEXT("100%")));
		StyleHudText(HealthTextWidget, 10, FLinearColor(0.35f, 0.95f, 0.48f, 1.0f), true);
		if (UHorizontalBoxSlot* HSlot = HealthHeaderRow->AddChildToHorizontalBox(HealthTextWidget))
		{
			HSlot->SetHorizontalAlignment(HAlign_Right);
		}

		USizeBox* HealthBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSizeBox"));
		HealthBarSize->SetHeightOverride(9.0f);
		if (UVerticalBoxSlot* VSlot = LeftBox->AddChildToVerticalBox(HealthBarSize))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
		HealthBarWidget = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
		HealthBarWidget->SetPercent(1.0f);
		ApplyCustomProgressBarStyle(HealthBarWidget, DarkTrough, FLinearColor(0.18f, 0.82f, 0.34f, 1.0f));
		HealthBarSize->SetContent(HealthBarWidget);

		// Guven Satiri (Sol: Etiket, Sag: Deger)
		UHorizontalBox* TrustHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TrustHeaderRow"));
		if (UVerticalBoxSlot* VSlot = LeftBox->AddChildToVerticalBox(TrustHeaderRow))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}
		UTextBlock* TrustLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TrustLabel"));
		TrustLabel->SetText(FText::FromString(TEXT("KOLONİ GÜVENİ")));
		StyleHudText(TrustLabel, 9, FLinearColor(0.68f, 0.86f, 0.98f, 0.95f), true);
		if (UHorizontalBoxSlot* HSlot = TrustHeaderRow->AddChildToHorizontalBox(TrustLabel))
		{
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetHorizontalAlignment(HAlign_Left);
		}

		TrustTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TrustText"));
		TrustTextWidget->SetText(FText::FromString(TEXT("50 • Tarafsiz")));
		StyleHudText(TrustTextWidget, 9, FLinearColor(0.98f, 0.82f, 0.38f, 1.0f), true);
		if (UHorizontalBoxSlot* HSlot = TrustHeaderRow->AddChildToHorizontalBox(TrustTextWidget))
		{
			HSlot->SetHorizontalAlignment(HAlign_Right);
		}

		USizeBox* TrustBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TrustBarSizeBox"));
		TrustBarSize->SetHeightOverride(6.0f);
		if (UVerticalBoxSlot* VSlot = LeftBox->AddChildToVerticalBox(TrustBarSize))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		}
		TrustBarWidget = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TrustBar"));
		TrustBarWidget->SetPercent(0.5f);
		ApplyCustomProgressBarStyle(TrustBarWidget, DarkTrough, FLinearColor(0.22f, 0.68f, 0.96f, 1.0f));
		TrustBarSize->SetContent(TrustBarWidget);

		// Alt Taktik Rozet Seridi (Emir, Yol, Ani Arsivi)
		UTextBlock* LeftTacticalBadge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LeftTacticalBadgeText"));
		LeftTacticalBadge->SetText(FText::FromString(TEXT("EMİR [V]: SAVAŞ   •   YOL: Tuz İttifakı   •   ANI: 3/3")));
		StyleHudText(LeftTacticalBadge, 8, FLinearColor(0.84f, 0.78f, 0.62f, 0.94f), false);
		LeftBox->AddChildToVerticalBox(LeftTacticalBadge);
		GLeftTacticalBadgeText = LeftTacticalBadge;

		// =========================================================================================
		// 2. UST ORTA PANEL: Dalga Sancağı + Dinamik Boss Can Barı
		// =========================================================================================
		UBorder* TopCrestOuter = nullptr;
		UBorder* TopCrestInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("TopCrestOuter"), TEXT("TopCrestInner"),
			FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f),
			FVector2D(0.0f, 18.0f), FVector2D(360.0f, 36.0f), /*bAutoSize=*/true,
			BronzeTrim, ObsidianBg, FMargin(16.0f, 6.0f), TopCrestOuter);

		UHorizontalBox* CrestHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopCrestHBox"));
		TopCrestInner->SetContent(CrestHBox);

		WaveTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WaveText"));
		WaveTextWidget->SetText(FText::FromString(TEXT("DALGA I")));
		StyleHudText(WaveTextWidget, 11, FLinearColor(1.0f, 0.80f, 0.28f, 1.0f), true);
		if (UHorizontalBoxSlot* HSlot = CrestHBox->AddChildToHorizontalBox(WaveTextWidget))
		{
			HSlot->SetVerticalAlignment(VAlign_Center);
			HSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}

		RemainingTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RemainingText"));
		RemainingTextWidget->SetText(FText::FromString(TEXT("TEHDİT: 0")));
		StyleHudText(RemainingTextWidget, 10, FLinearColor(0.96f, 0.42f, 0.36f, 1.0f), true);
		if (UHorizontalBoxSlot* HSlot = CrestHBox->AddChildToHorizontalBox(RemainingTextWidget))
		{
			HSlot->SetVerticalAlignment(VAlign_Center);
			HSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		}

		ScoreTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreText"));
		ScoreTextWidget->SetText(FText::FromString(TEXT("SKOR: 0")));
		StyleHudText(ScoreTextWidget, 10, FLinearColor(0.88f, 0.92f, 0.98f, 0.95f), true);
		if (UHorizontalBoxSlot* HSlot = CrestHBox->AddChildToHorizontalBox(ScoreTextWidget))
		{
			HSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Ust Orta Dinamik Boss Can Bari (Sadece Boss yakindayken veya savastayken acilir)
		UBorder* BossOuter = nullptr;
		UBorder* BossInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("BossBarOuter"), TEXT("BossBarInner"),
			FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f),
			FVector2D(0.0f, 56.0f), FVector2D(410.0f, 38.0f), /*bAutoSize=*/false,
			FLinearColor(0.78f, 0.32f, 0.18f, 0.88f), FLinearColor(0.03f, 0.015f, 0.02f, 0.92f),
			FMargin(12.0f, 5.0f), BossOuter);
		BossOuter->SetVisibility(ESlateVisibility::Hidden);
		GBossBannerBorder = BossOuter;

		UVerticalBox* BossVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BossBarVBox"));
		BossInner->SetContent(BossVBox);

		UHorizontalBox* BossHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BossHeaderRow"));
		if (UVerticalBoxSlot* VSlot = BossVBox->AddChildToVerticalBox(BossHeader))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}
		UTextBlock* BossNameTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossNameText"));
		BossNameTxt->SetText(FText::FromString(TEXT("AYÇİÇEĞİ TARLASININ KORUYUCUSU")));
		StyleHudText(BossNameTxt, 9, FLinearColor(1.0f, 0.84f, 0.36f, 1.0f), true);
		if (UHorizontalBoxSlot* HSlot = BossHeader->AddChildToHorizontalBox(BossNameTxt))
		{
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetHorizontalAlignment(HAlign_Left);
		}
		GBossNameText = BossNameTxt;

		UTextBlock* BossHPTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossHPText"));
		BossHPTxt->SetText(FText::FromString(TEXT("850 / 850")));
		StyleHudText(BossHPTxt, 9, FLinearColor(0.95f, 0.88f, 0.82f, 0.95f), true);
		if (UHorizontalBoxSlot* HSlot = BossHeader->AddChildToHorizontalBox(BossHPTxt))
		{
			HSlot->SetHorizontalAlignment(HAlign_Right);
		}
		GBossHPText = BossHPTxt;

		USizeBox* BossBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BossBarSizeBox"));
		BossBarSize->SetHeightOverride(7.0f);
		BossVBox->AddChildToVerticalBox(BossBarSize);

		UProgressBar* BossBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BossHealthBar"));
		BossBar->SetPercent(1.0f);
		ApplyCustomProgressBarStyle(BossBar, DarkTrough, FLinearColor(0.88f, 0.22f, 0.16f, 1.0f));
		BossBarSize->SetContent(BossBar);
		GBossHealthBar = BossBar;

		// =========================================================================================
		// 3. SAG UST PANEL: Koloni / Kadim Kaynak Rozetleri & Koloni/Filo Durumu
		// =========================================================================================
		UBorder* RightOuter = nullptr;
		UBorder* RightInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("RightResourceOuter"), TEXT("RightResourceInner"),
			FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(-20.0f, 18.0f), FVector2D(390.0f, 66.0f), /*bAutoSize=*/true,
			BronzeTrim, ObsidianBg, FMargin(10.0f, 8.0f), RightOuter);

		UVerticalBox* RightBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightResourceVBox"));
		RightInner->SetContent(RightBox);

		UHorizontalBox* ResourcePillsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResourcePillsRow"));
		if (UVerticalBoxSlot* VSlot = RightBox->AddChildToVerticalBox(ResourcePillsRow))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		GResWoodText  = CreateResourcePill(WidgetTree, ResourcePillsRow, TEXT("ResWoodPill"),  TEXT("ODUN: 12"),   FLinearColor(0.72f, 0.48f, 0.20f, 0.85f), FLinearColor(0.96f, 0.82f, 0.52f, 1.0f));
		GResIronText  = CreateResourcePill(WidgetTree, ResourcePillsRow, TEXT("ResIronPill"),  TEXT("DEMİR: 5"),   FLinearColor(0.52f, 0.58f, 0.66f, 0.85f), FLinearColor(0.86f, 0.90f, 0.95f, 1.0f));
		GResSteelText = CreateResourcePill(WidgetTree, ResourcePillsRow, TEXT("ResSteelPill"), TEXT("ÇELİK: 2"),   FLinearColor(0.35f, 0.68f, 0.92f, 0.85f), FLinearColor(0.68f, 0.90f, 1.0f, 1.0f));
		GResSaltText  = CreateResourcePill(WidgetTree, ResourcePillsRow, TEXT("ResSaltPill"),  TEXT("TUZ: 8"),     FLinearColor(0.32f, 0.82f, 0.88f, 0.85f), FLinearColor(0.84f, 0.98f, 1.0f, 1.0f));
		GResBagText   = CreateResourcePill(WidgetTree, ResourcePillsRow, TEXT("ResBagPill"),   TEXT("ÇANTA: 0/24"),FLinearColor(0.66f, 0.54f, 0.30f, 0.85f), FLinearColor(0.95f, 0.88f, 0.70f, 1.0f));

		UTextBlock* ColonyStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ColonyStatusText"));
		ColonyStatus->SetText(FText::FromString(TEXT("ŞANTİYE: Demirci Ocağı   •   KÜTÜPHANE: 4/4 Tarif   •   FİLO: Amiral Gemisi")));
		StyleHudText(ColonyStatus, 8, FLinearColor(0.86f, 0.82f, 0.70f, 0.94f), false);
		RightBox->AddChildToVerticalBox(ColonyStatus);
		GColonyStatusText = ColonyStatus;

		// =========================================================================================
		// 4. SOL ALT PANEL: Silah Yuvasi & RPG Kisayol Bari (Hotbar)
		// =========================================================================================
		UBorder* HotbarOuter = nullptr;
		UBorder* HotbarInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("BottomLeftHotbarOuter"), TEXT("BottomLeftHotbarInner"),
			FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f),
			FVector2D(20.0f, -18.0f), FVector2D(292.0f, 68.0f), /*bAutoSize=*/true,
			BronzeTrim, ObsidianBg, FMargin(10.0f, 7.0f), HotbarOuter);

		UVerticalBox* HotbarVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HotbarVBox"));
		HotbarInner->SetContent(HotbarVBox);

		UTextBlock* ActiveWeaponTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActiveWeaponTitleText"));
		ActiveWeaponTitle->SetText(FText::FromString(TEXT("SİLAH: [1] Çift Yüzlü Balta")));
		StyleHudText(ActiveWeaponTitle, 10, FLinearColor(1.0f, 0.84f, 0.36f, 1.0f), true);
		if (UVerticalBoxSlot* VSlot = HotbarVBox->AddChildToVerticalBox(ActiveWeaponTitle))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
		GActiveWeaponTitleText = ActiveWeaponTitle;

		UTextBlock* ActiveWeaponSub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActiveWeaponSubText"));
		ActiveWeaponSub->SetText(FText::FromString(TEXT("[Sol Tık] Kombo  •  [Sağ Tık] Ağır  •  [F] Blok/Parry")));
		StyleHudText(ActiveWeaponSub, 8, FLinearColor(0.76f, 0.84f, 0.92f, 0.92f), false);
		if (UVerticalBoxSlot* VSlot = HotbarVBox->AddChildToVerticalBox(ActiveWeaponSub))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
		GActiveWeaponSubText = ActiveWeaponSub;

		UHorizontalBox* KeycapsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("KeycapsRow"));
		HotbarVBox->AddChildToVerticalBox(KeycapsRow);
		const FLinearColor KeycapTrim(0.45f, 0.50f, 0.58f, 0.78f);
		const FLinearColor KeycapText(0.84f, 0.88f, 0.94f, 0.95f);
		CreateResourcePill(WidgetTree, KeycapsRow, TEXT("KeyQ"), TEXT("[Q] Silah"),     KeycapTrim, KeycapText);
		CreateResourcePill(WidgetTree, KeycapsRow, TEXT("Key4"), TEXT("[4] Balyoz"),    KeycapTrim, KeycapText);
		CreateResourcePill(WidgetTree, KeycapsRow, TEXT("KeyE"), TEXT("[E] Etkileşim"), KeycapTrim, KeycapText);
		CreateResourcePill(WidgetTree, KeycapsRow, TEXT("KeyN"), TEXT("[N] İnşaat"),    KeycapTrim, KeycapText);

		// =========================================================================================
		// 5. SAG ALT: Basarim (Achievement) Bildirim Karti
		// =========================================================================================
		UBorder* SteamAchOuter = nullptr;
		UBorder* SteamAchInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("SteamAchievementOuter"), TEXT("SteamAchievementInner"),
			FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f),
			FVector2D(-20.0f, -96.0f), FVector2D(310.0f, 62.0f), /*bAutoSize=*/true,
			FLinearColor(0.92f, 0.74f, 0.22f, 0.90f), FLinearColor(0.04f, 0.07f, 0.12f, 0.95f),
			FMargin(12.0f, 8.0f), SteamAchOuter);
		SteamAchOuter->SetVisibility(ESlateVisibility::Hidden);
		GSteamAchBanner = SteamAchOuter;

		UVerticalBox* AchVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SteamAchievementVBox"));
		SteamAchInner->SetContent(AchVBox);

		UTextBlock* AchTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SteamAchievementTitle"));
		AchTitle->SetText(FText::FromString(TEXT("BAŞARIM AÇILDI: TABANLARI YAĞLAMAK")));
		StyleHudText(AchTitle, 10, FLinearColor(1.0f, 0.84f, 0.24f, 1.0f), true);
		AchVBox->AddChildToVerticalBox(AchTitle);
		GSteamAchTitleText = AchTitle;

		UTextBlock* AchDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SteamAchievementDesc"));
		AchDesc->SetText(FText::FromString(TEXT("Sıvı Yağ Sürülmüş Ayakkabı kuşanıldı (+%36 Hareket Hızı)")));
		StyleHudText(AchDesc, 8, FLinearColor(0.88f, 0.94f, 1.0f, 0.95f), false);
		AchDesc->SetAutoWrapText(true);
		AchDesc->SetWrapTextAt(285.0f);
		AchVBox->AddChildToVerticalBox(AchDesc);
		GSteamAchDescText = AchDesc;

		// =========================================================================================
		// 6. ALT ORTA PANEL: Akıl Hocası & Vicdan Seçim Diyalog Kutusu (Sıfır Taşma + Gerçek Portre)
		// =========================================================================================
		UBorder* GuideOuter = nullptr;
		UBorder* GuideInner = CreateFramedPanel(
			WidgetTree, RootCanvas,
			TEXT("GuideMessageBoxOuter"), TEXT("GuideMessageBoxInner"),
			FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f),
			FVector2D(18.0f, -18.0f), FVector2D(610.0f, 70.0f), /*bAutoSize=*/true,
			BronzeTrim, FLinearColor(0.012f, 0.018f, 0.030f, 0.92f),
			FMargin(12.0f, 8.0f), GuideOuter);
		GGuideBanner = GuideOuter;

		UHorizontalBox* GuideHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GuideMentorHBox"));
		GuideInner->SetContent(GuideHBox);

		// Sol Portre Cercevesi: /Game/Colony/Mentor/T_FatherPortrait
		UBorder* PortraitFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MentorFatherPortraitFrame"));
		PortraitFrame->SetBrushColor(FLinearColor(0.78f, 0.62f, 0.28f, 0.92f));
		PortraitFrame->SetPadding(FMargin(2.0f));
		if (UHorizontalBoxSlot* PSlot = GuideHBox->AddChildToHorizontalBox(PortraitFrame))
		{
			PSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			PSlot->SetVerticalAlignment(VAlign_Center);
			PSlot->SetHorizontalAlignment(HAlign_Center);
			PSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		UImage* FatherPortraitImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MentorFatherPortraitImage"));
		FatherPortraitImg->SetDesiredSizeOverride(FVector2D(48.0f, 48.0f));
		if (UTexture2D* FatherTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Colony/Mentor/T_FatherPortrait.T_FatherPortrait")))
		{
			FatherPortraitImg->SetBrushFromTexture(FatherTex, false);
			FatherPortraitImg->SetDesiredSizeOverride(FVector2D(48.0f, 48.0f));
			FatherPortraitImg->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			FatherPortraitImg->SetColorAndOpacity(FLinearColor(0.16f, 0.22f, 0.30f, 0.95f));
		}
		PortraitFrame->SetContent(FatherPortraitImg);

		UVerticalBox* GuideBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GuideMessageVBox"));
		if (UHorizontalBoxSlot* TextHSlot = GuideHBox->AddChildToHorizontalBox(GuideBox))
		{
			TextHSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* GuideTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GuideTitleText"));
		GuideTitle->SetText(FText::FromString(TEXT("AKIL HOCASI")));
		StyleHudText(GuideTitle, 9, FLinearColor(0.98f, 0.82f, 0.38f, 0.98f), true);
		if (UVerticalBoxSlot* VSlot = GuideBox->AddChildToVerticalBox(GuideTitle))
		{
			VSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
		GGuideTitleText = GuideTitle;

		UTextBlock* GuideBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GuideBodyText"));
		GuideBody->SetText(FText::FromString(TEXT("\"Adımlarını sağlam bas evlat; ulu ağaçların yamacındaki körpe fidanlara kıyma, toprağa tohum [C] bırakmayı unutma.\"")));
		StyleHudText(GuideBody, 9, FLinearColor(0.92f, 0.94f, 0.98f, 0.96f), false);
		GuideBody->SetAutoWrapText(true);
		GuideBody->SetWrapTextAt(515.0f);
		GuideBox->AddChildToVerticalBox(GuideBody);
		GGuideBodyText = GuideBody;

		// =========================================================================================
		// 7. ORTA EKRAN: Oyun Bitti / Zafer Sancağı
		// =========================================================================================
		GameOverBannerWidget = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameOverBanner"));
		GameOverBannerWidget->SetBrushColor(FLinearColor(0.05f, 0.02f, 0.02f, 0.92f));
		GameOverBannerWidget->SetPadding(FMargin(28.0f, 20.0f));
		GameOverBannerWidget->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* CenterSlot = RootCanvas->AddChildToCanvas(GameOverBannerWidget))
		{
			CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CenterSlot->SetPosition(FVector2D(0.0f, -40.0f));
			CenterSlot->SetAutoSize(true);
		}

		GameOverTextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameOverText"));
		GameOverTextWidget->SetText(FText::GetEmpty());
		StyleHudText(GameOverTextWidget, 22, FLinearColor(1.0f, 0.85f, 0.25f, 1.0f), true);
		GameOverBannerWidget->SetContent(GameOverTextWidget);
	}

	return Super::RebuildWidget();
}

void UDSHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ADSWaveManager* WaveManager = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ADSWaveManager::StaticClass())))
	{
		WaveManager->OnWaveChanged.AddDynamic(this, &UDSHUDWidget::HandleWaveChanged);
		if (WaveManager->CurrentWaveNumber > 0)
		{
			HandleWaveChanged(WaveManager->CurrentWaveNumber, 0);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDSHUDWidget: sahnede DSWaveManager bulunamadi, dalga bilgisi guncellenmeyecek"));
	}

	if (ACombatGameMode* GameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		GameMode->OnScoreChanged.AddDynamic(this, &UDSHUDWidget::HandleScoreChanged);
		GameMode->OnPlayerDied.AddDynamic(this, &UDSHUDWidget::HandlePlayerDied);
		GameMode->OnGameWon.AddDynamic(this, &UDSHUDWidget::HandleGameWon);

		if (GameMode->TrustComponent)
		{
			GameMode->TrustComponent->OnTrustChanged.AddDynamic(this, &UDSHUDWidget::HandleTrustChanged);
			HandleTrustChanged(GameMode->TrustComponent->GetTrustValue(), GameMode->TrustComponent->GetTrustName());
		}
	}

	RefreshPolledValues();
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &UDSHUDWidget::RefreshPolledValues, PollAraligi, true);
}

void UDSHUDWidget::NativeDestruct()
{
	if (ADSWaveManager* WaveManager = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ADSWaveManager::StaticClass())))
	{
		WaveManager->OnWaveChanged.RemoveDynamic(this, &UDSHUDWidget::HandleWaveChanged);
	}

	if (ACombatGameMode* GameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		GameMode->OnScoreChanged.RemoveDynamic(this, &UDSHUDWidget::HandleScoreChanged);
		GameMode->OnPlayerDied.RemoveDynamic(this, &UDSHUDWidget::HandlePlayerDied);
		GameMode->OnGameWon.RemoveDynamic(this, &UDSHUDWidget::HandleGameWon);

		if (GameMode->TrustComponent)
		{
			GameMode->TrustComponent->OnTrustChanged.RemoveDynamic(this, &UDSHUDWidget::HandleTrustChanged);
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
	}

	Super::NativeDestruct();
}

void UDSHUDWidget::RefreshPolledValues()
{
	ACombatCharacter* PlayerCharacter = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (PlayerCharacter)
	{
		const float CanYuzdesi = FMath::Clamp(PlayerCharacter->GetHealthPercent(), 0.0f, 1.0f);

		if (HealthBarWidget)
		{
			HealthBarWidget->SetPercent(CanYuzdesi);
			const FLinearColor BarColor = CanYuzdesi > 0.5f
				? FLinearColor(0.18f, 0.82f, 0.34f, 1.0f)
				: (CanYuzdesi > 0.25f ? FLinearColor(0.95f, 0.68f, 0.16f, 1.0f) : FLinearColor(0.92f, 0.18f, 0.16f, 1.0f));
			HealthBarWidget->SetFillColorAndOpacity(BarColor);
		}
		if (HealthTextWidget)
		{
			HealthTextWidget->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), CanYuzdesi * 100.0f)));
		}

		OnPlayerHealthChanged(CanYuzdesi);

		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(HUDDebugKey_OyuncuCani, PollAraligi * 1.5f, FColor::Green, FString::Printf(TEXT("Oyuncu Cani: %.0f%%"), CanYuzdesi * 100.0f));
		}

		const int32 CurrentScore = GetWorld()->GetAuthGameMode<ACombatGameMode>() ? GetWorld()->GetAuthGameMode<ACombatGameMode>()->Score : 0;
		const FDSCharacterBodyRig* PlayerRig = FDSCharacterBodyRig::GetOrCreateRig(PlayerCharacter);
		const EDSCombatWeaponStyle PlayerWeaponStyle = PlayerRig ? PlayerRig->WeaponStyle : EDSCombatWeaponStyle::BattleAxe;
		if (ScoreTextWidget)
		{
			ScoreTextWidget->SetText(FText::FromString(FString::Printf(TEXT("SKOR: %d"), CurrentScore)));
		}
		if (GActiveWeaponTitleText.IsValid())
		{
			GActiveWeaponTitleText->SetText(FText::FromString(FString::Printf(TEXT("SİLAH: %s"), *FDSCharacterBodyRig::GetWeaponDisplayName(PlayerWeaponStyle, this))));
		}
		if (GActiveWeaponSubText.IsValid())
		{
			const TCHAR* StanceStr = (PlayerWeaponStyle == EDSCombatWeaponStyle::LivingDarkSword)
				? TEXT("[Sol Tık] 3'lü Kılıç Kombo  •  [Sağ Tık] Ağır  •  [F] Kusursuz Parry")
				: ((PlayerWeaponStyle == EDSCombatWeaponStyle::WarHammer)
					? TEXT("[Sol Tık] Ezici Darbe  •  [Sağ Tık] Zırh Kıran  •  [4] Maden Kazma")
					: TEXT("[Sol Tık] Hızlı Kombo  •  [Sağ Tık] Ağır Biçme  •  [Space] Sıçra"));
			GActiveWeaponSubText->SetText(FText::FromString(StanceStr));
		}
	}

	auto GetBossMaxHP = [](const ACombatEnemy* E) -> float
	{
		if (!E) return 100.0f;
		if (E->ActorHasTag(FName("Boss_Sunflower"))) return 850.0f;
		if (E->ActorHasTag(FName("Boss_Treeguard"))) return 800.0f;
		if (E->ActorHasTag(FName("Boss_AncientGuardian"))) return 480.0f;
		if (E->ActorHasTag(FName("Boss_Pirate"))) return 175.0f;
		return FMath::Max(160.0f, E->CurrentHP);
	};

	int32 LiveWorldZombies = 0;
	ACombatEnemy* NearestActiveBoss = nullptr;
	float NearestBossDist = 920.0f;
	if (GetWorld())
	{
		const FVector PLoc = PlayerCharacter ? PlayerCharacter->GetActorLocation() : FVector::ZeroVector;
		for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
		{
			ACombatEnemy* E = *It;
			if (!IsValid(E) || E->GetMesh()->IsSimulatingPhysics())
			{
				continue;
			}

			// "kalan zombi" yalnizca siradan zombileri sayar: Boss_* ve WanderingKnight* etiketliler haric
			const bool bBossOrKnightTagged = E->Tags.ContainsByPredicate([](const FName& Tag)
			{
				const FString TagStr = Tag.ToString();
				return TagStr.StartsWith(TEXT("Boss_")) || TagStr.StartsWith(TEXT("WanderingKnight"));
			});
			if (!bBossOrKnightTagged)
			{
				++LiveWorldZombies;
			}

			const bool bIsBoss = E->ActorHasTag(FName("Boss_Sunflower"))
				|| E->ActorHasTag(FName("Boss_Treeguard"))
				|| E->ActorHasTag(FName("Boss_Pirate"))
				|| E->ActorHasTag(FName("Boss_AncientGuardian"))
				|| E->ActorHasTag(FName("WanderingKnight"));
			if (bIsBoss && PlayerCharacter)
			{
				const float D2D = FVector::Dist2D(PLoc, E->GetActorLocation());
				const float BMax = GetBossMaxHP(E);
				const bool bDamagedBoss = (E->CurrentHP < BMax * 0.99f);
				if (bDamagedBoss || D2D < NearestBossDist)
				{
					NearestBossDist = bDamagedBoss ? 0.0f : D2D;
					NearestActiveBoss = E;
				}
			}
		}
	}

	if (GBossBannerBorder.IsValid())
	{
		if (NearestActiveBoss)
		{
			const float BMax = FMath::Max(GetBossMaxHP(NearestActiveBoss), NearestActiveBoss->CurrentHP);
			GBossBannerBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
			const TCHAR* BTitle = NearestActiveBoss->ActorHasTag(FName("Boss_Sunflower"))
				? (NearestActiveBoss->ActorHasTag(FName("Boss_Sunflower_Muttefik")) ? TEXT("AYÇİÇEĞİ KORUYUCUSU (DOST MÜTTEFİK)") : TEXT("AYÇİÇEĞİ TARLASININ KORUYUCUSU"))
				: (NearestActiveBoss->ActorHasTag(FName("Boss_Treeguard")) ? TEXT("ORMAN MUHAFIZI — KADİM TREEGUARD")
				: (NearestActiveBoss->ActorHasTag(FName("Boss_Pirate")) ? (NearestActiveBoss->ActorHasTag(FName("Boss_Pirate_Muttefik")) ? TEXT("KORSAN KAPTAN (DOST MÜTTEFİK)") : TEXT("DERİN SULAR KORSAN KAPTANI"))
				: (NearestActiveBoss->ActorHasTag(FName("Boss_AncientGuardian")) ? TEXT("KADİM LABİRENT VE YER ALTI BEKÇİSİ") : TEXT("GEZGİN ŞÖVALYE"))));
			if (GBossNameText.IsValid())
			{
				GBossNameText->SetText(FText::FromString(BTitle));
			}
			if (GBossHPText.IsValid())
			{
				GBossHPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(0.0f, NearestActiveBoss->CurrentHP), BMax)));
			}
			if (GBossHealthBar.IsValid())
			{
				const float Ratio = FMath::Clamp(NearestActiveBoss->CurrentHP / FMath::Max(BMax, 1.0f), 0.0f, 1.0f);
				GBossHealthBar->SetPercent(Ratio);
				const bool bAllied = NearestActiveBoss->ActorHasTag(FName("Boss_Sunflower_Muttefik")) || NearestActiveBoss->ActorHasTag(FName("Boss_Pirate_Muttefik"));
				GBossHealthBar->SetFillColorAndOpacity(bAllied ? FLinearColor(0.20f, 0.86f, 0.48f, 1.0f) : FLinearColor(0.88f, 0.22f, 0.16f, 1.0f));
			}
		}
		else
		{
			GBossBannerBorder->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (ADSWaveManager* WaveManager = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ADSWaveManager::StaticClass())))
	{
		const int32 Kalan = FMath::Max(WaveManager->GetTotalRemainingEnemies(), LiveWorldZombies);
		if (RemainingTextWidget)
		{
			RemainingTextWidget->SetText(FText::FromString(FString::Printf(TEXT("TEHDİT: %d"), Kalan)));
		}
		if (WaveTextWidget && WaveManager->CurrentWaveNumber > 0)
		{
			WaveTextWidget->SetText(FText::FromString(FString::Printf(TEXT("DALGA %d"), WaveManager->CurrentWaveNumber)));
		}

		OnRemainingEnemiesChanged(Kalan);

		if (bShowDebugMessages && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(HUDDebugKey_KalanDusman, PollAraligi * 1.5f, FColor::Yellow, FString::Printf(TEXT("Kalan Dusman: %d"), Kalan));
		}
	}
	else if (RemainingTextWidget)
	{
		RemainingTextWidget->SetText(FText::FromString(FString::Printf(TEXT("TEHDİT: %d"), LiveWorldZombies)));
	}

	// Faz 1 Canli Koloni Envanter, Ambar, Tuz Ekonomisi ve Santiye Durumu (Her zaman guncellenir)
	if (GetWorld())
	{
		FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
		const TCHAR* OrderStr = (WS.VillagerOrder == EDSVillagerCombatOrder::FightOrder)
			? TEXT("SAVAŞ")
			: ((WS.VillagerOrder == EDSVillagerCombatOrder::WorkAndDefendOrder) ? TEXT("ÇALIŞ & SAVUN") : TEXT("GERİ ÇEKİL"));
		const TCHAR* BuildStageStr = (WS.ActiveBuildStage == 0)
			? TEXT("Demirci Ocağı")
			: ((WS.ActiveBuildStage == 1) ? TEXT("Kıyı Tuzlası") : ((WS.ActiveBuildStage == 2) ? TEXT("Kütüphane") : TEXT("Tamamlandı")));
		const TCHAR* MoralStr = WS.bDarkPathChosen ? TEXT("Karanlık Yol") : (WS.bNeighborAllianceFormed ? TEXT("Tuz İttifakı") : TEXT("Tarafsız"));

		if (GLeftTacticalBadgeText.IsValid())
		{
			GLeftTacticalBadgeText->SetText(FText::FromString(FString::Printf(
				TEXT("EMİR [V]: %s   •   YOL: %s   •   ANI: %d/3"),
				OrderStr, MoralStr, WS.MemoryArchiveCount)));
		}
		if (GResWoodText.IsValid())
		{
			GResWoodText->SetText(FText::FromString(FString::Printf(TEXT("ODUN: %d"), WS.WoodCount + WS.CarriedWood)));
		}
		if (GResIronText.IsValid())
		{
			GResIronText->SetText(FText::FromString(FString::Printf(TEXT("DEMİR: %d"), WS.StoredIronOre + WS.CarriedIronOre)));
		}
		if (GResSteelText.IsValid())
		{
			GResSteelText->SetText(FText::FromString(FString::Printf(TEXT("ÇELİK: %d"), WS.StoredSteelIngots)));
		}
		if (GResSaltText.IsValid())
		{
			GResSaltText->SetText(FText::FromString(FString::Printf(TEXT("TUZ: %d"), WS.StoredSalt + WS.CarriedSalt)));
		}
		if (GResBagText.IsValid())
		{
			GResBagText->SetText(FText::FromString(FString::Printf(TEXT("ÇANTA: %d/%d"), WS.GetTotalCarriedItems(), WS.MaxCarryCapacity)));
		}
		if (GColonyStatusText.IsValid())
		{
			GColonyStatusText->SetText(FText::FromString(FString::Printf(
				TEXT("ŞANTİYE [N]: %s   •   KÜTÜPHANE: %d/4 Tarif   •   FİLO: %s"),
				BuildStageStr, WS.ArchivedRecipesCount, *WS.GetShipClassShortName())));
		}
	}

	// Canli V2 Koloni & Dunya Durumu Guncellemesi (AutoCapturePreview disinda her zaman dinamik calisir)
	if (!FParse::Param(FCommandLine::Get(), TEXT("AutoCapturePreview")) && GetWorld())
	{
		FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);
		const TCHAR* OrderStr = (WS.VillagerOrder == EDSVillagerCombatOrder::FightOrder)
			? TEXT("SAVAS (Hucum)")
			: ((WS.VillagerOrder == EDSVillagerCombatOrder::WorkAndDefendOrder) ? TEXT("CALIS & SAVUN") : TEXT("GERI CEKIL"));

		// Basarim Bildirim Karti ("TABANLARI YAGLAMAK") zamanlayici kontrolu
		if (WS.ActiveAchievementBannerTimer > 0.0f)
		{
			WS.ActiveAchievementBannerTimer = FMath::Max(0.0f, WS.ActiveAchievementBannerTimer - PollAraligi);
			if (GSteamAchBanner.IsValid())
			{
				GSteamAchBanner->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			if (GSteamAchTitleText.IsValid())
			{
				GSteamAchTitleText->SetText(FText::FromString(TEXT("BASARIM ACILDI: TABANLARI YAGLAMAK")));
			}
			if (GSteamAchDescText.IsValid())
			{
				GSteamAchDescText->SetText(FText::FromString(WS.ActiveAchievementBannerText));
			}
		}
		else if (GSteamAchBanner.IsValid())
		{
			GSteamAchBanner->SetVisibility(ESlateVisibility::Hidden);
		}

		// Derin Sular koyundaki sualti canlilarinin (DeepWaterLife) su icinde canli sekilde suzulmesi
		// ve Aycicegi Boss'un Minik Aycicegi Yancilarinin (SunflowerMinion) gunese/oyuncuya donup ziplayarak hareket etmesi
		const float NowSec = GetWorld()->GetTimeSeconds();
		int32 FishCounter = 0;
		int32 MinionCounter = 0;
		APawn* CurrentPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Act = *It;
			if (!IsValid(Act))
			{
				continue;
			}
			if (Act->ActorHasTag(FName("LeviathanWhale")))
			{
				const float BobZ = FMath::Abs(FMath::Sin(NowSec * 1.8f)) * 12.0f;
				Act->SetActorLocationAndRotation(
					FVector(760.0f + FMath::Cos(NowSec * 0.45f) * 14.0f, 2245.0f + FMath::Sin(NowSec * 0.45f) * 14.0f, 240.0f + BobZ),
					FRotator(-6.0f + FMath::Sin(NowSec * 1.8f) * 4.0f, 42.0f + FMath::Cos(NowSec * 0.45f) * 6.0f, 0.0f));
			}
			else if (Act->ActorHasTag(FName("LeviathanWhaleTail")))
			{
				const float BobZ = FMath::Abs(FMath::Sin(NowSec * 1.8f)) * 14.0f;
				Act->SetActorLocationAndRotation(
					FVector(685.0f + FMath::Cos(NowSec * 0.45f) * 14.0f, 2315.0f + FMath::Sin(NowSec * 0.45f) * 14.0f, 268.0f + BobZ),
					FRotator(34.0f + FMath::Sin(NowSec * 1.8f) * 5.0f, 42.0f + FMath::Cos(NowSec * 0.45f) * 6.0f, 0.0f));
			}
			else if (Act->ActorHasTag(FName("DeepWaterLife")))
			{
				const float SwimAngle = NowSec * 0.85f + float(FishCounter) * 1.04f;
				const FVector SwimPos(910.0f + FMath::Cos(SwimAngle) * 185.0f, 2095.0f + FMath::Sin(SwimAngle) * 165.0f, Act->GetActorLocation().Z);
				Act->SetActorLocationAndRotation(SwimPos, FRotator(0.0f, SwimAngle * 57.3f + 90.0f, 0.0f));
				++FishCounter;
			}
			else if (Act->ActorHasTag(FName("SunflowerMinion")))
			{
				const float HopZ = FMath::Abs(FMath::Sin(NowSec * 6.5f + float(MinionCounter))) * 14.0f;
				FVector MLoc = Act->GetActorLocation();
				if (Act->ActorHasTag(FName("SunflowerMinion_Fighting")) && CurrentPlayerPawn)
				{
					const FVector ToP = (CurrentPlayerPawn->GetActorLocation() - MLoc).GetSafeNormal2D();
					MLoc += ToP * (135.0f * PollAraligi);
					Act->SetActorLocationAndRotation(
						FVector(MLoc.X, MLoc.Y, 225.0f + HopZ),
						FRotator(22.0f, ToP.Rotation().Yaw, FMath::Sin(NowSec * 8.0f + float(MinionCounter)) * 12.0f)
					);
				}
				else
				{
					// Boss "HAYIR ONLAR BENIM, BANA BIRAKIN!" dedikten sonra kenarda guvende gunese bakip sallanirlar
					Act->SetActorRotation(FRotator(28.0f, -36.0f + FMath::Sin(NowSec * 3.0f + float(MinionCounter)) * 18.0f, 0.0f));
				}
				++MinionCounter;
			}
		}

		if (WS.ActiveNotificationTimer > 0.0f)
		{
			WS.ActiveNotificationTimer = FMath::Max(0.0f, WS.ActiveNotificationTimer - PollAraligi);
			SetGuideMessageBox(
				TEXT("KEHANET REHBERI & CANLI DUNYA BILDIRIMI"),
				WS.ActiveNotificationText,
				FLinearColor(0.25f, 0.95f, 0.55f, 1.0f));
		}
		else if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector PLoc = PlayerPawn->GetActorLocation();
			bool bContextShown = false;

			// Gezgin Sovalye veya Korsan Boss yakinlik ipucu
			for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
			{
				if (!IsValid(*It))
				{
					continue;
				}
				if (It->ActorHasTag(FName("WanderingKnight")) && !It->ActorHasTag(FName("WanderingKnight_Hostile")) && !WS.bWanderingKnightDefeated)
				{
					if (FVector::Dist2D(PLoc, It->GetActorLocation()) <= 400.0f)
					{
						SetGuideMessageBox(
							TEXT("GEZGIN SOVALYE - ONUR DUELLOSU TEKLIFI"),
							TEXT("\"Bilegin saglama benziyor yabanci! Benimle onurlu bir teke tek duelloya var misin?\"  [E: Duelloyu Kabul Et | R: Henuz Hazir Degilim, Ertele]"),
							FLinearColor(0.25f, 0.82f, 1.0f, 1.0f));
						bContextShown = true;
						break;
					}
				}
				else if (It->ActorHasTag(FName("Boss_Pirate")) && !It->ActorHasTag(FName("Boss_Pirate_Muttefik")))
				{
					if (FVector::Dist2D(PLoc, It->GetActorLocation()) <= 420.0f)
					{
						SetGuideMessageBox(
							TEXT("BOSS #3 - KORSAN KAPTAN (CIFT ELLI KILIC & DENIZCILIK USTASI)"),
							TEXT("\"Derin Sular'in sirlari ve Cift Elli Kilic teknigi bilegi bukulemeyene aittir! Beni yenersen sana Daha Guclu Olta/Ag ve Cift Elli Kilic (Kombo + [F] Parry) ogretecegim!\""),
							FLinearColor(0.98f, 0.42f, 0.22f, 1.0f));
						bContextShown = true;
						break;
					}
				}
				else if (It->ActorHasTag(FName("Boss_AncientGuardian")))
				{
					if (FVector::Dist2D(PLoc, It->GetActorLocation()) <= 440.0f)
					{
						SetGuideMessageBox(
							TEXT("BOSS #4 - KADIM LABIRENT & YER ALTI BEKCISI (OBSIDYEN ZIRH & SUTUN STAGGER)"),
							TEXT("Obsidyen zirhi kesici silahlara %60 direnclidir! [4] Savas Cekici/Balyoz kullanarak veya Tas Sutunlara (AncientLabyrinthPillar) carpdirarak zirhini kir!"),
							FLinearColor(0.72f, 0.28f, 1.0f, 1.0f));
						bContextShown = true;
						break;
					}
				}
			}

			// Supheli Kaya yakinlik ipucu
			if (!bContextShown && !WS.bHiddenCityPortalOpened)
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(FName("SuspiciousBoulder")) && FVector::Dist2D(PLoc, It->GetActorLocation()) <= 440.0f)
					{
						SetGuideMessageBox(
							TEXT("SUPHELI KAYA (SUSPICIOUS BOULDER) - GIZLI GECIT"),
							TEXT("Kayanin altindan kadim bir isik siziyor... Gizli Kadim Sehir Portalini acmak icin [E] tusuna bas!"),
							FLinearColor(0.20f, 0.90f, 1.0f, 1.0f));
						bContextShown = true;
						break;
					}
				}
			}

			// Derin Sular Iskelesi & Gemi Filosu yakinlik ipucu
			if (!bContextShown)
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && (It->ActorHasTag(FName("ShipyardPier")) || It->ActorHasTag(FName("DeepWaterZone"))) && FVector::Dist2D(PLoc, It->GetActorLocation()) <= 480.0f)
					{
						SetGuideMessageBox(
							FString::Printf(TEXT("DERIN SULAR TERSANESI & GEMI FILOSU (%d GEMI)"), WS.FleetShipCount),
							TEXT("Akil Hocasi: \"Topraga cakili kalmak zorunda degilsin evlat; koloniyi kucuk tutup [G] ile filonu buyuterek ve [B] / [E] ile Derin Sular'da kesif yaparak da hayatta kalabilirsin.\""),
							FLinearColor(0.18f, 0.88f, 0.98f, 1.0f));
						bContextShown = true;
						break;
					}
				}
			}

			// Faz 1 Koloni Yakinlik Ipuclari: Santiye, Koy Ambari/Tuz Ambari, Maden Damari ve Yerdeki Fiziksel Loot
			if (!bContextShown)
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* Act = *It;
					if (!IsValid(Act)) continue;
					const float D2D = FVector::Dist2D(PLoc, Act->GetActorLocation());
					if ((Act->ActorHasTag(FName("AncientCraftingAltar")) || Act->ActorHasTag(FName("ColonyLibraryLectern"))) && D2D <= 280.0f)
					{
						SetGuideMessageBox(
							TEXT("KADIM URETIM TEZGAHI & KOLONI KUTUPHANESI [E / K]"),
							FString::Printf(TEXT("Boss ganimetleriyle Kadim Karanlik Kilic ve Muhafiz Zirhi uret; acilan tarifleri (%d/4) Kutuphaneye arsivle (GDD 16: Olumde Kaybolmaz)!"), WS.GetUnlockedRecipesCount()),
							FLinearColor(0.75f, 0.38f, 1.0f, 1.0f));
						bContextShown = true;
						break;
					}
					else if (Act->ActorHasTag(FName("ColonyBuildSite")) && D2D <= 300.0f)
					{
						SetGuideMessageBox(
							TEXT("KOLONI INSAAT SANTIYESI ? [E] / [N] / [4-BALYOZ] ILE INSA ET"),
							FString::Printf(TEXT("Cantadaki ve ambardaki Odun/Tas/Demir malzemelerini kullanarak santiyeyi insa et (Ilerleme: %.0f%%)."), WS.BuildSiteProgress),
							FLinearColor(1.0f, 0.78f, 0.25f, 1.0f));
						bContextShown = true;
						break;
					}
					else if ((Act->ActorHasTag(FName("ColonyStorageChest")) || Act->ActorHasTag(FName("SaltGranary"))) && D2D <= 260.0f)
					{
						SetGuideMessageBox(
							TEXT("KOY AMBARI & TUZ AMBARI ? [E] ILE LOOT TESLIM ET"),
							FString::Printf(TEXT("Sirt cantandaki (%d/%d) Odun, Demir, Tuz ve Taslari koy ambarina bosalt; Demirleri Celik Kulceye, Tuzlari Kurutulmus Ete isle!"), WS.GetTotalCarriedItems(), WS.MaxCarryCapacity),
							FLinearColor(0.35f, 0.92f, 1.0f, 1.0f));
						bContextShown = true;
						break;
					}
					else if (Act->ActorHasTag(FName("OreVein")) && D2D <= 260.0f)
					{
						const TCHAR* VeinName = Act->ActorHasTag(FName("OreVein_Iron")) ? TEXT("DEMIR CEVHERI DAMARI") : (Act->ActorHasTag(FName("OreVein_Salt")) ? TEXT("KIYI KAYA TUZU DAMARI") : TEXT("KAYA / TAS DAMARI"));
						SetGuideMessageBox(
							FString::Printf(TEXT("%s ? [4] SAVAS CEKICI / BALYOZ ILE KAZ"), VeinName),
							TEXT("Damarlara [4] Balyoz ile vurmak 2.5x maden hasari verir ve yere fiziksel 3B cevher parcalari (WorldLoot) dusurur!"),
							FLinearColor(1.0f, 0.58f, 0.22f, 1.0f));
						bContextShown = true;
						break;
					}
				}
			}

			if (!bContextShown)
			{
				// Akil Hocasi oyunun her asamasinda cok baskin olmadan, dingin ve babacan bir sekilde yanimizda olur
				int32 CurWave = 1;
				if (ADSWaveManager* WM = Cast<ADSWaveManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ADSWaveManager::StaticClass())))
				{
					CurWave = FMath::Max(1, WM->CurrentWaveNumber);
				}

				// Sahnedeki Akil Hocasi figurunun yuzunu yavasca oyuncuya donmesi
				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(FName("AkilHocasi_Baba")))
					{
						const FVector ToPlayer = (PLoc - It->GetActorLocation()).GetSafeNormal2D();
						if (!ToPlayer.IsNearlyZero())
						{
							It->SetActorRotation(FMath::RInterpTo(It->GetActorRotation(), ToPlayer.Rotation(), PollAraligi, 3.5f));
						}
					}
				}

				FString StageWhisper;
				if (WS.UnplantedTreeCuts > 0 && !WS.bTreeguardAwakened)
				{
					StageWhisper = FString::Printf(TEXT("\"Evlat, kestigin agacin yeri bos kalmasin; [C] ile tohum ek ki Treeguard ofkelenmesin.\"  (Gazap: %d/3)"), WS.ForestWrathCounter);
				}
				else if (CurWave == 1)
				{
					StageWhisper = FString::Printf(TEXT("\"Nefesini tut evlat, sakalli savascilar ve koyluler emrinde ([V]: %s). Yasli agaclarin dibindeki genc fidanlara kiyma.\""), OrderStr);
				}
				else if (CurWave == 2)
				{
					StageWhisper = TEXT("\"Ikinci dalga yaklasiyor... Yarali dusen olursa [E] ile elini uzat, koylulerin sana guveni her seyden degerli.\"");
				}
				else
				{
					StageWhisper = FString::Printf(TEXT("\"Isciler kaynak icin kaleden uzaklasiyor evlat; vakti gelince [G] ile gemiyi hazirla ya da [H] ile kaleyi tahkim et.\" (Gemi: %.0f%%)"), WS.BoatBuildProgress);
				}

				SetGuideMessageBox(
					FString::Printf(TEXT("AKIL HOCASI  -  ASAMA %d"), CurWave),
					StageWhisper,
					FLinearColor(0.95f, 0.80f, 0.38f, 0.92f));
			}
		}
	}

	// -AnimCheck: gercek iskelet animasyonu + silah kavrayisi + kombo/ziplama dogrulama karelerini otomatik yakalar ve oyunu kapatir
	if (FParse::Param(FCommandLine::Get(), TEXT("AnimCheck")) && GetWorld())
	{
		GAreScreenMessagesEnabled = false;
		static int32 AnimStep = 0;
		const float T = GetWorld()->GetTimeSeconds();
		ACombatCharacter* PC = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		const FString Dir = TEXT("C:/Users/Monster/.gemini/antigravity/brain/d01a74a5-0554-4d03-8a9b-2c9b494801b5/");
		auto PlaceCam = [PC](float YawOffset)
		{
			if (PC && PC->GetController())
			{
				PC->GetController()->SetControlRotation(FRotator(-10.0f, PC->GetActorRotation().Yaw + YawOffset, 0.0f));
			}
		};
		struct FStep { float Time; int32 Action; const TCHAR* Shot; };
		static const FStep Steps[] = {
			{ 2.0f, 1, nullptr },                         // konumla + kamera yandan
			{ 3.0f, 0, TEXT("anim_check_01_idle_weapon.png") },
			{ 3.4f, 2, nullptr },                         // kombo 1
			{ 3.62f, 0, TEXT("anim_check_02_combo_A.png") },
			{ 3.7f, 2, nullptr },                         // girdi tamponla -> kombo 2
			{ 4.05f, 0, TEXT("anim_check_03_combo_B.png") },
			{ 4.1f, 2, nullptr },                         // girdi tamponla -> kombo 3
			{ 4.75f, 0, TEXT("anim_check_04_combo_C.png") },
			{ 5.15f, 0, TEXT("anim_check_05_combo_C_late.png") },
			{ 7.0f, 3, nullptr },                         // ziplama
			{ 7.28f, 0, TEXT("anim_check_06_jump.png") },
			{ 8.6f, 4, nullptr },                         // agir saldiri
			{ 9.3f, 0, TEXT("anim_check_07_heavy.png") },
			{ 13.0f, 5, nullptr },                        // blok
			{ 13.6f, 0, TEXT("anim_check_08_block.png") },
			{ 13.8f, 7, nullptr },                        // Boss Sunflower sahneye getir
			{ 14.6f, 0, TEXT("anim_check_09_boss_sunflower.png") },
			{ 15.2f, 6, nullptr },                        // kapat
		};
		if (PC && AnimStep < UE_ARRAY_COUNT(Steps) && T >= Steps[AnimStep].Time)
		{
			const FStep& S = Steps[AnimStep++];
			switch (S.Action)
			{
			case 1:
				if (APlayerController* PCon = Cast<APlayerController>(PC->GetController()))
				{
					PCon->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
				}
				PC->SetActorLocationAndRotation(FVector(1245.0f, 1625.0f, 302.0f), FRotator(0.0f, 12.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				// sadece dogrulama icin: oyuncu, silahi ve zemin disindaki her seyi gizle, kamerayi yana al
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* A = *It;
					if (!A || A == PC || A->GetAttachParentActor() == PC || A->GetOwner() == PC)
					{
						continue;
					}
					const FString N = A->GetName();
					if (N.Contains(TEXT("Floor")) || N.Contains(TEXT("Landscape")) || N.Contains(TEXT("Light")) || N.Contains(TEXT("Sky")) || N.Contains(TEXT("Fog")) || N.Contains(TEXT("PostProcess")))
					{
						continue;
					}
					FBox Bx = A->GetComponentsBoundingBox(true);
					if (Bx.IsValid && Bx.GetSize().X > 2500.0f && Bx.GetSize().Z < 900.0f)
					{
						continue; // buyuk duz zemin parcalari
					}
					A->SetActorHiddenInGame(true);
				}
				if (PC->GetCameraBoom())
				{
					PC->GetCameraBoom()->TargetArmLength = 330.0f;
					PC->GetCameraBoom()->bEnableCameraLag = false;
					PC->GetCameraBoom()->bEnableCameraRotationLag = false;
				}
				PlaceCam(100.0f);
				break;
			case 2: PC->DoComboAttackStart(); break;
			case 3:
				PC->PerformCombatJump();
				break;
			case 4: PC->DoChargedAttackStart(); break;
			case 5: FDSColonyAndWorldState::Get(this).PlayerParryWindowEndTime = T + 0.8f; break;
			case 7:
				for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
				{
					ACombatEnemy* Boss = *It;
					if (IsValid(Boss) && Boss->ActorHasTag(FName("Boss_Sunflower")))
					{
						Boss->SetActorHiddenInGame(false);
						const FVector BossLoc = PC->GetActorLocation() + PC->GetActorForwardVector() * 240.0f + PC->GetActorRightVector() * 80.0f;
						Boss->SetActorLocationAndRotation(
							BossLoc,
							(PC->GetActorLocation() - BossLoc).Rotation(),
							false, nullptr, ETeleportType::TeleportPhysics);
						if (UAnimInstance* BossAnim = Boss->GetMesh() ? Boss->GetMesh()->GetAnimInstance() : nullptr)
						{
							if (UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Anims_UAL/Manny/MM_UAL_Sword_Regular_A.MM_UAL_Sword_Regular_A")))
							{
								BossAnim->PlaySlotAnimationAsDynamicMontage(Seq, FName("DefaultSlot"), 0.05f, 0.2f, 0.45f, 1, -1.0f, 0.0f);
							}
						}
						break;
					}
				}
				if (PC->GetCameraBoom())
				{
					PC->GetCameraBoom()->TargetArmLength = 430.0f;
				}
				PlaceCam(22.0f);
				break;
			case 6: FGenericPlatformMisc::RequestExit(false); break;
			default: break;
			}
			if (S.Shot)
			{
				if (USkeletalMeshComponent* SK = PC->GetMesh())
				{
					const FDSCharacterBodyRig* R = FDSCharacterBodyRig::GetOrCreateRig(PC);
					if (UStaticMeshComponent* W = R ? R->CachedWeaponMesh.Get() : nullptr)
					{
						const FBox WB = W->Bounds.GetBox();
						UE_LOG(LogTemp, Warning, TEXT("ANIMCHECK %s: weaponAttachedTo=%s socket=%s handDist=%.1f weaponSize=%s hidden=%d"),
							S.Shot, *GetNameSafe(W->GetAttachParent()), *W->GetAttachSocketName().ToString(),
							FVector::Dist(WB.GetCenter(), SK->GetSocketLocation(TEXT("hand_r"))), *WB.GetSize().ToString(), W->bHiddenInGame ? 1 : 0);
					}
					UE_LOG(LogTemp, Warning, TEXT("ANIMCHECK %s: Z=%.1f attacking=%d montage=%d"), S.Shot, PC->GetActorLocation().Z,
						SK->GetAnimInstance() ? (int32)SK->GetAnimInstance()->IsAnyMontagePlaying() : -1, SK->GetAnimInstance() ? (int32)SK->GetAnimInstance()->IsAnyMontagePlaying() : -1);
				}
				FScreenshotRequest::RequestScreenshot(Dir + S.Shot, true, false);
			}
			// yan kamera: karakterin sagindan 2.6m mesafeden bakar
			if (APlayerController* PCtrl = Cast<APlayerController>(PC->GetController()))
			{
				static TWeakObjectPtr<ACameraActor> SideCam;
				if (!SideCam.IsValid())
				{
					SideCam = GetWorld()->SpawnActor<ACameraActor>();
				}
				if (SideCam.IsValid())
				{
					const FVector Base = PC->GetActorLocation();
					if (T >= 7.0f)
					{
						const FVector CamLoc = Base + PC->GetActorRightVector() * 360.0f + PC->GetActorForwardVector() * 140.0f + FVector(0.0f, 0.0f, 70.0f);
						const FVector LookAt = Base + PC->GetActorForwardVector() * 150.0f + FVector(0.0f, 0.0f, 55.0f);
						SideCam->SetActorLocationAndRotation(CamLoc, (LookAt - CamLoc).Rotation());
					}
					else
					{
						const FVector CamLoc = Base + PC->GetActorRightVector() * 260.0f + PC->GetActorForwardVector() * 60.0f + FVector(0.0f, 0.0f, 35.0f);
						SideCam->SetActorLocationAndRotation(CamLoc, (Base + FVector(0.0f, 0.0f, 10.0f) - CamLoc).Rotation());
					}
					if (PCtrl->GetViewTarget() != SideCam.Get())
					{
						PCtrl->SetViewTarget(SideCam.Get());
					}
				}
			}
		}
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("AutoCapturePreview")) && GetWorld())
	{
		GAreScreenMessagesEnabled = false;
		static int32 CaptureStep = 0;
		const float TimeSec = GetWorld()->GetTimeSeconds();
		ACombatCharacter* PlayerChar = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		const FString OutDir = TEXT("C:/Users/Monster/.gemini/antigravity/brain/d01a74a5-0554-4d03-8a9b-2c9b494801b5/");
		FDSColonyAndWorldState& WS = FDSColonyAndWorldState::Get(this);

		if (CaptureStep == 0 && TimeSec >= 6.0f)
		{
			CaptureStep = 1;
			if (PlayerChar)
			{
				if (APlayerController* PCon = Cast<APlayerController>(PlayerChar->GetController()))
				{
					PCon->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
				}
				// KARE 1 (D1): Komsu Koy Elcisi (NeighborVillageEnvoy) ve Vicdan Secim Masasi (NeighborDiplomacyTable)
				if (FDSCharacterBodyRig* PRig = FDSCharacterBodyRig::GetOrCreateRig(PlayerChar))
				{
					PRig->WeaponStyle = EDSCombatWeaponStyle::BattleAxe;
					PRig->ApplyWeaponMeshToComponent();
				}

				WS.bRecipeLivingDarkSword = WS.bRecipeGuardianArmor = WS.bRecipeSunOilElixir = WS.bRecipePirateSalupa = true;
				WS.ArchivedRecipesCount = 4;
				WS.CurrentShipClass = EDSShipClass::Flagship;
				WS.FleetShipCount = 5;
				WS.ResolveNeighborDiplomacyChoice(GetWorld(), 0);

				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(FName("NeighborVillageEnvoy")))
					{
						It->SetActorLocationAndRotation(FVector(1235.0f, 1585.0f, 292.0f), FRotator(0.0f, -135.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
					}
				}

				PlayerChar->SetActorLocationAndRotation(FVector(1155.0f, 1505.0f, 292.0f), FRotator(0.0f, 45.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				if (PlayerChar->GetCameraBoom())
				{
					PlayerChar->GetCameraBoom()->TargetArmLength = 220.0f;
				}
				if (AController* PC = PlayerChar->GetController())
				{
					PC->SetControlRotation(FRotator(-8.0f, 45.0f, 0.0f));
				}
				SetGuideMessageBox(
					TEXT("FAZ 4 (D1) - KOMSU KOY DIPLOMASISI VE VICDAN SECIMLERI [E / X / Z]"),
					TEXT("Komsu Koy Elcisi silah ve tuz istiyor: [E] Tuz & Silah Ittifaki (+Usta Zanaatkar) | [X] Karanlik Yol: Koyu Bas & Demirci Kacir | [Z] Reddet!"),
					FLinearColor(0.25f, 0.92f, 0.65f, 1.0f)
				);
			}
		}
		else if (CaptureStep == 1 && TimeSec >= 6.4f)
		{
			CaptureStep = 2;
			FScreenshotRequest::RequestScreenshot(OutDir + TEXT("faz4_01_komsu_koy_elcisi_ve_detroit_secim_masasi.png"), true, false);
		}
		else if (CaptureStep == 2 && TimeSec >= 7.2f)
		{
			CaptureStep = 3;
			if (PlayerChar)
			{
				// KARE 2 (D1): Karanlik Yol Secimi ([X] Komsu Koyu Basip Usta Demirci Kacirma & Gercek Dunya Sonucu)
				if (FDSCharacterBodyRig* PRig = FDSCharacterBodyRig::GetOrCreateRig(PlayerChar))
				{
					PRig->WeaponStyle = EDSCombatWeaponStyle::LivingDarkSword;
					PRig->ApplyWeaponMeshToComponent();
				}

				WS.ResolveNeighborDiplomacyChoice(GetWorld(), 1);
				PlayerChar->SetActorLocationAndRotation(FVector(1155.0f, 1505.0f, 292.0f), FRotator(0.0f, 45.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				if (PlayerChar->GetCameraBoom())
				{
					PlayerChar->GetCameraBoom()->TargetArmLength = 225.0f;
				}
				if (AController* PC = PlayerChar->GetController())
				{
					PC->SetControlRotation(FRotator(-8.0f, 45.0f, 0.0f));
				}
				PlayerChar->DoComboAttackStart();
				SetGuideMessageBox(
					TEXT("FAZ 4 (D1) - KARANLIK YOL SECIMI [X]: KOMSU KOYDEN USTA DEMIRCI KACIRMA"),
					TEXT("Ahlak puani yerine gercek dunya sonucu: Zorla getirilen Demirci Ambara +6 Celik ve +8 Demir uretti, fakat Kisisel Guven 15'e dustu ve Intikam Baskini basladi!"),
					FLinearColor(1.0f, 0.28f, 0.22f, 1.0f)
				);
			}
		}
		else if (CaptureStep == 3 && TimeSec >= 7.52f)
		{
			CaptureStep = 4;
			FScreenshotRequest::RequestScreenshot(OutDir + TEXT("faz4_02_karanlik_yol_demirci_kacirma_ve_sonuclar.png"), true, false);
		}
		else if (CaptureStep == 4 && TimeSec >= 8.4f)
		{
			CaptureStep = 5;
			if (PlayerChar)
			{
				// KARE 3 (D2): Vicdan Tarzi Gizli Zombi Isirigi Muayenesi ve Karantina Merhem Tedavisi ([E] / [M])
				ADSVillager* InspectedV = nullptr;
				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(FName("NeighborVillageEnvoy")))
					{
						InspectedV = *It;
						break;
					}
				}
				WS.ResolveBiteInspectionDilemma(InspectedV, true);
				FDSColonyAndWorldState::SpawnRemnantTelegraphRing(GetWorld(), FVector(1235.0f, 1585.0f, 212.0f), 90.0f, FLinearColor(0.18f, 0.96f, 0.68f));

				PlayerChar->SetActorLocationAndRotation(FVector(1165.0f, 1515.0f, 292.0f), FRotator(0.0f, 45.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				if (PlayerChar->GetCameraBoom())
				{
					PlayerChar->GetCameraBoom()->TargetArmLength = 210.0f;
				}
				if (AController* PC = PlayerChar->GetController())
				{
					PC->SetControlRotation(FRotator(-8.0f, 45.0f, 0.0f));
				}
				SetGuideMessageBox(
					TEXT("FAZ 4 (D2) - VICDAN TARZI GIZLI ZOMBI ISIRIGI MUAYENESI VE KARANTINA TEDAVISI [E/M]"),
					TEXT("Supheli koyludeki gizli isirik izi muayene edildi; surgun yerine Tuzlu Gunes Merhemi ile karantina tedavisi uygulanarak koylu kurtarildi (Kisisel Guven: 96/100)!"),
					FLinearColor(0.20f, 0.96f, 0.72f, 1.0f)
				);
			}
		}
		else if (CaptureStep == 5 && TimeSec >= 9.0f)
		{
			CaptureStep = 6;
			FScreenshotRequest::RequestScreenshot(OutDir + TEXT("faz4_03_detroit_isirik_muayenesi_ve_karantina_tedavisi.png"), true, false);
		}
		else if (CaptureStep == 6 && TimeSec >= 9.6f)
		{
			CaptureStep = 7;
			if (PlayerChar)
			{
				// KARE 4 (D3): Duygusal Cekirdek — Korunakli Ani Koyu Kosesi ve Olumsuz Ani NPC'leri (Akil Hocasi Baba, Bilge Dede, Sefkatli Anneanne)
				for (TActorIterator<ADSVillager> It(GetWorld()); It; ++It)
				{
					if (!IsValid(*It)) continue;
					if (It->ActorHasTag(FName("AkilHocasi_Baba")))
					{
						It->SetActorLocationAndRotation(FVector(1315.0f, 1765.0f, 292.0f), FRotator(0.0f, -135.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
						WS.InteractWithImmortalMemoryNPC(*It);
					}
					else if (It->ActorHasTag(FName("AniNPC_Dede")))
					{
						It->SetActorLocationAndRotation(FVector(1385.0f, 1745.0f, 292.0f), FRotator(0.0f, -135.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
						WS.InteractWithImmortalMemoryNPC(*It);
					}
					else if (It->ActorHasTag(FName("AniNPC_Anneanne")))
					{
						It->SetActorLocationAndRotation(FVector(1355.0f, 1825.0f, 292.0f), FRotator(0.0f, -140.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
						WS.InteractWithImmortalMemoryNPC(*It);
					}
				}
				WS.MemoryArchiveCount = 3;

				PlayerChar->SetActorLocationAndRotation(FVector(1245.0f, 1685.0f, 292.0f), FRotator(0.0f, 45.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
				if (PlayerChar->GetCameraBoom())
				{
					PlayerChar->GetCameraBoom()->TargetArmLength = 235.0f;
				}
				if (AController* PC = PlayerChar->GetController())
				{
					PC->SetControlRotation(FRotator(-8.0f, 45.0f, 0.0f));
				}
				SetGuideMessageBox(
					TEXT("FAZ 4 (D3) - DUYGUSAL CEKIRDEK: KORUNAKLI ANI KOSESI VE OLUMSUZ ANI NPC'LERI (ARSIV: 3/3)"),
					TEXT("\"Huzur, kaybedilebilir oldugu icin huzurdur.\" Akil Hocasi Baba, Bilge Dede ve Sefkatli Anneanne mekanik olarak zarar gormez ve Yasayan Ani Arsivinde (3/3) yasar!"),
					FLinearColor(1.0f, 0.84f, 0.28f, 1.0f)
				);
			}
		}
		else if (CaptureStep == 7 && TimeSec >= 10.1f)
		{
			CaptureStep = 8;
			FScreenshotRequest::RequestScreenshot(OutDir + TEXT("faz4_04_olumsuz_ani_npcleri_baba_dede_anneanne_ocagi.png"), true, false);
		}
		else if (CaptureStep == 8 && TimeSec >= 10.6f)
		{
			CaptureStep = 9;
			FPlatformMisc::RequestExit(false);
		}
	}
}

void UDSHUDWidget::HandleWaveChanged(int32 WaveNumber, int32 HedefDusmanSayisiPerSpawner)
{
	if (WaveTextWidget)
	{
		WaveTextWidget->SetText(FText::FromString(FString::Printf(TEXT("DALGA %d"), WaveNumber)));
	}

	OnWaveNumberChanged(WaveNumber);

	if (bShowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(HUDDebugKey_Dalga, 4.0f, FColor::Cyan, FString::Printf(TEXT("Dalga %d basladi (spawner basina hedef: %d)"), WaveNumber, HedefDusmanSayisiPerSpawner));
	}
}

void UDSHUDWidget::HandleScoreChanged(int32 YeniSkor)
{
	if (ScoreTextWidget)
	{
		ScoreTextWidget->SetText(FText::FromString(FString::Printf(TEXT("SKOR: %d"), YeniSkor)));
	}
	if (ACombatCharacter* PlayerCharacter = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (GActiveWeaponTitleText.IsValid())
		{
			if (const FDSCharacterBodyRig* Rig = FDSCharacterBodyRig::GetOrCreateRig(PlayerCharacter))
			{
				GActiveWeaponTitleText->SetText(FText::FromString(FString::Printf(TEXT("SİLAH: %s"), *FDSCharacterBodyRig::GetWeaponDisplayName(Rig->WeaponStyle, this))));
			}
		}
	}

	OnScoreChanged(YeniSkor);

	if (bShowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(HUDDebugKey_Skor, 3.0f, FColor::White, FString::Printf(TEXT("Skor: %d"), YeniSkor));
	}
}

void UDSHUDWidget::HandleTrustChanged(float YeniTrustDegeri, FString TrustIsmi)
{
	if (TrustBarWidget)
	{
		TrustBarWidget->SetPercent(FMath::Clamp(YeniTrustDegeri / 100.0f, 0.0f, 1.0f));
	}
	if (TrustTextWidget)
	{
		TrustTextWidget->SetText(FText::FromString(FString::Printf(TEXT("%.0f • %s"), YeniTrustDegeri, *TrustIsmi)));
	}

	UpdateTrustDisplay(YeniTrustDegeri, TrustIsmi);

	if (bShowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(HUDDebugKey_Guven, 4.0f, FColor::Magenta, FString::Printf(TEXT("Guven: %.0f (%s)"), YeniTrustDegeri, *TrustIsmi));
	}
}

void UDSHUDWidget::HandlePlayerDied()
{
	const FString Msg = TEXT("OYUN BITTI - 5 saniye sonra yeniden basliyor...");
	if (GameOverBannerWidget && GameOverTextWidget)
	{
		GameOverBannerWidget->SetBrushColor(FLinearColor(0.35f, 0.02f, 0.02f, 0.92f));
		GameOverTextWidget->SetText(FText::FromString(Msg));
		GameOverBannerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ShowGameOverMessage(Msg);
}

void UDSHUDWidget::HandleGameWon()
{
	const FString Msg = TEXT("TEBRIKLER! TUM DALGALAR TEMIZLENDI! (5 sn sonra yeniden basliyor)");
	if (GameOverBannerWidget && GameOverTextWidget)
	{
		GameOverBannerWidget->SetBrushColor(FLinearColor(0.02f, 0.28f, 0.08f, 0.92f));
		GameOverTextWidget->SetText(FText::FromString(Msg));
		GameOverBannerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ShowGameOverMessage(Msg);
}
