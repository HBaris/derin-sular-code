# Zombi Tipleri — Stat Önerileri ve Dengeleme

Bu belge GDD'deki 6 zombi tipinin stat değerlerini ve dengeleme gerekçelerini
listeler. Veri yapısı `Source/DerinSular/DSZombiTypes.h` içindeki
`FZombiTipiVerisi` struct'ıdır; uygulama akışı `CombatEnemy::ApplyZombiTipiVerisi`
(BeginPlay'de HP init'ten sonra, StateTree başlamadan önce çağrılır) ile
`DT_ZombiTipleri` adlı bir DataTable'dan satır okuyarak yapılır.

**Kapsam notu:** C++ tarafı `HP` (MaxHP/CurrentHP), `MoveSpeed` (MaxWalkSpeed),
`Damage` (MeleeDamage) ve artık `TintColor`'ı düşmana uyguluyor —
`ApplyZombiTipiVerisi`, mesh'in 0. materyal slotuna bir dynamic material
instance oluşturup DataTable'daki rengi materyalin `PaintTint` adlı Vector/Color
parametresine yazıyor (UI'da "Paint Tint" olarak görünür; gerçek parametre adı
bu — `MI_Zombie_Aylak_HeadLegs`/`MI_Zombie_Aylak_Torso`'da doğrulandı, `TintColor`
değil). **Önkoşul:** zombi mesh'inin kullandığı materyalde bu isimde
(`PaintTint`) bir parametre olmalı; yoksa MID oluşur ama renk sessizce
değişmez (Output Log'da bir hata görmezsiniz, sadece görsel olarak fark
etmez). `AbilityTag` alanı hâlâ DataTable'da taşınıyor ama davranışları
StateTree task/condition olarak henüz yazılmadı (Aşama 2,
bkz. `DERIN_SULAR_YOL_HARITASI.md`).

## Stat Tablosu

| Tip | HP | Speed | Damage | AbilityTag | Durum |
|---|---|---|---|---|---|
| Aylak | 3 | 300 | 1 | — | Uygulandı (`BP_Zombie_Aylak`) |
| Kosucu | 2 | 550 | 0.5 | — | Planlanan |
| Siskin | 8 | 150 | 3 | `Zombi.Yetenek.Patlama` | Planlanan |
| Cigirtkan | 2 | 250 | 0.5 | `Zombi.Yetenek.Ciglik` | Planlanan |
| Zirhli | 6 | 200 | 1.5 | `Zombi.Yetenek.ZirhAzaltma` | Planlanan |
| Drifter | 4 | 350 | 1 | `Zombi.Yetenek.FazGecis` | Planlanan |

Aylak, referans/baseline tip olarak alınır: orta HP, orta hız, orta hasar.
Diğer tipler bu baseline'a göre bir eksende belirgin şekilde sapar, böylece
oyuncu görüş anında tipi ayırt edip önceliklendirebilir.

## Dengeleme Gerekçeleri

**Kosucu (HP 2 / Speed 550 / Damage 0.5)**
Hız ekseninde uç örnek: Aylak'ın ~1.8 katı hızda ama HP'si ve hasarı düşük.
Yetenek etiketi yok — hız tek başına mekanik farkı yaratıyor (sprint,
kod/veri tarafında ayrı bir "yetenek" olarak modellenmiyor). Amaç: oyuncuyu
konumlanmaya zorlamak — yakalanırsa hızlıca öldürülebilir (düşük HP), ama
yakalanmazsa hızla mesafe kapatıp sık vuruş yapabilir. Tek başına tehlikeli
değil, sürü içinde flank tehdidi.

**Siskin (HP 8 / Speed 150 / Damage 3)**
Tank arketipi: en yüksek HP, en yüksek darbe hasarı, en yavaş hareket.
`Zombi.Yetenek.Patlama` ile öldüğünde alan hasarı verir — bu, oyuncuyu
"düşürünce hemen üstüne çıkma" refleksini cezalandırır ve öldürme anını da
taktiksel hale getirir (mesafeden bitirme ya da geri çekilme). Yavaş hızı
sayesinde oyuncu onu görünce kaçabilir; asıl risk kalabalıkta sıkışıp
patlamaya yakalanmak.

**Cigirtkan (HP 2 / Speed 250 / Damage 0.5)**
Tek başına en zayıf savaşçı (Kosucu ile aynı HP/Damage ama orta hız).
`Zombi.Yetenek.Ciglik` ile yakındaki zombileri uyarıp/çağırarak sürüyü
büyütür — gerçek tehdidi kendi stat'larında değil, erken öldürülmezse
yarattığı ikincil dalga baskısında. Düşük savaş stat'ları bilinçli: oyuncuyu
"önce çığırtkanı sustur" önceliklendirmesine iter, ama bunu yaparken
kolayca öldürülebilir olması riski dengeler.

**Zirhli (HP 6 / Speed 200 / Damage 1.5)**
Orta-yüksek HP ve orta hasar, yavaşa yakın hız — görünürde Siskin'den daha
az tehlikeli ama `Zombi.Yetenek.ZirhAzaltma` ile gelen hasarı azaltarak
efektif HP'sini stat tablosundaki değerden daha yüksek gösterir. Bu,
oyuncunun tek vuruşluk combo yerine sürdürülebilir/şarjlı saldırıya
yönelmesini teşvik eder; yavaş hızı sayesinde geri çekilip başka hedefe
geçmek de geçerli bir strateji.

