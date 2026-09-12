#pragma once


class Input
{
private:
	static BYTE m_OldKeyState[256];
	static BYTE m_KeyState[256];

	static float m_MouseX;
	static float m_MouseY;
public:
	static void Init();
	static void Uninit();
	static void Update();

	static bool GetKeyPress( BYTE KeyCode );
	static bool GetKeyTrigger( BYTE KeyCode );

	static float GetMouseX() { return m_MouseX; }
	static float GetMouseY() { return m_MouseY; }

	// Treat this key's current press as already handled, so nothing else
	// sees a trigger for it this frame. Used when a UI click must not also
	// reach gameplay (the reward pick swallows the click that picked a card,
	// otherwise the player swings the sword on the same frame).
	static void ConsumeKeyTrigger(BYTE KeyCode);
};
