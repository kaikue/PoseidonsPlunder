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
	return new MeshBuffer(data_path("test_level.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *player_transform = nullptr;
Scene::Transform *harpoon_transform = nullptr;
Scene::Transform *treasure_transform = nullptr;

Scene::Camera *camera = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;
	//load transform hierarchy:
	ret->load(data_path("test_level.scene"), [](Scene &s, Scene::Transform *t, std::string const &m){
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

  //TODO: load scene and specially mark player, harpoon, and treasure transforms (need to draw those dynamically)

	/*//look up paddle and ball transforms:
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
  */

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

  //TODO: set camera somehow based on player starting position

	return ret;
});

GameMode::GameMode(Client &client_) : client(client_) {
	
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

  //update controls based on keyboard input

  //movement
  if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
    if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
      controls.fwd = (evt.type == SDL_KEYDOWN);
      return true;
    }
    else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
      controls.back = (evt.type == SDL_KEYDOWN);
      return true;
    }
    else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
      controls.left = (evt.type == SDL_KEYDOWN);
      return true;
    }
    else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
      controls.right = (evt.type == SDL_KEYDOWN);
      return true;
    }
  }

  //shoot harpoon
  if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
    controls.fire = true;
    return true;
  }

  //TODO: grapple (right click)

  //grab treasure
  if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
    controls.grab = true;
    return true;
  }

  //mouse movement
  //handle tracking the mouse for rotation control:
  if (!mouse_captured) {
    if (evt.type == SDL_MOUSEBUTTONDOWN) {
      SDL_SetRelativeMouseMode(SDL_TRUE);
      mouse_captured = true;
      return true;
    }
  }
  else if (mouse_captured) {
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      SDL_SetRelativeMouseMode(SDL_FALSE);
      mouse_captured = false;
      //show_pause_menu();
      return true;
    }
    if (evt.type == SDL_MOUSEMOTION) {
      //Note: float(window_size.y) * camera->fovy is a pixels-to-radians conversion factor
      float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
      float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
      yaw = -yaw;
      pitch = -pitch;
      camera->transform->rotation = glm::normalize(
        camera->transform->rotation
        * glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f))
        * glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f))
      );
      return true;
    }
  }

	return false;
}

void GameMode::draw_message(std::string message, float y) {
  float height = 0.06f;
  float width = text_width(message, height);
  draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
  draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
}

//TODO: these should be in a separate LobbyMode
/*
//when in lobby:
void send_lobby_info(Connection *c) {
  if (c) {
    c->send('n'); //team and name
    c->send(team);
    c->send_raw(nickname, NICKNAME_LENGTH); //NICKNAME_LENGTH is constant, nickname should be padded if necessary
  }
}

void send_start(Connection *c) {
  if (c) {
    c->send('k'); //ok to start game
  }
}
*/
//when in game:
void GameMode::send_action(Connection *c) {
  if (c) {
    c->send('p'); //player update
                  //send player ID? or use sockets completely?
                  //movement

    glm::vec3 pos = player.position;
    glm::vec3 vel = player.velocity;
    glm::quat rot = player.orientation;

    c->send(pos.x);
    c->send(pos.y);
    c->send(pos.z);
    c->send(vel.x);
    c->send(vel.y);
    c->send(vel.z);
    c->send(rot.w);
    c->send(rot.x);
    c->send(rot.y);
    c->send(rot.z);

    c->send(controls.fire);
    c->send(controls.grab);
  }
}

