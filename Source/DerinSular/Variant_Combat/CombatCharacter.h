// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "CombatCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UCombatLifeBar;
class UWidgetComponent;
class ADSMeleeWeapon;
class USoundBase;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatCharacter, Log, All);

/**
 *  An enhanced Third Person Character with melee combat capabilities:
 *  - Combo attack string
 *  - Press and hold charged attack
 *  - Damage dealing and reaction
 *  - Death
 *  - Respawning
 */
UCLASS(abstract)
class ACombatCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Combo Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ComboAttackAction;

	/** Charged Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ChargedAttackAction;

	/** Toggle Camera Side Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ToggleCameraAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 100.0f;

	/** Current amount of HP the character has */
	UPROPERTY(VisibleAnywhere, Category="Damage")
	float CurrentHP = 0.0f;

	/** Life bar widget fill color */
	UPROPERTY(EditAnywhere, Category="Damage")
	FLinearColor LifeBarColor;

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	TObjectPtr<UCombatLifeBar> LifeBarWidget;

	/** Max amount of time that may elapse for a non-combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float AttackInputCacheTimeTolerance = 1.0f;

	/** Time at which an attack button was last pressed */
	float CachedAttackInputTime = -100.0f;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;

	/** Distance ahead of the character that enemies will be notified of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float DangerTraceDistance = 300.0f;

	/** Radius of the sphere trace to notify enemies of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float DangerTraceRadius = 100.0f;

	/** Amount of damage a melee attack will deal */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	/** BeginPlay'de EquippedWeapon'a spawn edilecek silah sinifi. Sadece gorsel: hasar hala DoAttackTrace'in kendi kemik-sweep'inden gelir, bu deger vurus tespitini etkilemez */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Weapon")
	TSubclassOf<ADSMeleeWeapon> WeaponClass;

	/** Silahin eklenecegi mesh soketi (orn. hand_r); soket yoksa mesh origin'ine eklenir */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Weapon")
	FName WeaponAttachSocketName = FName("hand_r");

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Max amount of time that may elapse for a combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** Index of the current stage of the melee attack combo */
	int32 ComboCount = 0;

	/** AnimMontage that will play for charged attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Name of the AnimMontage section that corresponds to the charge loop */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Name of the AnimMontage section that corresponds to the attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Flag that determines if the player is currently holding the charged attack input */
	bool bIsChargingAttack = false;
	
	/** If true, the charged attack hold check has been tested at least once */
	bool bHasLoopedChargedAttack = false;

	/** If true, the character wants to release and resolve the charged attack. */
	bool bHasReleasedChargedAttack = false;

	/** Silahli kombo: o an oynayan dinamik montaj (Quaternius UAL2 kilic/balta animasyonlari, Manny iskeletine retarget edildi) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveWeaponMontage;

	/** Silahli kombo adimi (0: Sword_Regular_A, 1: Sword_Regular_B, 2: Sword_Regular_C finisher) */
	int32 WeaponComboStep = 0;

	/** Silahli kombo: vurus izi ve zincir/toparlanma zamanlayicilari */
	FTimerHandle WeaponHitTimer;
	FTimerHandle WeaponHitTimer2;
	FTimerHandle WeaponChainTimer;
	FTimerHandle WeaponRecoverTimer;

	/** [F] blok durusu animasyonu oynuyor mu */
	bool bPlayingBlockAnim = false;

	/** Silahli kombo adimini gercek animasyonla oynatir; false donerse animasyon bulunamamistir */
	bool PlayWeaponComboStep(int32 Step);

	/** Silahli agir (sarjli) saldiri: Sword_Heavy_Combo; false donerse animasyon bulunamamistir */
	bool PlayWeaponHeavyAttack();

	/** Kombo penceresinde birikmis saldiri girdisi varsa sonraki adima gecer, yoksa toparlanma animasyonu oynatir */
	void ResolveWeaponComboChain();

	/** Manny iskeletine retarget edilmis UAL animasyonunu yukler */
	static class UAnimSequence* LoadUALAnim(const TCHAR* Name);

public:
	/** Gercek ziplama animasyonu (MM_UAL_Jump_Start) + dikey sicrama uygular */
	void PerformCombatJump();

