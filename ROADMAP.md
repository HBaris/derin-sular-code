# ROADMAP — Derin Sular

Durum takibi: tamamlanan işler, aktif bug fix'i ve sıradaki işler. Detaylı mimari için `CLAUDE.md`'ye bakın.

## Tamamlananlar

- [x] Dalga sistemi çekirdeği: `DSWaveManager` — sayaç, `OnSpawnerDepleted` bind'i, dalgalar arası zamanlayıcı, otomatik döngü
- [x] `CombatEnemySpawner::ResetSpawner()` — spawner'lar çoklu dalgada yeniden kullanılabiliyor
- [x] Doğrusal zorluk rampası: `BaseEnemiesPerWave + (Wave-1) * EnemiesPerWaveIncrement`, `SetSpawnCount()` entegrasyonu
- [x] İlk zombi tipi: `BP_Zombie_Aylak` (yeşil materyal)
- [x] Level Blueprint test tetikleyicisi (T tuşu → `StartNextWave`)
- [x] `CLAUDE.md` — proje mimarisi, dalga akışı, derleme kuralları
- [x] Data-driven zombi tipi altyapısı: `FZombiTipiVerisi` struct'ı + `UDSZombiTipiKutuphanesi::ZombiTipiVerisiYukle` DataTable yükleyicisi (`DSZombiTypes.h/.cpp`)
- [x] Kalıcı oyun akışı: `bAutoStartFirstWave` + `FirstWaveDelay` ile Wave 1 otomatik başlıyor (T tuşu yedek)
- [x] 6 zombi tipi: `DT_ZombiTipleri` satırları + `BP_Zombie_*` alt sınıfları, `DalgaTipiKarisimlari` dolduruldu
- [x] ZombieTint (M_Mannequin ayrı katman) + Şişkin patlama yeteneği
- [x] Çığırtkan çığlık/aggro yayma, Zırhlı hasar azaltma, Drifter faz geçişi + Niagara vuruş/patlama/faz VFX entegrasyonu
- [x] Vuruş geri bildirimi altyapısı: `OnHitReceived` delegesi + hit-stop + `NS_Damage` Niagara vuruş efekti
- [x] Tam görsel UMG HUD (`UDSHUDWidget`): Sol üstte Oyuncu Canı ve Koloni Güveni barları, sağ üstte Dalga/Kalan Zombi/Skor paneli, ortada Oyun Bitti / Zafer banner'ı (`RebuildWidget` ile otomatik inşa + `CombatPlayerController` fallback)
- [x] Ölüm/kazanma GameMode hook'ları: `OnPlayerDied`/`OnGameWon`/skor/`RestartCurrentLevel` + tam ragdoll ölüm (`SetAllBodiesSimulatePhysics(true)` + `SetPhysicsBlendWeight(1.0f)`)
- [x] 8 adet gerçek WAV ses asset'i (`/Game/Audio/SFX_*`) üretildi, `USoundWave` olarak import edildi ve C++/BP tarafında Zombi/Oyuncu/Barikat/Köylü olaylarına bağlandı
- [x] 38 adet CC0 3B model (`Quaternius` / `Kenney`) indirildi ve projeye import edildi:
  - Silahlar: `SM_BattleAxe`, `SM_SurvivalAxe`, `SM_MedievalAxe`, `SM_WarHammer` (`ADSMeleeWeapon` artık küp yerine gerçek çift ağızlı savaş baltası kuşanıyor)
  - Köylüler: `BP_Villager_01..03` sivil kıyafet renk ayrımları, `BP_Villager_02` kadın karakter mesh'i (`SKM_Quinn_Simple`) ve sırtta otomatik `SM_Backpack` çanta eklentisi
  - Zombiler: Tip bazlı siluet ölçeklendirme + soket tabanlı 3B zırh/miğfer (`SM_Helmet_Knight`, `SM_Armor_Metal`, `SM_Helmet_Horned`, `SM_Armor_Leather`, `SM_Armor_Black`) + ek `SK_Quaternius_Zombie` ve `SM_Zombie_*` modelleri
  - Barikatlar: `ADSBarikat` kutu çarpışmasını koruyarak üstüne gerçek 3B ahşap barikat (`SM_Barikat_WoodFence` / `SM_Barikat_Boards`) giydirildi; `NotifyDanger` Seçenek C Hibrit alarm sistemi ve dost ateşi koruması eklendi
  - Orman Haritası (`Lvl_Forest`): `/Game/Variant_Combat/Maps/Lvl_Forest` oluşturuldu; çam ağaçları (`SM_PineTree_01/02`), kurumuş ağaçlar (`SM_DeadTree_01`), yosunlu kayalar (`SM_ForestRock_*`), kamp ateşi (`SM_Bonfire`), çadırlar (`SM_SurvivorTent`) ve köy evleri (`SM_VillageHouse_01`) yerleştirildi (`Lvl_Combat` kamp çevresi de zenginleştirildi)