void GameMode::poll_server() {
  //poll server for current game state
  client.poll([&](Connection *c, Connection::Event event) {
    if (event == Connection::OnOpen) {
      //probably won't get this.
    }
    else if (event == Connection::OnClose) {
      std::cerr << "Lost connection to server." << std::endl;
    }
    else {
      while (!(c->recv_buffer.empty())) {
        assert(event == Connection::OnRecv);
        // game begins
        if (c->recv_buffer[0] == 'b') {
          if (c->recv_buffer.size() < 1 + 2 * sizeof(int)) {
            return; //wait for more data
          }
          else {
            std::cout << "Enter the lobby." << std::endl;
            memcpy(&state.player_count, c->recv_buffer.data() + 1 + 0 * sizeof(int), sizeof(int));
            memcpy(&player.team, c->recv_buffer.data() + 1 + 1 * sizeof(int), sizeof(int));
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 2 * sizeof(int));
          }
        }
        // receive info about other players
        else if (c->recv_buffer[0] == 't') {
          if (c->recv_buffer.size() < 1 + state->player_count * sizeof(int)) {
            return; //wait for more data
          }
          else {
            std::cout << "receving the info about other players" << std::endl;
            for (int i = 0; i < player_count; i++) {
              memcpy(&state.players[i].team, c->recv_buffer.data() + 1 + i * sizeof(int), sizeof(int));
            }
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + state->player_count * sizeof(int));
          }
        }

        // in game
        else {
          assert(c->recv_buffer[0] == 's');
          if (c->recv_buffer.size() < 1 + (player_count + 1) * sizeof(bool) + (3 + player_count) * sizeof(float) + 1 * sizeof(int)) {
            return; //wait for more data
          }
          else {
            //TODO: if buffer length is more than twice the length of a full update, skip all but the last one
            memcpy(&player.is_shot, c->recv_buffer.data() + 1 + 0 * sizeof(bool) + 0 * sizeof(float) + 0 * sizeof(int), sizeof(bool));
            // update the players and the harpoons
            for (int i = 0; i < player_count; i++) {
              memcpy(&players[i].pos.x, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 0) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].pos.y, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 1) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].pos.z, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 2) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].vel.x, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 3) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].vel.y, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 4) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].vel.z, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 5) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].rot.q1, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 6) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].rot.q2, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 7) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].rot.q3, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 8) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&players[i].rot.q4, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 9) * sizeof(float) + 0 * sizeof(int), sizeof(float));

              memcpy(&players[i].is_firing, c->recv_buffer.data() + 1 + (1 + i) * sizeof(bool) + (i * 13 + 10) * sizeof(float) + 0 * sizeof(int), sizeof(bool));
              memcpy(&state.harpoons[i].pos.x, c->recv_buffer.data() + 1 + (2 + i) * sizeof(bool) + (i * 13 + 10) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].pos.y, c->recv_buffer.data() + 1 + (2 + i) * sizeof(bool) + (i * 13 + 11) * sizeof(float) + 0 * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].pos.z, c->recv_buffer.data() + 1 + (2 + i) * sizeof(bool) + (i * 13 + 12) * sizeof(float) + 0 * sizeof(int), sizeof(float));
            }
            // update treasure pos and state
            for (int j = 0; j < 2; j++) {
              memcpy(&state.treasure[i].pos.x, c->recv_buffer.data() + 1 + (1 + player_count) * sizeof(bool) + (player_count * 13 + j * 3 + 0) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasure[i].pos.y, c->recv_buffer.data() + 1 + (1 + player_count) * sizeof(bool) + (player_count * 13 + j * 3 + 1) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasure[i].pos.z, c->recv_buffer.data() + 1 + (1 + player_count) * sizeof(bool) + (player_count * 13 + j * 3 + 2) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasure[i].held_by, c->recv_buffer.data() + 1 + (1 + player_count) * sizeof(bool) + (player_count * 13 + j * 3 + 2) * sizeof(float) + j * sizeof(int), sizeof(int));
            }
            // (player_count + 1) * bool + (3 * treasure_count + player_count * 13) * float + treasure_count * int
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + (player_count + 1) * sizeof(bool) + (6 + player_count * 13) * sizeof(float) + 2 * sizeof(int));
          }
        }
      }
    }
  });
}

void GameMode::update(float elapsed) {
	state.update(elapsed);

  poll_server(); //TODO: not every frame?

  /*if (controls.up) {
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
  }*/

  //update own position based on controls
  //TODO: make this affect state (position, velocity, rotation) too
  glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
  float amt = 5.0f * elapsed; //TODO player speed
  if (controls.fwd) camera->transform->position -= amt * directions[2];
  if (controls.back) camera->transform->position += amt * directions[2];
  if (controls.left) camera->transform->position -= amt * directions[0];
  if (controls.right) camera->transform->position += amt * directions[0];

	if (client.connection) {
    send_action(client.connection);
	}
  controls.fire = false;
  controls.grab = false;

	//TODO copy game state to scene positions
	/*ball_transform->position.x = state.ball.x;
	ball_transform->position.y = state.ball.y;
  
  bullet1_transform->position.x = state.bullet1.x;
  bullet1_transform->position.y = state.bullet1.y;

  bullet2_transform->position.x = state.bullet2.x;
  bullet2_transform->position.y = state.bullet2.y;

  glm::vec3 paddle1_angles(0, 0, glm::radians(state.paddle1));
  paddle1_transform->rotation = glm::quat(paddle1_angles);
  
  glm::vec3 paddle2_angles(0, 0, glm::radians(state.paddle2));
  paddle2_transform->rotation = glm::quat(paddle2_angles);*/
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

  //TODO draw any UI text
  /*std::string message = " TO ";
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
  }*/

	GL_ERRORS();
}
