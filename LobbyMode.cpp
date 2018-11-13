#include "LobbyMode.hpp"
#include "GameState.hpp"
#include "GameMode.hpp"
#include "draw_text.hpp"

#include "Load.hpp"
#include "compile_program.hpp"
#include "MeshBuffer.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <string>
#include <iterator>
#include <time.h>

//---------- resources ------------
Load< MeshBuffer > l_menu_meshes(LoadTagInit, [](){
	return new MeshBuffer(data_path("menu.p"));
});


//Uniform locations in menu_program:
GLint l_menu_program_mvp = -1;
GLint l_menu_program_color = -1;

Load< GLuint > l_menu_program(LoadTagInit, [](){
	GLuint *ret = new GLuint(compile_program(
		"#version 330\n"
		"uniform mat4 mvp;\n"
		"in vec4 Position;\n"
		"void main() {\n"
		"	gl_Position = mvp * Position;\n"
		"}\n"
	,
		"#version 330\n"
		"uniform vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	));

	l_menu_program_mvp = glGetUniformLocation(*ret, "mvp");
	l_menu_program_color = glGetUniformLocation(*ret, "color");

	return ret;
});

//Binding for using menu_program on menu_meshes:
Load< GLuint > l_menu_binding(LoadTagDefault, [](){
	return new GLuint(l_menu_meshes->make_vao_for_program(*l_menu_program));
});

GLint l_fade_program_color = -1;

Load< GLuint > l_fade_program(LoadTagInit, [](){
	GLuint *ret = new GLuint(compile_program(
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
		"}\n"
	,
		"#version 330\n"
		"uniform vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	));

	l_fade_program_color = glGetUniformLocation(*ret, "color");

	return ret;
});

//----------------------

std::vector<std::string> read_file(std::string url) {
	//from https://stackoverflow.com/a/15138839
	std::ifstream is(url);
	std::istream_iterator<std::string> start(is), end;
	std::vector<std::string> result(start, end);
	return result;
}

LobbyMode::LobbyMode(Client &client_) : client(client_) {
	background = nullptr;

	choices.emplace_back("* LOBBY *");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("");
	choices.emplace_back("SWITCH TEAM", [this]()
	{
		int t = (team + 1) % GameState::num_teams;
		switch_team(t);
	});
	choices.emplace_back("");
	choices.emplace_back("RANDOMIZE NAME", [this]()
	{
		change_nickname();
	});
	choices.emplace_back("");
	choices.emplace_back("READY", [this]()
	{
		if (client.connection) {
			ready = !ready;
			send_ready(&client.connection);
		}
	});

	/*choices.emplace_back("QUIT", []()
	{
		Mode::set_current(nullptr);
	});*/

	selected = 15;

	names_first = read_file(data_path("names_first.txt"));
	names_second = read_file(data_path("names_second.txt"));

	std::random_device rd;
	gen = std::mt19937(rd());
	dist_name_first = std::uniform_int_distribution<uint32_t>(0, static_cast<uint32_t>(names_first.size()) - 1);
	dist_name_second = std::uniform_int_distribution<uint32_t>(0, static_cast<uint32_t>(names_second.size()) - 1);

	nickname = get_nickname();
	std::cout << "My name is " << nickname << std::endl;

	//TODO: assign starting team based on which team is smaller?

	send_lobby_info(&client.connection);
}

void LobbyMode::send_lobby_info(Connection *c) {
	if (c) {
		c->send('n'); //team and name
		c->send(team);
		//c->send(nickname); //NICKNAME_LENGTH is constant, nickname should be padded if necessary
		c->send_raw(nickname.c_str(), Player::NICKNAME_LENGTH); //each player ID's nickname
	}
}

void LobbyMode::send_ready(Connection *c) {
	if (c) {
		c->send('k'); //ready update
		c->send(ready);
	}
}

void LobbyMode::switch_team(int new_team) {
	team = new_team;
	if (client.connection) {
		send_lobby_info(&client.connection);
	}
}

std::string LobbyMode::get_nickname() {
	uint32_t first_line = dist_name_first(gen);
	uint32_t second_line = dist_name_second(gen);
	std::string first = names_first[first_line];
	std::string second = names_second[second_line];
	std::string name = first + second;
	name.resize(Player::NICKNAME_LENGTH, ' ');
	return name;
}

void LobbyMode::change_nickname() {
	nickname = get_nickname();

	if (client.connection) {
		send_lobby_info(&client.connection);
	}
}

void LobbyMode::start_game() {
	std::shared_ptr<GameMode> game = std::make_shared<GameMode>(client, player_id, player_count, player_teams, nicknames);
	Mode::set_current(game);
}

bool LobbyMode::handle_event(SDL_Event const &e, glm::uvec2 const &window_size) {
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_ESCAPE) {
			Mode::set_current(nullptr);
			return true;
		} else if (e.key.keysym.sym == SDLK_UP) {
			//find previous selectable thing that isn't selected:
			uint32_t old = selected;
			selected -= 1;
			while (selected < choices.size() && !choices[selected].on_select) --selected;
			if (selected >= choices.size()) selected = old;

			return true;
		} else if (e.key.keysym.sym == SDLK_DOWN) {
			//find next selectable thing that isn't selected:
			uint32_t old = selected;
			selected += 1;
			while (selected < choices.size() && !choices[selected].on_select) ++selected;
			if (selected >= choices.size()) selected = old;

			return true;
		} else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
			if (selected < choices.size() && choices[selected].on_select) {
				choices[selected].on_select();
			}
			return true;
		}
	}
	return false;
}

void LobbyMode::update(float elapsed) {
	bounce += elapsed / 0.7f;
	bounce -= std::floor(bounce);

	if (background) {
		background->update(elapsed * background_time_scale);
	}

	poll_server();
}

void LobbyMode::poll_server() {
	//poll server for start command
	client.poll([&](Connection *c, Connection::Event event) {
		if (event == Connection::OnOpen) {
			//probably won't get this.
			std::cout << "Opened connection" << std::endl;
		}
		else if (event == Connection::OnClose) {
			std::cerr << "Lost connection to server." << std::endl;
		}
		else {
			while (!(c->recv_buffer.empty())) {
				assert(event == Connection::OnRecv);
				// game begins
				if (c->recv_buffer[0] == 'u') {
					//lobby update- number of players and this player's ID
					if (c->recv_buffer.size() < 1 + 2 * sizeof(int)) {
						return; //wait for more data
					}
					else {
						memcpy(&player_count, c->recv_buffer.data() + 1 + 0 * sizeof(int), sizeof(int));
						memcpy(&player_id, c->recv_buffer.data() + 1 + 1 * sizeof(int), sizeof(int));
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 2 * sizeof(int));
					}
				}
				else if (c->recv_buffer[0] == 't') {
					//team info
					if (c->recv_buffer.size() < 1 + player_count * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int))) {
						return; //wait for more data
					}
					else {
						player_teams.resize(player_count);
						nicknames.resize(player_count);
						int team_sizes[GameState::num_teams] = { };
						for (int i = 0; i < player_count; i++) {
							int player_team;
							memcpy(&player_team, c->recv_buffer.data() + 1 + i * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int)), sizeof(int));
							player_teams[i] = player_team;
							char *start = c->recv_buffer.data() + 1 + i * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int)) + sizeof(int);
							char *end = start + Player::NICKNAME_LENGTH * sizeof(char);
							std::string nick(start, end);
							nicknames[i] = nick;
							team_sizes[player_team]++;
						}

						if (!checked_teams) {
							//switch to team with lowest size
							int smallest_team = 0;
							for (int i = 0; i < GameState::num_teams; i++) {
								if (team_sizes[i] < team_sizes[smallest_team] - 1) {
									smallest_team = i;
								}
							}
							switch_team(smallest_team);
							checked_teams = true;
						}
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + player_count * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int)));
					}
				}
				else if (c->recv_buffer[0] == 'b') {
					//begin game
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
					start_game();
					return;
				}
			}
		}
	}, 0.01);
}

