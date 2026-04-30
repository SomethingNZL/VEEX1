#include "veex/DepthMapGenerator.h"
#include "veex/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace veex {

// ─── Public API ──────────────────────────────────────────────────────────────

bool DepthMapGenerator::GenerateFromNormalFast(const uint8_t* normalData,
                                                int width, int height,
                                                std::vector<uint8_t>& outHeightData) {
    if (!normalData || width <= 0 || height <= 0) {
        return false;
    }

    const int pixelCount = width * height;
    outHeightData.resize(pixelCount * 4);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;

            // Decode normal Z from [0,255] → [-1,1]
            // Standard tangent-space normal maps: Z is usually in blue channel
            float nz = (normalData[idx + 2] / 255.0f) * 2.0f - 1.0f;

            // Clamp Z to valid range to avoid division issues
            nz = std::max(-1.0f, std::min(1.0f, nz));

            // Height ≈ 1 - Nz  (flat surface = 0, tilted = higher)
            float h = 1.0f - nz;

            // Normalize to [0,1] (Nz in [-1,1] means h in [0,2])
            h = h * 0.5f;
            h = std::max(0.0f, std::min(1.0f, h));

            uint8_t pixel = static_cast<uint8_t>(h * 255.0f + 0.5f);
            int outIdx = (y * width + x) * 4;
            outHeightData[outIdx + 0] = pixel;
            outHeightData[outIdx + 1] = pixel;
            outHeightData[outIdx + 2] = pixel;
            outHeightData[outIdx + 3] = 255;
        }
    }

    return true;
}

bool DepthMapGenerator::GenerateFromNormalIntegrated(const uint8_t* normalData,
                                                       const uint8_t* diffuseData,
                                                       int width, int height,
                                                       std::vector<uint8_t>& outHeightData,
                                                       float diffuseWeight) {
    if (!normalData || width <= 0 || height <= 0) {
        return false;
    }

    const int pixelCount = width * height;

    // Step 1: Decode normals
    std::vector<float> nx, ny, nz;
    DecodeNormals(normalData, width, height, nx, ny, nz);

    // Step 2: Compute gradients from normals
    std::vector<float> gx, gy;
    ComputeGradients(nx, ny, nz, width, height, gx, gy);

    // Step 3: Integrate gradients to get height
    std::vector<float> heightMap;
    IntegrateGradients(gx, gy, width, height, heightMap);

    // Step 4: Normalize to [0,1]
    NormalizeHeight(heightMap);

    // Step 5: If diffuse is provided, blend luminance for low-frequency shape
    if (diffuseData && diffuseWeight > 0.0f) {
        std::vector<float> luminance;
        ComputeLuminance(diffuseData, width, height, luminance);

        // Blend: final = height * (1 - w) + luminance * w
        float invW = 1.0f - diffuseWeight;
        for (int i = 0; i < pixelCount; ++i) {
            heightMap[i] = heightMap[i] * invW + luminance[i] * diffuseWeight;
        }
        // Re-normalize after blending
        NormalizeHeight(heightMap);
    }

    // Step 6: Pack to RGBA8
    PackToRGBA8(heightMap, width, height, outHeightData);
    return true;
}

