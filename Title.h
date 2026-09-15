#pragma once
#include "Scene.h"

class Title : public Scene
{
private:
    // Which entry the arrow is on. Kept here rather than in a GameObject
    // because the menu is drawn by the scene itself - see Manager::Draw,
    // which now calls Scene::Draw.
    int m_Selected = 0;

    // Fades the "press" hint and the selected entry, so the screen is not a
    // still image.
    float m_Time = 0.0f;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
