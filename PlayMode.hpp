#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	struct Pillar {
		glm::vec3 position;
		float radius;
		float height;
	};
	std::vector<Pillar> pillars;

	struct Egg {
		Scene::Transform *transform = nullptr;
		Scene::Drawable *drawable = nullptr;
		bool collected = false;
	};
	std::vector<Egg> eggs;
	uint32_t eggs_collected = 0;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *dragon = nullptr;
	glm::quat dragon_rotation;
	float speed = 0.0f;
	float max_speed = 90.0f;
	float turn_speed = 40.0f;
	float accel = 4.0f;
	float decel = 6.0f;
	float vert = 0.0f;
	float horiz = 0.0f;
	bool won = false;
	float time = 0.0f;

	Scene::Transform *wingL = nullptr;
	Scene::Transform *wingR = nullptr;
	glm::quat wingL_base;
	glm::quat wingR_base;
	float flap = 0.0f;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
