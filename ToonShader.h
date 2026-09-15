#pragma once

#include "renderer.h"

// The cel-shaded look, loaded once and shared by everything that wants it.
//
// Enemy used to own the only copy: its own vertex shader, its own pixel
// shader and its own ramp texture, all built in its own Init. Putting the
// same look on the scenery that way would have meant a copy of all three per
// object, and a stage carries around ninety props plus the crates and the
// hedges - a few hundred shader file reads and PNG decodes per stage load,
// for objects that all draw with identical state.
//
// Same shape as SlashEffect's shared frames: Game::Init loads it, and
// Manager::Uninit frees it once at shutdown.
class ToonShader
{
public:
    static void LoadShared();    // idempotent - safe to call from anywhere
    static void UninitShared();

    // Bind for this draw call, then draw the mesh as usual. The mesh's own
    // texture goes to t0 (AnimationModel and ModelRenderer both set it); this
    // only binds the ramp at t1 and the shaders.
    //
    // Parameter is what toonPS reads:
    //    x  which row of the ramp to sample - one texture, several looks
    //    y  where the rim darkening starts (more negative = thicker)
    //    z  how dark the rim goes (1 = off)
    //    w  how soft the transition into it is
    static void Bind(const XMFLOAT4& Parameter);

    // The two looks in use. Scenery gets a thinner, paler rim than a
    // character does - an outline that reads well on one enemy turns a
    // hillside of bushes into noise.
    static const XMFLOAT4 CharacterLook;
    static const XMFLOAT4 SceneryLook;
};