**Drifter (HP 4 / Speed 350 / Damage 1)**
Tüm stat'larda Aylak'a yakın ama hafif üstünde — asıl fark
`Zombi.Yetenek.FazGecis` ile beklenmedik yana kayma/faz geçişi hareketi.
Stat'lar kasıtlı olarak vasat tutuldu; tehdidi ham güçten değil,
öngörülemeyen hareket paterninden gelir (oyuncunun trace/swing zamanlamasını
şaşırtır). Sürüde "çeşni" görevi görür.

## GameplayTag Ön Koşulu

`Zombi.Yetenek.Patlama`, `Zombi.Yetenek.Ciglik`, `Zombi.Yetenek.ZirhAzaltma`
ve `Zombi.Yetenek.FazGecis` etiketleri `Config/DefaultGameplayTags.ini`'ye
native tag olarak eklendi. Kosucu'nun `AbilityTag` alanı, yeteneği hıza
gömülü olduğu için boş bırakılır.

---

## Editörde Yapılacaklar (her tip için)

Önkoşul: `DT_ZombiTipleri` DataTable'ı zaten oluşturulmuş olmalı (Row
Structure: `ZombiTipiVerisi`), Aylak satırı içeriyor olmalı (önceki iş).
Aşağıdaki adımları Kosucu, Siskin, Cigirtkan, Zirhli ve Drifter için tekrarlayın.

### 1) DataTable satırını gir

`DT_ZombiTipleri`'yi açın, **+ Add** ile yeni satır ekleyin, satır adını tipin
Türkçe karaktersiz adıyla verin (`Kosucu`, `Siskin`, `Cigirtkan`, `Zirhli`,
`Drifter`). Yukarıdaki tablodaki `HP` / `MoveSpeed` / `Damage` değerlerini
girin. `TintColor` için tipe özgü ayırt edici bir renk seçin (örn. Kosucu:
turuncu, Siskin: koyu yeşil, Cigirtkan: mor, Zirhli: gri-mavi, Drifter: mavi).
`AbilityTag` alanına yukarıdaki tabloda listelenen tag'i seçin (Kosucu için
boş bırakın). Tag seçilebilmesi için önce Build.bat ile tam derleme yapılmış
ve editör yeniden açılmış olmalı (native gameplay tag'ler derleme sonrası
Project Settings > GameplayTags altında görünür).

### 2) Blueprint alt sınıfını oluştur

`BP_Zombie_Aylak`'ı sağ tık > **Duplicate** ile kopyalayın (aynı mesh/anim
kurulumunu miras almak için en hızlı yol), adını `BP_Zombie_<Tip>` yapın
(örn. `BP_Zombie_Kosucu`). Duplicate yerine Content Browser'dan
`ACombatEnemy`'den yeni bir Blueprint Class oluşturmak da eşdeğerdir, ama mesh/
anim/melee trace ayarlarını elle tekrar girmeniz gerekir.

### 3) Zombi tipi verisini bağla

Yeni BP'yi açın, **Class Defaults** sekmesinde **Zombi Tipi** kategorisi
altında: `Zombi Tipleri Tablosu` = `DT_ZombiTipleri`, `Zombi Tipi Satir Adi` =
DataTable'a girdiğiniz satır adı (örn. `Kosucu`). Compile + Save.

### 4) Görsel ayırt edicilik

`ApplyZombiTipiVerisi` artık `TintColor`'ı otomatik uyguluyor — ekstra BP işi
gerekmiyor, **tek önkoşul** zombi mesh'inin materyalinde `PaintTint` adında
("Paint Tint" olarak görünür) bir Vector/Color parametresi olması —
`MI_Zombie_Aylak_HeadLegs`/`MI_Zombie_Aylak_Torso`'da bu parametre zaten mevcut.
Yeni tip BP'leri için kullandığınız materyal instance'ında da aynı parametrenin
bulunduğunu doğrulayın; yoksa ekleyip mesh'in base color'ına bağlayın.

### 5) Doğrulama

PIE'de bir spawner'ın `Enemy Class`'ını yeni BP'ye çevirip spawn ettirin;
Output Log'da `"<isim>: zombi tipi '<Tip>' uygulandi (HP=... Hiz=... Hasar=... Tint=...)"`
satırını görmelisiniz. Görmüyorsanız (uyarı logu varsa) tablo/satır adı eşleşmesini
kontrol edin. Mesh rengi değişmiyor ama log doğruysa, materyaldeki `TintColor`
parametresi eksik/yanlış adlandırılmış demektir (madde 4).

### 6) Dalga/tip eşlemesine ekleme (tüm 5 tip için BP'ler hazır olunca)

`DSWaveManager`'ı seçin, Details panelinde **Dalga Tipi Karisimlari**
(`Wave Manager` kategorisi) listesine satırlar ekleyin: her satır bir
`Baslangic Dalgasi` (bu dalgadan itibaren geçerli) ve bir `Dusman Siniflari`
listesi (o aralıkta karışacak `BP_Zombie_*` sınıfları) alır. Örnek:
Wave 1'den → sadece `BP_Zombie_Aylak`; Wave 3'ten → `BP_Zombie_Aylak` +
`BP_Zombie_Kosucu`; Wave 5'ten → hepsi. Liste boş bırakılırsa (varsayılan)
her spawner kendi `Enemy Class`'ını kullanmaya devam eder, hiçbir şey
bozulmaz.
