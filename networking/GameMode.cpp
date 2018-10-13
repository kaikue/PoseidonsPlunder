#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("paddle-ball.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *paddle1_transform = nullptr;
Scene::Transform *paddle2_transform = nullptr;
Scene::Transform *bullet1_transform = nullptr;
Scene::Transform *bullet2_transform = nullptr;
Scene::Transform *ball_transform = nullptr;

Scene::Camera *camera = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;
	//load transform hierarchy:
	ret->load(data_path("paddle-ball.scene"), [](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Object *obj = s.new_object(t);

		obj->program = vertex_color_program->program;
		obj->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		obj->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		obj->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		MeshBuffer::Mesh const &mesh = meshes->lookup(m);
		obj->vao = *meshes_for_vertex_color_program;
		obj->start = mesh.start;
		obj->count = mesh.count;
	});

	//look up paddle and ball transforms:
	for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
		if (t->name == "Paddle1") {
			if (paddle1_transform) throw std::runtime_error("Multiple 'Paddle1' transforms in scene.");
			paddle1_transform = t;
		}
    if (t->name == "Paddle2") {
      if (paddle2_transform) throw std::runtime_error("Multiple 'Paddle2' transforms in scene.");
      paddle2_transform = t;
    }
    if (t->name == "Bullet1") {
      if (bullet1_transform) throw std::runtime_error("Multiple 'Bullet1' transforms in scene.");
      bullet1_transform = t;
    }
    if (t->name == "Bullet2") {
      if (bullet2_transform) throw std::runtime_error("Multiple 'Bullet2' transforms in scene.");
      bullet2_transform = t;
    }
		if (t->name == "Ball") {
			if (ball_transform) throw std::runtime_error("Multiple 'Ball' transforms in scene.");
			ball_transform = t;
		}
	}
	if (!paddle1_transform) throw std::runtime_error("No 'Paddle1' transform in scene.");
	if (!paddle2_transform) throw std::runtime_error("No 'Paddle2' transform in scene.");
	if (!bullet1_transform) throw std::runtime_error("No 'Bullet1' transform in scene.");
	if (!bullet2_transform) throw std::runtime_error("No 'Bullet2' transform in scene.");
	if (!ball_transform) throw std::runtime_error("No 'Ball' transform in scene.");

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");
	return ret;
});

GameMode::GameMode(Client &client_) : client(client_) {
	client.connection.send_raw("h", 1); //send a 'hello' to the server
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

  if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
    if (evt.key.keysym.scancode == SDL_SCANCODE_UP) {
      controls.up = (evt.type == SDL_KEYDOWN);
      return true;
    }
    else if (evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
      controls.down = (evt.type == SDL_KEYDOWN);
      return true;
    }
  }

  //fire bullet
  if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
    controls.fire = true;
    return true;
  }

	return false;
}

void GameMode::draw_message(std::string message, float y) {
  float height = 0.06f;
  float width = text_width(message, height);
  draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
  draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
}

