#include "GreedyMesher.h"
#include "../Chunk.h"
#include <vector>

// ─────────────────────────────────────────────
// Satu cell di "face mask" — mewakili satu face
// yang perlu di-render di posisi (u, v) pada slice tertentu
// ─────────────────────────────────────────────
struct FaceMaskCell {
  BlockType blockType; // tipe blok pemilik face ini
  bool isBackFace;     // false = face menghadap +axis, true = menghadap -axis
  int textureLayerId;  // index layer di GL_TEXTURE_2D_ARRAY
};

// ─────────────────────────────────────────────
// Helper: push 1 vertex ke vertex buffer
// Format per vertex: [x, y, z, u, v, textureLayer]
// ─────────────────────────────────────────────
static inline void pushVertex(std::vector<float> &vertexBuffer, float x,
                              float y, float z, float u, float v,
                              float textureLayer) {
  vertexBuffer.push_back(x);
  vertexBuffer.push_back(y);
  vertexBuffer.push_back(z);
  vertexBuffer.push_back(u);
  vertexBuffer.push_back(v);
  vertexBuffer.push_back(textureLayer);
}

// ─────────────────────────────────────────────
// Pilih texture layer berdasarkan tipe blok + arah face
//
// d = axis normal face:
//   d=0 → face menghadap X (sisi kiri/kanan)
//   d=1 → face menghadap Y (sisi atas/bawah)  ← top vs bottom Grass di sini
//   d=2 → face menghadap Z (sisi depan/belakang)
//
// Grass punya 3 texture berbeda:
//   layer 0 = top (rumput hijau)
//   layer 1 = side (grass side)
//   layer 2 = bottom (tanah)
// ─────────────────────────────────────────────
static inline int getTextureLayerForFace(BlockType blockType, int faceAxis,
                                         bool isBackFace) {
  if (blockType == BlockType::Grass) {
    if (faceAxis == 1)
      return isBackFace ? 2 : 0; // Y-axis: atas=0, bawah=2
    return 1;                    // sisi = layer 1
  }
  if (blockType == BlockType::Dirt)
    return 2;
  if (blockType == BlockType::Stone)
    return 3;
  if (blockType == BlockType::Sand)
    return 4;
  return 0;
}

// ─────────────────────────────────────────────
// Emit satu quad (persegi panjang) sebagai 2 segitiga
//
// 4 corner quad (winding order penting untuk face culling!):
//
// w = lebar quad dalam satuan blok (arah u)
// h = tinggi quad dalam satuan blok (arah v)
// UV di-stretch sesuai w/h → seamless tiling otomatis
//
// faceAxis: menentukan orientasi UV mapping
//   faceAxis=0 (X) dan faceAxis=2 (Z): texture normal
//   faceAxis=1 (Y / top-bottom): rotasi UV berbeda
// ─────────────────────────────────────────────
static void emitQuad(std::vector<float> &vertexBuffer, float p0x, float p0y,
                     float p0z, float p1x, float p1y, float p1z,
                     float p2x, float p2y, float p2z,
                     float p3x, float p3y, float p3z,
                     int faceAxis, bool isBackFace, int quadWidth,
                     int quadHeight, int textureLayerId,
                     int uvOffsetU, int uvOffsetV) {
  float uvWidth  = (float)quadWidth;
  float uvHeight = (float)quadHeight;
  float u0 = uvOffsetU,            v0 = uvOffsetV;
  float u1 = uvOffsetU + uvWidth,  v1 = uvOffsetV + uvHeight;

  if (faceAxis == 1) {
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v1, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v0, textureLayerId);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v1, textureLayerId);
    }
  } else if (faceAxis == 2) {
    // North/South face — tetap sama, sudah benar
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v0, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v1, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v1, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v1, textureLayerId);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u1, v1, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u1, v0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u0, v1, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u0, v0, textureLayerId);
    }
} else {
    // East/West face (faceAxis == 0) — swap U dan V
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, v0, u1, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, v1, u0, textureLayerId);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId);
      pushVertex(vertexBuffer, p3x, p3y, p3z, v1, u0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId);
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId);
      pushVertex(vertexBuffer, p1x, p1y, p1z, v0, u1, textureLayerId);
    }
}
}

