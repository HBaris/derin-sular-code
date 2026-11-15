# Orman Level'ı — Hazırlık

Bu belge `Lvl_Forest` için editör tarafında yapılacak kurulum adımlarını,
ücretsiz asset önerilerini ve dalga sisteminin level-bağımsız çalıştığının
doğrulamasını içerir. Tüm adımlar editörde yapılır; bu iş için kod değişikliği
gerekmedi (bkz. madde c).

## a) Lvl_Forest Boş Level Oluşturma

1. Content Browser'da yeni bir klasör açın: `Content/Variant_Combat/Maps/`
   (mevcut `Lvl_ThirdPerson` gibi diğer level'lar da genelde kendi variant
   klasöründe duruyor; orman level'ı Combat variant'ı kullanacağı için oraya
   koymak tutarlı olur).
2. **File > New Level**, şablon olarak **Empty Level**'ı seçin (Basic/VR
   şablonlarını değil — kendi ışık/atmosfer kurulumunu elle yapacağız, bkz.
   madde d).
3. **Save Current Level As** ile `Lvl_Forest` adıyla yukarıdaki klasöre
   kaydedin. Türkçe karakter kullanmayın (proje kuralı, bkz. `CLAUDE.md`).
4. Bu aşamada `Lvl_Forest`'ı proje varsayılan haritası yapmanıza gerek yok:
   `DefaultEngine.ini`'deki `EditorStartupMap`/`GameDefaultMap` hâlâ
   `Lvl_ThirdPerson`'a işaret ediyor. Test için editörde **File > Open
   Level > Lvl_Forest** ile manuel açıp PIE ile oynayabilirsiniz. Level'ı
   projenin gerçek oyun haritası yapmak istediğinizde bu iki ayarı
   `Lvl_Forest`'a çevirmeniz yeterli (kod değişikliği gerekmez).

## b) Ücretsiz Orman Asset Önerileri (Fab / Quixel)

Fab (eski Quixel Bridge/Marketplace birleşik mağazası) üzerinde "Free"
filtresiyle aranacak, orman biome'una uygun paketler:

- **Ağaç:** Quixel Megascans **"Forest Tree Pack"** ve tekil Megascans ağaç
  scan'leri (Fab'da "Megascans Trees" araması, çoğu Epic hesabıyla ücretsiz);
  alternatif olarak Epic'in **"Photorealistic Nature Pack"** (ücretsiz
  aylık Fab paketlerinden biri olarak sık çıkar) iyi çeşitlilikte gövde/tepe
  varyasyonu sunar.
