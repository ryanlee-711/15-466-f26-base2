#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <string>

GLuint level_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level.pnct"));
	level_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = level_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*level_scene) {
	std::unordered_map< Scene::Transform *, Scene::Drawable * > transform_to_drawable;
	for (auto &d : scene.drawables) {
		transform_to_drawable[d.transform] = &d;
	}
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Dragon") dragon = &transform;
		if (transform.name.starts_with("Pillar")) {
			Pillar pil;
			pil.position = transform.position;
			pil.radius = 1.14f * std::max(std::abs(transform.scale.x), std::abs(transform.scale.y));
			pil.height = transform.position.z + (0.5f * 2.0f * std::abs(transform.scale.z));
			pillars.emplace_back(pil);
		}
		if (transform.name.starts_with("Egg")) {
			Egg egg;
			egg.transform = &transform;
			auto drawb = transform_to_drawable.find(&transform);
			if (drawb != transform_to_drawable.end()) egg.drawable = drawb->second;
			eggs.emplace_back(egg);
		}
		if (transform.name == "WingL") wingL = &transform;
		if (transform.name == "WingR") wingR = &transform;
	}
	if (dragon == nullptr) throw std::runtime_error("Dragon not found.");
	if (wingL == nullptr) throw std::runtime_error("WingL not found.");
	if (wingR == nullptr) throw std::runtime_error("WingR not found.");
	wingL_base = wingL->rotation;
	wingR_base = wingR->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
		}

	return false;
}

void PlayMode::update(float elapsed) {
	if (!won) time += elapsed;
	flap += elapsed * 6.0f;
	float angle = std::sin(flap) * 1.2f;
	wingL->rotation = wingL_base * glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
	wingR->rotation = wingR_base * glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));

	if (won) return;
	float vertIn = 0.0f;
	float horizIn = 0.0f;
	if (left.pressed && !right.pressed) horizIn = 1.0f;
	if (!left.pressed && right.pressed) horizIn = -1.0f;
	if (up.pressed && !down.pressed) vertIn = 1.0f;
	if (!up.pressed && down.pressed) vertIn = -1.0f;

	vert += vertIn * 0.9f * elapsed;
	horiz += horizIn * 1.0f * elapsed;
	vert = glm::clamp(vert, -1.0f, 1.0f);

	float turning = std::min(1.0f, std::abs(horizIn) + std::abs(vertIn));
	float next_speed = max_speed - turning * (max_speed - turn_speed);
	next_speed -= std::sin(vert) * 15.0f;
	float rate = accel;
	if (next_speed < speed) rate = decel;
	speed += (next_speed - speed) * std::min(1.0f, rate * elapsed);

	float newTurnDip = -horizIn * 0.6f;
	turnDip += (newTurnDip - turnDip) * std::min(1.0f, 4.0f * elapsed);

	dragon->rotation = glm::angleAxis(horiz, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::angleAxis(vert, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::angleAxis(turnDip, glm::vec3(0.0f, 1.0f, 0.0f));

	glm::vec3 forward = dragon->rotation * glm::vec3(0.0f, 1.0f, 0.0f);
	dragon->position += forward * speed * elapsed;
	dragon->position.z = glm::clamp(dragon->position.z, 5.0f, 500.0f);
	dragon->position.x = glm::clamp(dragon->position.x, -400.0f, 400.0f);
	dragon->position.y = glm::clamp(dragon->position.y, -400.0f, 400.0f);

	for (auto const &pillar : pillars) {
		if (dragon->position.z >= pillar.height) continue;
		glm::vec2 offset = glm::vec2(dragon->position) - glm::vec2(pillar.position);
		float dist = glm::length(offset);
		float minDist = pillar.radius + 12;

		if (dist < minDist) {
			glm::vec2 normal = (dist > 0.0001f ? offset / dist : glm::vec2(1.0f, 0.0f));
			glm::vec2 pushed = glm::vec2(pillar.position) + normal * (minDist);
			dragon->position.x = pushed.x;
			dragon->position.y = pushed.y;
			speed *= 0.5f;
		}
	}

	for (auto &egg : eggs) {
		if (egg.collected) continue;
		float dist = glm::length(dragon->position - egg.transform->position);
		if (dist < 13) {
			egg.collected = true;
			if (egg.drawable) egg.drawable->pipeline.count = 0;
			eggs_collected += 1;
			if (eggs_collected == eggs.size()) won = true;
		}
	}

	//Camera Position
	glm::vec3 camPos = dragon->position - forward * 115.0f + glm::vec3(0.0f, 0.0f, 15.0f);
	camera->transform->position = glm::mix(camera->transform->position, camPos, std::min(1.0f, 6.0f * elapsed));
	if (camera->transform->position.z < 1.0f) camera->transform->position.z = 1.0f;
	glm::vec3 z_axis = glm::normalize(camera->transform->position - dragon->position);
	glm::vec3 x_axis = glm::normalize(glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), z_axis));
	glm::vec3 y_axis = glm::cross(z_axis, x_axis);
	camera->transform->rotation = glm::quat_cast(glm::mat3(x_axis, y_axis, z_axis));

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.08f, 0.19f, 0.4f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;

		auto draw_text = [&](std::string const &text, float x, float y, float size) {
			lines.draw_text(text, glm::vec3(x, y, 0.0f),
				glm::vec3(size, 0.0f, 0.0f), glm::vec3(0.0f, size, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(text, glm::vec3(x + ofs, y + ofs, 0.0f),
				glm::vec3(size, 0.0f, 0.0f), glm::vec3(0.0f, size, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		int intSpeed = int(speed);
		std::string speed_text = "Speed: " + std::to_string(intSpeed);
		draw_text(speed_text, -aspect + 0.1f * H, -1.0f + 2.9f * H, H);

		int intTime = int(time);
		int tenths = int((time - float(intTime)) * 10.0f);
		std::string time_text = "Time: " + std::to_string(intTime) + "." + std::to_string(tenths) + "s";
		draw_text(time_text, -aspect + 0.1f * H, -1.0f + 1.7f * H, H);

		std::string eggCounter = "Eggs: " + std::to_string(eggs_collected) + " / " + std::to_string(eggs.size());
		draw_text(eggCounter, -aspect + 0.1f * H, -1.0f + 0.5f * H, H);

		if (won) {
			draw_text("ALL EGGS COLLECTED!", -aspect * 0.3f, 0.0f, H * 1.5f);
			std::string final_time = "Final Time: " + std::to_string(intTime) + "." + std::to_string(tenths) + "s";
			draw_text(final_time, -aspect * 0.2f, -0.15f, H * 1.5f);
		}	
	}
}
