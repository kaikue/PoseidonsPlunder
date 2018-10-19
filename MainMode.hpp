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

    GameState state;

    Controls controls;

    bool mouse_captured = false;

    glm::vec3 player_up, player_right;
    Scene::Transform *player_trans = nullptr;
    Scene::Transform *debug_trans = nullptr;
    Scene::Transform *gun_trans = nullptr;
    Scene::Transform *harpoon_trans = nullptr;
    float azimuth, elevation;
};