- **Çimen / Yer Örtüsü:** Megascans **"Grass & Ground Cover"** koleksiyonları
  (Fab'da "Megascans Grass" araması) — Nanite destekli, Landscape üzerine
  foliage tool ile serpiştirmeye uygun. Alt bitki örtüsü için Megascans
  "Undergrowth"/"Ferns" paketleri.
- **Kaya:** Megascans **"Cliff & Rock"** ve **"Boulder"** scan setleri —
  ölçek çeşitliliği için hem büyük kayalık formasyon hem küçük taş
  varyantlarını indirin.
- **Skybox / Atmosfer:** Ayrı bir skybox mesh/texture paketine gerek yok;
  UE5'in yerleşik **SkyAtmosphere + SkyLight + VolumetricCloud** aktörleri
  fotogerçekçi gökyüzü için yeterli (bkz. madde d). Ek çeşitlilik isterseniz
  Fab'da ücretsiz **"Ultra Dynamic Sky"** free/lite sürümü (varsa) veya
  Epic'in kendi örnek projelerinden (Valley of the Ancient, Electric Dreams)
  türetilmiş ücretsiz atmosfer preset'leri değerlendirilebilir.

Not: Fab'daki "ücretsiz" etiketli içerik genelde Epic Games hesabına bağlı
bir kerelik "claim" gerektirir (Marketplace'teki eski "Free This Month" /
kalıcı ücretsiz paketler mantığıyla aynı) — indirmeden önce Fab hesabınızın
projeye bağlı Epic Games hesabıyla aynı olduğundan emin olun.

## c) Dalga Sisteminin Level-Bağımsız Çalıştığının Doğrulanması

`DSWaveManager` ve `CombatEnemySpawner` kodunda hardcoded level referansı
arandı (`Lvl_`, `GetWorld()->GetName()`, `OpenLevel`, level path string'i vb.)
— **hiçbir dosyada böyle bir referans yok.** İki sınıf da salt Actor
davranışı: `DSWaveManager::ManagedSpawners` editörde elle atanan bir
`TArray<AActor*>`, spawner'lar kendi `SpawnCapsule` transform'larına göre
spawn oluyor. Hangi level'a yerleştirilirlerse orada çalışırlar; level adını
hiç sorgulamıyorlar.

Tek content-path referansı `CombatEnemySpawner`'ın constructor'ındaki
varsayılan `EnemyClass` ataması:

```cpp
static ConstructorHelpers::FClassFinder<ACombatEnemy> ZombieClassFinder(
    TEXT("/Game/Variant_Combat/Blueprints/AI/Zombies/BP_Zombie_Aylak.BP_Zombie_Aylak_C"));
```

Bu bir **level referansı değil, asset referansı**dır (bulunamazsa sadece
uyarı loglar ve `EnemyClass` boş kalır) ve her spawner instance'ında
Details panelinden farklı bir zombi BP'siyle override edilebilir. Orman
level'ında spawner'lar bu varsayılanı aynen miras alır (Aylak spawn eder)
ama isterseniz her spawner'ı diğer zombi tipleriyle (bkz. `ZOMBI_TIPLERI.md`)
ayarlayabilirsiniz.

**Sonuç:** Kod tarafında hiçbir değişiklik gerekmiyor. `Lvl_Forest`'a bir
`DSWaveManager` ve istediğiniz kadar `CombatEnemySpawner` yerleştirip
`ManagedSpawners`'ı doldurmak, `Lvl_ThirdPerson`'daki kurulumun birebir
aynısı kadar basit.

## d) Basit Level Kurulumu (500x500m alan)

1. **Landscape:** Landscape Mode'da yeni bir Landscape oluşturun,
   *Section Size* 63x63, *Sections Per Component* 1x1, *Number of
   Components* 8x8 gibi bir kombinasyonla ~505x505m civarına gelecek şekilde
   ayarlayın (tam 500m için gerekirse Landscape transform'unun Scale'ini
   sonradan ince ayar yapın). Varsayılan düz Landscape materyali yeterli;
   orman zemin dokusu için Megascans toprak/çim texture'larından basit bir
   Landscape Material kurabilirsiniz (opsiyonel, ilk iterasyon için şart
   değil).
2. **DirectionalLight:** Sahneye bir `DirectionalLight` sürükleyin, Mobility
   = Movable, güneşe uygun bir Rotation verin (örn. Pitch -45°). *Atmosphere
   Sun Light* seçeneğini işaretleyin ki SkyAtmosphere ile senkron çalışsın.
3. **SkyAtmosphere:** Bir `SkyAtmosphere` aktörü ekleyin — varsayılan
   ayarlar fotogerçekçi gökyüzü/atmosferik perspektif için yeterli
   başlangıç noktasıdır.
4. **SkyLight:** `SkyAtmosphere` ile birlikte bir `SkyLight` ekleyin
   (Real Time Capture açık), ortam ışığının atmosferden beslenmesi için.
5. **ExponentialHeightFog:** Bir `ExponentialHeightFog` aktörü ekleyin;
   orman derinlik hissi için *Fog Density* değerini düşük tutup (0.02–0.05
   civarı) *Volumetric Fog*'u açmanızı öneririz — ağaçların arasında hafif
   pus, mesafe algısını güçlendirir.
6. **Post Process Volume (opsiyonel ama önerilir):** *Infinite Extent*
   işaretli bir `PostProcessVolume` ekleyip exposure'ı sabitlemek (Auto
   Exposure min/max eşitleyin) test sırasında ışık değişimlerinin göz
   yormasını engeller.
7. **Player Start:** Bir `PlayerStart` yerleştirin (Combat variant'ın
   `BP_ThirdPersonGameMode`'unu bu level'ın World Settings'inde GameMode
   Override olarak seçmeyi unutmayın, aksi halde varsayılan template
   GameMode'u kullanılır ve dalga sistemi/HUD çalışmaz).
8. **DSWaveManager + Spawner'lar:** `ADSWaveManager` ve istediğiniz kadar
   `ACombatEnemySpawner`'ı Landscape üzerine, ağaç/kaya kümelerinin arasına
   dağıtarak yerleştirin; `DSWaveManager`'ın `Managed Spawners` listesine
   hepsini ekleyin (bkz. madde c — bu adım her level'da tekrarlanan, koddan
   bağımsız editör işi).
9. **Foliage:** Foliage Mode ile madde (b)'deki ağaç/çimen/kaya asset'lerini
   Landscape üzerine serpiştirin; ilk iterasyonda yoğunluğu düşük tutup
   performansı gözlemleyin, sonra artırın.

Bu adımlar tamamlandığında `Lvl_Forest`, `Lvl_ThirdPerson` ile aynı
C++ altyapısını (madde c) kullanarak dalga tabanlı zombi spawn'ını
çalıştırabilir hale gelir.