void draw_item(std::string label, float x_offset, float y, float height, glm::vec4 color, glm::mat4 projection, bool is_selected = false, float select_bounce = 0.0f) {

	//character width and spacing helpers:
	// (...in terms of the menu font's default 3-unit height)
	auto width = [](char a) {
		if (a == 'I' || a == 'i') return 1.0f;
		else if (a == 'L' || a == 'l') return 2.0f;
		else if (a == 'M' || a == 'm' || a == 'W' || a == 'w') return 4.0f;
		else return 3.0f;
	};
	auto spacing = [](char a, char b) {
		return 1.0f;
	};

	float total_width = 0.0f;
	for (uint32_t i = 0; i < label.size(); ++i) {
		if (i > 0) total_width += spacing(label[i - 1], label[i]);
		total_width += width(label[i]);
	}
	if (is_selected) {
		total_width += 2.0f * select_bounce;
	}

	float x = -0.5f * total_width + x_offset;
	for (uint32_t i = 0; i < label.size(); ++i) {
		if (i > 0) x += spacing(label[i - 1], label[i]);
		if (is_selected && (i == 1 || i + 1 == label.size())) {
			x += select_bounce;
		}

		if (label[i] != ' ') {
			float s = height * (1.0f / 3.0f);
			glm::mat4 mvp = projection * glm::mat4(
				glm::vec4(s, 0.0f, 0.0f, 0.0f),
				glm::vec4(0.0f, s, 0.0f, 0.0f),
				glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
				glm::vec4(s * x, y, 0.0f, 1.0f)
			);
			glUniformMatrix4fv(l_menu_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
			glUniform4fv(l_menu_program_color, 1, glm::value_ptr(color));

			MeshBuffer::Mesh const &mesh = l_menu_meshes->lookup(label.substr(i, 1));
			glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
		}

		x += width(label[i]);
	}
}

float x_offset_from_team(int team) {
	float amt = (float)team / (GameState::num_teams - 1); //0 to 1
	return amt - 0.5f;
}

void LobbyMode::draw(glm::uvec2 const &drawable_size) {
	if (background && background_fade < 1.0f) {
		background->draw(drawable_size);

		glDisable(GL_DEPTH_TEST);
		if (background_fade > 0.0f) {
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUseProgram(*l_fade_program);
			glUniform4fv(l_fade_program_color, 1, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, background_fade)));
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glUseProgram(0);
			glDisable(GL_BLEND);
		}
	}
	glDisable(GL_DEPTH_TEST);

	float aspect = drawable_size.x / float(drawable_size.y);
	//scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(1.0f / aspect, 1.0f);
	glm::mat4 projection = glm::mat4(
		glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);

	float total_height = 0.0f;
	for (auto const &choice : choices) {
		total_height += choice.height + 2.0f * choice.padding;
	}

	glUseProgram(*l_menu_program);
	glBindVertexArray(*l_menu_binding);

	float select_bounce = std::abs(std::sin(bounce * 3.1415926f * 2.0f));
	glm::vec4 white = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

	float y = 0.5f * total_height;
	for (auto const &choice : choices) {
		y -= choice.padding;
		y -= choice.height;

		bool is_selected = (&choice - &choices[0] == selected);
		std::string label = choice.label;

		if (is_selected) {
			label = "*" + label + "*";
		}

		glm::vec4 color = white;
		if (&choice == &choices[15]) {
			glm::vec4 green = glm::vec4(0.3f, 1.0f, 0.3f, 1.0f);
			color = ready ? green : white;
		}
		draw_item(label, 0, y, choice.height, color, projection, is_selected, select_bounce);

		y -= choice.padding;
	}

	//draw team nicknames in columns
	{
		std::vector<float> team_ys;
		for (int t = 0; t < GameState::num_teams; t++) {
			float height = 0.1f;
			glm::vec4 color = GameMode::team_colors[t];
			draw_item("TEAM " + std::to_string(t + 1), x_offset_from_team(t) * 40, 0.6f, height, color, projection);
			team_ys.push_back(0.59f);
		}
		
		for (int i = 0; i < player_count; i++) {
			std::string nickname = nicknames[i];
			//trim trailing whitespace
			size_t last = nickname.find_last_not_of(' ');
			nickname = nickname.substr(0, (last + 1));

			float height = 0.07f;
			float padding = 0.01f;

			int team = player_teams[i];
			glm::vec4 color = GameMode::team_colors[team];
			team_ys[team] -= padding;
			team_ys[team] -= height;
			draw_item(nickname, x_offset_from_team(team) * 60, team_ys[team], height, color, projection);
			team_ys[team] -= padding;
		}
	}
	//draw our own nickname
	{
		std::string my_nickname = nickname;
		//trim trailing whitespace
		size_t last = my_nickname.find_last_not_of(' ');
		my_nickname = my_nickname.substr(0, (last + 1));

		glm::vec4 color = GameMode::team_colors[team];

		float height = 0.1f;
		draw_item("Name: " + my_nickname, 0, -0.59f, height, color, projection);
	}

	glEnable(GL_DEPTH_TEST);
}
