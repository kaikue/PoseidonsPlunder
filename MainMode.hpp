#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "GameState.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <random>

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif // M_PI_2


struct MainMode : public Mode {
    MainMode();

    virtual ~MainMode();

    //handle_event is called when new mouse or keyboard events are received:
    // (note that this might be many times per frame or never)
    //The function should return 'true' if it handled the event.
    virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

    //update is called at the start of a new frame, after events are handled:
    virtual void update(float elapsed) override;

    //draw is called after update:
    virtual void draw(glm::uvec2 const &drawable_size) override;

    void draw_message(std::string message, float y);

    //starts up a 'quit/resume' pause menu:
    void show_pause_menu();

    void spawn_player(uint32_t id);

    GameState state;

    struct Controls {
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
};
