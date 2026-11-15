# DERİN SULAR — YOL HARİTASI & DEVİR-TESLİM
*Son güncelleme: 13 Temmuz 2026*

Bu dosya, "Derin Sular" oyun projesinin güncel durumunu ve sıradaki işleri özetler. Yeni bir sohbete başlarken bu dosyayı ve repodaki `CLAUDE.md`, `ROADMAP.md`, `DURUM.md` dosyalarını okut.

---

## PROJE ÖZETİ

- **Ne:** Türkçe, zombi dalga hayatta kalma oyunu (wave survival)
- **Motor:** Unreal Engine 5.8, C++ + Blueprint
- **Konum:** `C:\Projects\DerinSular`
- **Repo:** github.com/HBaris/derin-sular (private, main branch aktif)
- **Geliştirici:** Solo (Barış)
- **Temel şablon:** UE Variant_Combat (zombiler `ACombatCharacter` hiyerarşisinde yaşıyor)

---

## ÇALIŞMA ORTAMI & AKIŞ

- **Editör işleri** (BP, DataTable, level, materyal, PIE testi): ev bilgisayarında, editör açıkken
- **Kod + doküman işleri**: Claude Code (CMD veya Desktop app) — remote control ile telefondan da erişilebiliyor
- **Remote control:** `claude --remote-control` → telefonda Claude app → Code sekmesi → yeşil "Connected"
- **Token tasarrufu:** kısa/tek-işli promptlar, `/compact`, gereksiz uzun log yapıştırmama

### KRİTİK DERLEME KURALLARI
1. **Yeni UPROPERTY / UFUNCTION / header değişikliği / yeni dosya** → `Build.bat` ile TAM derleme gerekir (Live Coding yetmez). Editör KAPALI olmalı.
   ```
   "C:\UE_5.8\Engine\Build\BatchFiles\Build.bat" DerinSularEditor Win64 Development -Project="C:\Projects\DerinSular\DerinSular.uproject" -WaitMutex
   ```
2. **Sadece .cpp gövde değişikliği** → editörde Ctrl+Alt+F11 (Live Coding) yeterli.
3. **ALTIN KURAL:** `CombatEnemy.h` / `CombatAIController.h` / `CombatCharacter.h` değişen her tam derlemeden sonra → editörde `ST_CombatEnemy` asset'ini aç → **F7 (Compile)** → **Ctrl+S**. StateTree'nin C++ property binding'leri header değişiminde geçersizleşiyor; yapılmazsa StartLogic sessizce çalışmaz (durum=Unset), zombiler idle kalır.

### PROJE KURALLARI
- Commit mesajları: tek satır, Türkçe, ASCII (Türkçe karaktersiz)
- Dosya yollarında ve DataTable satır adlarında Türkçe karakter YOK (Kosucu, Siskin, Cigirtkan, Zirhli — ç/ş/ı değil)
- İrreversible sunucu/git işlemlerinde önce doğrulama
- Derleme gerektiren commit'ler mesajda `[Build.bat]` etiketli

---

## TAMAMLANANLAR ✅

1. **Dalga sistemi çekirdeği** (`DSWaveManager`)
   - Otomatik ilk dalga (`bAutoStartFirstWave`, `FirstWaveDelay` varsayılan 10sn)
   - Dalga başına artan düşman sayısı (`SetSpawnCount`)
   - Dalga tamamlanınca otomatik sonraki dalga (8sn ara)
   - T tuşu manuel tetikleme (yedek)
   - Spawner reset mantığı

2. **StateTree AI — "born-dead" bug'ı kalıcı çözüldü**
   - Kök neden 1: Possession (`OnPossess`) pawn'un BeginPlay'inden önce çalışıyordu → StateTree HP=0 iken başlayıp Dead state'e düşüyordu. Çözüm: `StartStateTreeLogic()` HP init sonrasına ertelendi.
   - Kök neden 2: Header değişince StateTree binding'i geçersizleşiyor → StartLogic sessizce çalışmıyor. Çözüm: ALTIN KURAL (yukarıda).
   - Constructor'da `bStartAILogicOnPossess=false` + `SetStartLogicAutomatically(false)` ile başlatma tamamen manuel kontrolde.

