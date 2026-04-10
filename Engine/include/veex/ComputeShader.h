#pragma once
// veex/ComputeShader.h
// OpenCL 1.1 compute shader abstraction for tile-based rendering and lightmap processing.
// Provides GPU-accelerated culling, lightmap calculations, and parallel processing.

#include "veex/Common.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace veex {

// ── Compute Shader Configuration ──────────────────────────────────────────────
struct ComputeConfig {
    int workGroupSizeX = 16;        // Work group size for X dimension
    int workGroupSizeY = 16;        // Work group size for Y dimension
    int workGroupSizeZ = 1;         // Work group size for Z dimension
    bool enableProfiling = false;   // Enable OpenCL profiling
    bool useInterop = true;         // Use OpenGL interop for shared resources
};

// ── Compute Kernel Types ──────────────────────────────────────────────────────
enum class ComputeKernelType {
    TILE_CULLING,           // Tile-based face culling
    LIGHTMAP_PROCESSING,    // Lightmap generation and filtering
    FACE_EXTRACTION,        // BSP face extraction and processing
    SHADOW_MAP_GENERATION,  // Shadow map calculations (if needed)
    PARTICLE_SIMULATION,    // Particle system updates
    PHYSICS_COMPUTE         // Physics calculations
};

// ── Compute Buffer ────────────────────────────────────────────────────────────
class ComputeBuffer {
public:
    ComputeBuffer();
    ~ComputeBuffer();
    
    // Initialize buffer with data
    bool Initialize(const void* data, size_t size, bool readOnly = false);
    
    // Update buffer data
    bool Update(const void* data, size_t size, size_t offset = 0);
    
    // Get buffer size
    size_t GetSize() const { return m_size; }
    
    // Get OpenCL buffer ID (internal use)
    void* GetCLBuffer() const { return m_clBuffer; }
    
    // Get OpenGL buffer ID (for interop)
    uint32_t GetGLBuffer() const { return m_glBuffer; }

private:
    size_t m_size = 0;
    void* m_clBuffer = nullptr;     // OpenCL buffer
    uint32_t m_glBuffer = 0;        // OpenGL buffer for interop
    bool m_readOnly = false;
};

// ── Compute Kernel ────────────────────────────────────────────────────────────
class ComputeKernel {
public:
    ComputeKernel();
    ~ComputeKernel();
    
    // Load kernel from source
    bool LoadFromSource(const std::string& source, const std::string& kernelName);
    
    // Load kernel from file
    bool LoadFromFile(const std::string& filename, const std::string& kernelName);
    
    // Set kernel arguments
    bool SetArgument(int index, const ComputeBuffer& buffer);
    bool SetArgument(int index, const glm::vec3& value);
    bool SetArgument(int index, const glm::mat4& value);
    bool SetArgument(int index, float value);
    bool SetArgument(int index, int value);
    bool SetArgument(int index, size_t value);
    
    // Execute kernel
    bool Execute(size_t globalSizeX, size_t globalSizeY = 1, size_t globalSizeZ = 1,
                 size_t localSizeX = 0, size_t localSizeY = 0, size_t localSizeZ = 0);
    
    // Get execution time (if profiling enabled)
    float GetExecutionTime() const { return m_executionTime; }
    
    // Get kernel name
    const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    void* m_clKernel = nullptr;     // OpenCL kernel
    std::vector<void*> m_arguments; // OpenCL kernel arguments
    float m_executionTime = 0.0f;
};

// ── Compute Context ───────────────────────────────────────────────────────────
class ComputeContext {
public:
    ComputeContext();
    ~ComputeContext();
    
    // Initialize OpenCL context
    bool Initialize(const ComputeConfig& config);
    
    // Create compute buffer
    std::unique_ptr<ComputeBuffer> CreateBuffer(const void* data, size_t size, bool readOnly = false);
    
    // Create compute kernel
    std::unique_ptr<ComputeKernel> CreateKernel(const std::string& source, const std::string& kernelName);
    
    // Create kernel from file
    std::unique_ptr<ComputeKernel> CreateKernelFromFile(const std::string& filename, const std::string& kernelName);
    
    // Execute multiple kernels in sequence
    bool ExecuteKernels(const std::vector<ComputeKernel*>& kernels);
    
    // Synchronize all compute operations
    void Synchronize();
    
