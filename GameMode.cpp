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
    glm::quat rot = player.rotation;

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
          if (c->recv_buffer.size() < 1 + state.player_count * sizeof(int)) {
            return; //wait for more data
          }
          else {
            std::cout << "receving the info about other players" << std::endl;
            for (int i = 0; i < state.player_count; i++) {
              memcpy(&state.players[i].team, c->recv_buffer.data() + 1 + i * sizeof(int), sizeof(int));
            }
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + state.player_count * sizeof(int));
          }
        }

        // in game
        else {
          assert(c->recv_buffer[0] == 's');
          if (c->recv_buffer.size() < 1 + (state.player_count + 1) * sizeof(bool) + (3 + state.player_count) * sizeof(float) + 1 * sizeof(int)) {
            return; //wait for more data
          }
          else {
            //TODO: if buffer length is more than twice the length of a full update, skip all but the last one
            memcpy(&player.is_shot, c->recv_buffer.data() + 1, sizeof(bool));
            // update the players and the harpoons
            for (int i = 0; i < state.player_count; i++) {
              memcpy(&state.players[i].position.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 0) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].position.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 1) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].position.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 2) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].velocity.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 3) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].velocity.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 4) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].velocity.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 5) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].rotation.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 6) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].rotation.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 7) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].rotation.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 8) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
              memcpy(&state.players[i].rotation.w, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 9) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));

              memcpy(&state.harpoons[i].state,      c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 10) * sizeof(float) + (i + 0) * sizeof(int), sizeof(int));
              memcpy(&state.harpoons[i].position.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 10) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].position.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 11) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].position.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 12) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].velocity.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 13) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].velocity.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 14) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
              memcpy(&state.harpoons[i].velocity.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 16 + 15) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
            }
            // update treasure pos and state
            for (int j = 0; j < 2; j++) {
              memcpy(&state.treasures[j].position.x, c->recv_buffer.data() + 1 + (1 + state.player_count) * sizeof(bool) + (state.player_count * 16 + j * 3 + 0) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasures[j].position.y, c->recv_buffer.data() + 1 + (1 + state.player_count) * sizeof(bool) + (state.player_count * 16 + j * 3 + 1) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasures[j].position.z, c->recv_buffer.data() + 1 + (1 + state.player_count) * sizeof(bool) + (state.player_count * 16 + j * 3 + 2) * sizeof(float) + j * sizeof(int), sizeof(float));
              memcpy(&state.treasures[j].held_by,    c->recv_buffer.data() + 1 + (1 + state.player_count) * sizeof(bool) + (state.player_count * 16 + j * 3 + 3) * sizeof(float) + j * sizeof(int), sizeof(int));
            }

            //1 bool for is_shot
            //player_count * 16 floats for pos(3), vel(3), rot(4), harpoon pos(3), and harpoon vel(3), plus 6 for two treasure pos(3)
            //player_count ints for harpoon states, plus 2 for two treasure held_by
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 * sizeof(bool) + (state.player_count * 16 + 6) * sizeof(float) + (state.player_count + 2) * sizeof(int));
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

	if (client.connection) {
    send_action(&client.connection);
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

	GL_ERRORS();
}
