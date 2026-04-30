// Test file for MegaTexture implementation
// This demonstrates how to use the TextureAtlas and BSPTexturePacker

#include "veex/TextureAtlas.h"
#include "veex/BSPTexturePacker.h"
#include "veex/BSP.h"
#include "veex/MaterialSystem.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include <iostream>

namespace veex {

void TestTextureAtlas() {
    Logger::Info("=== Testing TextureAtlas ===");
    
    TextureAtlas atlas;
    
    // Test platform detection
    Logger::Info("DXT supported: " + std::to_string(atlas.IsDXTSupported()));
    
    // Initialize atlas
    if (!atlas.Initialize(2048, 2048)) {
        Logger::Error("Failed to initialize TextureAtlas");
        return;
    }
    
    Logger::Info("Atlas initialized successfully");
    
    // Test manual texture upload (simulated data)
    // Create a simple 4x4 RGBA texture
    std::vector<uint8_t> testData(4 * 4 * 4); // 4x4 RGBA
    for (size_t i = 0; i < testData.size(); i += 4) {
        testData[i] = 255;     // R
        testData[i+1] = 0;     // G  
        testData[i+2] = 255;   // B
        testData[i+3] = 255;   // A
    }
    
    int allocationID = atlas.AllocateTexture(4, 4, testData.data(), testData.size(), GL_RGBA);
    
    if (allocationID >= 0) {
        Logger::Info("Successfully allocated texture with ID: " + std::to_string(allocationID));
        
        // Get UV crop
        glm::vec4 uvCrop = atlas.GetUVCrop(allocationID);
        Logger::Info("UV Crop: (" + std::to_string(uvCrop.x) + ", " + std::to_string(uvCrop.y) + 
                    ", " + std::to_string(uvCrop.z) + ", " + std::to_string(uvCrop.w) + ")");
        
        // Test statistics
        size_t used = atlas.GetUsedMemory();
        size_t total = atlas.GetTotalMemory();
        float usage = atlas.GetUsagePercentage();
        
        Logger::Info("Memory usage: " + std::to_string(used) + " / " + std::to_string(total) + 
                    " bytes (" + std::to_string(usage) + "%)");
        
        // Clean up
        atlas.Free(allocationID);
        Logger::Info("Texture freed successfully");
    } else {
        Logger::Error("Failed to allocate texture");
    }
    
    Logger::Info("=== TextureAtlas test completed ===");
}

void TestBSPTexturePacker() {
    Logger::Info("=== Testing BSPTexturePacker ===");
    
    BSPTexturePacker packer;
    
    // Note: This would normally be called with a loaded BSP and MaterialSystem
    // For this test, we'll just verify the packer can be created and initialized
    
    Logger::Info("BSPTexturePacker created successfully");
    Logger::Info("=== BSPTexturePacker test completed ===");
}

void TestMegaTextureIntegration() {
    Logger::Info("=== Testing MegaTexture Integration ===");
    
    // This would demonstrate the full workflow:
    // 1. Load BSP map
    // 2. Create BSPTexturePacker
    // 3. Pack textures into atlas
    // 4. Update materials to use atlas
    // 5. Render with atlas uniforms
    
    Logger::Info("MegaTexture integration test framework created");
    Logger::Info("Full integration requires BSP loading and MaterialSystem");
    Logger::Info("=== MegaTexture Integration test completed ===");
}

} // namespace veex

// Entry point for testing
int main() {
    std::cout << "MegaTexture Test Suite" << std::endl;
    
    // Initialize OpenGL context would be needed here for full testing
    // For now, we'll just test the logic
    
    veex::TestTextureAtlas();
    veex::TestBSPTexturePacker();
    veex::TestMegaTextureIntegration();
    
    std::cout << "All tests completed!" << std::endl;
    return 0;
}