// ─────────────────────────────────────────────
// GreedyMesher::build — entry point utama
//
// Algoritma greedy meshing:
// 1. Loop 3 axis (X, Y, Z)
// 2. Per axis, loop tiap "slice" (layer)
// 3. Di tiap slice, bangun face mask → array of FaceMaskCell
// 4. Scan mask, merge face yang sama jadi quad besar
// 5. Emit quad ke vertex buffer
// ─────────────────────────────────────────────
void GreedyMesher::build(Chunk &chunk, Chunk *neighborNX, Chunk *neighborPX,
                         Chunk *neighborNZ, Chunk *neighborPZ) {
  chunk.vertices.clear();
  chunk.empty = true;

  // ── Helper query blok lintas chunk ──
  // Didefinisikan DI SINI, sebelum semua loop
  auto getBlockWithNeighbors = [&](int lx, int ly, int lz) -> BlockType {
    if (ly < 0 || ly >= Chunk::HEIGHT)
      return BlockType::Air;

    if (lx >= 0 && lx < Chunk::SIZE && lz >= 0 && lz < Chunk::SIZE)
      return chunk.getBlock(lx, ly, lz);

    if (lx >= 0 && lx < Chunk::SIZE && lz >= 0 && lz < Chunk::SIZE && ly >= 0 &&
        ly < Chunk::HEIGHT)
      return chunk.getBlock(lx, ly, lz);

    if (lx < 0)
      return neighborNX ? neighborNX->getBlock(lx + Chunk::SIZE, ly, lz)
                        : BlockType::Air;

    if (lx >= Chunk::SIZE)
      return neighborPX ? neighborPX->getBlock(lx - Chunk::SIZE, ly, lz)
                        : BlockType::Air;

    if (lz < 0)
      return neighborNZ ? neighborNZ->getBlock(lx, ly, lz + Chunk::SIZE)
                        : BlockType::Air;

    if (lz >= Chunk::SIZE)
      return neighborPZ ? neighborPZ->getBlock(lx, ly, lz - Chunk::SIZE)
                        : BlockType::Air;

    return BlockType::Air; // default fallback
  };

  // Face mask untuk satu slice
  // Ukuran = area maksimum satu slice (SIZE × HEIGHT atau SIZE × SIZE)
  FaceMaskCell faceMask[Chunk::SIZE * Chunk::HEIGHT * Chunk::SIZE];

  // Dimensi chunk: [SIZE, HEIGHT, SIZE] → diakses via dims[axis]
  int chunkDimensions[3] = {Chunk::SIZE, Chunk::HEIGHT, Chunk::SIZE};

  // Posisi saat iterasi (x[0]=X, x[1]=Y, x[2]=Z)
  int currentPos[3];

  // Step vektor 1 unit ke arah axis d (untuk akses blok "tetangga")
  int stepAlongAxis[3];

  for (int faceAxis = 0; faceAxis < 3; faceAxis++) {
    // Dua axis yang tegak lurus terhadap faceAxis
    // Contoh: faceAxis=1(Y) → uAxis=2(Z), vAxis=0(X)
    int uAxis = (faceAxis + 1) % 3;
    int vAxis = (faceAxis + 2) % 3;

    // stepAlongAxis = vektor 1 langkah ke arah faceAxis
    stepAlongAxis[0] = stepAlongAxis[1] = stepAlongAxis[2] = 0;
    stepAlongAxis[faceAxis] = 1;

    // ── Loop tiap slice di axis faceAxis ──
    // Mulai dari -1 agar bisa cek face di tepi negatif chunk
    for (currentPos[faceAxis] = -1;
         currentPos[faceAxis] < chunkDimensions[faceAxis];
         currentPos[faceAxis]++) {
      // ── Bangun face mask untuk slice ini ──
      int maskIndex = 0;
      for (currentPos[vAxis] = 0; currentPos[vAxis] < chunkDimensions[vAxis];
           currentPos[vAxis]++)
        for (currentPos[uAxis] = 0; currentPos[uAxis] < chunkDimensions[uAxis];
             currentPos[uAxis]++) {
          // blockA = blok di posisi saat ini
          // blockB = blok di posisi saat ini + 1 langkah ke faceAxis
          BlockType blockA =
              (currentPos[faceAxis] >= 0)
                  ? getBlockWithNeighbors(currentPos[0], currentPos[1],
                                          currentPos[2])
                  : BlockType::Air;

          BlockType blockB =
              (currentPos[faceAxis] < chunkDimensions[faceAxis] - 1)
                  ? getBlockWithNeighbors(currentPos[0] + stepAlongAxis[0],
                                          currentPos[1] + stepAlongAxis[1],
                                          currentPos[2] + stepAlongAxis[2])
                  : getBlockWithNeighbors( // ← dulu ini langsung return Air
                        currentPos[0] + stepAlongAxis[0],
                        currentPos[1] + stepAlongAxis[1],
                        currentPos[2] + stepAlongAxis[2]);

          bool isBlockASolid = (blockA != BlockType::Air);
          bool isBlockBSolid = (blockB != BlockType::Air);

          if (isBlockASolid == isBlockBSolid) {
            // Keduanya sama (solid-solid atau air-air) → tidak ada face
            faceMask[maskIndex++] = {BlockType::Air, false, -1};
          } else if (isBlockASolid) {
            // A solid, B air → face menghadap ke arah +faceAxis (front face)
            faceMask[maskIndex++] = {
                blockA, false, getTextureLayerForFace(blockA, faceAxis, false)};
          } else {
            // A air, B solid → face menghadap ke arah -faceAxis (back face)
            faceMask[maskIndex++] = {
                blockB, true, getTextureLayerForFace(blockB, faceAxis, true)};
          }
        }

      // Maju ke slice berikutnya (kompensasi dari loop for yang akan increment
      // lagi)
      currentPos[faceAxis]++;

      // Helper lambda: konversi (uPos, vPos) → flat index di faceMask
      auto getMaskIndex = [&](int uPos, int vPos) {
        return uPos + vPos * chunkDimensions[uAxis];
      };

      // ── Scan mask + greedy merge ──
      for (int vPos = 0; vPos < chunkDimensions[vAxis]; vPos++)
        for (int uPos = 0; uPos < chunkDimensions[uAxis];) {
          FaceMaskCell currentCell = faceMask[getMaskIndex(uPos, vPos)];
          if (currentCell.blockType == BlockType::Air) {
            uPos++;
            continue; // skip cell kosong
          }

          // Expand ke kanan (tambah lebar quad selama tipe+layer sama)
          int mergedWidth = 1;
          while (uPos + mergedWidth < chunkDimensions[uAxis]) {
            FaceMaskCell &neighbor =
                faceMask[getMaskIndex(uPos + mergedWidth, vPos)];
            if (neighbor.blockType != currentCell.blockType ||
                neighbor.isBackFace != currentCell.isBackFace ||
                neighbor.textureLayerId != currentCell.textureLayerId)
              break;
            mergedWidth++;
          }

          // Expand ke bawah (tambah tinggi quad selama seluruh row cocok)
          int mergedHeight = 1;
          bool canExpandDown = false;
          while (vPos + mergedHeight < chunkDimensions[vAxis] &&
                 !canExpandDown) {
            for (int col = 0; col < mergedWidth; col++) {
              FaceMaskCell &neighbor =
                  faceMask[getMaskIndex(uPos + col, vPos + mergedHeight)];
              if (neighbor.blockType != currentCell.blockType ||
                  neighbor.isBackFace != currentCell.isBackFace ||
                  neighbor.textureLayerId != currentCell.textureLayerId) {
                canExpandDown = true;
                break;
              }
            }
            if (!canExpandDown)
              mergedHeight++;
          }

          // Vektor quad: duStep = arah U × lebar, dvStep = arah V × tinggi
          int duStep[3] = {0, 0, 0};
          int dvStep[3] = {0, 0, 0};
          duStep[uAxis] = mergedWidth;
          dvStep[vAxis] = mergedHeight;

          // Origin quad di world space (lokal chunk)
          int quadOrigin[3] = {0, 0, 0};
          quadOrigin[faceAxis] = currentPos[faceAxis];
          quadOrigin[uAxis] = uPos;
          quadOrigin[vAxis] = vPos;

          // 4 corner quad (float untuk dikirim ke GPU)
          float corner0x = (float)quadOrigin[0];
          float corner0y = (float)quadOrigin[1];
          float corner0z = (float)quadOrigin[2];
          float corner1x = corner0x + duStep[0];
          float corner1y = corner0y + duStep[1];
          float corner1z = corner0z + duStep[2];
          float corner2x = corner1x + dvStep[0];
          float corner2y = corner1y + dvStep[1];
          float corner2z = corner1z + dvStep[2];
          float corner3x = corner0x + dvStep[0];
          float corner3y = corner0y + dvStep[1];
          float corner3z = corner0z + dvStep[2];

          // Offset world position berdasarkan posisi chunk di dunia
          float chunkOffsetX = (float)(chunk.chunkX * Chunk::SIZE);
          float chunkOffsetZ = (float)(chunk.chunkZ * Chunk::SIZE);
          corner0x += chunkOffsetX;
          corner1x += chunkOffsetX;
          corner2x += chunkOffsetX;
          corner3x += chunkOffsetX;
          corner0z += chunkOffsetZ;
          corner1z += chunkOffsetZ;
          corner2z += chunkOffsetZ;
          corner3z += chunkOffsetZ;

          int worldU = quadOrigin[uAxis] + (uAxis == 0 ? chunk.chunkX * Chunk::SIZE
                                              : uAxis == 2 ? chunk.chunkZ * Chunk::SIZE
                                                           : 0);
          int worldV = quadOrigin[vAxis] + (vAxis == 0 ? chunk.chunkX * Chunk::SIZE
                                              : vAxis == 2 ? chunk.chunkZ * Chunk::SIZE
                                                           : 0);

          emitQuad(chunk.vertices, corner0x, corner0y, corner0z, corner1x,
                   corner1y, corner1z, corner2x, corner2y, corner2z, corner3x,
                   corner3y, corner3z, faceAxis, currentCell.isBackFace,
                   mergedWidth, mergedHeight, currentCell.textureLayerId,
                   worldU, worldV);

          chunk.empty = false;

          // Hapus cell yang sudah di-merge dari mask
          // supaya tidak di-proses lagi di iterasi berikutnya
          for (int row = 0; row < mergedHeight; row++)
            for (int col = 0; col < mergedWidth; col++)
              faceMask[getMaskIndex(uPos + col, vPos + row)] = {BlockType::Air,
                                                                false, -1};

          uPos += mergedWidth;
        }

      // Kembalikan posisi (kompensasi dari increment manual di atas)
      currentPos[faceAxis]--;
    }
  }
}