protected:

	/** Camera boom length while the character is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DeathCameraDistance = 400.0f;

	/** Camera boom length when the character respawns */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DefaultCameraDistance = 100.0f;

	/** Time to wait before respawning the character */
	UPROPERTY(EditAnywhere, Category="Respawn", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 3.0f;

	/** Vurulunca calinacak ses; bos birakilirsa sadece log uyarisi verilir (TODO: ata, Asama 5 ses tasarimi) */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* HitSound;

	/** Olunce calinacak ses; bos birakilirsa sadece log uyarisi verilir (TODO: ata, Asama 5 ses tasarimi) */
	UPROPERTY(EditAnywhere, Category="Ses")
	USoundBase* DeathSound;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Character respawn timer */
	FTimerHandle RespawnTimer;

	/** Kismi ragdoll ("stagger") gorunumunun en fazla ne kadar surebilecegi; Landed() sadece knockback
	 *  impulse ile havalanildiginda tetiklenir, RadialDamage gibi impulse uygulamayan hasarlarda hic
	 *  tetiklenmeyebilir - bu yuzden suresiz kalmayi onlemek icin ayri bir zamanlayici kullanilir */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, Units = "s"))
	float StaggerSuresi = 4.0f;

	/** Stagger'i bitirip kismi ragdoll blend weight'ini sifirlamak icin kullanilan zamanlayici */
	FTimerHandle StaggerTimer;

	/** Copy of the mesh's transform so we can reset it after ragdoll animations */
	FTransform MeshStartingTransform;

	// ---- Odul sinirlari (ham tus odulleri ve [E] Ani NPC; sinirsiz tekrar edilmesin diye) ----

	/** [B] zipkin/balina avi: tekrar kullanilabilmesi icin gereken sure */
	UPROPERTY(EditAnywhere, Category="Oduller|B Zipkin", meta = (ClampMin = 0, Units = "s"))
	float HarpoonCooldown = 30.0f;

	/** [B] zipkin/balina avi sonrasi iyilesen can */
	UPROPERTY(EditAnywhere, Category="Oduller|B Zipkin", meta = (ClampMin = 0))
	float HarpoonHealAmount = 20.0f;

	/** [G] gemi sinifi yukseltme: gereken odun (ambar + sirt cantasi) */
	UPROPERTY(EditAnywhere, Category="Oduller|G Gemi", meta = (ClampMin = 0))
	int32 ShipUpgradeWoodCost = 3;

	/** [G] gemi sinifi yukseltme: Sal'dan sonraki her kademe icin gereken demir cevheri (ambar + sirt cantasi) */
	UPROPERTY(EditAnywhere, Category="Oduller|G Gemi", meta = (ClampMin = 0))
	int32 ShipUpgradeIronCost = 1;

	/** [X] Karanlik Yol baskini: tekrar yapilabilmesi icin gereken sure */
	UPROPERTY(EditAnywhere, Category="Oduller|X Karanlik Yol", meta = (ClampMin = 0, Units = "s"))
	float DarkPathRaidCooldown = 180.0f;

	/** [X] Karanlik Yol baskini: Komsu Koy Elcisi'ne en fazla bu mesafeden yapilabilir */
	UPROPERTY(EditAnywhere, Category="Oduller|X Karanlik Yol", meta = (ClampMin = 0, Units = "cm"))
	float DarkPathRaidRange = 450.0f;

	/** [X] Karanlik Yol baskini: kazanilan celik kulce */
	UPROPERTY(EditAnywhere, Category="Oduller|X Karanlik Yol", meta = (ClampMin = 0))
	int32 DarkPathSteelReward = 6;

	/** [X] Karanlik Yol baskini: kazanilan demir cevheri */
	UPROPERTY(EditAnywhere, Category="Oduller|X Karanlik Yol", meta = (ClampMin = 0))
	int32 DarkPathIronReward = 8;

	/** [X] Karanlik Yol baskini: kazanilan tuz */
	UPROPERTY(EditAnywhere, Category="Oduller|X Karanlik Yol", meta = (ClampMin = 0))
	int32 DarkPathSaltReward = 5;

	/** [J] koylu meslek vardiyasi: tekrar verilebilmesi icin gereken sure */
	UPROPERTY(EditAnywhere, Category="Oduller|J Vardiya", meta = (ClampMin = 0, Units = "s"))
	float VillagerShiftCooldown = 90.0f;

	/** [J] koylu meslek vardiyasi: ambara gelen odun */
	UPROPERTY(EditAnywhere, Category="Oduller|J Vardiya", meta = (ClampMin = 0))
	int32 VillagerShiftWoodReward = 3;

	/** [J] koylu meslek vardiyasi: ambara gelen demir cevheri */
	UPROPERTY(EditAnywhere, Category="Oduller|J Vardiya", meta = (ClampMin = 0))
	int32 VillagerShiftIronReward = 2;

	/** [J] koylu meslek vardiyasi: ambara gelen tuz */
	UPROPERTY(EditAnywhere, Category="Oduller|J Vardiya", meta = (ClampMin = 0))
	int32 VillagerShiftSaltReward = 2;

	/** [E] Olumsuz Ani NPC kutsamasi (tam can, skor, guven): tekrar alinabilmesi icin gereken sure */
	UPROPERTY(EditAnywhere, Category="Oduller|E Ani NPC", meta = (ClampMin = 0, Units = "s"))
	float MemoryNPCBlessingCooldown = 120.0f;

	/** [M] isirik muayenesi: en yakin koylu en fazla bu mesafede olmali */
	UPROPERTY(EditAnywhere, Category="Oduller|M Isirik Muayenesi", meta = (ClampMin = 0, Units = "cm"))
	float BiteInspectionRange = 300.0f;

	/** Cooldown'lar: ilgili odulun tekrar verilebilecegi en erken dunya zamani (sn) */
	float NextHarpoonAllowedTime = 0.0f;
	float NextDarkPathRaidAllowedTime = 0.0f;
	float NextVillagerShiftAllowedTime = 0.0f;
	float NextMemoryNPCBlessingAllowedTime = 0.0f;

	/** Cooldown dolduysa true doner ve bir sonraki izin zamanini kurar; dolmadiysa kalan sureyi bildirir */
	bool TryConsumeCooldown(float& NextAllowedTime, float Cooldown, const TCHAR* ActionName);

