#include "GreedyMesher.h"
#include "../Chunk.h"
#include <iostream>
#include <vector>

// A single cell in the "face mask" — represents one face
// that needs to be rendered at position (u, v) on a specific slice
struct FaceMaskCell {
  BlockType blockType; // block type owning this face
  bool isBackFace;     // false = face faces +axis, true = faces -axis
  int textureLayerId;  // layer index in GL_TEXTURE_2D_ARRAY
};

// Helper: push 1 vertex to the vertex buffer
// Vertex format: [x, y, z, u, v, textureLayer, light]
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

// Select the texture layer based on block type + face direction
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

// Emit a single quad (rectangle) as 2 triangles
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
  
  float baseFaceLight = 1.0f;
  if (faceAxis == 1) {
    baseFaceLight = isBackFace ? 0.55f : 1.0f;
  } else if (faceAxis == 0) {
    baseFaceLight = 0.80f;
  } else {
    baseFaceLight = 0.70f;
  }

  float light = baseFaceLight; // Flat, clean lighting per face direction (No AO)

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

// GreedyMesher::build — main entry point
void GreedyMesher::build(const Chunk &chunk, Chunk *neighborNX,
                         Chunk *neighborPX, Chunk *neighborNZ,
                         Chunk *neighborPZ, std::vector<float> &outVertices) {
  outVertices.clear();

  // Cross-chunk block query helper
  auto getBlockWithNeighbors = [&](int lx, int ly, int lz) -> BlockType {
    if (ly < 0 || ly >= Chunk::HEIGHT)
      return BlockType::Air;

    if (lx >= 0 && lx < Chunk::SIZE && lz >= 0 && lz < Chunk::SIZE)
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

    return BlockType::Air;
  };

  FaceMaskCell faceMask[Chunk::SIZE * Chunk::HEIGHT * Chunk::SIZE];
  int chunkDimensions[3] = {Chunk::SIZE, Chunk::HEIGHT, Chunk::SIZE};
  int currentPos[3];
  int stepAlongAxis[3];

  for (int faceAxis = 0; faceAxis < 3; faceAxis++) {
    int uAxis = (faceAxis + 1) % 3;
    int vAxis = (faceAxis + 2) % 3;

    stepAlongAxis[0] = stepAlongAxis[1] = stepAlongAxis[2] = 0;
    stepAlongAxis[faceAxis] = 1;

    for (currentPos[faceAxis] = -1;
         currentPos[faceAxis] < chunkDimensions[faceAxis];
         currentPos[faceAxis]++) {
      int maskIndex = 0;
      for (currentPos[vAxis] = 0; currentPos[vAxis] < chunkDimensions[vAxis];
           currentPos[vAxis]++)
        for (currentPos[uAxis] = 0; currentPos[uAxis] < chunkDimensions[uAxis];
             currentPos[uAxis]++) {
          BlockType blockA = getBlockWithNeighbors(currentPos[0], currentPos[1],
                                                   currentPos[2]);

          BlockType blockB =
              getBlockWithNeighbors(currentPos[0] + stepAlongAxis[0],
                                    currentPos[1] + stepAlongAxis[1],
                                    currentPos[2] + stepAlongAxis[2]);

          bool isBlockASolid = (blockA != BlockType::Air);
          bool isBlockBSolid = (blockB != BlockType::Air);

          if (isBlockASolid == isBlockBSolid) {
            faceMask[maskIndex++] = {BlockType::Air, false, -1};
          } else if (isBlockASolid) {
            faceMask[maskIndex++] = {
                blockA, false, getTextureLayerForFace(blockA, faceAxis, false)};
          } else {
            faceMask[maskIndex++] = {
                blockB, true, getTextureLayerForFace(blockB, faceAxis, true)};
          }
        }

      currentPos[faceAxis]++;

      auto getMaskIndex = [&](int uPos, int vPos) {
        return uPos + vPos * chunkDimensions[uAxis];
      };

      for (int vPos = 0; vPos < chunkDimensions[vAxis]; vPos++)
        for (int uPos = 0; uPos < chunkDimensions[uAxis];) {
          FaceMaskCell currentCell = faceMask[getMaskIndex(uPos, vPos)];
          if (currentCell.blockType == BlockType::Air) {
            uPos++;
            continue;
          }

          // Expand right
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

          // Expand down
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

          int duStep[3] = {0, 0, 0};
          int dvStep[3] = {0, 0, 0};
          duStep[uAxis] = mergedWidth;
          dvStep[vAxis] = mergedHeight;

          int quadOrigin[3] = {0, 0, 0};
          quadOrigin[faceAxis] = currentPos[faceAxis];
          quadOrigin[uAxis] = uPos;
          quadOrigin[vAxis] = vPos;

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

          emitQuad(outVertices, corner0x, corner0y, corner0z, corner1x,
                   corner1y, corner1z, corner2x, corner2y, corner2z, corner3x,
                   corner3y, corner3z, faceAxis, currentCell.isBackFace,
                   mergedWidth, mergedHeight, currentCell.textureLayerId,
                   worldU, worldV);

          for (int v = 0; v < mergedHeight; v++) {
            for (int u = 0; u < mergedWidth; u++) {
              faceMask[getMaskIndex(uPos + u, vPos + v)] = {BlockType::Air,
                                                            false, -1};
            }
          }

          uPos += mergedWidth;
        }

      currentPos[faceAxis]--;
    }
  }
}