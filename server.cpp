#include "Connection.hpp"
#include "GameState.hpp"

#include <iostream>
#include <set>
#include <chrono>

//when starting game:
void send_begin(Connection *c, GameState *state, int player_id) {
  if (c) {
    c->send('b'); //begin game- send number of players and that player's ID
    c->send(state->player_count);
    c->send(player_id);

    c->send('t'); //team info
    for (int i = 0, i < state->player_count, i++) {
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

    bool is_shot = state->players[player_id].is_shot(); //whether the player is stunned by a harpoon
    c->send(is_shot);

    //players
    for (int i = 0, i < state->player_count, i++) {
      vec3 pos = state->players[i].position;
      vec3 vel = state->players[i].velocity;
      quat rot = state->players[i].orientation;
      c->send(pos.x);
      c->send(pos.y);
      c->send(pos.z);
      c->send(vel.x);
      c->send(vel.y);
      c->send(vel.z);
      c->send(rot.q1);
      c->send(rot.q2);
      c->send(rot.q3);
      c->send(rot.q4);

      //harpoon
      if (players[i].firing) {
        c->send(true); //harpoon is fired

        vec3 harpoon_pos = state.harpoons[i].pos;
        vec3 harpoon_vel = state.harpoons[i].vel;
        //rotation can be determined from velocity so don't send that
        c->send(pos.x);
        c->send(pos.y);
        c->send(pos.z);
        c->send(vel.x);
        c->send(vel.y);
        c->send(vel.z);

        //harpoon collision happens serverside only- if player and harpoon from different team are too close, that player is shot and the harpoon stops
        //stopped harpoons (after hitting land or player) don't hit anything
        //do retracting harpoons count as hitting things? (god of war)
      }
      else {
        c->send(false); //harpoon isn't fired
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
        c->send(0);
      }
    }
    // treasure
    for (int i = 0; i < 2; i++) {
      vec3 pos = treasure[i].pos;
      int is_held_by = is_holding(players, treasure[i]);  //is_holding checks which player owns the treasure 
      c->send(pos.x);
      c->send(pos.y);
      c->send(pos.z);
      c->send(is_held_by);
    }
  }
}

void update_server(GameState &state, float time) {
  state->update(diff);
  send_state(c1, state);
  send_state(c2, state);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}
	
	Server server(argv[1]);

	GameState state;
  int num_players = 0;

  std::unordered_map< Connection *, player_id > player_ledger;

  auto then = std::chrono::high_resolution_clock::now();

	while (1) {
    //get updates from clients
    server.poll([&](Connection *c, Connection::Event evt) {
      if (evt == Connection::OnOpen) {
        player_ledger.insert(make_pair(c, player_count));
        player_count++;
      }
      else if (evt == Connection::OnClose) {
        //lost connection with player :(
      }
      else {
        assert(evt == Connection::OnRecv);
        uint32_t player_id = player_ledger.find(c)->second;// get player ID corresponding to connection
        Player* player_data = &state.players.find(player_id);
        if (c->recv_buffer[0] == 'k') {
          c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
          ready_to_start.insert(make_pair(player_id, 1));
        }
        else if (c->recv_buffer[0] == 'n') {
          if (c->recv_buffer.size() < 1 + 10 * sizeof(float) + 2 * sizeof(bool)) {
            return; //wait for more data      
          }
          else {
            memcpy(player_data->team, c->recv_buffer.data() + 1 + 0 * sizeof(uint32_t), sizeof(uint32_t));
            memcpy(player_data->nickname, c->recv_buffer.data() + 1 + 1 * sizeof(uint32_t), sizeof(char) * NICKNAME_LENGTH);
            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 1 * sizeof(uint32_t) + 1 * sizeof(char) * NICKNAME_LENGTH);
          }
        }
        else if (c->recv_buffer[0] == 'p') {
          if (c->recv_buffer.size() < 1 + 10 * sizeof(float) + 2 * sizeof(bool)) {
            return; //wait for more data
          }
          else {
            /* vv relocate to server "update"
            // assert((!has_tr_1 && !has_tr_2) || (has_tr_1 != has_tr_2)); // want to enforce {has neither treasure} or {can have one of two treasures}
            */
            memcpy(player_data->position->x, c->recv_buffer.data() + 1 + 0 * sizeof(float), sizeof(float));
            memcpy(player_data->position->y, c->recv_buffer.data() + 1 + 1 * sizeof(float), sizeof(float));
            memcpy(player_data->position->z, c->recv_buffer.data() + 1 + 2 * sizeof(float), sizeof(float));
            memcpy(player_data->velocity->x, c->recv_buffer.data() + 1 + 3 * sizeof(float), sizeof(float));
            memcpy(player_data->velocity->y, c->recv_buffer.data() + 1 + 4 * sizeof(float), sizeof(float));
            memcpy(player_data->velocity->z, c->recv_buffer.data() + 1 + 5 * sizeof(float), sizeof(float));
            memcpy(player_data->orientation->x, c->recv_buffer.data() + 1 + 6 * sizeof(float), sizeof(float));
            memcpy(player_data->orientation->y, c->recv_buffer.data() + 1 + 7 * sizeof(float), sizeof(float));
            memcpy(player_data->orientation->z, c->recv_buffer.data() + 1 + 8 * sizeof(float), sizeof(float));
            memcpy(player_data->orientation->w, c->recv_buffer.data() + 1 + 9 * sizeof(float), sizeof(float));
            memcpy(player_data->shot_harpoon, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 0 * sizeof(bool), sizeof(bool));
            memcpy(player_data->grab, c->recv_buffer.data() + 1 + 10 * sizeof(float) + 1 * sizeof(bool), sizeof(bool));

            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 10 * sizeof(float) + 2 * sizeof(bool));
          }
        }
      }
    }, 0.01);
		
    //based on https://stackoverflow.com/a/14391562
    auto now = std::chrono::high_resolution_clock::now();
    float diff = std::chrono::duration_cast<std::chrono::duration<float>>(now - then).count();
		if (diff > 0.05f) {
			then = now;

      update_server(&state, diff);
		}
	}
}
