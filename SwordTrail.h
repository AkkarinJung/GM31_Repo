#pragma once

#include "GameObject.h"

// The ribbon the blade drags behind it during a swing.
//
// This is the MOTION half of the attack VFX. SlashEffect draws the arc - one
// crescent sprite, played once - and that alone reads as a flash rather than
// as a blade travelling, because a single sprite has no history in it. This
// keeps the last few positions of the blade and joins them into one tapered
// strip, so what the player sees is where the sword has actually been.
//
// It carries no damage, exactly like SlashEffect. The hitbox is Sword::Use
// and the two are driven from different places in Player::Update, so this can
// be made as long or as bright as it needs to be without touching what a
// swing hits.
//
// ONE of these exists per player, built in Player::Init and living as long as
// the player does - it is not spawned per swing. A swing turns it on and off
// (Begin/End) and the samples are pushed in by the owner (Sample). Nothing
// here allocates after Init.
//
// The owner drives Sample() rather than this object reading the blade in its
// own Update, and that is deliberate: BoneAttachPoint is a COMPONENT of the
// player, so the sword's matrix is only correct after Player::Update has run
// its components. Manager also re-sorts its object list by camera depth every
// frame, so the order two GameObjects update in is not fixed - sampling from
// the player's own Update is the only way to be sure the blade transform is
// this frame's rather than last frame's.
class SwordTrail : public GameObject
{
private:
    // One captured position of the blade: where the ribbon's two edges were
    // at that instant, and how long ago that was.
    struct TrailSample
    {
        Vector3 Base{ 0.0f, 0.0f, 0.0f };  // near the grip
        Vector3 Tip{ 0.0f, 0.0f, 0.0f };   // the point of the blade
        float   Age = 0.0f;                // seconds since it was taken
    };

    // How many positions the ribbon remembers. At one sample a frame this is
    // 0.4s of blade history, which is longer than any swing here - the age
    // fade below is what actually decides the length, and this is only the
    // ceiling. Index 0 is always the newest.
    static const int TRAIL_MAX = 24;

    TrailSample m_Sample[TRAIL_MAX];
    int m_SampleCount = 0;

    bool m_Emitting = false;

    // Impact freeze. The game holds the animation still for a few frames when
    // a hit lands (Player::m_HitStopFrames), and a ribbon that kept ageing
    // through that would fade out during exactly the moment it is meant to
    // be selling - the blade is not moving, so there is nothing to capture,
    // but what is already drawn has to stay put rather than die.
    bool m_Frozen = false;

    // The blade being followed, and how far up it the two edges of the ribbon
    // sit. Read through GetMatrx(), so the socket's offset and the player's
    // own turn are already folded in - nothing here assumes which way the
    // player is facing.
    GameObject* m_Blade = nullptr;
    float m_BaseReach = 0.18f;
    float m_TipReach = 1.05f;

    // ---- tuning ---------------------------------------------------------
    //
    // TrailFadeTime. How long a captured position survives, in seconds. This
    // is the ribbon's LENGTH: at 60fps a life of 0.18s is about eleven
    // samples of blade travel. Longer reads as slower - a trail that outlives
    // the swing stops selling speed and starts looking like a smear.
    float m_SampleLife = 0.18f;

    // How sharply the ribbon dies off along its length. Above 1 it holds its
    // brightness near the blade and drops away hard at the tail, which is
    // what keeps the leading edge crisp.
    float m_FadePower = 1.6f;

    // How much of the blade's width is left at the tail, as a fraction. This
    // is the taper: 1.0 would be a flat band, and a small number pulls the
    // old end into a point behind the blade.
    float m_TailWidth = 0.12f;

    // Overall brightness. Additive, so above 1 simply blows the core out to
    // white and widens the hot part of the ribbon.
    float m_Gain = 1.25f;

    // Hot at the blade, cool at the tail. Additive blending turns the two
    // into a white-cored streak with a coloured wake, which is the whole
    // anime-slash look and costs nothing but two constants.
    XMFLOAT4 m_HeadColour{ 1.00f, 1.00f, 1.00f, 1.0f };
    XMFLOAT4 m_TailColour{ 0.42f, 0.70f, 1.00f, 1.0f };

    // GPU resources. Per object, not shared statics like SlashEffect's: there
    // is exactly one trail in a scene, so there is nothing to share it with,
    // and Particle - the other single-instance emitter here - is built the
    // same way.
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Which object to follow, and where on it the ribbon's two edges sit.
    // Reach is measured along the blade's own long axis, in world units.
    void SetBlade(GameObject* Blade, float BaseReach, float TipReach);

    // Start a new ribbon. Always clears the history first: a swing that
    // picked up where the last one stopped would draw one long polygon
    // bridging the gap between them.
    void Begin();

    // Stop capturing. What is already on screen stays and fades out on its
    // own, which is what makes the trail die with the swing instead of being
    // cut off mid-air.
    void End();

    // Throw the ribbon away this instant - nothing fades. For the cases where
    // continuing it would be a lie about where the blade went: the player
    // turning round mid-swing, a swing interrupted, death, a scene change.
    void Clear();

    // Called once a frame by the owner, AFTER its components have moved the
    // weapon. Does nothing unless emitting.
    void Sample();

    bool IsEmitting() const { return m_Emitting; }

    // Hold the ribbon exactly as it is - no ageing, no fading. Set from the
    // owner's hit-stop counter.
    void SetFrozen(bool Frozen) { m_Frozen = Frozen; }

    // Tuning, so a heavier swing can wear a longer, brighter ribbon without a
    // second class. Set before Begin().
    void SetSampleLife(float Seconds) { m_SampleLife = Seconds; }
    void SetGain(float Gain) { m_Gain = Gain; }
    void SetColours(const XMFLOAT4& Head, const XMFLOAT4& Tail)
    {
        m_HeadColour = Head;
        m_TailColour = Tail;
    }
};
