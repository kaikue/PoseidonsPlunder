#include "MainMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <time.h>

Load< MeshBuffer > meshes(LoadTagDefault, []() {
  return new MeshBuffer(data_path("test_level.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

static Scene::Lamp *sun = nullptr;

static Scene::Camera *camera = nullptr;

Load<Scene> scene(LoadTagDefault, []()
{
  Scene *ret = new Scene;

  	//pre-build some program info (material) blocks to assign to each object:
	Scene::Object::ProgramInfo vertex_color_program_info;
	vertex_color_program_info.program = vertex_color_program->program;
	vertex_color_program_info.vao = *meshes_for_vertex_color_program;
	vertex_color_program_info.mvp_mat4  = vertex_color_program->object_to_clip_mat4;
	vertex_color_program_info.mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
	vertex_color_program_info.itmv_mat3 = vertex_color_program->normal_to_light_mat3;

	//load transform hierarchy:
	ret->load(data_path("test_level.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Object *obj = s.new_object(t);

		obj->programs[Scene::Object::ProgramTypeDefault] = vertex_color_program_info;

		MeshBuffer::Mesh const &mesh = meshes->lookup(m);
		obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

		obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
	});

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

	//look up the spotlight:
	for (Scene::Lamp *l = ret->first_lamp; l != nullptr; l = l->alloc_next) {
		if (l->transform->name == "Sun") {
			if (sun) throw std::runtime_error("Multiple 'Sun' objects in scene.");
			if (l->type != Scene::Lamp::Directional) throw std::runtime_error("Lamp 'Sun' is not a sun.");
			sun= l;
		}
	}
	if (!sun) throw std::runtime_error("No 'Sun' spotlight in scene.");

return ret;
});

MainMode::MainMode() {

  Scene::Transform *camera_trans = camera->transform;

  player_up = glm::vec3(0.0f, 0.0f, 1.0f);
  player_at = camera_trans->position - 1.7f;
  player_right = glm::vec3(1.0f, 0.0f, 0.0f);

  std::cout << glm::to_string(player_up) << std::endl;
  std::cout << glm::to_string(player_right) << std::endl;
  std::cout << glm::to_string(glm::cross(player_up, player_right)) << std::endl;

  elev_offset = std::atan2f(
      std::sqrtf(player_up.x * player_up.x + player_up.y * player_up.y),
      player_up.z);

  camera->transform->position = player_at;
  camera->transform->rotation = glm::angleAxis(elev_offset + elevation, player_right);
}

MainMode::~MainMode() {
}

bool MainMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//handle tracking the state of WSAD for movement control:
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}

	//handle tracking the mouse for rotation control:
	if (!mouse_captured) {
		if (evt.type == SDL_MOUSEBUTTONDOWN) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
			return true;
		}
	} else if (mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      //SDL_SetRelativeMouseMode(SDL_FALSE);
      //mouse_captured = false;
      show_pause_menu();
			return true;
		}
		if (evt.type == SDL_MOUSEMOTION) {
      // Note: float(window_size.y) * camera->fovy is a pixels-to-radians
      // conversion factor
      float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
      float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
      azimuth -= yaw;
      elevation -= pitch;
      camera->transform->rotation =
          glm::normalize(glm::angleAxis(azimuth, player_up) *
          glm::angleAxis(elev_offset + elevation, player_right));
			return true;
		}
	}
	return false;
}

void MainMode::update(float elapsed) {
	glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
	float amt = 5.0f * elapsed;
	if (controls.right) camera->transform->position += amt * directions[0];
	if (controls.left) camera->transform->position -= amt * directions[0];
	if (controls.backward) camera->transform->position += amt * directions[2];
	if (controls.forward) camera->transform->position -= amt * directions[2];

}

void MainMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene->draw(camera);

	glUseProgram(0);

	GL_ERRORS();
}

void MainMode::draw_message(std::string message, float y) {
  float height = 0.06f;
  float width = text_width(message, height);
  draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
  draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void MainMode::show_pause_menu() {
	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;

	menu->choices.emplace_back("PAUSED");
	menu->choices.emplace_back("");
	menu->choices.emplace_back("RESUME", [game](){
		Mode::set_current(game);
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 2;

	Mode::set_current(menu);
}
