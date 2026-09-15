#pragma once
#include "renderer.h"   // ID3D11* below, same reason Weapon.h pulls it in
#include "GameObject.h"

// A full screen black wipe.
//
// Purely a visual overlay: it does NOT change the scene. This project already
// has one way to do that - Manager::ChangeScene<T>(seconds), which schedules
// the swap and rebuilds everything when the timer runs out - and a fade that
// drove a second, parallel route would mean two mechanisms that could disagree
// about which scene is coming. So a transition is two independent halves:
//
//     Manager::ChangeScene<Result>(3.0f);   // what happens
//     Fade::OutBefore(3.0f);                // what it looks like
//
// and the new scene opens with Fade::In() in its Init.
//
// Two things about this object are deliberately unusual, and both exist
// because a transition has to cover everything:
//
//   * it draws on FADE_LAYER, which is outside the 0..4 range Manager::Draw
//     walks, and Manager draws it by hand after the scene's own Draw. Fading
//     out from the title screen while the title's letters showed straight
//     through it would not be a transition.
//
//   * it keeps updating while the game is paused (UpdatesWhilePaused). The
//     reward pick at the start of every map pauses the objects, so a fade-in
//     spawned by Game::Init would otherwise freeze at solid black until the
//     player picked a card - which is to say, on a screen they cannot see.
class Fade : public GameObject
{
public:
    // Outside the 0..4 that Manager::Draw's layer loop walks, so nothing can
    // land on it by accident and the fade can never be drawn twice.
    static const int FADE_LAYER = 5;

private:
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    bool  m_FadeOut = false;
    float m_Duration = 0.5f;
    float m_StartDelay = 0.0f;  // sits fully transparent for this long first
    float m_Time = 0.0f;

    float Alpha() const;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    bool UpdatesWhilePaused() const override { return true; }

    // Black -> clear. Destroys itself once it is done, so a scene can spawn
    // one in Init and forget about it.
    static Fade* In(float Duration = 0.6f);

    // Clear -> black, then HOLDS at black. It does not destroy itself: the
    // screen has to stay dark until the scene actually swaps, and the swap
    // deletes every object anyway. Only ever pair it with a scene change.
    static Fade* Out(float Duration = 0.6f, float StartDelay = 0.0f);

    // The one to reach for. Times a fade so the screen is fully black exactly
    // when a change scheduled for ChangeDelay seconds from now fires - pass it
    // the same number given to Manager::ChangeScene.
    static Fade* OutBefore(float ChangeDelay, float Duration = 0.6f);
};
