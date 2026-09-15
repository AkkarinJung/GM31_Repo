#pragma once
#include "Scene.h"

class Result : public Scene
{
private:
    // Captured in Init, not read live in Draw. Everything the summary reports
    // belongs to a run that has already ended, and the scene change has
    // already destroyed the objects that held it.
    // Won or died. The screen is the same one either way, so it has to be
    // told which run it is summarising.
    bool m_Complete = false;

    int m_StagesCleared = 0;
    int m_Kills = 0;

    float m_Time = 0.0f;

    // True while the cursor is over the button, so Draw can light it without
    // repeating the hit test.
    bool m_Hovered = false;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
