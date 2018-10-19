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

    GameState state;

    Controls controls;

    bool mouse_captured = false;

    glm::vec3 player_at, player_up, player_right;
    static constexpr float player_to_camera_offset = 0.8f;
    float azimuth, elevation = float(M_PI_2);
    float elev_offset;
};
