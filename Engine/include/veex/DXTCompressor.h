#pragma once

#include <cstdint>

namespace veex {

/**
 * DXTCompressor - Software DXT compression for pre-baked megatextures
 * 
 * Uses simple compression algorithm to convert RGBA8 textures to DXT1 format.
 * The compressed output can be saved to disk and loaded directly,
 * bypassing macOS limitations on glCompressedTexSubImage2D.
 */
class DXTCompressor {
public:
    /**
     * Compress RGBA8 image to DXT1 format
     * @param rgba Input RGBA8 image data
     * @param width Image width (will be padded to multiple of 4)
     * @param height Image height (will be padded to multiple of 4)
     * @param output Output buffer (must be at least (width+3)/4 * (height+3)/4 * 8 bytes)
     */
    static void CompressRGBA8ToDXT1(const uint8_t* rgba, int width, int height, uint8_t* output);
    
    /**
     * Get the size of compressed DXT1 data for given dimensions
     */
    static size_t GetDXT1Size(int width, int height) {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        return blocksX * blocksY * 8; // 8 bytes per block
    }
};

} // namespace veex