    // Get device information
    struct DeviceInfo {
        std::string name;
        std::string vendor;
        std::string version;
        size_t maxWorkGroupSize = 0;
        size_t maxComputeUnits = 0;
        size_t globalMemorySize = 0;
        size_t localMemorySize = 0;
        bool supportsInterop = false;
    };
    
    const DeviceInfo& GetDeviceInfo() const { return m_deviceInfo; }
    
    // Check if OpenCL is available
    bool IsAvailable() const { return m_initialized; }
    
    // Get configuration
    const ComputeConfig& GetConfig() const { return m_config; }

private:
    bool m_initialized = false;
    ComputeConfig m_config;
    DeviceInfo m_deviceInfo;
    
    void* m_clContext = nullptr;    // OpenCL context
    void* m_clQueue = nullptr;      // OpenCL command queue
    void* m_clDevice = nullptr;     // OpenCL device
    
    // Platform and device selection
    bool SelectBestDevice();
    bool CreateContextAndQueue();
    bool CheckInteropSupport();
};

// ── Compute Shader Manager ────────────────────────────────────────────────────
class ComputeShaderManager {
public:
    ComputeShaderManager();
    ~ComputeShaderManager();
    
    // Initialize compute shader system
    bool Initialize(const ComputeConfig& config);
    
    // Create tile culling kernel
    std::unique_ptr<ComputeKernel> CreateTileCullingKernel();
    
    // Create lightmap processing kernel
    std::unique_ptr<ComputeKernel> CreateLightmapProcessingKernel();
    
    // Create face extraction kernel
    std::unique_ptr<ComputeKernel> CreateFaceExtractionKernel();
    
    // Execute tile-based culling
    bool ExecuteTileCulling(const std::vector<glm::vec3>& faceCenters,
                           const std::vector<glm::vec3>& faceNormals,
                           const std::vector<glm::vec2>& faceBounds,
                           std::vector<bool>& visibilityOut);
    
    // Execute lightmap processing
    bool ExecuteLightmapProcessing(const std::vector<glm::vec3>& lightmapData,
                                  std::vector<glm::vec3>& processedData);
    
    // Execute face extraction
    bool ExecuteFaceExtraction(const std::vector<uint8_t>& bspData,
                              std::vector<glm::vec3>& faceVerticesOut,
                              std::vector<glm::vec3>& faceNormalsOut);
    
    // Get compute context
    ComputeContext* GetContext() { return m_context.get(); }
    const ComputeContext* GetContext() const { return m_context.get(); }
    
    // Get statistics
    struct ComputeStats {
        int totalKernels = 0;
        int totalExecutions = 0;
        float totalExecutionTime = 0.0f;
        float averageExecutionTime = 0.0f;
        bool isUsingInterop = false;
    };
    
    ComputeStats GetStats() const { return m_stats; }
    
    // Check if compute shaders are available
    bool IsAvailable() const { return m_context && m_context->IsAvailable(); }

private:
    std::unique_ptr<ComputeContext> m_context;
    ComputeConfig m_config;
    ComputeStats m_stats;
    
    // Built-in kernel sources
    std::string GetTileCullingSource() const;
    std::string GetLightmapProcessingSource() const;
    std::string GetFaceExtractionSource() const;
    
    // Update statistics
    void UpdateStats(float executionTime);
};

// ── Compute Shader Utilities ──────────────────────────────────────────────────
namespace ComputeUtils {
    
    // Check if OpenCL is available on the system
    bool IsOpenCLAvailable();
    
    // Get available OpenCL platforms
    std::vector<std::string> GetAvailablePlatforms();
    
    // Get available OpenCL devices for a platform
    std::vector<std::string> GetAvailableDevices(const std::string& platform);
    
    // Check if OpenGL interop is supported
    bool IsInteropSupported();
    
    // Get recommended work group size for a problem size
    glm::ivec3 GetRecommendedWorkGroupSize(int problemSizeX, int problemSizeY = 1, int problemSizeZ = 1);
    
    // Calculate global work size from local work size
    glm::ivec3 CalculateGlobalWorkSize(int problemSizeX, int problemSizeY, int problemSizeZ,
                                      int localSizeX, int localSizeY, int localSizeZ);
    
    // Convert OpenCL error code to string
    std::string GetErrorString(int errorCode);
    
    // Check if device supports double precision
    bool SupportsDoublePrecision();
    
    // Get maximum work group size for current device
    size_t GetMaxWorkGroupSize();
    
    // Get maximum compute units for current device
    size_t GetMaxComputeUnits();
}

} // namespace veex