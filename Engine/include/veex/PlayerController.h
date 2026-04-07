#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace veex {

class BSP;

struct CollisionTri {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
};

struct TraceResult {
    bool      hit        = false;
    float     fraction   = 1.0f;
    glm::vec3 hitNormal  = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 endPos     = glm::vec3(0.0f);
    bool      startSolid = false;
};

class PlayerController {
public:
    // --- CONVERSION & HULL ---
    static constexpr float kScale       = 0.03125f; // 1/32
    static constexpr float kHullHW      = 16.0f * kScale;
    static constexpr float kHullHH      = 36.0f * kScale;
    static constexpr float kEyeHeight   = 64.0f * kScale;
    static constexpr float kStepHeight  = 18.0f * kScale;
    static constexpr float kSkinWidth   = 0.01f; 
    static constexpr float kSlopeLimit  = 0.7f;

    // --- PHYSICS (Pre-scaled for Metric World) ---
    static constexpr float kJumpImpulse = 260.0f * kScale; 
    static constexpr float kGravity     = 800.0f * kScale; 
    static constexpr float kWalkSpeed   = 190.0f * kScale;
    static constexpr float kSprintSpeed = 320.0f * kScale;
    static constexpr float kAccelGround = 10.0f;
    static constexpr float kAccelAir    = 5.0f;
    static constexpr float kFriction    = 4.0f;
    static constexpr float kStopSpeed   = 100.0f * kScale;

    // --- Additions for Source-like physics ---
    static constexpr float kAirControlFactor = 0.3f; // Air control strength
    static constexpr float kBhopSpeedBoost = 1.05f;  // Bunny hop speed multiplier
    static constexpr float kJumpBufferTime = 0.2f;   // Jump input buffering time

    PlayerController();

    void      BuildCollisionMesh(const BSP& bsp);
    void      Teleport(const glm::vec3& feetPos);
    glm::vec3 DropToFloor(const glm::vec3& startPos);

    glm::vec3 Move(const glm::vec3& feetPos,
                   const glm::vec3& wishDir,
                   float wishSpeed,
                   bool jumpPressed,
                   float dt);

    bool      IsGrounded()  const { return m_grounded; }
    glm::vec3 GetVelocity() const { return m_velocity; }
    void      SetNoclip(bool enabled) { m_noclip = enabled; }
    bool      IsNoclip() const { return m_noclip; }

private:
    TraceResult HullTrace(const glm::vec3& feetPos, const glm::vec3& delta) const;
    int  TryPlayerMove(glm::vec3& pos, glm::vec3& vel, float dt) const;
    void StepMove(glm::vec3& pos, float dt);
    void CategorizePosition(glm::vec3& pos);

    bool AABBOverlapsTri(const glm::vec3& center, const glm::vec3& he, const CollisionTri& tri) const;
    bool TestAxis(const glm::vec3& axis, const glm::vec3& he,
                  const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) const;

    std::vector<CollisionTri> m_tris;
    glm::vec3  m_velocity           = glm::vec3(0.0f);
    bool       m_grounded           = false;
    bool       m_noclip             = false;
    bool       m_jumpHeld           = false;
    bool       m_loggedFirstGround  = false; 
    float      m_jumpBufferTimer    = 0.0f; // Timer for jump buffering
};

} // namespace veex