#pragma once

#include <glm/glm.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <unordered_map>

struct Player {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::quat orientation;
  int team;
  bool has_treasure_1;
  bool has_treasure_2;
  bool is_shot;
  bool shot_harpoon;
  bool grab;
  std::string nickname;
};

struct Harpoon {
  int player_id; //who fired it- can this just be determined from index in list?
                 //int team; //can be determined from player_id
  int state; //0: held, 1: firing, 2: landed, 3: retracting
  glm::vec3 position; //used when firing, landed, retracting
  glm::vec3 velocity; //used when firing, retracting
};

struct GameState {
  int player_count;
  std::unordered_map<uint32_t, uint32_t> ready_to_start;
  std::unordered_map<uint32_t, Player> players;
  std::unordered_map<uint32_t, glm::vec3> harpoons;
  glm::vec3 treasure_1_loc;
  glm::vec3 treasure_2_loc;

  void update(float time);
};
