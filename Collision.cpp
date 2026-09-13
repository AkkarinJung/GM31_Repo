#include "main.h"
#include "renderer.h"
#include "Collision.h"
#include "manager.h"
#include "Box.h"
#include <algorithm>

static float Clamp(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

bool Collision::SphereVsSphere(
    const Vector3& centerA,
    float radiusA,
    const Vector3& centerB,
    float radiusB)
{
    Vector3 dir = centerB - centerA;
    float radius = radiusA + radiusB;

    return VectorMag(dir) < radius;
}

bool Collision::SphereVsAABB(
    const Vector3& sphereCenter,
    float sphereRadius,
    const Vector3& boxCenter,
    const Vector3& boxHalfSize)
{
    Vector3 closest(
        Clamp(sphereCenter.x, boxCenter.x - boxHalfSize.x, boxCenter.x + boxHalfSize.x),
        Clamp(sphereCenter.y, boxCenter.y - boxHalfSize.y, boxCenter.y + boxHalfSize.y),
        Clamp(sphereCenter.z, boxCenter.z - boxHalfSize.z, boxCenter.z + boxHalfSize.z)
    );

    Vector3 dir = sphereCenter - closest;

    return VectorMag(dir) < sphereRadius;
}

bool Collision::AABBvsAABB(const AABB& A, const AABB& B)
{
    // Touching exactly is not overlapping: a body resting on a crate sits at
    // the crate's top, and counting that as an overlap would push it off.
    if (A.MaxX() <= B.MinX() || A.MinX() >= B.MaxX()) return false;
    if (A.MaxY() <= B.MinY() || A.MinY() >= B.MaxY()) return false;
    if (A.MaxZ() <= B.MinZ() || A.MinZ() >= B.MaxZ()) return false;

    return true;
}

AABB Collision::BodyAABB(const Vector3& Position, const Vector3& HalfSize)
{
    AABB body;
    body.Center = Vector3(Position.x, Position.y + HalfSize.y, Position.z);
    body.HalfSize = HalfSize;

    return body;
}

AABB Collision::SolidFromBox(const Vector3& Position, const Vector3& Scale)
{
    // A crate stands on its position: base at Position.y, top at
    // Position.y + Scale.y * 2. The old code used that height for landing on
    // top but Position.y +/- Scale.y for the sides, which is why a player
    // could end up floating above a crate it had just landed on.
    AABB solid;
    solid.Center = Vector3(Position.x, Position.y + Scale.y, Position.z);
    solid.HalfSize = Scale;

    return solid;
}

std::vector<AABB> Collision::GatherSolids()
{
    std::vector<AABB> solids;

    auto boxes = Manager::GetGameObjs<Box>();
    for (auto box : boxes)
        solids.push_back(SolidFromBox(box->GetPosition(), box->GetScale()));

    return solids;
}

bool Collision::MoveX(Vector3& Position, const Vector3& HalfSize, float Delta,
    const std::vector<AABB>& Solids)
{
    Position.x += Delta;

    if (Delta == 0.0f)
        return false;

    bool blocked = false;

    for (int i = 0; i < (int)Solids.size(); i++)
    {
        const AABB& solid = Solids[i];
        AABB body = BodyAABB(Position, HalfSize);

        if (!AABBvsAABB(body, solid))
            continue;

        // Only X moves here. Whatever the body is doing vertically is left
        // alone, so a crate cannot boost or drop it sideways.
        if (Delta > 0.0f)
            Position.x = solid.MinX() - HalfSize.x;
        else
            Position.x = solid.MaxX() + HalfSize.x;

        blocked = true;
    }

    return blocked;
}

bool Collision::MoveY(Vector3& Position, const Vector3& HalfSize, float Delta,
    const std::vector<AABB>& Solids, bool& Landed)
{
    Position.y += Delta;
    Landed = false;

    if (Delta == 0.0f)
        return false;

    bool blocked = false;

    for (int i = 0; i < (int)Solids.size(); i++)
    {
        const AABB& solid = Solids[i];
        AABB body = BodyAABB(Position, HalfSize);

        if (!AABBvsAABB(body, solid))
            continue;

        if (Delta < 0.0f)
        {
            // falling onto the top of it
            Position.y = solid.MaxY();
            Landed = true;
        }
        else
        {
            // jumping into the underside of it
            Position.y = solid.MinY() - HalfSize.y * 2.0f;
        }

        blocked = true;
    }

    return blocked;
}

void Collision::PushOutOfSolids(Vector3& Position, const Vector3& HalfSize,
    const std::vector<AABB>& Solids)
{
    for (int i = 0; i < (int)Solids.size(); i++)
    {
        const AABB& solid = Solids[i];
        AABB body = BodyAABB(Position, HalfSize);

        if (!AABBvsAABB(body, solid))
            continue;

        // Leave along the shallower axis - that is the direction the body
        // came from, so it pops out the side it was pushed through instead
        // of teleporting over the top.
        float overlapX = (body.HalfSize.x + solid.HalfSize.x) - fabsf(body.Center.x - solid.Center.x);
        float overlapY = (body.HalfSize.y + solid.HalfSize.y) - fabsf(body.Center.y - solid.Center.y);

        if (overlapX <= overlapY)
            Position.x += (body.Center.x < solid.Center.x) ? -overlapX : overlapX;
        else
            Position.y += (body.Center.y < solid.Center.y) ? -overlapY : overlapY;
    }
}
