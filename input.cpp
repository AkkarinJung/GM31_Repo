
#include "main.h"
#include "input.h"


BYTE Input::m_OldKeyState[256];
BYTE Input::m_KeyState[256];
float Input::m_MouseX = 0.0f;
float Input::m_MouseY = 0.0f;


void Input::Init()
{

	memset( m_OldKeyState, 0, 256 );
	memset( m_KeyState, 0, 256 );

}

void Input::Uninit()
{


}

void Input::Update()
{

	memcpy( m_OldKeyState, m_KeyState, 256 );

	GetKeyboardState( m_KeyState );

	POINT cursor;
	GetCursorPos(&cursor);
	ScreenToClient(GetWindow(), &cursor);

	// The 2D UI always draws in a fixed SCREEN_WIDTH x SCREEN_HEIGHT space,
	// so scale the client position into it - the client area is not
	// guaranteed to be exactly that size.
	RECT client;
	GetClientRect(GetWindow(), &client);

	float clientWidth = (float)(client.right - client.left);
	float clientHeight = (float)(client.bottom - client.top);

	m_MouseX = clientWidth > 0.0f ? cursor.x * (SCREEN_WIDTH / clientWidth) : (float)cursor.x;
	m_MouseY = clientHeight > 0.0f ? cursor.y * (SCREEN_HEIGHT / clientHeight) : (float)cursor.y;
}

bool Input::GetKeyPress(BYTE KeyCode)
{
	return (m_KeyState[KeyCode] & 0x80);
}

bool Input::GetKeyTrigger(BYTE KeyCode)
{
	return ((m_KeyState[KeyCode] & 0x80) && !(m_OldKeyState[KeyCode] & 0x80));
}

void Input::ConsumeKeyTrigger(BYTE KeyCode)
{
	m_OldKeyState[KeyCode] = m_KeyState[KeyCode];
}
