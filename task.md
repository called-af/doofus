# TASK: Buat Voxel Terrain Generator dari Nol — 3-Tier Vertical Realm
(Heaven / Normal / Hell), pakai teknik Feature-Point + Distance-Field
(BUKAN threshold-noise biasa)

Anggap tidak ada kode terrain generator sebelumnya sama sekali. Bangun dari
nol sistem yang menghasilkan dunia voxel dengan 3 lapisan tema vertikal
dalam satu kolom yang sama, dengan bentuk yang LEBAR, MASIF, dan SOLID
(bukan filamen/jarum tipis yang saling menjalar random — ini kesalahan
umum kalau geometri ditentukan murni dari threshold noise).

================================================================
PRINSIP DASAR ALGORITMA (WAJIB DIIKUTI)
================================================================
JANGAN gunakan pola "if (noise(x,z) > threshold) → solid" sebagai penentu
UTAMA bentuk pulau/pillar/canyon. Noise-threshold murni secara matematis
menghasilkan pola percolation (jaringan benang tipis saling terhubung
acak), bukan blob/gumpalan padat, berapa pun nilai threshold-nya diubah.

Sebagai gantinya, pakai pendekatan FEATURE-POINT + DISTANCE-FIELD
(teknik metaball/blob standar untuk procedural generation):

1. Sebar titik-titik "seed"/pusat fitur secara terkontrol di grid sel
   (misal tiap sel 150-250 block), dengan jitter acak dalam sel supaya
   tidak terlihat grid-aligned kaku. Tiap seed punya properti: posisi,
   radius pengaruh, tinggi/ketebalan, dan variasi bentuk — semua
   deterministic dari hash/noise berbasis seed dunia (reproducible).
2. Untuk tiap kolom dunia (x,z), cari seed terdekat (cukup cek sel
   sendiri + sekitarnya, tidak perlu scan seluruh dunia).
3. Hitung jarak kolom ke seed tersebut, normalisasi terhadap radius
   seed (0 = pusat, 1 = tepi). Gunakan smoothstep untuk falloff supaya
   tepinya melembut secara natural, bukan terpotong tegas.
4. BARU di tahap ini noise dipakai — sebagai perturbasi KECIL pada tepi
   (supaya bentuknya organik, tidak seperti lingkaran sempurna), BUKAN
   sebagai penentu solid/tidaknya seluruh struktur.
5. Jika ada dua seed yang salling berdekatan/tumpang tindih radiusnya,
   gabungkan secara mulus (smooth-union pada distance field) sehingga
   formasi bisa terlihat sebagai satu massa besar yang menyambung,
   bukan dua objek terpisah yang kebetulan bersinggungan.

Prinsip ini berlaku untuk SEMUA elemen geometri utama: pulau melayang,
canyon, dan pilar penghubung. Noise klasik (FBM biasa) tetap boleh
dipakai untuk hal-hal SEKUNDER: variasi ketinggian dalam batas kecil,
tekstur permukaan, penempatan vegetasi/kristal, penentuan biome/suhu,
dsb — bukan untuk menentukan ada/tidaknya massa solid di suatu titik.

================================================================
TIER 3 — HEAVEN (langit, paling atas)
================================================================
Referensi mood: pulau-pulau batu besar dan lebar melayang di atas lautan
awan, terpisah satu sama lain, dengan siluet puncak runcing di bagian
atas tapi badan bawahnya SANGAT LEBAR dan masif (bukan mengerucut dari
bawah — badan utama harus tetap tebal/gemuk, hanya bagian PALING atas
yang meruncing jadi puncak gunung). Bagian bawah pulau gelap dan sedikit
undercut (menipis tipis di beberapa block paling dasar saja, bukan
seluruh badan). Permukaan atas ditutupi rumput hijau lebat.

Implementasi:
- Tiap island-seed punya radius besar (40-90 block) dan thickness
  (30-70 block).
- 70% ketebalan bagian bawah = SLAB: solid penuh di seluruh area yang
  dicover oleh distance-field seed (edgeFactor > 0), lebar & masif,
  cuma menipis di 6-8 block paling bawah (undercut tipis).
- 30% ketebalan bagian atas = PUNCAK GUNUNG: mengerucut, dan makin
  dekat kolom itu ke PUSAT seed (dist kecil), makin tinggi puncak boleh
  terbentuk — sehingga puncak otomatis muncul di tengah pulau secara
  natural, bukan acak di pinggir.
- Beberapa pulau besar (radius terbesar / seed yang jarang muncul, misal
  1 dari setiap 6-8 seed) dijadikan "anchor" dengan formasi batu/kristal
  ekstra besar di puncaknya — jadi landmark visual utama.
- Sesekali (sangat jarang), dua seed berdekatan boleh digabung membentuk
  ARCH: dua "lengan" pilar dari masing-masing seed yang saling condong
  dan bertemu di puncak, membentuk jembatan/gerbang alami dengan celah
  pulau kecil melayang di bawahnya.
- Tandai kolom tepi pulau sebagai sumber air terjun (dipakai render/fluid
  system terpisah).
- Palet: batu terang keputihan untuk badan, rumput hijau cerah di atas,
  aksen kristal jarang di singkapan tebing bagian dalam yang gelap.

