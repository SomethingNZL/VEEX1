#pragma once

#include <cstdint>
#include <vector>

namespace veex {

/**
 * DepthMapGenerator — Generates height/depth maps from normal maps for parallax mapping.
 *
 * Approaches (from fast → accurate):
 *   1. Fast:      height ≈ 1 - Nz  (uses only normal Z component)
 *   2. Integrated: cumulative integration of normal gradients + diffuse blend
 *
 * The result is stored as RGBA8 where R=G=B=height [0,255], A=255.
 * This makes it compatible with standard RGBA texture atlases.
 */
class DepthMapGenerator {
public:
    /**
     * Generate a height map from a normal map using the fast Nz method.
     * @param normalData  RGBA8 tangent-space normal map ([0,1] encoded).
     * @param width       Width in pixels.
     * @param height      Height in pixels.
     * @param outHeightData Output RGBA8 buffer (will be resized to width*height*4).
     * @return true if generation succeeded.
     */
    static bool GenerateFromNormalFast(const uint8_t* normalData,
                                       int width, int height,
                                       std::vector<uint8_t>& outHeightData);

    /**
     * Generate a height map by integrating normal gradients.
     * This produces more accurate large-scale shape than the fast method.
     * @param normalData  RGBA8 tangent-space normal map.
     * @param diffuseData Optional RGBA8 diffuse for low-frequency shape bias (can be nullptr).
     * @param width       Width in pixels.
     * @param height      Height in pixels.
     * @param outHeightData Output RGBA8 buffer.
     * @param diffuseWeight How much to blend diffuse luminance (0.0 = normals only, 1.0 = full blend).
     * @return true if generation succeeded.
     */
    static bool GenerateFromNormalIntegrated(const uint8_t* normalData,
                                              const uint8_t* diffuseData,
                                              int width, int height,
                                              std::vector<uint8_t>& outHeightData,
                                              float diffuseWeight = 0.3f);

    /**
     * Convenience wrapper: chooses the best method based on available data.
     * If diffuseData is provided, uses integrated + diffuse blend.
     * Otherwise falls back to the fast Nz method.
     */
    static bool Generate(const uint8_t* normalData,
                         const uint8_t* diffuseData,
                         int width, int height,
                         std::vector<uint8_t>& outHeightData);

private:
    // Convert normal map RGB [0,255] → tangent-space vector [-1,1]
    static void DecodeNormals(const uint8_t* normalData,
                              int width, int height,
                              std::vector<float>& outNx,
                              std::vector<float>& outNy,
                              std::vector<float>& outNz);

    // Compute surface gradients from decoded normals
    static void ComputeGradients(const std::vector<float>& nx,
                                 const std::vector<float>& ny,
                                 const std::vector<float>& nz,
                                 int width, int height,
                                 std::vector<float>& outGx,
                                 std::vector<float>& outGy);

    // Integrate gradients via cumulative path integration (rows then columns)
    static void IntegrateGradients(const std::vector<float>& gx,
                                   const std::vector<float>& gy,
                                   int width, int height,
                                   std::vector<float>& outHeight);

    // Normalize height values to [0,1] range
    static void NormalizeHeight(std::vector<float>& height);

    // Compute diffuse luminance [0,1] from RGBA8 data
    static void ComputeLuminance(const uint8_t* diffuseData,
                                 int width, int height,
                                 std::vector<float>& outLum);

    // Pack float height [0,1] into RGBA8 buffer
    static void PackToRGBA8(const std::vector<float>& heightMap,
                            int width, int imgHeight,
                            std::vector<uint8_t>& outData);
};

} // namespace veex
