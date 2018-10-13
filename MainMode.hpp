#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

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

  void interact();

  struct Phone {
    Scene::Object* obj;
    std::string name;
  };

  bool close_to_player(Phone phone);

  void interact_phone(Phone phone);

  void check_score();

  void show_phone_message(std::string message1, std::string message2, bool ring_next);

  void end_game(bool won);

  void get_response(Phone phone);

  std::string get_random_phone();

  void phone_ring();

	//starts up a 'quit/resume' pause menu:
	void show_pause_menu();

	struct {
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
	} controls;

	bool mouse_captured = false;
  
	Scene scene;
	Scene::Camera *camera = nullptr;

	Scene::Object *large_crate = nullptr;
	Scene::Object *small_crate = nullptr;

  std::vector<Phone> phones;

  const uint32_t max_strikes = 3;
  uint32_t num_strikes = 0;

  const uint32_t max_merits = 10;
  uint32_t num_merits = 0;

  const float interact_distance = 3.0f;

  bool must_call = false;

  const float ring_time = 20.0f;
  float phone_countdown = ring_time;

  std::string ringing_phone = "";
  std::string phone_to_call = "";
  std::string correct_response = "";

	std::shared_ptr< Sound::PlayingSample > loop;
	std::shared_ptr< Sound::PlayingSample > ring;

  std::vector<std::string> responses;

  std::mt19937 mt;
};
