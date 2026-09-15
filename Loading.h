#pragma once
#include "Scene.h"

// The screen between the title and the first map.
//
// Game::Init is not cheap - it builds the whole stage, and the scenery alone
// is around ninety models plus the tree line. Manager runs a scene's Init
// synchronously, so that work blocks the message loop: whatever was on
// screen when it started is what the player stares at until it finishes.
// Before, that was the title screen with START still lit, which reads as the
// click having been ignored.
//
// So this scene exists to be the thing on screen while that happens. It
// draws itself, and only then asks for the map - see Update. It owns a
// couple of flat quads and nothing else, because a loading screen that has
// to load something is no use.
class Loading : public Scene
{
private:
    float m_Time = 0.0f;

    // Frames this scene has actually been drawn for. The handover waits on
    // this rather than on m_Time alone: time passing does not prove anything
    // reached the screen, and the whole point is that it did.
    int m_FramesDrawn = 0;

    bool m_Requested = false; // the map has been asked for - only once

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