3. **Data-driven zombi tipi altyapısı** (`DSZombiTypes.h/.cpp`)
   - `FZombiTipiVerisi` struct (FTableRowBase): HP, MoveSpeed, Damage, TintColor, AbilityTag (FGameplayTag)
   - `UDSZombiTipiKutuphanesi::ZombiTipiVerisiYukle()` — DataTable satır okuyucu
   - `ApplyZombiTipiVerisi()` — CombatEnemy BeginPlay'de veriyi düşmana uyguluyor (HP, MoveSpeed, MeleeDamage)
   - Build.cs'e `GameplayTags` modülü eklendi

4. **Dalga bazlı stat rampası**
   - `HPCarpaniPerWave` (0.10) + `HizCarpaniPerWave` (0.05)
   - SpawnActorDeferred ile çarpanlar BeginPlay'den önce uygulanıyor
   - Wave N: HP × (1 + (N-1)×0.10) — doğrulandı (Wave 4 zombisi 4 yumruk)

5. **DT_ZombiTipleri DataTable + Aylak tipi**
   - `Content/Variant_Combat/Blueprints/AI/Data/DT_ZombiTipleri`
   - Aylak satırı girildi (HP=3, Speed=300, Damage=1), BP_Zombie_Aylak'a bağlandı — çalışıyor