public:

	/** Constructor */
	ACombatCharacter();

	/** Su an kusanilmis silah; BeginPlay'de WeaponClass'tan spawn edilir */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Melee Attack|Weapon")
	ADSMeleeWeapon* EquippedWeapon;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for combo attack input */
	void ComboAttackPressed();

	/** Called for combo attack input pressed */
	void ChargedAttackPressed();

	/** Called for combo attack input released */
	void ChargedAttackReleased();

	/** Called for toggle camera side input */
	void ToggleCamera();

	/** Called for interact input */
	void InteractPressed();

	/** BP hook to animate the camera side switch */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void BP_ToggleCamera();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles combo attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackStart();

	/** Handles combo attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackEnd();

	/** Handles charged attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackStart();

	/** Handles charged attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackEnd();

protected:

	/** Resets the character's current HP to maximum */
	void ResetHP();

	/** Performs a combo attack */
	void ComboAttack();

	/** Performs a charged attack */
	void ChargedAttack();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	
public:

	// ~begin CombatAttacker interface

	/** Performs the collision check for an attack */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs the combo string check */
	virtual void CheckCombo() override;

	/** Performs the charged attack hold check */
	virtual void CheckChargedAttack() override;

	/** Loops or resolves the charged attack animation */
	void LoopOrResolveChargedAttack();

	// ~end CombatAttacker interface

	// ~begin CombatDamageable interface

	/** Notifies nearby enemies that an attack is coming so they can react */
	void NotifyEnemiesOfIncomingAttack();

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows reaction to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end CombatDamageable interface

	/** Called from the respawn timer to destroy and re-create the character */
	void RespawnCharacter();

	/** StaggerTimer tarafindan cagrilir: kismi ragdoll blend weight'ini sifirlar (Landed() tetiklenmese bile) */
	void BitirStagger();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Blueprint handler to play damage dealt effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DealtDamage(float Damage, const FVector& ImpactPoint);

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Handles input bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handles possessed initialization */
	virtual void NotifyControllerChanged() override;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** HUD icin: oyuncunun can yuzdesini (0-1) dondurur */
	FORCEINLINE float GetHealthPercent() const { return MaxHP > 0.0f ? CurrentHP / MaxHP : 0.0f; }
};
