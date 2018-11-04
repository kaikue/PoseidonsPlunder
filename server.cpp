#include "Connection.hpp"
#include "GameState.hpp"
#include "Load.hpp"

#include <iostream>
#include <set>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>


//when in lobby:
void send_lobby_update(Connection *c, GameState *state, int player_id) {
	if (c) {
		c->send('u'); //update- send number of players and that player's ID
		c->send(state->player_count);
		c->send(player_id);

		c->send('t'); //team info
		for (int i = 0; i < state->player_count; i++) {
			c->send(state->players[i].team); //each player ID's team
			c->send(state->players[i].nickname); //each player ID's nickname
		}
	}
}

//when starting game:
void send_begin(Connection *c, GameState *state, int player_id) {
  if (c) {
	std::cout << "Begin " << player_id << std::endl;
    c->send('b'); //begin game- send number of players and that player's ID
  }
}

//when in game:
void send_state(Connection *c, GameState *state, int player_id) {
  if (c) {
    c->send('s'); //state update
                  // is_shot + player_count * (pos + vel + quat + is_fires + harpoon_pos + harpoon_vel) + treasure_pos + is_held_by 
                  // (player_count + 1) * bool + (3 * treasure_count + player_count * 13) * float + treasure_count * int

    bool is_shot = state->players[player_id].is_shot; //whether the player is stunned by a harpoon
    c->send(is_shot);

    //current points
    c->send(state->current_points);

    //players
    for (int i = 0; i < state->player_count; i++) {
      glm::vec3 pos = state->players[i].position;
      glm::vec3 vel = state->players[i].velocity;
      glm::quat rot = state->players[i].rotation;
      c->send(pos);
      c->send(vel);
      c->send(rot);

      //harpoon
      c->send(state->harpoons[i].state);

      glm::vec3 harpoon_pos = state->harpoons[i].position;
      glm::vec3 harpoon_vel = state->harpoons[i].velocity;
      glm::quat harpoon_rot = state->harpoons[i].rotation;
      c->send(harpoon_pos);
      c->send(harpoon_vel);
      c->send(harpoon_rot);
    }

    // treasure
    for (int i = 0; i < 2; i++) {
      glm::vec3 pos = state->treasures[i].position;
      int is_held_by = state->treasures[i].held_by;
      c->send(pos);
      c->send(is_held_by);
    }
  }
}

void update_lobby(GameState *state, std::unordered_map< Connection *, int > *player_ledger) {
	//send lobby state to all clients
	for (auto iter = player_ledger->begin(); iter != player_ledger->end(); iter++) {
		send_lobby_update(iter->first, state, iter->second);
	}
}

void update_server(GameState *state, std::unordered_map< Connection *, int > *player_ledger, float time) {
  state->update(time);
  //send state to all clients
  for (auto iter = player_ledger->begin(); iter != player_ledger->end(); iter++) {
    send_state(iter->first, state, iter->second);
  }
}

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}
	
	Server server(argv[1]);

  call_load_functions();

  GameState state;

  std::unordered_map< Connection *, int > player_ledger;

  std::unordered_map< int, int > ready_to_start;

  bool playing = false;

  auto then = std::chrono::high_resolution_clock::now();

  while (true) {
	  //get updates from clients
	  server.poll([&](Connection *c, Connection::Event evt) {
		  if (evt == Connection::OnOpen) {
			  std::cout << "Connection open" << std::endl;
			  int player_id = state.player_count;
			  player_ledger.insert(std::make_pair(c, player_id));
			  state.add_player(player_id, 0);
			  update_lobby(&state, &player_ledger);
		  }
		  else if (evt == Connection::OnClose) {
			  std::cout << "Connection close" << std::endl;
			  //lost connection with player :(
		  }
		  else {
			  //        std::cout << "Connection receive" << std::endl;
			  assert(evt == Connection::OnRecv);
			  uint32_t player_id = player_ledger.find(c)->second; // get player ID corresponding to connection
			  Player* player_data = &state.players.find(player_id)->second;

              while (!(c->recv_buffer.empty())) {
                  if (c->recv_buffer[0] == 'k') {
                      std::cout << "OK to start" << std::endl;
                      c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
                      ready_to_start[player_id] = 1;
                      if (ready_to_start.size() == state.player_count) { //TODO: and all the values are set to 1
                          //start game
                          for (std::pair<Connection*, int> p : player_ledger) {
                              send_begin(p.first, &state, p.second);
                          }
                          playing = true;
                      }
                  }
                  else if (c->recv_buffer[0] == 'n') {
                      if (c->recv_buffer.size() < 1 + 1 * sizeof(uint32_t) + 1 * sizeof(char) * Player::NICKNAME_LENGTH) {
                          return; //wait for more data
                      }
                      else {
                          std::cout << "Nickname/team update" << std::endl;
                          memcpy(&player_data->team, c->recv_buffer.data() + 1 + 0 * sizeof(int), sizeof(int));
                          memcpy(&player_data->nickname, c->recv_buffer.data() + 1 + 1 * sizeof(int), sizeof(char) * Player::NICKNAME_LENGTH);
                          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 1 * sizeof(int) + 1 * sizeof(char) * Player::NICKNAME_LENGTH);
                          update_lobby(&state, &player_ledger);
                      }
                  }
                  else if (c->recv_buffer[0] == 'p') {
                      if (c->recv_buffer.size() < 1 + 10 * sizeof(float) + 2 * sizeof(bool)) {
                          return; //wait for more data
                      }
                      else {
                          memcpy(&player_data->position, c->recv_buffer.data() + 1, sizeof(glm::vec3));
                          memcpy(&player_data->velocity, c->recv_buffer.data() + 1 + 1 * sizeof(glm::vec3), sizeof(glm::vec3));
                          memcpy(&player_data->rotation, c->recv_buffer.data() + 1 + 2 * sizeof(glm::vec3), sizeof(glm::quat));


                          bool shot = false;
                          bool grabbed = false;
                          memcpy(&shot, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 0 * sizeof(bool), sizeof(bool));
                          memcpy(&grabbed, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 1 * sizeof(bool), sizeof(bool));
                          if (shot) {
                              player_data->shot_harpoon = true;
                          }
                          if (grabbed) {
                              player_data->grab = true;
                          }

                          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 10 * sizeof(float) + 2 * sizeof(bool));
                      }
                  }
              }
		  }
	  }, 0.01);

	  if (playing) {
		  //based on https://stackoverflow.com/a/14391562
		  auto now = std::chrono::high_resolution_clock::now();
		  float diff = std::chrono::duration_cast<std::chrono::duration<float>>(now - then).count();
		  if (diff > 0.03f) {
			  then = now;
			  update_server(&state, &player_ledger, diff);
		  }
	  }
  }
}
