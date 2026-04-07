#include "veex/PlayerController.h"
#include "veex/BSP.h"
#include "veex/Logger.h"
#include <algorithm>
#include <cmath>

namespace veex {

static constexpr int kMaxClipPlanes = 5;
static constexpr int kNumBumps      = 4;

// Conversion factor for Source Units to Metric (1/32)
static constexpr float kScale       = 0.03125f; 

static inline glm::vec3 SourceToGL(const glm::vec3& v)
{
    return glm::vec3(v.x, v.z, -v.y);
}

PlayerController::PlayerController() 
    : m_velocity(0.0f), m_grounded(false), m_jumpHeld(false), m_loggedFirstGround(false) {}

void PlayerController::BuildCollisionMesh(const BSP& bsp)
{
    m_tris.clear();

    const auto& parser  = bsp.GetParser();
    const auto& faces   = parser.GetFaces();
    const auto& texinfo = parser.GetTexinfo();

    if (faces.empty()) {
        Logger::Error("PlayerController: Cannot build mesh, BSP faces are empty!");
        return;
    }

    constexpr int kNoCollideFlags = 0x0004 | 0x0040 | 0x0080 | 0x0100 | 0x0200;
    int skipped = 0;

    for (const auto& face : faces)
    {
        if (face.texinfo >= 0 && face.texinfo < (int)texinfo.size()) {
            if (texinfo[face.texinfo].flags & kNoCollideFlags) {
                skipped++;
                continue;
            }
        }

        std::vector<glm::vec3> faceVerts;
        parser.GetFaceVertices(face, faceVerts);
        if (faceVerts.size() < 3) continue;

        glm::vec3 normal = parser.GetFaceNormal(face);
        glm::vec3 n = SourceToGL(normal);

        for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
            CollisionTri tri;
            tri.v0     = SourceToGL(faceVerts[0])   * kScale;
            tri.v1     = SourceToGL(faceVerts[i])    * kScale;
            tri.v2     = SourceToGL(faceVerts[i+1])  * kScale;
            tri.normal = n;
            m_tris.push_back(tri);

            // Double-sided collision for thin brushes
            CollisionTri backTri;
            backTri.v0     = tri.v0;
            backTri.v1     = tri.v2;
            backTri.v2     = tri.v1;
            backTri.normal = -n;
            m_tris.push_back(backTri);
        }
    }

    Logger::Info("PlayerController: Collision mesh ready with "
                 + std::to_string(m_tris.size()) + " triangles.");
}

void PlayerController::Teleport(const glm::vec3& feetPos)
{
    m_velocity = glm::vec3(0.0f);
    m_grounded = false;
    m_jumpHeld = false;
    m_loggedFirstGround = false;
}

glm::vec3 PlayerController::DropToFloor(const glm::vec3& startPos)
{
    const float kDropDist = 256.0f * kScale;
    TraceResult tr = HullTrace(startPos, glm::vec3(0.0f, -kDropDist, 0.0f));
    if (tr.hit) {
        m_grounded = true;
        return tr.endPos;
    }
    return startPos;
}

TraceResult PlayerController::HullTrace(const glm::vec3& feetPos, const glm::vec3& delta) const
{
    const glm::vec3 he(kHullHW * kScale, kHullHH * kScale, kHullHW * kScale);
    const glm::vec3 center = feetPos + glm::vec3(0.0f, kHullHH * kScale, 0.0f);

    TraceResult best;
    best.fraction = 1.0f;
    best.endPos   = feetPos + delta;
    best.hit      = false;

    float deltaLen = glm::length(delta);
    if (deltaLen < 1e-8f) return best;

    for (const auto& tri : m_tris) {
        float extent = std::abs(tri.normal.x) * he.x
                     + std::abs(tri.normal.y) * he.y
                     + std::abs(tri.normal.z) * he.z;
        
        float planeDist = glm::dot(tri.normal, tri.v0);
        float startD    = glm::dot(tri.normal, center) - planeDist;

        if (startD < -extent - kSkinWidth) continue;

        float denom = glm::dot(tri.normal, delta);
        if (denom >= -1e-7f) continue;

        float t = (extent - startD) / denom;
        if (t < 0.0f || t >= best.fraction) continue;

        glm::vec3 contactCenter = center + delta * t;
        if (!AABBOverlapsTri(contactCenter, he, tri)) continue;

        best.hit       = true;
        best.fraction  = std::max(0.0f, t);
        best.hitNormal = tri.normal;
        best.endPos    = feetPos + delta * t;
    }
    return best;
}

int PlayerController::TryPlayerMove(glm::vec3& pos, glm::vec3& vel, float dt) const
{
    glm::vec3 planes[kMaxClipPlanes];
    int numPlanes = 0;
    glm::vec3 remain = vel * dt;

    for (int bump = 0; bump < kNumBumps; ++bump) {
        if (glm::length(remain) < 1e-6f) break;
        TraceResult tr = HullTrace(pos, remain);

        if (tr.fraction > 0.0f) {
            float moveFrac = std::max(0.0f, tr.fraction - kSkinWidth / std::max(glm::length(remain), 1e-6f));
            pos    += remain * moveFrac;
            remain -= remain * moveFrac;
        }

        if (!tr.hit) break;
        
        if (numPlanes < kMaxClipPlanes)
            planes[numPlanes++] = tr.hitNormal;

        for (int i = 0; i < numPlanes; ++i) {
            float d = glm::dot(vel, planes[i]);
            if (d < 0.0f)
                vel -= planes[i] * d;
        }

        remain = vel * dt * (1.0f - tr.fraction);
    }
    return numPlanes > 0 ? 1 : 0;
}

