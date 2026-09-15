#include "main.h"
#include "renderer.h"
#include "Collision.h"
#include "manager.h"
#include "Box.h"
#include "Hedge.h"
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

    // The map edge. A hedge stands on its position the same way a crate does,
    // but its half size comes off the model it loaded rather than its scale,
    // so the invisible wall is always the size of the hedge you can see.
    auto hedges = Manager::GetGameObjs<Hedge>();
    for (auto hedge : hedges)
    {
        Vector3 halfSize = hedge->GetSolidHalfSize();
        Vector3 position = hedge->GetPosition();

        AABB solid;
        solid.Center = Vector3(position.x, position.y + halfSize.y, position.z);
        solid.HalfSize = halfSize;

        solids.push_back(solid);
    }

    return solids;
}

bool Collision::MoveX(Vector3& Position, const Vector3& HalfSize, float Delta,
    const std::vector<AABB>& Solids)
{
    if (Delta == 0.0f)
        return false;

    // Swept, not "teleport to the destination and then look for an overlap".
    //
    // The old version moved the body the whole way first and only then tested,
    // so it could only stop something whose overlap SURVIVED the entire step.
    // Anything it stepped clean over in one frame was never seen at all, and
    // the map edge is the worst case in the game for that: the hedge model
    // arrives 400 units long and is scaled to 4, which leaves its collider
    // about 0.3 units thick once the quarter turn swaps its axes. A thin wall
    // is exactly what a move-then-test sweep misses.
    //
    // Worse, a body that ended a step with its centre even slightly past a
    // thin wall's centre was then pushed the REST of the way through by
    // PushOutOfSolids, which leaves along whichever side the centre is on.
    // Finding the first blocking face along the path removes both problems:
    // the body can never end a frame beyond a solid it should have hit.
    AABB body = BodyAABB(Position, HalfSize);

    float travel = Delta;
    bool blocked = false;

    for (int i = 0; i < (int)Solids.size(); i++)
    {
        const AABB& solid = Solids[i];

        // The sweep only matters if the body already lines up with the solid
        // on the other two axes - otherwise it passes above, below or behind.
        if (body.MaxY() <= solid.MinY() || body.MinY() >= solid.MaxY())
            continue;
        if (body.MaxZ() <= solid.MinZ() || body.MinZ() >= solid.MaxZ())
            continue;

        if (Delta > 0.0f)
        {
            // Only solids the body has not already reached. One it is already
            // inside is PushOutOfSolids' job, not this function's - trying to
            // resolve it here would shove the body backwards mid-step.
            if (body.MaxX() > solid.MinX())
                continue;

            float allowed = solid.MinX() - body.MaxX();
            if (allowed < travel)
            {
                travel = allowed;
                blocked = true;
            }
        }
        else
        {
            if (body.MinX() < solid.MaxX())
                continue;

            float allowed = solid.MaxX() - body.MinX(); // negative
            if (allowed > travel)
            {
                travel = allowed;
                blocked = true;
            }
        }
    }

    Position.x += travel;

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

bool Collision::SegmentBlocked(const Vector3& A, const Vector3& B,
    const std::vector<AABB>& Solids)
{
    const float dx = B.x - A.x;
    const float dy = B.y - A.y;

    for (int i = 0; i < (int)Solids.size(); i++)
    {
        const AABB& solid = Solids[i];

        // Slab test: clip the segment's 0..1 parameter against the box on each
        // axis in turn. Whatever survives both is inside the box.
        float tMin = 0.0f;
        float tMax = 1.0f;
        bool  miss = false;

        // X
        if (fabsf(dx) < 0.000001f)
        {
            if (A.x < solid.MinX() || A.x > solid.MaxX())
                miss = true;
        }
        else
        {
            float t1 = (solid.MinX() - A.x) / dx;
            float t2 = (solid.MaxX() - A.x) / dx;
            if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
        }

        // Y
        if (!miss)
        {
            if (fabsf(dy) < 0.000001f)
            {
                if (A.y < solid.MinY() || A.y > solid.MaxY())
                    miss = true;
            }
            else
            {
                float t1 = (solid.MinY() - A.y) / dy;
                float t2 = (solid.MaxY() - A.y) / dy;
                if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
            }
        }

        if (!miss && tMin <= tMax)
            return true;
    }

    return false;
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

        float overlapX = (body.HalfSize.x + solid.HalfSize.x) - fabsf(body.Center.x - solid.Center.x);
        float overlapY = (body.HalfSize.y + solid.HalfSize.y) - fabsf(body.Center.y - solid.Center.y);

        // Leave along the shallower axis - but only vertically when the body
        // really is resting on the solid or hanging under it.
        //
        // "Shallower axis" on its own is wrong the moment something is pushed
        // deep into a crate's SIDE. An enemy body is 1.4 tall and a crate is
        // 4 wide, so once a crowd has shoved it more than ~1.4 units in, X
        // becomes the DEEPER axis and this used to pop the enemy 1.4 units
        // straight down, through the floor of the box. The mesh field then
        // pulled it back to the ground still inside the crate, and it did it
        // again the next frame - which is one of the ways an enemy appeared
        // to teleport with another enemy behind it.
        //
        // A genuine landing never penetrates by more than the body's own half
        // height (MoveY stops it at the surface), so that is the test.
        bool leaveVertically = (overlapY < overlapX) && (overlapY <= HalfSize.y);

        if (!leaveVertically)
            Position.x += (body.Center.x < solid.Center.x) ? -overlapX : overlapX;
        else
            Position.y += (body.Center.y < solid.Center.y) ? -overlapY : overlapY;
    }
}
