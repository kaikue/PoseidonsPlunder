#include "Connection.hpp"
#include "GameState.hpp"
#include "Load.hpp"

#include <iostream>
#include <set>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>

//when starting game:
void send_begin(Connection *c, GameState *state, int player_id) {
  if (c) {
    c->send('b'); //begin game- send number of players and that player's ID
    c->send(state->player_count);
    c->send(player_id);

    c->send('t'); //team info
    for (int i = 0; i < state->player_count; i++) {
      c->send(state->players[i].team); //each player ID's team
    }
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

    //players
    for (int i = 0; i < state->player_count; i++) {
      glm::vec3 pos = state->players[i].position;
      glm::vec3 vel = state->players[i].velocity;
      glm::quat rot = state->players[i].rotation;
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

      //harpoon
      //if (state->players[i].shot_harpoon) {
      c->send(state->harpoons[i].state);

      glm::vec3 harpoon_pos = state->harpoons[i].position;
      glm::vec3 harpoon_vel = state->harpoons[i].velocity;
//      glm::quat harpoon_rot = state->harpoons[i].rotation; //TODO
      c->send(harpoon_pos.x);
      c->send(harpoon_pos.y);
      c->send(harpoon_pos.z);
      c->send(harpoon_vel.x);
      c->send(harpoon_vel.y);
      c->send(harpoon_vel.z);

      //harpoon collision happens serverside only- if player and harpoon from different team are too close, that player is shot and the harpoon stops
      //stopped harpoons (after hitting land or player) don't hit anything
      //do retracting harpoons count as hitting things? (god of war)
      /*}
      else {
        c->send(false); //harpoon isn't fired
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
      }*/
    }
    // treasure
    //TODO
    for (int i = 0; i < 2; i++) {
      glm::vec3 pos = state->treasures[i].position;
      int is_held_by = state->treasures[i].held_by;
      c->send(pos.x);
      c->send(pos.y);
      c->send(pos.z);
      c->send(is_held_by);
    }
  }
}

void update_server(GameState *state, std::unordered_map< Connection *, int > *player_ledger, float time) {
  //TODO: check ready to start, then start game if so
  //TODO: set state.player_count when starting game

  state->update(time);
  //send state to all clients
  //for (std::pair<Connection *, int> p : player_ledger) {
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

  auto then = std::chrono::high_resolution_clock::now();

	while (1) {
    //get updates from clients
    server.poll([&](Connection *c, Connection::Event evt) {
      if (evt == Connection::OnOpen) {
        std::cout << "Connection open" << std::endl;
        int player_id = state.player_count;
        player_ledger.insert(std::make_pair(c, player_id));
        state.add_player(player_id, 0);
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
        if (c->recv_buffer[0] == 'k') {
          std::cout << "OK to start" << std::endl;
          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
          ready_to_start.insert(std::make_pair(player_id, 1));
        }
        else if (c->recv_buffer[0] == 'n') {
          if (c->recv_buffer.size() < 1 + 10 * sizeof(float) + 2 * sizeof(bool)) {
            return; //wait for more data
          }
          else {
            std::cout << "Nickname/team update" << std::endl;
            memcpy(&player_data->team, c->recv_buffer.data() + 1 + 0 * sizeof(uint32_t), sizeof(uint32_t));
            memcpy(&player_data->nickname, c->recv_buffer.data() + 1 + 1 * sizeof(uint32_t), sizeof(char) * Player::NICKNAME_LENGTH);
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 1 * sizeof(uint32_t) + 1 * sizeof(char) * Player::NICKNAME_LENGTH);
          }
        }
        else if (c->recv_buffer[0] == 'p') {
          if (c->recv_buffer.size() < 1 + 10 * sizeof(float) + 2 * sizeof(bool)) {
            return; //wait for more data
          }
          else {
            //std::cout << "Player update" << std::endl;
            /* vv relocate to server "update"
            // assert((!has_tr_1 && !has_tr_2) || (has_tr_1 != has_tr_2)); // want to enforce {has neither treasure} or {can have one of two treasures}
            // or just...
            // assert !(has_tr_1 && has_tr_2); //want to enforce {doesn't have both treasures}
            */
            memcpy(&player_data->position.x, c->recv_buffer.data() + 1 + 0 * sizeof(float), sizeof(float));
            memcpy(&player_data->position.y, c->recv_buffer.data() + 1 + 1 * sizeof(float), sizeof(float));
            memcpy(&player_data->position.z, c->recv_buffer.data() + 1 + 2 * sizeof(float), sizeof(float));
            memcpy(&player_data->velocity.x, c->recv_buffer.data() + 1 + 3 * sizeof(float), sizeof(float));
            memcpy(&player_data->velocity.y, c->recv_buffer.data() + 1 + 4 * sizeof(float), sizeof(float));
            memcpy(&player_data->velocity.z, c->recv_buffer.data() + 1 + 5 * sizeof(float), sizeof(float));
            memcpy(&player_data->rotation.w, c->recv_buffer.data() + 1 + 6 * sizeof(float), sizeof(float));
            memcpy(&player_data->rotation.x, c->recv_buffer.data() + 1 + 7 * sizeof(float), sizeof(float));
            memcpy(&player_data->rotation.y, c->recv_buffer.data() + 1 + 8 * sizeof(float), sizeof(float));
            memcpy(&player_data->rotation.z, c->recv_buffer.data() + 1 + 9 * sizeof(float), sizeof(float));
            memcpy(&player_data->shot_harpoon, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 0 * sizeof(bool), sizeof(bool));
            memcpy(&player_data->grab, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 1 * sizeof(bool), sizeof(bool));

            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 10 * sizeof(float) + 2 * sizeof(bool));
            //std::cout << "Received (" << player_data->position.x << ", " << player_data->position.y << ", " << player_data->position.z << "), etc..." << std::endl;
          }
        }
      }
    }, 0.01);

    //based on https://stackoverflow.com/a/14391562
    auto now = std::chrono::high_resolution_clock::now();
    float diff = std::chrono::duration_cast<std::chrono::duration<float>>(now - then).count();
		if (diff > 0.03f) {
			then = now;
      update_server(&state, &player_ledger, diff);
		}
	}
}
