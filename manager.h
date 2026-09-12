#pragma once

class GameObject;
class Scene;
class Manager
{
private:
	static std::list<GameObject*> m_GameObjects;
	static Scene* m_Scene;
	static Scene* m_NextScene;
	static float m_ChangeTime;
	static bool m_Pause;

public:
	static void Init();
	static void Uninit();
	static void Update();
	static void Draw();
	// Freezes every GameObject's Update. The current Scene keeps updating,
	// so whatever paused the game (the start-of-map reward pick) can still
	// read input and unpause itself. Cleared on every scene change.
	static void SetPause(bool Pause) { m_Pause = Pause; }
	static bool IsPause() { return m_Pause; }

	template<typename T>
	static void ChangeScene(float time)
	{
		if (m_NextScene != nullptr)
			return;

		m_ChangeTime = time;
		m_NextScene = new T();
	}

	template<typename T>
	static T* AddGameObj()
	{
		T* obj = new T();
		obj->Init();
		m_GameObjects.push_back(obj);

		return obj;
	}

	template<typename T>
	static T* GetGameObj()
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if (find != nullptr)
			{
				return find;
			}
		}
		return nullptr;
	}

	template<typename T>
	static std::vector<T*> GetGameObjs()
	{
		std::vector<T*> gameObjects;
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if (find != nullptr)
			{
				gameObjects.push_back(find);
			}
		}
		return gameObjects;
	}


};