6. **Yakın dövüş silah sistemi** (`DSMeleeWeapon` + `ACombatCharacter`)
   - Silah actor'ü oyuncunun eline spawn+attach ediliyor (hand_r soketi)
   - Placeholder: Engine Cube mesh (görsel dikdörtgen prizma)
   - Hasar mevcut combo/charged-attack sistemi üzerinden (DoAttackTrace'e dokunulmadı — bilinçli)
   - WeaponClass = DSMeleeWeapon atandı, IA_Attack input bağlı

7. **Aşama 1 — Zombi çeşitliliği** (editörde tamamlandı)
   - 6 zombi tipi: `DT_ZombiTipleri` satırları + `BP_Zombie_Aylak/Kosucu/Siskin/Cigirtkan/Zirhli/Drifter`
   - `ZombieTint` — `M_Mannequin`'e eklenen ayrı katman, Paint Tint'in logo maskeli davranışına dokunmadan tüm gövdeyi renklendiriyor
   - `DalgaTipiKarisimlari` dolduruldu: Wave 1-2 Aylak, 3-4 +Kosucu, 5-6 +Siskin/Zirhli, 7+ tüm tipler

8. **Aşama 2 — Zombi yetenekleri** (kod tamamlandı, 13 Temmuz gece oturumu — **henüz derlenmedi/test edilmedi**)
   - Şişkin — patlama (önceki turda tamamlanmış, editörde henüz doğrulanmadı)
   - Çığırtkan — çığlık/aggro yayma: mevcut `GetPlayerInfo` StateTree task'ı üzerinden, yeni StateTree node'u eklenmeden
   - Zırhlı — hasar azaltma (%35, `TakeDamage`'da)
   - Drifter — faz geçişi (periyodik yanal itki + kısa hasarsızlık penceresi)

9. **Aşama 3/4/5'in kod ile yapılabilecek kısmı** (kod tamamlandı, 13 Temmuz gece oturumu — **henüz derlenmedi/test edilmedi**)
   - Vuruş geri bildirimi altyapısı: `OnHitReceived` delegesi + hit-stop
   - DSWaveManager level-agnostik doğrulandı (kod değişikliği gerekmedi)
   - HUD C++ iskeleti: `UDSHUDWidget` + `DSWaveManager`/`GameMode` veri delegeleri
   - Ölüm/kazanma GameMode hook'ları: `OnPlayerDied`/`OnGameWon`/skor/`RestartCurrentLevel`
   - Ses tetikleme hook'ları (placeholder, TODO'lu — Şişkin `ExplosionEffect` deseniyle aynı)
   - Lvl_Forest iskeleti denemesi ATLANDI (UE MCP sunucusuna bağlanılamadı, editörde elle yapılmalı)

---

## SIRADAKİ İŞLER (öncelik sırasıyla)

### HEMEN — 13 Temmuz gece oturumunun derlemesi ve testi (kritik, kullanıcı)
1. **Build.bat tam derleme** (editör kapalıyken) — bu gece 8 commit `CombatEnemy.h`, `CombatGameMode.h`, `DSWaveManager.h`, `CombatEnemySpawner.h`, `CombatCharacter.h` değiştirdi.
2. **ALTIN KURAL:** `ST_CombatEnemy`'yi aç → F7 → Ctrl+S.
3. **Aşama 2 yetenek testi (PIE):**
   - Çığırtkan bir düşman spawn olsun, oyuncuyu görünce yakındaki diğer zombiler oyuncuya yönelmeli (log: "ciglik atildi").
   - Zırhlı bir düşmana vur, diğer tiplere göre daha az hasar almalı (~%35 az).
   - Drifter bir düşman birkaç saniyede bir rastgele yana kaçmalı ve o anda vurulunca hasar almamalı.
   - Aylak/Koşucu'da hiçbir yeni davranış TETİKLENMEMELİ (regresyon kontrolü).
4. **HUD debug mesajları:** `UDSHUDWidget`'ı bir WBP'ye taşı/test et, ekranda "Dalga X", "Skor Y", "Kalan Dusman Z", "Oyuncu Cani %W" mesajları çıkmalı.
5. **Ölüm/kazanma testi:** oyuncu ölünce log'da "oyuncu oldu" çıkmalı; 10 dalga tamamlanınca "dalga 10 tamamlandi" + kazanma logu çıkmalı (varsayılan hedef `KazanmaIcinGerekliDalgaSayisi=10`, test için düşürülebilir).

### AŞAMA 3 — Görsel & his iyileştirme (kod tamamlandı, görsel iş editörde)
- ~~Vuruş geri bildirimi altyapısı~~ ✅ kod tamamlandı (`OnHitReceived` + hit-stop), test bekliyor
- **Gerçek silah mesh'i:** placeholder küp yerine balta/sopa mesh'i (Fab/Marketplace ücretsiz asset veya basit modelleme)
- **Zombi görselleri:** stok manken yerine gerçek zombi görünümü — materyal (kan, çürüme, yırtık kıyafet) veya Fab'dan zombi karakter modelleri
- **Ragdoll iyileştirme:** ölüm anında daha inandırıcı fizik
- **Gerçek vuruş efekti/kamera sarsıntısı:** `OnHitReceived`'a bağlanacak (şu an TODO)

### AŞAMA 4 — World building (orman haritası)
- ~~DSWaveManager level-agnostik doğrula~~ ✅ kod incelendi, değişiklik gerekmedi
- **Lvl_Forest oluştur:** yeni level (DirectionalLight + SkyAtmosphere + ExponentialHeightFog + Landscape) — MCP denemesi bağlantı hatasıyla atlandı, editörde elle yapılmalı (veya MCP sunucusu başlatılıp tekrar denenebilir)
- **Orman asset'leri:** Fab/Quixel'den ücretsiz ağaç, çimen, kaya, skybox
- **Oynanabilir level tasarımı:** oyuncu başlangıcı, spawn noktaları, çevre engelleri
- **HEDEF DENEYİM:** yeşillikte, ağaçların arasında baltayla zombi kafası ezme

### AŞAMA 5 — Oyun döngüsü & meta (C++ iskeleti tamamlandı, görsel/ses iş editörde)
- ~~HUD altyapısı~~ ✅ kod tamamlandı (`UDSHUDWidget`), gerçek WBP görsel tasarımı editörde yapılmalı
- ~~Ölüm/kazanma delegeleri + RestartCurrentLevel~~ ✅ kod tamamlandı, gerçek Game Over/Kazandın UMG ekranları yok
- ~~Ses tetikleme hook'ları~~ ✅ placeholder eklendi, gerçek ses asset'leri atanmadı
- (Uzun vade) Koloni/barınak mekaniği, gece-gündüz, kaynak toplama

---

## GERÇEKÇİ ZAMAN TAHMİNİ
- **Aşama 1** (çeşitlilik): 1 oturum (birkaç saat)
- **Aşama 2** (yetenekler): 2-3 oturum
- **Aşama 3** (görsel his): 1-2 hafta
- **Aşama 4** (orman): 1-2 hafta (asset entegrasyonu + level tasarımı)
- **Aşama 5** (meta): 1+ ay

---

## BİLİNEN SORUNLAR / NOTLAR
- **13 Temmuz gece oturumunun tamamı derlenmedi/test edilmedi** — Build.bat + ALTIN KURAL + PIE testi bekliyor (bkz. yukarıdaki "HEMEN" listesi)
- Silah mesh'i placeholder (dikdörtgen prizma) — Aşama 3'te değişecek
- HUD gerçek widget'ı yok, sadece C++ iskelet + debug mesajları
- Game Over / Kazandın ekranları yok, sadece delegeler
- Ses hiç yok, sadece TODO'lu placeholder hook'lar
- Lvl_Forest henüz oluşturulmadı
- ZDBG teşhis logları hâlâ kodda — fix iyice oturunca temizlenecek, şimdilik kalsın
- Deprecation warning: `APawn::GetMovementBase` (motor kaynaklı, zararsız, görmezden gel)

---

## UE MCP BAĞLANTISI

- **Plugin:** `ModelContextProtocol` + `MCPClientToolset`, ikisi de UE 5.8 `Engine\Plugins\Experimental` altında gömülü, `DerinSular.uproject`'te `Enabled: true`.
- **Sunucu OTOMATİK BAŞLIYOR** (`bAutoStartServer=True`, `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` içinde `[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]` altında ayarlandı). Editör her açıldığında sunucu kendiliğinden başlar, manuel `ModelContextProtocol.StartServer` yazmaya gerek yok.
  - Varsayılan adres: `http://localhost:8000/mcp`
  - `Saved/` klasörü git tarafından ignore edildiği için bu ayar makineye özeldir; başka bir makinede tekrar ayarlanması gerekir (bkz. madde altındaki opsiyonel çözüm adımları veya ini'ye elle ekleme).
- Sunucu çalışmadan Claude Code'un UE MCP tool'ları (material graph inceleme, asset sorgulama vb.) ÇALIŞMAZ.
- **Claude Code tarafında bir kere bağlantı eklenmesi yeterli** (kalıcı, tekrar eklemeye gerek yok):
  ```
  claude mcp add --transport http unreal http://localhost:8000/mcp
  ```
- **ÖNEMLİ:** MCP bağlantısı bir Claude Code oturumu BAŞLADIKTAN SONRA eklenirse, o oturum tool'ları görmez — yeni oturum açmak gerekir. Yani `mcp add` komutu, o günkü ilk Claude Code oturumunu başlatmadan ÖNCE çalıştırılmalı.
- **Yeni bir makinede kurulumu tekrarlamak için:** Project Settings > Plugins > Model Context Protocol > Auto Start Server işaretlenir (veya yukarıdaki ini satırı elle eklenir).

---

## YENİ SOHBETE BAŞLARKEN
1. Bu dosyayı + `CLAUDE.md` + `ROADMAP.md` + `DURUM.md`'yi okut
2. "Aşama X'ten devam ediyoruz" diyerek net başla
3. Editör mü kod mu gerektiğini baştan belirt (derleme kuralları!)
4. Barış'a "komutanım" hitabı, Türkçe, direkt ve uygulanabilir adımlar
