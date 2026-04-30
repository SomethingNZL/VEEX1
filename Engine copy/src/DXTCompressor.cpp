#include "veex/DXTCompressor.h"
#include "veex/Logger.h"
#include <cstring>
#include <algorithm>

namespace veex {

// Helper to find min/max colors in a 4x4 block
static void FindMinMaxColors(const uint8_t* block, int stride, uint8_t& minR, uint8_t& minG, uint8_t& minB,
                            uint8_t& maxR, uint8_t& maxG, uint8_t& maxB) {
    minR = minG = minB = 255;
    maxR = maxG = maxB = 0;
    
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            const uint8_t* pixel = block + y * stride + x * 4;
            minR = std::min(minR, pixel[0]);
            minG = std::min(minG, pixel[1]);
            minB = std::min(minB, pixel[2]);
            maxR = std::max(maxR, pixel[0]);
            maxG = std::max(maxG, pixel[1]);
            maxB = std::max(maxB, pixel[2]);
        }
    }
}

// Pack color to 565 format
static uint16_t PackColor565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// Get color index based on distance
static int GetColorIndex(const uint8_t* pixel, uint8_t r0, uint8_t g0, uint8_t b0,
                        uint8_t r1, uint8_t g1, uint8_t b1) {
    int dr0 = pixel[0] - r0, dg0 = pixel[1] - g0, db0 = pixel[2] - b0;
    int dr1 = pixel[0] - r1, dg1 = pixel[1] - g1, db1 = pixel[2] - b1;
    
    int dist0 = dr0*dr0 + dg0*dg0 + db0*db0;
    int dist1 = dr1*dr1 + dg1*dg1 + db1*db1;
    
    int r2 = (r0 * 2 + r1) / 3, g2 = (g0 * 2 + g1) / 3, b2 = (b0 * 2 + b1) / 3;
    int r3 = (r0 + r1 * 2) / 3, g3 = (g0 + g1 * 2) / 3, b3 = (b0 + b1 * 2) / 3;
    
    int dr2 = pixel[0] - r2, dg2 = pixel[1] - g2, db2 = pixel[2] - b2;
    int dr3 = pixel[0] - r3, dg3 = pixel[1] - g3, db3 = pixel[2] - b3;
    
    int dist2 = dr2*dr2 + dg2*dg2 + db2*db2;
    int dist3 = dr3*dr3 + dg3*dg3 + db3*db3;
    
    if (dist0 <= dist1 && dist0 <= dist2 && dist0 <= dist3) return 0;
    if (dist1 <= dist2 && dist1 <= dist3) return 1;
    if (dist2 <= dist3) return 2;
    return 3;
}

// Static method implementation
void DXTCompressor::CompressRGBA8ToDXT1(const uint8_t* rgba, int width, int height, uint8_t* output) {
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    
    for (int by = 0; by < blockHeight; by++) {
        for (int bx = 0; bx < blockWidth; bx++) {
            uint8_t block[16 * 4];
            
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = std::min(bx * 4 + x, width - 1);
                    int py = std::min(by * 4 + y, height - 1);
                    memcpy(block + (y * 4 + x) * 4, rgba + (py * width + px) * 4, 4);
                }
            }
            
            uint8_t minR, minG, minB, maxR, maxG, maxB;
            FindMinMaxColors(block, 16, minR, minG, minB, maxR, maxG, maxB);
            
            uint16_t c0 = PackColor565(maxR, maxG, maxB);
            uint16_t c1 = PackColor565(minR, minG, minB);
            
            uint16_t* out16 = reinterpret_cast<uint16_t*>(output);
            out16[0] = c0;
            out16[1] = c1;
            
            uint32_t indices = 0;
            for (int i = 0; i < 16; i++) {
                int idx = GetColorIndex(block + i * 4, maxR, maxG, maxB, minR, minG, minB);
                indices |= (idx << (i * 2));
            }
            
            uint32_t* out32 = reinterpret_cast<uint32_t*>(output + 4);
            out32[0] = indices;
            
            output += 8;
        }
    }
}

} // namespace veex
