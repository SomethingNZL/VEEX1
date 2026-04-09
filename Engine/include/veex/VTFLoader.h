#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace veex {

/**
 * VTF (Valve Texture Format) Loader
 * 
 * Supports reading VTF files used by Source Engine games.
 * This implementation handles common VTF formats and converts
 * them to raw pixel data suitable for OpenGL texture upload.
 * 
 * Supported formats:
 * - DXT1, DXT3, DXT5 (compressed)
 * - RGBA8888, RGB888
 * - I8 (grayscale)
 * - IA88 (grayscale + alpha)
 * - RGB565, RGBA5551, RGBA4444
 * 
 * License: Public domain / Unlicense (no restrictive dependencies)
 */

// VTF Image Format Enumeration (partial list of common formats)
enum class VTFFormat : uint32_t {
    NONE                = 0x00,
    RGBA8888            = 0x01,
    RGB888              = 0x02,
    RGB565              = 0x03,
    I8                  = 0x04,
    IA88                = 0x05,
    RGB888_BLUESCREEN   = 0x06,
    BGR888_BLUESCREEN   = 0x07,
    ARGB8888            = 0x08,
    ARGB8888_BLUESCREEN = 0x09,
    RGB565_BLUESCREEN   = 0x0A,
    DXT1                = 0x0B,
    DXT3                = 0x0D,
    DXT5                = 0x0E,
    BGR888              = 0x10,
    BGRX8888            = 0x11,
    BGRA8888            = 0x12,
    DXT1_ONEBITALPHA    = 0x13,
    BGRA5551            = 0x14,
    RGBX8888            = 0x15,
    BGR565              = 0x16,
    BGR555              = 0x17,
    BGRA4444            = 0x18,
    DXT5_ONEBITALPHA    = 0x19,
    ATI1N               = 0x1C,  // Single channel compression
    ATI2N               = 0x1D,  // Two channel compression (3DC)
};

// VTF Texture Flags
enum class VTFFlags : uint32_t {
    POINTSAMPLE                = 0x00000001,
    TRILINEAR                  = 0x00000002,
    CLAMPS                     = 0x00000004,
    CLAMPT                     = 0x00000008,
    CLAMPU                     = 0x00000010,
    ANISOTROPIC                = 0x00000020,
    HINT_DXT5                  = 0x00000040,
    SRGB                       = 0x00000080,
    NORMAL                     = 0x00000100,
    NODATA                     = 0x00000200,
    TENBIT                     = 0x00000400,
    ONEBITALPHA                = 0x00000800,
    EIGHTBITALPHA              = 0x00001000,
    ENVMAP                     = 0x00002000,
    RENDERTARGET               = 0x00004000,
    DEPTHRENDERTARGET          = 0x00008000,
    NODEBUGOVERRIDE            = 0x00010000,
    SINGLECOPY                 = 0x00020000,
    PRE_SRGB                   = 0x00040000,
    NODEPTHBUFFER              = 0x00080000,
    CLAMPU_ALT                 = 0x00100000,
    BORDER                     = 0x00200000,
    ANISOTROPIC_ALT            = 0x00400000,
};

// VTF Header structure (version 7.0-7.1)
// Total size: 64 bytes
#pragma pack(push, 1)
struct VTFHeader_v70 {
    char     signature[4];      // "VTF\0"
    uint32_t versionMajor;      // Major version (7)
    uint32_t versionMinor;      // Minor version (0 or 1)
    uint32_t headerSize;        // Size of this header (64 for v7.0)
    uint16_t flags;             // Flags
    uint16_t width;             // Width
    uint16_t height;            // Height
    uint32_t format;            // Pixel format
    uint32_t numFrames;         // Number of frames
    uint32_t firstResource;     // First resource offset (unused in v7.0)
    float    bumpmapScale;      // Bumpmap scale
    uint32_t highResImageFormat;// High res image format  
    int8_t   depth;             // Volume texture depth
    uint8_t  numResources;      // Number of resources (0 for v7.0)
};
#pragma pack(pop)

// VTF Header structure (version 7.2+)
// Total size: 80 bytes
#pragma pack(push, 1)
struct VTFHeader_v72 {
    char     signature[4];      // "VTF\0"
    uint32_t versionMajor;      // Major version (7)
    uint32_t versionMinor;      // Minor version (2+)
    uint32_t headerSize;        // Size of this header (80 for v7.2)
    uint16_t flags;             // Flags
    uint16_t width;             // Width
    uint16_t height;            // Height
    uint32_t format;            // Pixel format
    uint32_t numFrames;         // Number of frames
    uint32_t firstResource;     // First resource offset
    float    bumpmapScale;      // Bumpmap scale
    uint32_t highResImageFormat;// High res image format
    int8_t   depth;             // Volume texture depth
    uint8_t  numResources;      // Number of resources
    float    reflectivity[3];   // Reflectivity values (3 floats)
    uint32_t flagsEx;           // Extended flags (v7.3+)
};
#pragma pack(pop)