void PlayerController::StepMove(glm::vec3& pos, float dt)
{
    glm::vec3 downPos = pos;
    glm::vec3 downVel = m_velocity;
    TryPlayerMove(downPos, downVel, dt);

    float scaledStep = kStepHeight * kScale;
    glm::vec3 upPos = pos + glm::vec3(0.0f, scaledStep, 0.0f);
    TraceResult upCheck = HullTrace(pos, glm::vec3(0.0f, scaledStep, 0.0f));

    if (!upCheck.hit) {
        glm::vec3 upVel = m_velocity;
        TryPlayerMove(upPos, upVel, dt);
        
        TraceResult downCheck = HullTrace(upPos, glm::vec3(0.0f, -scaledStep, 0.0f));
        if (downCheck.hit && downCheck.hitNormal.y >= kSlopeLimit) {
            upPos = downCheck.endPos;
            if (upPos.y > downPos.y) {
                pos        = upPos;
                m_velocity = upVel;
                return;
            }
        }
    }
    pos        = downPos;
    m_velocity = downVel;
}

void PlayerController::CategorizePosition(glm::vec3& pos)
{
    if (m_velocity.y > 180.0f * kScale) {
        m_grounded = false;
        return;
    }

    const float kProbe = (2.0f * kScale) + kSkinWidth;
    TraceResult tr = HullTrace(pos, glm::vec3(0.0f, -kProbe, 0.0f));
    
    if (tr.hit && tr.hitNormal.y >= kSlopeLimit) {
        m_grounded = true;
        pos = tr.endPos;
        if (m_velocity.y < 0.0f) m_velocity.y = 0.0f;

        if (!m_loggedFirstGround) {
            m_loggedFirstGround = true;
            Logger::Info("PlayerController: Feet settled at Y = " + std::to_string(pos.y));
        }
    } else {
        m_grounded = false;
    }
}

glm::vec3 PlayerController::Move(const glm::vec3& feetPos, const glm::vec3& wishDir,
                                  float wishSpeed, bool jumpPressed, float dt)
{
    glm::vec3 pos = feetPos;
    float scaledWishSpeed = wishSpeed * kScale;

    // Ground friction
    if (m_grounded) {
        float speed = glm::length(glm::vec2(m_velocity.x, m_velocity.z));
        if (speed > 1e-5f) {
            float control  = std::max(speed, kStopSpeed * kScale);
            float drop     = control * kFriction * dt;
            float newSpeed = std::max(0.0f, speed - drop);
            m_velocity.x *= (newSpeed / speed);
            m_velocity.z *= (newSpeed / speed);
        } else {
            m_velocity.x = m_velocity.z = 0.0f;
        }
    }

    // Acceleration
    float accel    = m_grounded ? kAccelGround : kAccelAir;
    float curSpeed = glm::dot(m_velocity, wishDir);
    float addSpeed = scaledWishSpeed - curSpeed;
    if (addSpeed > 0.0f)
        m_velocity += wishDir * std::min(accel * scaledWishSpeed * dt, addSpeed);

    // Jump
    if (m_grounded && jumpPressed && !m_jumpHeld) {
        m_velocity.y = kJumpImpulse * kScale;
        m_grounded   = false;
        m_jumpHeld   = true;
    }
    if (!jumpPressed) m_jumpHeld = false;

    // Gravity
    if (!m_grounded) m_velocity.y -= (kGravity * kScale) * dt * 0.5f;
    
    if (m_grounded) StepMove(pos, dt);
    else            TryPlayerMove(pos, m_velocity, dt);
    
    if (!m_grounded) m_velocity.y -= (kGravity * kScale) * dt * 0.5f;

    CategorizePosition(pos);
    return pos;
}

bool PlayerController::AABBOverlapsTri(const glm::vec3& center, const glm::vec3& he,
                                        const CollisionTri& tri) const
{
    glm::vec3 v0 = tri.v0 - center;
    glm::vec3 v1 = tri.v1 - center;
    glm::vec3 v2 = tri.v2 - center;
    glm::vec3 e0 = v1 - v0, e1 = v2 - v1, e2 = v0 - v2;
    glm::vec3 boxAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};

    for (auto& ba : boxAxes) {
        for (auto* ep : {&e0, &e1, &e2}) {
            glm::vec3 axis = glm::cross(*ep, ba);
            if (glm::length(axis) < 1e-7f) continue;
            if (!TestAxis(axis, he, v0, v1, v2)) return false;
        }
    }
    if (!TestAxis({1,0,0}, he, v0, v1, v2)) return false;
    if (!TestAxis({0,1,0}, he, v0, v1, v2)) return false;
    if (!TestAxis({0,0,1}, he, v0, v1, v2)) return false;
    if (!TestAxis(tri.normal, he, v0, v1, v2)) return false;
    return true;
}

bool PlayerController::TestAxis(const glm::vec3& axis, const glm::vec3& he,
                                  const glm::vec3& v0, const glm::vec3& v1,
                                  const glm::vec3& v2) const
{
    float p0 = glm::dot(axis, v0);
    float p1 = glm::dot(axis, v1);
    float p2 = glm::dot(axis, v2);
    float mn = std::min({p0, p1, p2});
    float mx = std::max({p0, p1, p2});
    float r  = he.x * std::abs(axis.x) + he.y * std::abs(axis.y) + he.z * std::abs(axis.z);
    return !(mn > r || mx < -r);
}

} // namespace veex