void GameMode::update(float elapsed) {
	state.update(elapsed);

  client.poll([&](Connection *c, Connection::Event event) {
    if (event == Connection::OnOpen) {
      //probably won't get this.
    }
    else if (event == Connection::OnClose) {
      std::cerr << "Lost connection to server." << std::endl;
    }
    else {
      assert(event == Connection::OnRecv);
      if (c->recv_buffer[0] == 'p') {
        if (c->recv_buffer.size() < 2) {
          return; //wait for more data
        }
        else {
          if (c->recv_buffer[1] == '0') {
            std::cout << "Server already has 2 players." << std::endl;
            exit(0);
          }
          state.is_player1 = c->recv_buffer[1] == '1';
          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 2);
        }
      }
      else {
        assert(c->recv_buffer[0] == 's');
        if (c->recv_buffer.size() < 1 + 8 * sizeof(float) + 2 * sizeof(unsigned short int)) {
          return; //wait for more data
        }
        else {
          memcpy(&state.ball.x, c->recv_buffer.data() + 1 + 0 * sizeof(float), sizeof(float));
          memcpy(&state.ball.y, c->recv_buffer.data() + 1 + 1 * sizeof(float), sizeof(float));
          memcpy(&state.bullet1.x, c->recv_buffer.data() + 1 + 2 * sizeof(float), sizeof(float));
          memcpy(&state.bullet1.y, c->recv_buffer.data() + 1 + 3 * sizeof(float), sizeof(float));
          memcpy(&state.bullet2.x, c->recv_buffer.data() + 1 + 4 * sizeof(float), sizeof(float));
          memcpy(&state.bullet2.y, c->recv_buffer.data() + 1 + 5 * sizeof(float), sizeof(float));

          //don't adjust our own angle to the server's
          if (!state.is_player1) {
            memcpy(&state.paddle1, c->recv_buffer.data() + 1 + 6 * sizeof(float), sizeof(float));
          }
          else {
            memcpy(&state.paddle2, c->recv_buffer.data() + 1 + 7 * sizeof(float), sizeof(float));
          }

          memcpy(&state.score1, c->recv_buffer.data() + 1 + 8 * sizeof(float), sizeof(unsigned short int));
          memcpy(&state.score2, c->recv_buffer.data() + 1 + 8 * sizeof(float) + sizeof(unsigned short int), sizeof(unsigned short int));
          
          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 8 * sizeof(float) + 2 * sizeof(unsigned short int));
        }
      }
    }
  });

  if (controls.up) {
    if (state.is_player1) {
      state.paddle1 = glm::min(180.0f, state.paddle1 + paddle_speed);
    }
    else {
      state.paddle2 = glm::max(-180.0f, state.paddle2 - paddle_speed);
    }
  }
  if (controls.down) {
    if (state.is_player1) {
      state.paddle1 = glm::max(0.0f, state.paddle1 - paddle_speed);
    }
    else {
      state.paddle2 = glm::min(0.0f, state.paddle2 + paddle_speed);
    }
  }

	if (client.connection) {
		//send game state to server:
		client.connection.send_raw("s", 1);
    client.connection.send_raw(state.is_player1 ? "1" : "2", 1);
    client.connection.send_raw(controls.fire ? "1" : "0", 1);
		client.connection.send_raw(state.is_player1 ? &state.paddle1 : &state.paddle2, sizeof(float));
	}
  controls.fire = false;

	//copy game state to scene positions:
	ball_transform->position.x = state.ball.x;
	ball_transform->position.y = state.ball.y;
  
  bullet1_transform->position.x = state.bullet1.x;
  bullet1_transform->position.y = state.bullet1.y;

  bullet2_transform->position.x = state.bullet2.x;
  bullet2_transform->position.y = state.bullet2.y;

  glm::vec3 paddle1_angles(0, 0, glm::radians(state.paddle1));
  paddle1_transform->rotation = glm::quat(paddle1_angles);
  
  glm::vec3 paddle2_angles(0, 0, glm::radians(state.paddle2));
  paddle2_transform->rotation = glm::quat(paddle2_angles);
}

void GameMode::draw(glm::uvec2 const &drawable_size) {
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.25f, 0.0f, 0.5f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	scene->draw(camera);

  std::string message = " TO ";
  for (int i = 0; i < state.score1; i++) {
    message = "*" + message;
  }
  if (state.score1 == 0) {
    message = "NIL" + message;
  }
  for (int i = 0; i < state.score2; i++) {
    message = message + "*";
  }
  if (state.score2 == 0) {
    message = message + "NIL";
  }
  draw_message(message, 0.9f);

  if (state.score1 >= state.win_score) {
    state.won = state.is_player1;
    state.lost = !state.is_player1;
  }
  else if (state.score2 >= state.win_score) {
    state.won = !state.is_player1;
    state.lost = state.is_player1;
  }

  if (state.won) {
    draw_message("*** YOU WON ***", 0.0f);
  }
  else if (state.lost) {
    draw_message("*** YOU LOST ***", 0.0f);
  }

	GL_ERRORS();
}
