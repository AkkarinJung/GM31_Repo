#pragma once
#include "GameObject.h"

// Which value on the Stats component a bar tracks.
enum class BarStat
{
    HP,
    MP,
};

// Screen-space stat bar: a static background quad plus a fill quad that
// shrinks left-to-right based on Target's Stats component. Reusable for
// Player, Enemy, or anything else with a Stats component - pass a different
// Target, a different stat and a different fill texture.
//
// MPBar used to be a byte-for-byte copy of this file with GetHP swapped for
// GetMP, which meant every tweak to the art mapping or the easing had to be
// made twice.
class HPBar : public GameObject
{
private:
    float m_X = 0.0f, m_Y = 0.0f, m_Width = 0.0f, m_Height = 0.0f;

    class GameObject* m_Target = nullptr;
    BarStat m_Stat = BarStat::HP;

    ID3D11Buffer* m_BackgroundVertexBuffer = nullptr;
    ID3D11Buffer* m_FillVertexBuffer = nullptr;

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    ID3D11ShaderResourceView* m_BackgroundTexture = nullptr;
    ID3D11ShaderResourceView* m_FillTexture = nullptr;

    float m_DisplayRatio = 1.0f; // eases toward the real ratio each frame

    // The number printed on the bar. A bar alone says "about half"; the
    // number says how many hits that is, which is what the player is actually
    // deciding on.
    //
    // Deliberately NOT eased like the fill is. The fill slides so a hit reads
    // as an event, but the digits are information - lagging them by a third
    // of a second would show a value that is not true any more.
    bool  m_ShowValue = true;
    float m_ValueSize = 19.0f;

    // Where the digits sit, relative to this bar's own rectangle. The bar art
    // is a slanted parallelogram with transparent padding, so the text is
    // pulled in from the right edge to land on the solid part of it.
    float m_ValueRightInset = 34.0f;
    float m_ValueCenterY = 0.5f;   // fraction of the bar's height

    float StatRatio() const;                         // reads whichever stat this bar tracks
    void  StatValues(int& Value, int& Maximum) const; // and the raw numbers behind it

public:
    // Off for a bar where the number would be clutter rather than help.
    void SetShowValue(bool Show) { m_ShowValue = Show; }

    void Init() override {}
    void Init(float X, float Y, float Width, float Height, GameObject* Target,
        BarStat Stat, const WCHAR* FillTextureName);
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