================================================================
TIER 2 — NORMAL (tengah)
================================================================
Terrain natural standar (gunung, dataran, sungai, biome berbasis
suhu/kelembapan). Boleh tetap pakai FBM noise konvensional untuk height-
map di sini karena tujuannya memang variasi organik skala luas, bukan
untuk memisahkan "solid vs kosong" secara biner seperti pulau/canyon.
Tidak perlu elemen fantasi khusus — ini jembatan transisi biasa.

================================================================
TIER 1 — HELL (dasar dunia)
================================================================
Referensi mood: SATU celah/canyon besar yang jelas dan menyambung,
mengular mengikuti jalur tertentu, dengan dinding curam gelap terbuka ke
atas, dan di dasarnya ada retakan lava tipis menyala.

Implementasi (SPINE-BASED, bukan area-threshold):
- Generate 1 garis "tulang punggung" (spine) canyon per region dunia:
  untuk tiap koordinat sepanjang satu sumbu, tentukan 1 titik pusat
  canyon di sumbu lainnya, mengikuti noise domain-warped sebagai arah
  meander (seperti menggambar aliran sungai raksasa).
- Kedalaman canyon di suatu kolom = fungsi dari JARAK kolom itu ke garis
  spine pada posisi tersebut (bukan noise area independen). Semakin
  dekat ke spine, semakin dalam & lebar canyon-nya; smoothstep falloff
  supaya dinding curam tapi transisinya tetap natural.
- Ini menjamin hasilnya SATU celah besar yang jelas dan menyambung utuh,
  bukan retakan-retakan kecil tersebar acak di seluruh peta.
- Lava vein: ridge noise tipis (garis retakan menyala), tapi HANYA aktif
  di area yang sudah ditentukan sebagai dasar canyon (dekat spine) —
  jadi tetap tipis dan terkontrol dalam struktur besar yang solid,
  bukan noise independen yang bisa nongol di sembarang tempat.
- Tebing atas canyon (dekat perbatasan ke tier normal) tetap ditumbuhi
  rumput hijau, transisi natural ke biome di atasnya.
- Palet: basalt gelap untuk dinding/dasar, obsidian di sekitar lava,
  abu/cinder sebagai lapisan permukaan longgar dekat area panas.

================================================================
PILAR PENGHUBUNG (continent → hell, opsional tapi disarankan)
================================================================
Kalau ingin ada struktur pilar besar yang menyangga daratan tier normal
dari bawah (seperti fondasi raksasa menuju dasar dunia), JANGAN generate
dengan noise threshold terpisah — itu yang menyebabkan hasil seperti
"landak"/jarum-jarum tipis pada percobaan sebelumnya. Sebagai gantinya:
pilar hanya boleh muncul TEPAT DI BAWAH footprint yang sudah solid dari
continent-mask di atasnya (distance-field yang sama dipakai untuk bentuk
pulau/continent), dengan radius pilar yang mengecil secara smooth menuju
bagian tengah ketinggian (efek jam pasir/hourglass) — dikendalikan oleh
fungsi jarak dari pusat continent, BUKAN oleh nilai noise acak per-block.
Ini menjamin pilar selalu menyatu solid dengan daratan yang didukungnya.

================================================================
TRANSISI ANTAR TIER
================================================================
Gunakan smoothstep berbasis ketinggian Y di zona perbatasan supaya
kepadatan terrain menipis gradual mendekati batas tier berikutnya — tidak
boleh ada "lantai" tegas yang terasa buatan.

================================================================
DELIVERABLE
================================================================
1. Struct "FeatureSeed" (posisi X/Z, radius, thickness/ketebalan, tipe
   fitur: island/pillar/canyon-spine-point, flag khusus seperti hasPeak
   atau isAnchor) dan fungsi generate seed per grid-cell yang
   deterministic terhadap world seed.
2. Fungsi pencarian seed terdekat untuk suatu kolom dunia (cukup scan
   grid sel di sekitarnya, efisien, tidak scan seluruh dunia).
3. Fungsi distance-field → solid-fill dengan smoothstep falloff dan
   smooth-union untuk seed yang saling tumpang tindih.
4. Generator terpisah per tier (hell via spine-distance, normal via FBM
   heightmap konvensional, heaven via seed distance-field) dipanggil
   berurutan dari satu entry point per-chunk, dengan cache per-kolom
   supaya noise tidak dihitung ulang di setiap pass (base/surface/cave).
5. Sampler standalone (height & block-at-point) yang PAKAI ALGORITMA
   IDENTIK dengan generator chunk penuh — supaya versi LOD jarak jauh
   dan chunk detail dekat tetap konsisten, tidak ada seam/mismatch.
6. Daftar tipe blok baru yang dibutuhkan beserta kegunaannya (batu gelap
   utk canyon, obsidian, abu/cinder, lava, batu langit terang, kristal).
7. Parameter yang bisa di-tuning dikumpulkan di satu tempat (ukuran grid
   sel per tier, range radius/thickness seed, lebar canyon dasar,
   amplitude meander, threshold lava, dll).
8. Penjelasan singkat kenapa distance-field dipilih dibanding threshold-
   noise murni untuk tiap elemen structural (island, pillar, canyon).

Full implementasi C++ yang bisa langsung dipakai/diadaptasi, bukan cuma
pseudocode. Reuse convention penamaan & struktur (ColumnCache-style
per-column cache, static class methods) yang lazim dipakai di voxel
engine berbasis chunk.