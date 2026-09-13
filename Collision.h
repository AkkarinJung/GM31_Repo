#pragma once
#include <vector>
#include "Vector3.h"

// An axis aligned box. Solids in the world and the bodies that walk into
// them are both described with one, so there is a single overlap test in the
// game rather than one per pair of object types.
struct AABB
{
    Vector3 Center;
    Vector3 HalfSize;

    float MinX() const { return Center.x - HalfSize.x; }
    float MaxX() const { return Center.x + HalfSize.x; }
    float MinY() const { return Center.y - HalfSize.y; }
    float MaxY() const { return Center.y + HalfSize.y; }
    float MinZ() const { return Center.z - HalfSize.z; }
    float MaxZ() const { return Center.z + HalfSize.z; }
};

// How the 2.5D collision works, and why it is shaped like this:
//
//  * Bodies move one axis at a time - X first, then Y - and each move is
//    pushed back out along that axis only. Resolving both axes at once is
//    what makes a character catch on corners, and "put it back where it was
//    last frame" (what this game did before) breaks the moment two things
//    push in the same frame: the remembered position is already wrong.
//
//  * Solids are absolute, characters are soft. Enemies nudge each other and
//    the player apart, and then everyone is pushed back out of the solids,
//    so a crowd can never squeeze anyone through a crate.
//
//  * A body stands on its position: Position is at its feet, exactly like
//    the player and the enemies already do, and HalfSize.y is half its
//    height.
class Collision
{
public:
    static bool SphereVsSphere(
        const Vector3& centerA,
        float radiusA,
        const Vector3& centerB,
        float radiusB);

    static bool SphereVsAABB(
        const Vector3& sphereCenter,
        float sphereRadius,
        const Vector3& boxCenter,
        const Vector3& boxHalfSize);

    static bool AABBvsAABB(const AABB& A, const AABB& B);

    // The body of something standing at Position.
    static AABB BodyAABB(const Vector3& Position, const Vector3& HalfSize);

    // The one place that says how tall a crate is and where its base sits.
    // Change it here if a model's origin turns out to be its centre.
    static AABB SolidFromBox(const Vector3& Position, const Vector3& Scale);

    // Every solid in the scene. Called by whoever is about to move.
    static std::vector<AABB> GatherSolids();

    // Move along one axis and push back out of anything hit. Returns true
    // when the move was blocked, which is what the caller zeroes its
    // velocity on.
    static bool MoveX(Vector3& Position, const Vector3& HalfSize, float Delta,
        const std::vector<AABB>& Solids);

    static bool MoveY(Vector3& Position, const Vector3& HalfSize, float Delta,
        const std::vector<AABB>& Solids, bool& Landed);

    // After something soft moved a body (an enemy shoving the player, a
    // crowd separating), shove it back out of any solid it ended up inside,
    // along whichever axis it is least deep in.
    static void PushOutOfSolids(Vector3& Position, const Vector3& HalfSize,
        const std::vector<AABB>& Solids);
};
