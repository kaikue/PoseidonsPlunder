#pragma once

#include "Mode.hpp"
#include "Connection.hpp"
#include "GameState.hpp"

#include <functional>
#include <vector>
#include <string>

struct LobbyMode : public Mode {
	LobbyMode(Client &client);
	virtual ~LobbyMode() { }

	virtual bool handle_event(SDL_Event const &event, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	struct Choice {
		Choice(std::string const &label_, std::function< void() > on_select_ = nullptr) : label(label_), on_select(on_select_) { }
		std::string label;
		std::function< void() > on_select;
		//height / padding give item height and padding relative to a screen of height 2:
		float height = 0.1f;
		float padding = 0.01f;
	};
	std::vector< Choice > choices;
	uint32_t selected = 0;
	float bounce = 0.0f;

	//will render this mode in the background if not null:
	std::shared_ptr< Mode > background;
	float background_time_scale = 1.0f;
	float background_fade = 0.5f;

	int team = 0;

	char nickname[Player::NICKNAME_LENGTH];
	int player_id = 0;
	int player_count = 0;
	std::vector<int> player_teams; //TODO: can this be made an int[]?

	void switch_team();

	void change_nickname();

	void start_game();
	
	//------ networking ------
	Client &client; //client object; manages connection to server.

	void send_lobby_info(Connection *c);

	void send_start(Connection *c);

	void poll_server();

};