bool DepthMapGenerator::Generate(const uint8_t* normalData,
                                  const uint8_t* diffuseData,
                                  int width, int height,
                                  std::vector<uint8_t>& outHeightData) {
    if (diffuseData) {
        return GenerateFromNormalIntegrated(normalData, diffuseData, width, height, outHeightData);
    }
    return GenerateFromNormalFast(normalData, width, height, outHeightData);
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void DepthMapGenerator::DecodeNormals(const uint8_t* normalData,
                                       int width, int height,
                                       std::vector<float>& outNx,
                                       std::vector<float>& outNy,
                                       std::vector<float>& outNz) {
    const int count = width * height;
    outNx.resize(count);
    outNy.resize(count);
    outNz.resize(count);

    for (int i = 0; i < count; ++i) {
        int idx = i * 4;
        outNx[i] = (normalData[idx + 0] / 255.0f) * 2.0f - 1.0f;
        outNy[i] = (normalData[idx + 1] / 255.0f) * 2.0f - 1.0f;
        outNz[i] = (normalData[idx + 2] / 255.0f) * 2.0f - 1.0f;

        // Normalize to unit length
        float len = std::sqrt(outNx[i] * outNx[i] + outNy[i] * outNy[i] + outNz[i] * outNz[i]);
        if (len > 1e-6f) {
            outNx[i] /= len;
            outNy[i] /= len;
            outNz[i] /= len;
        } else {
            outNx[i] = 0.0f;
            outNy[i] = 0.0f;
            outNz[i] = 1.0f;
        }
    }
}

void DepthMapGenerator::ComputeGradients(const std::vector<float>& nx,
                                          const std::vector<float>& ny,
                                          const std::vector<float>& nz,
                                          int width, int height,
                                          std::vector<float>& outGx,
                                          std::vector<float>& outGy) {
    const int count = width * height;
    outGx.resize(count, 0.0f);
    outGy.resize(count, 0.0f);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;

            // Guard against Nz ≈ 0 (vertical normals)
            float z = nz[idx];
            if (std::abs(z) < 1e-4f) {
                z = (z >= 0.0f) ? 1e-4f : -1e-4f;
            }

            // ∂h/∂x = -Nx / Nz,  ∂h/∂y = -Ny / Nz
            outGx[idx] = -nx[idx] / z;
            outGy[idx] = -ny[idx] / z;
        }
    }
}

void DepthMapGenerator::IntegrateGradients(const std::vector<float>& gx,
                                            const std::vector<float>& gy,
                                            int width, int height,
                                            std::vector<float>& outHeight) {
    const int count = width * height;
    outHeight.resize(count, 0.0f);

    // Two-pass integration: integrate along rows, then along columns
    // This reduces drift compared to simple single-path integration.

    // Pass 1: row-wise cumulative integration
    std::vector<float> rowInt(count, 0.0f);
    for (int y = 0; y < height; ++y) {
        rowInt[y * width] = 0.0f; // anchor each row at x=0
        for (int x = 1; x < width; ++x) {
            int idx = y * width + x;
            rowInt[idx] = rowInt[idx - 1] + gx[idx];
        }
    }

    // Pass 2: column-wise cumulative integration on the row results
    std::vector<float> colInt(count, 0.0f);
    for (int x = 0; x < width; ++x) {
        colInt[x] = 0.0f; // anchor each column at y=0
        for (int y = 1; y < height; ++y) {
            int idx = y * width + x;
            int prev = (y - 1) * width + x;
            colInt[idx] = colInt[prev] + gy[idx];
        }
    }

    // Combine both passes with equal weight
    for (int i = 0; i < count; ++i) {
        outHeight[i] = (rowInt[i] + colInt[i]) * 0.5f;
    }
}

void DepthMapGenerator::NormalizeHeight(std::vector<float>& height) {
    if (height.empty()) return;

    float hMin = height[0];
    float hMax = height[0];
    for (float h : height) {
        if (h < hMin) hMin = h;
        if (h > hMax) hMax = h;
    }

    float range = hMax - hMin;
    if (range < 1e-6f) {
        // Flat — set to mid-grey
        std::fill(height.begin(), height.end(), 0.5f);
        return;
    }

    for (float& h : height) {
        h = (h - hMin) / range;
    }
}

void DepthMapGenerator::ComputeLuminance(const uint8_t* diffuseData,
                                          int width, int height,
                                          std::vector<float>& outLum) {
    const int count = width * height;
    outLum.resize(count);

    for (int i = 0; i < count; ++i) {
        int idx = i * 4;
        // Perceptual luminance
        float r = diffuseData[idx + 0] / 255.0f;
        float g = diffuseData[idx + 1] / 255.0f;
        float b = diffuseData[idx + 2] / 255.0f;
        outLum[i] = r * 0.299f + g * 0.587f + b * 0.114f;
    }
}

void DepthMapGenerator::PackToRGBA8(const std::vector<float>& heightMap,
                                     int width, int imgHeight,
                                     std::vector<uint8_t>& outData) {
    const int count = width * imgHeight;
    outData.resize(count * 4);

    for (int i = 0; i < count; ++i) {
        uint8_t v = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, heightMap[i])) * 255.0f + 0.5f);
        int idx = i * 4;
        outData[idx + 0] = v;
        outData[idx + 1] = v;
        outData[idx + 2] = v;
        outData[idx + 3] = 255;
    }
}

} // namespace veex
