#include "LobbyMode.hpp"
#include "GameState.hpp"
#include "GameMode.hpp"

#include "Load.hpp"
#include "compile_program.hpp"
#include "MeshBuffer.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <string>

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
		"uniform vec3 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = vec4(color, 1.0);\n"
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

LobbyMode::LobbyMode(Client &client_) : client(client_) {
	background = nullptr;

	choices.emplace_back("* LOBBY *");
	choices.emplace_back("");
	choices.emplace_back("SWITCH TEAM", [this]()
	{
		switch_team();
	});
	choices.emplace_back("RANDOMIZE NAME", [this]()
	{
		change_nickname();
	});
	choices.emplace_back("");
	choices.emplace_back("READY", [this]()
	{
		if (client.connection) {
			send_start(&client.connection);
		}
	});

	/*choices.emplace_back("QUIT", []()
	{
		Mode::set_current(nullptr);
	});*/

	selected = 2;

	
}

void LobbyMode::send_lobby_info(Connection *c) {
	if (c) {
		c->send('n'); //team and name
		c->send(team);
		c->send_raw(nickname, Player::NICKNAME_LENGTH); //NICKNAME_LENGTH is constant, nickname should be padded if necessary
	}
}

void LobbyMode::send_start(Connection *c) {
	if (c) {
		c->send('k'); //ok to start game
	}
}

void LobbyMode::switch_team() {
	team = 1 - team;
	if (client.connection) {
		send_lobby_info(&client.connection);
	}
}

void LobbyMode::change_nickname() {
	std::string nick = "asdfghjkasdfghj"; //TODO
	strcpy(nickname, nick.c_str());
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
						std::cout << "Player count " << player_count << std::endl;
					}
				}
				else if (c->recv_buffer[0] == 't') {
					//team info
					if (c->recv_buffer.size() < 1 + player_count * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int))) {
						return; //wait for more data
					}
					else {
						for (int i = 0; i < player_count; i++) {
							int player_team;
							memcpy(&player_team, c->recv_buffer.data() + 1 + i * sizeof(int), sizeof(int));
							player_teams.push_back(player_team);
							char nickname[Player::NICKNAME_LENGTH];
							memcpy(&nickname, c->recv_buffer.data() + 1 + i * (Player::NICKNAME_LENGTH * sizeof(char) + sizeof(int)) + sizeof(int), Player::NICKNAME_LENGTH * sizeof(char));
							nicknames.push_back(nickname);
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

	//character width and spacing helpers:
	// (...in terms of the menu font's default 3-unit height)
	auto width = [](char a) {
		if (a == 'I') return 1.0f;
		else if (a == 'L') return 2.0f;
		else if (a == 'M' || a == 'W') return 4.0f;
		else return 3.0f;
	};
	auto spacing = [](char a, char b) {
		return 1.0f;
	};

	float select_bounce = std::abs(std::sin(bounce * 3.1515926f * 2.0f));

	float y = 0.5f * total_height;
	for (auto const &choice : choices) {
		y -= choice.padding;
		y -= choice.height;

		bool is_selected = (&choice - &choices[0] == selected);
		std::string label = choice.label;

		if (is_selected) {
			label = "*" + label + "*";
		}

		float total_width = 0.0f;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0) total_width += spacing(label[i-1], label[i]);
			total_width += width(label[i]);
		}
		if (is_selected) {
			total_width += 2.0f * select_bounce;
		}

		float x = -0.5f * total_width;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0) x += spacing(label[i-1], label[i]);
			if (is_selected && (i == 1 || i + 1 == label.size())) {
				x += select_bounce;
			}

			if (label[i] != ' ') {
				float s = choice.height * (1.0f / 3.0f);
				glm::mat4 mvp = projection * glm::mat4(
					glm::vec4(s, 0.0f, 0.0f, 0.0f),
					glm::vec4(0.0f, s, 0.0f, 0.0f),
					glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
					glm::vec4(s * x, y, 0.0f, 1.0f)
				);
				glUniformMatrix4fv(l_menu_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(l_menu_program_color, 1.0f, 1.0f, 1.0f);

				MeshBuffer::Mesh const &mesh = l_menu_meshes->lookup(label.substr(i,1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(label[i]);
		}

		y -= choice.padding;
	}

	glEnable(GL_DEPTH_TEST);
}
