#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Connection.hpp"
#include "GameState.hpp"
#include "Scene.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode: public Mode
{
    GameMode(Client &client_, int player_id, int player_count, std::vector<int> player_teams, std::vector<char*> nicknames);
    virtual ~GameMode();

    //handle_event is called when new mouse or keyboard events are received:
    // (note that this might be many times per frame or never)
    //The function should return 'true' if it handled the event.
    virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

    //update is called at the start of a new frame, after events are handled:
    virtual void update(float elapsed) override;

    //draw is called after update:
    virtual void draw(glm::uvec2 const &drawable_size) override;

    void draw_message(std::string message, float y);

    void send_action(Connection *c);

    void poll_server();

    //starts up a 'quit/resume' pause menu:
    void show_pause_menu();

    void spawn_player(uint32_t id, int team, char nickname[Player::NICKNAME_LENGTH]);

    inline Player &get_own_player();

    GameState state;

    struct Controls
    {
        bool fwd = false;   // vv
        bool back = false;  // these 4 only of internal interest
        bool left = false;
        bool right = false; // ^^
        bool fire = false;
        bool grab = false;
    } controls;

    bool mouse_captured = false;

    uint32_t player_id;
    std::unordered_map<uint32_t, Scene::Transform *> players_transform;
    std::unordered_map<uint32_t, Scene::Transform *> guns_transform;
    std::unordered_map<uint32_t, Scene::Transform *> harpoons_transform;
    float azimuth, elevation;

    //------ networking ------
    Client &client; //client object; manages connection to server.
};
