#include "GreedyMesher.h"
#include "../Chunk.h"
#include <iostream>
#include <vector>

// ─────────────────────────────────────────────
// A single cell in the "face mask" — represents one face
// that needs to be rendered at position (u, v) on a specific slice
// ─────────────────────────────────────────────
struct FaceMaskCell {
  BlockType blockType; // block type owning this face
  bool isBackFace;     // false = face faces +axis, true = faces -axis
  int textureLayerId;  // layer index in GL_TEXTURE_2D_ARRAY
};

// ─────────────────────────────────────────────
// Helper: push 1 vertex to the vertex buffer
// Vertex format: [x, y, z, u, v, textureLayer, light]
// ─────────────────────────────────────────────
static inline void pushVertex(std::vector<float> &vertexBuffer, float x,
                              float y, float z, float u, float v,
                              float textureLayer, float light) {
  vertexBuffer.push_back(x);
  vertexBuffer.push_back(y);
  vertexBuffer.push_back(z);

  vertexBuffer.push_back(u);
  vertexBuffer.push_back(v);

  vertexBuffer.push_back(textureLayer);

  vertexBuffer.push_back(light);
}

// ─────────────────────────────────────────────
// Select the texture layer based on block type + face direction
//
// faceAxis = face normal axis:
//   faceAxis=0 → face faces X (left/right side)
//   faceAxis=1 → face faces Y (top/bottom side)  ← top vs bottom Grass is handled here
//   faceAxis=2 → face faces Z (front/back side)
//
// Grass has 3 different textures:
//   layer 0 = top (green grass)
//   layer 1 = side (grass side)
//   layer 2 = bottom (dirt)
// ─────────────────────────────────────────────
static inline int getTextureLayerForFace(BlockType blockType, int faceAxis,
                                         bool isBackFace) {
  if (blockType == BlockType::Grass) {
    if (faceAxis == 1)
      return isBackFace ? 2 : 0; // Y-axis: top=0, bottom=2
    return 1;                    // side = layer 1
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
// Emit a single quad (rectangle) as 2 triangles
//
// 4 quad corners (winding order is crucial for face culling!):
//
// quadWidth = quad width in block units (u direction)
// quadHeight = quad height in block units (v direction)
// UVs are stretched according to width/height → automatic seamless tiling
//
// faceAxis: determines the UV mapping orientation
//   faceAxis=0 (X) and faceAxis=2 (Z): normal texture orientation
//   faceAxis=1 (Y / top-bottom): different UV rotation
// ─────────────────────────────────────────────
static void emitQuad(std::vector<float> &vertexBuffer, float p0x, float p0y,
                     float p0z, float p1x, float p1y, float p1z, float p2x,
                     float p2y, float p2z, float p3x, float p3y, float p3z,
                     int faceAxis, bool isBackFace, int quadWidth,
                     int quadHeight, int textureLayerId, int uvOffsetU,
                     int uvOffsetV) {
  float uvWidth = (float)quadWidth;
  float uvHeight = (float)quadHeight;
  float u0 = uvOffsetU, v0 = uvOffsetV;
  float u1 = uvOffsetU + uvWidth, v1 = uvOffsetV + uvHeight;
  float light = 1.0f;

  if (faceAxis == 1) {
    light = isBackFace ? 0.55f : 1.0f;
  } else if (faceAxis == 0) {
    light = 0.80f;
  } else {
    light = 0.70f;
  }

  if (faceAxis == 1) {
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v0, textureLayerId, light);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v1, textureLayerId, light);
    }
  } else if (faceAxis == 2) {
    // North/South face — remains the same, already correct
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u0, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u1, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u0, v1, textureLayerId, light);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, u1, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, u1, v0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, u0, v1, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, u0, v0, textureLayerId, light);
    }
  } else {
    // East/West face (faceAxis == 0) — swap U and V
    if (!isBackFace) {
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, v0, u1, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, v1, u0, textureLayerId, light);
    } else {
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId, light);
      pushVertex(vertexBuffer, p3x, p3y, p3z, v1, u0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId, light);
      pushVertex(vertexBuffer, p0x, p0y, p0z, v0, u0, textureLayerId, light);
      pushVertex(vertexBuffer, p2x, p2y, p2z, v1, u1, textureLayerId, light);
      pushVertex(vertexBuffer, p1x, p1y, p1z, v0, u1, textureLayerId, light);
    }
  }
}

