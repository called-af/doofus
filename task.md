TUJUAN:
Ubah/tambah sistem generasi pulau melayang jadi bertema "Heaven Cluster" —
bukan sekadar single floating island terpisah-pisah ala skyblock generik,
tapi kelompok pulau yang terasa seperti "kepulauan surgawi" yang saling
terhubung secara visual meski tidak bersentuhan fisik.

VIBES YANG DIINGINKAN:
1. Cluster, bukan pulau tunggal acak — beberapa pulau kecil/menengah
   mengorbit atau berkumpul di sekitar 1 pulau anchor besar per cluster,
   mirip formasi arsipelago vertikal. Gunakan CellularNoise (islandF1/F2/
   cellId yang sudah ada) untuk menentukan cluster center dan variasi
   ukuran per-pulau dalam cluster yang sama.
2. Silhouette pulau melayang: bagian atas landai/plateau (tempat build-able
   flat area seperti skyblock), bagian bawah meruncing halus (bukan potongan
   rata) dengan variasi ketebalan/kedalaman under-belly yang smooth — hindari
   hard-cutoff noise threshold (pelajaran dari bug pillar sebelumnya: jangan
   sample tepat di kink tajam RidgeNoise, dan selalu domain-warp SEKALI per
   kolom, bukan per-Y, untuk menghindari pola spiral/donut).
3. Elevasi bertingkat antar pulau dalam cluster — pulau anchor paling tinggi,
   pulau satelit di ketinggian bervariasi di atas/bawahnya, menciptakan
   kesan "melayang di langit berlapis" bukan semuanya rata di 1 bidang Y.
4. Void di antara pulau harus benar-benar kosong (tidak ada floating debris
   acak) supaya kesan "mengambang di angkasa" terasa bersih, bukan noise
   berantakan.
5. Opsional tapi disarankan: pulau anchor terbesar per cluster punya
   fitur unik (mountain peak kecil, atau formasi kristal/pillar simetris)
   sebagai focal point visual, memakai islandMountain yang sudah ada di
   TerrainSample.

BATASAN TEKNIS:
- Semua parameter baru harus masuk ke Setting.h dengan naming convention
  yang konsisten sama yang sudah ada (contoh: heavenClusterScale,
  heavenClusterSpacing, dst).
- Jangan ubah signature fungsi publik yang sudah ada (sampleHeightAt,
  sampleLODHeightAt, sampleBlockAt) — LOD system bergantung pada ini.
- Pastikan cocok dengan sistem LOD 5-level yang sudah ada: bentuk cluster
  harus tetap approximable di LOD3+ tanpa perlu re-sample noise mahal
  (baca komentar existing soal "band-limited height field" di
  sampleLODHeightAt).
- Gunakan smoothstep/domain-warp untuk semua transisi baru, hindari
  hard threshold cutoff kecuali sudah dibuktikan aman (pelajaran dari
  bug pillar: threshold dekat 0.9+ pada RidgeNoise itu curam dan
  menghasilkan speckle/aliasing).
- Beri komentar penjelasan role tiap noise layer baru, seperti gaya
  komentar yang sudah ada di TerrainSampler.cpp dan TerrainGenerator.cpp.