- [x] B1/C7 Barikat Saldırı & Hasar Zinciri: `FStateTreeGetPlayerInfoTask` barikat yaklaşımlarında `Succeeded` dönüp 300cm menzilde `DoAIComboAttack()` tetikliyor; `ACombatEnemy::DoAttackTrace` ve `ADSMeleeWeapon::DoAttackTrace` `ECC_WorldDynamic` sorgusuyla barikatlara tam hasar veriyor
- [x] E Tuşu Etkileşim Garantisi: `ACombatPlayerController` `IMC_Default`'u otomatik ekliyor; `ACombatCharacter::InteractPressed` 250cm içindeki hasarlı barikatları tamir ediyor ve baygın köylüleri (`Kurtar`) ayağa kaldırıyor
- [x] **V2 Köylü & Sakallı Savaşçı Savaş Emri (`[V]` Tuşu):** Sakallı savaşçılar (`Villager02` + `SM_BattleAxe`) ve diğer köylüler (`Villager01`/`03` + `SM_SurvivalAxe`/`SM_MedievalAxe`) Savaş Emri verildiğinde zombilerden kaçmak yerine baltalarıyla zombilere saldırıyor (`DSVillager.cpp`, `DSVillagerController.cpp`)
- [x] **V2 1. Öncelik Bosslar (`Boss_Sunflower` & `Boss_Treeguard`) ve `[6] Kadim Orman Kılıcı`:** Ayçiçeği Koruyucusu yağ tenekesi fırlatma + Faz 3 Tanrıça Gücü + yenilince ölmeden müttefik olup zombilerle savaşma; ağaç kesme (`+4 Odun`) ile Treeguard uyanışı, `[C]` ile fidan dikme, Canlı Kütük ve `[6] LivingDarkSword` entegrasyonu
- [x] **V2 Canlı Dünya Etkileşimleri & Detroit Koloni Sistemi:** `[E]` ile Gizli Isırık Tetkiki, Geri Dönen Köylü kabulü, Şüpheli Kaya (Wakanda Tarzı Saklı Şehir Portalı), Gezgin Şövalye Onur Düellosu (Usta Dövüşçü ödülü), Swain Karga Easter Egg'i ve `[G]/[H]` Tersane Gemi İnşası / Kale Savunması kararları

## Çözülen Buglar

- [x] **StateTree born-dead / `Unset`'te kalma (19 Ağustos)** — Tam derleme + `ST_CombatEnemy` derlemesi ile çözüldü; `CombatEnemy.h` `UPROPERTY` düzeni korunarak ("ALTIN KURAL") yeni 3B ekipmanlar dinamik bileşenlerle eklendi.
- [x] **B1 / C7 Barikat Hasar ve Yaklaşma Sorunu (19 Ekim)** — İki kök neden C++ tarafında kalıcı olarak çözüldü: (1) `ACombatEnemy::DoAttackTrace` yalnızca `ECC_Pawn` aradığı için `BlockAllDynamic` (`ECC_WorldDynamic`) olan `ADSBarikat`'ı ıskalıyordu; `ECC_WorldDynamic` + 320cm doğrudan barikat menzil kontrolü eklendi. (2) `FStateTreeGetPlayerInfoTask::EnterState` barikat bulunduğunda `Failed` döndüğü ve geniş barikatların merkez pivotuna 50cm yaklaşılamadığı için zombiler ~250cm'de takılıyordu; `NearestBarikatDist <= 300.0f` iken barikata dönüp `DoAIComboAttack()` tetikleyecek ve `Succeeded` dönecek şekilde güncellendi.

- [x] **Canlı Oynanış Görsel ve Mekanik İyileştirmeleri (19 Ekim)** — Canlı oynanış ekran görüntüsü testlerinde tespit edilen 4 sorun giderildi: (1) Baygın köylünün (`bIsDowned`) ayakta kalması yerine `FDSCharacterBodyRig::bIsDownedPose` ile yere çökmesi ve kurtarılınca ayağa kalkması sağlandı, (2) Köylü çantası (`SM_Backpack`) göğüsten sırta (`Y=-13`) taşındı ve omuz-kol boşluğu kapatıldı, (3) Köylü kurtarma (`+10` Güven, `+50` Skor) ve barikat tamiri (`+5` Güven, `+25` Skor) anlık Koloni Güveni/Skor sistemine bağlandı, (4) `DSWaveManager` spawner otomatik keşfi ve HUD'da `SKOR + SILAH` ile canlı zombi sayacı düzeltildi.

## Sıradaki İşler ve İyileştirme Notları (M2 Öncesi / M2)

1. **(Görsel Geri Bildirim İyileştirmesi)** Hasarlı barikatların üzerinde dünya uzayında (World Space) can barı + yaklaşınca `[E] Tamir Et` etiketi ve baygın köylülerin üzerinde kalan kurtarma süresi göstergesi
2. **(İleriki Aşama - M2)** Gece/Gündüz döngüsü ve kaynak toplama (odun/erzak) ekonomisi (barikat tamirinin odun harcaması)
3. **(İleriki Aşama - M2)** Yeni yakın dövüş / menzilli silah çeşitlerinin envanter seçimi ile değiştirilmesi