// ─────────────────────────────────────────────
// GreedyMesher::build — main entry point
//
// Greedy meshing algorithm:
// 1. Loop through the 3 axes (X, Y, Z)
// 2. For each axis, loop through every "slice" (layer)
// 3. In each slice, build a face mask → array of FaceMaskCell
// 4. Scan the mask, merge identical faces into a larger quad
// 5. Emit quads into the vertex buffer
// ─────────────────────────────────────────────
void GreedyMesher::build(const Chunk &chunk, Chunk *neighborNX,
                         Chunk *neighborPX, Chunk *neighborNZ,
                         Chunk *neighborPZ, std::vector<float> &outVertices) {
  outVertices.clear();

  // ── Cross-chunk block query helper ──
  // Defined HERE, before any loops
  auto getBlockWithNeighbors = [&](int lx, int ly, int lz) -> BlockType {
    if (ly < 0 || ly >= Chunk::HEIGHT)
      return BlockType::Air;

    // Within own chunk range
    if (lx >= 0 && lx < Chunk::SIZE && lz >= 0 && lz < Chunk::SIZE)
      return chunk.getBlock(lx, ly, lz);

    // Neighbor X
    if (lx < 0)
      return neighborNX ? neighborNX->getBlock(lx + Chunk::SIZE, ly, lz)
                        : BlockType::Air;
    if (lx >= Chunk::SIZE)
      return neighborPX ? neighborPX->getBlock(lx - Chunk::SIZE, ly, lz)
                        : BlockType::Air;

    // Neighbor Z
    if (lz < 0)
      return neighborNZ ? neighborNZ->getBlock(lx, ly, lz + Chunk::SIZE)
                        : BlockType::Air;
    if (lz >= Chunk::SIZE)
      return neighborPZ ? neighborPZ->getBlock(lx, ly, lz - Chunk::SIZE)
                        : BlockType::Air;

    return BlockType::Air;
  };

  // Face mask for a single slice
  // Size = maximum area of a single slice (SIZE × HEIGHT or SIZE × SIZE)
  FaceMaskCell faceMask[Chunk::SIZE * Chunk::HEIGHT * Chunk::SIZE];

  // Chunk dimensions: [SIZE, HEIGHT, SIZE] → accessed via chunkDimensions[axis]
  int chunkDimensions[3] = {Chunk::SIZE, Chunk::HEIGHT, Chunk::SIZE};

  // Iteration position (currentPos[0]=X, currentPos[1]=Y, currentPos[2]=Z)
  int currentPos[3];

  // Step vector 1 unit along axis faceAxis (to access the "neighboring" block)
  int stepAlongAxis[3];

  for (int faceAxis = 0; faceAxis < 3; faceAxis++) {
    // Two axes perpendicular to faceAxis
    // Example: faceAxis=1(Y) → uAxis=2(Z), vAxis=0(X)
    int uAxis = (faceAxis + 1) % 3;
    int vAxis = (faceAxis + 2) % 3;

    // stepAlongAxis = 1-step vector along faceAxis
    stepAlongAxis[0] = stepAlongAxis[1] = stepAlongAxis[2] = 0;
    stepAlongAxis[faceAxis] = 1;

    // ── Loop through each slice along faceAxis ──
    // Start from -1 to check faces on the negative edge of the chunk
    for (currentPos[faceAxis] = -1;
         currentPos[faceAxis] < chunkDimensions[faceAxis];
         currentPos[faceAxis]++) {
      // ── Build face mask for this slice ──
      int maskIndex = 0;
      for (currentPos[vAxis] = 0; currentPos[vAxis] < chunkDimensions[vAxis];
           currentPos[vAxis]++)
        for (currentPos[uAxis] = 0; currentPos[uAxis] < chunkDimensions[uAxis];
             currentPos[uAxis]++) {
          // blockA = block at current position
          // blockB = block at current position + 1 step along faceAxis
          BlockType blockA = getBlockWithNeighbors(currentPos[0], currentPos[1],
                                                   currentPos[2]);

          BlockType blockB =
              getBlockWithNeighbors(currentPos[0] + stepAlongAxis[0],
                                    currentPos[1] + stepAlongAxis[1],
                                    currentPos[2] + stepAlongAxis[2]);

          bool isBlockASolid = (blockA != BlockType::Air);
          bool isBlockBSolid = (blockB != BlockType::Air);

          if (isBlockASolid == isBlockBSolid) {
            // Both are identical (solid-solid or air-air) → no face generated
            faceMask[maskIndex++] = {BlockType::Air, false, -1};
          } else if (isBlockASolid) {
            // A is solid, B is air → face faces +faceAxis direction (front face)
            faceMask[maskIndex++] = {
                blockA, false, getTextureLayerForFace(blockA, faceAxis, false)};
          } else {
            // A is air, B is solid → face faces -faceAxis direction (back face)
            faceMask[maskIndex++] = {
                blockB, true, getTextureLayerForFace(blockB, faceAxis, true)};
          }
        }

      currentPos[faceAxis]++;

      // Helper lambda: convert (uPos, vPos) → flattened index in faceMask
      auto getMaskIndex = [&](int uPos, int vPos) {
        return uPos + vPos * chunkDimensions[uAxis];
      };

      // ── Scan mask + greedy merge ──
      for (int vPos = 0; vPos < chunkDimensions[vAxis]; vPos++)
        for (int uPos = 0; uPos < chunkDimensions[uAxis];) {
          FaceMaskCell currentCell = faceMask[getMaskIndex(uPos, vPos)];
          if (currentCell.blockType == BlockType::Air) {
            uPos++;
            continue; // skip empty cell
          }

          // Expand right (increase quad width as long as block type + layer match)
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

          // Expand down (increase quad height as long as the entire row matches)
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

          // Quad vectors: duStep = U direction × width, dvStep = V direction × height
          int duStep[3] = {0, 0, 0};
          int dvStep[3] = {0, 0, 0};
          duStep[uAxis] = mergedWidth;
          dvStep[vAxis] = mergedHeight;

          // Quad origin in world space (chunk local)
          int quadOrigin[3] = {0, 0, 0};
          quadOrigin[faceAxis] = currentPos[faceAxis];
          quadOrigin[uAxis] = uPos;
          quadOrigin[vAxis] = vPos;

          // 4 quad corners (floats to be sent to the GPU)
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

          // Offset world position based on the chunk's world coordinates
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

          int worldU =
              quadOrigin[uAxis] +
              (uAxis == 0 ? chunk.chunkX * Chunk::SIZE
                          : (uAxis == 2 ? chunk.chunkZ * Chunk::SIZE : 0));
          int worldV =
              quadOrigin[vAxis] +
              (vAxis == 0 ? chunk.chunkX * Chunk::SIZE
                          : (vAxis == 2 ? chunk.chunkZ * Chunk::SIZE : 0));

          // 2. Send data to worker thread's local outVertices
          emitQuad(outVertices, corner0x, corner0y, corner0z, corner1x,
                   corner1y, corner1z, corner2x, corner2y, corner2z, corner3x,
                   corner3y, corner3z, faceAxis, currentCell.isBackFace,
                   mergedWidth, mergedHeight, currentCell.textureLayerId,
                   worldU, worldV);

          // 3. THE KEY TO GREEDY MESHING: Clear masked cells that have been merged
          // so they aren't reprocessed in subsequent uPos coordinate iterations
          for (int v = 0; v < mergedHeight; v++) {
            for (int u = 0; u < mergedWidth; u++) {
              faceMask[getMaskIndex(uPos + u, vPos + v)] = {BlockType::Air,
                                                            false, -1};
            }
          }

          // Advance uPos by the width that was successfully merged
          uPos += mergedWidth;
        }

      // Restore position (compensate for the manual increment above)
      currentPos[faceAxis]--;
    }
  }
}