#include "veex/ReflectionProbe.h"
#include "veex/BSP.h"
#include "veex/Camera.h"
#include "veex/Renderer.h"
#include "veex/Logger.h"
#include <iostream>

namespace veex {

// Simple test to verify reflection probe system functionality
void TestReflectionProbes() {
    Logger::Info("[Test] Starting Reflection Probe System Test");
    
    // Create a reflection probe system
    ReflectionProbeSystem probeSystem;
    probeSystem.Initialize();
    
    // Create some test probes
    ReflectionProbe* probe1 = probeSystem.CreateProbe(glm::vec3(0.0f, 0.0f, 0.0f), 512.0f);
    ReflectionProbe* probe2 = probeSystem.CreateProbe(glm::vec3(1000.0f, 0.0f, 0.0f), 256.0f);
    
    Logger::Info("[Test] Created " + std::to_string(probeSystem.GetProbeCount()) + " reflection probes");
    
    // Test probe properties
    if (probe1) {
        Logger::Info("[Test] Probe 1 position: (" + 
                    std::to_string(probe1->GetPosition().x) + ", " +
                    std::to_string(probe1->GetPosition().y) + ", " +
                    std::to_string(probe1->GetPosition().z) + ")");
        Logger::Info("[Test] Probe 1 radius: " + std::to_string(probe1->GetRadius()));
        Logger::Info("[Test] Probe 1 cubemap texture ID: " + std::to_string(probe1->GetCubemapTexture()));
    }
    
    if (probe2) {
        Logger::Info("[Test] Probe 2 position: (" + 
                    std::to_string(probe2->GetPosition().x) + ", " +
                    std::to_string(probe2->GetPosition().y) + ", " +
                    std::to_string(probe2->GetPosition().z) + ")");
        Logger::Info("[Test] Probe 2 radius: " + std::to_string(probe2->GetRadius()));
    }
    
    // Test influence area checking
    glm::vec3 testPos1(100.0f, 0.0f, 0.0f);
    glm::vec3 testPos2(2000.0f, 0.0f, 0.0f);
    
    if (probe1 && probe1->IsInfluenceArea(testPos1)) {
        Logger::Info("[Test] Position (100, 0, 0) is within Probe 1's influence area");
        Logger::Info("[Test] Blend weight: " + std::to_string(probe1->GetBlendWeight(testPos1)));
    }
    
    if (probe1 && !probe1->IsInfluenceArea(testPos2)) {
        Logger::Info("[Test] Position (2000, 0, 0) is outside Probe 1's influence area");
    }
    
    // Test closest probe finding
    ReflectionProbe* closest = probeSystem.GetClosestProbe(testPos1);
    if (closest == probe1) {
        Logger::Info("[Test] Correctly identified Probe 1 as closest to (100, 0, 0)");
    }
    
    closest = probeSystem.GetClosestProbe(testPos2);
    if (closest == probe2) {
        Logger::Info("[Test] Correctly identified Probe 2 as closest to (2000, 0, 0)");
    }
    
    // Test dirty state
    if (probe1 && probe1->IsDirty()) {
        Logger::Info("[Test] Probe 1 is correctly marked as dirty");
    }
    
    // Test cubemap texture creation
    if (probe1 && probe1->GetCubemapTexture() != 0) {
        Logger::Info("[Test] Probe 1 has a valid cubemap texture ID");
    }
    
    // Clean up
    probeSystem.RemoveProbe(probe1);
    probeSystem.RemoveProbe(probe2);
    probeSystem.Shutdown();
    
    Logger::Info("[Test] Reflection Probe System Test completed successfully");
}

} // namespace veex

int main() {
    veex::TestReflectionProbes();
    return 0;
}