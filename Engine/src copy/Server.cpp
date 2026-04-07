#include "veex/Server.h"
#include "veex/Logger.h"

namespace veex {

bool Server::Init()
{
    Logger::Info("Server initialized (AI/physics/nav placeholder)");
    m_simTime = 0.0;
    return true;
}

void Server::Tick(double dt)
{
    m_simTime += dt;
    // TODO: physics, AI, world simulation.
    // Future: world->Update(dt), ai->Update(dt), navmesh->Update(...)
}

void Server::Shutdown()
{
    Logger::Info("Server shutdown");
}

} // namespace veex