// Resource entry structure (v7.2+)
#pragma pack(push, 1)
struct VTFResource {
    uint32_t type;              // Resource type
    uint32_t data;              // Offset or inline data
};
#pragma pack(pop)

/**
 * VTF Loader class
 * Handles parsing and decompression of VTF texture files
 */
class VTFLoader {
public:
    VTFLoader();
    ~VTFLoader();

    /**
     * Load a VTF file from disk
     * @param path Path to the .vtf file
     * @return true if loading succeeded
     */
    bool LoadFromFile(const std::string& path);

    /**
     * Load VTF data from memory buffer
     * @param data Pointer to VTF data
     * @param size Size of data in bytes
     * @return true if loading succeeded
     */
    bool LoadFromMemory(const uint8_t* data, size_t size);

    /**
     * Get the decoded pixel data as RGBA8888
     * The returned data is owned by this instance and valid until
     * the next Load* call or destruction.
     * @param outWidth Output width
     * @param outHeight Output height
     * @return Pointer to RGBA pixel data, or nullptr if not loaded
     */
    const uint8_t* GetRGBAData(int* outWidth, int* outHeight) const;

    /**
     * Get raw pixel data in the native format
     * @param outWidth Output width
     * @param outHeight Output height
     * @param outFormat Output format (VTFFormat)
     * @param outSize Output data size in bytes
     * @return Pointer to pixel data, or nullptr if not loaded
     */
    const uint8_t* GetRawData(int* outWidth, int* outHeight, 
                              VTFFormat* outFormat, size_t* outSize) const;

    /**
     * Get compressed texture data for direct OpenGL upload
     * Only valid for compressed formats (DXT1, DXT3, DXT5)
     * @param outWidth Output width
     * @param outHeight Output height
     * @param outGLInternalFormat Output OpenGL internal format (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, etc.)
     * @param outDataSize Output size of compressed data in bytes
     * @return Pointer to compressed data, or nullptr if not a compressed format
     */
    const uint8_t* GetCompressedData(int* outWidth, int* outHeight, 
                                      uint32_t* outGLInternalFormat, size_t* outDataSize) const;

    /**
     * Get the number of mipmap levels available in the texture
     * @return Number of mipmaps (including base level)
     */
    int GetMipCount() const { return m_mipCount; }

    /**
     * Get compressed data for a specific mipmap level
     * @param mipLevel Mipmap level (0 = base, 1 = half size, etc.)
     * @param outDataSize Output size of compressed data in bytes
     * @return Pointer to compressed data for this mip level, or nullptr if invalid
     */
    const uint8_t* GetMipData(int mipLevel, size_t* outDataSize) const;

    /**
     * Check if the file is a valid VTF file
     * @param path Path to check
     * @return true if valid VTF
     */
    static bool IsValidVTF(const std::string& path);

    /**
     * Get the VTF format as a string for debugging
     */
    static const char* FormatToString(VTFFormat format);

    // Getters
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    VTFFormat GetFormat() const { return static_cast<VTFFormat>(m_format); }
    uint32_t GetFlags() const { return m_flags; }
    bool HasAlpha() const;
    bool IsCompressed() const;
    bool IsSRGB() const { return (m_flags & static_cast<uint32_t>(VTFFlags::SRGB)) != 0; }

private:
    // Format conversion helpers
    void ConvertToRGBA(const uint8_t* src, uint8_t* dst, size_t pixelCount, VTFFormat format);
    bool IsCompressedFormat(VTFFormat format) const;
    
    // Calculate image size for a given format
    size_t CalculateImageSize(int width, int height, VTFFormat format) const;
    size_t CalculateBlockSize(int width, int height, VTFFormat format) const;

    // Member data
    int m_width = 0;
    int m_height = 0;
    uint32_t m_format = 0;
    uint32_t m_flags = 0;
    uint32_t m_version = 0;
    int m_mipCount = 0;                     // Number of mipmap levels
    
    std::vector<uint8_t> m_compressedData;  // Original compressed DXT data
    std::vector<uint8_t> m_rgbaData;        // Decoded RGBA8888 data (fallback only)
    std::vector<std::vector<uint8_t>> m_mipData; // All mipmap levels
    bool m_loaded = false;
    bool m_isCompressed = false;
};

} // namespace veex