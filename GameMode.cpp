#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <array>

Load<MeshBuffer> meshes(LoadTagDefault, []()
{
    return new MeshBuffer(data_path("test_level_complex.pnc"));
});

Load<GLuint> meshes_for_vertex_color_program(LoadTagDefault, []()
{
    return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

static Scene::Lamp *sun = nullptr;

static Scene::Camera *camera = nullptr;

static Scene::Object::ProgramInfo *vertex_color_program_info = nullptr;

static std::string gun_mesh_name;

static std::string harpoon_mesh_name;

static std::string player_mesh_name;

static std::string rope_mesh_name;

static std::array<Scene::Transform *, 2> treasures_transform;

static Scene *current_scene = nullptr;

Load<Scene> scene(LoadTagDefault, []()
{
    Scene *ret = new Scene;
    current_scene = ret;

    //pre-build some program info (material) blocks to assign to each object:
    vertex_color_program_info = new Scene::Object::ProgramInfo;
    vertex_color_program_info->program = vertex_color_program->program;
    vertex_color_program_info->vao = *meshes_for_vertex_color_program;
    vertex_color_program_info->mvp_mat4 = vertex_color_program->object_to_clip_mat4;
    vertex_color_program_info->mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
    vertex_color_program_info->itmv_mat3 = vertex_color_program->normal_to_light_mat3;

    //load transform hierarchy:
    ret->load(data_path("test_level_complex.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m)
    {
        Scene::Object *obj = s.new_object(t);

        obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;

        MeshBuffer::Mesh const &mesh = meshes->lookup(m);
        obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

        obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;

        if (t->name == "Gun") gun_mesh_name = m;
        if (t->name == "Harpoon") harpoon_mesh_name = m;
        if (t->name == "Player") player_mesh_name = m;
        if (t->name == "Rope") rope_mesh_name = m;
    });

    //look up the camera:
    for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
        if (c->transform->name == "Camera") {
            if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
            camera = c;
        }
    }

    if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

    //look up the spotlight:
    for (Scene::Lamp *l = ret->first_lamp; l != nullptr; l = l->alloc_next) {
        if (l->transform->name == "Sun") {
            if (sun) throw std::runtime_error("Multiple 'Sun' objects in scene.");
            if (l->type != Scene::Lamp::Directional) throw std::runtime_error("Lamp 'Sun' is not a sun.");
            sun = l;
        }
    }

    if (!sun) throw std::runtime_error("No 'Sun' spotlight in scene.");

    //look up some empty transform handles:
    for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
        if (t->name == "Treasure1") {
            treasures_transform[0] = t;
        }
        if (t->name == "Treasure2") {
            treasures_transform[1] = t;
        }
    }

    return ret;
});

Player &GameMode::get_own_player()
{
    return state.players.at(player_id);
}


void GameMode::spawn_player(uint32_t id)
{
    state.players[id] = Player();
    state.harpoons[id] = Harpoon();
	std::cout << "Spawn 1" << std::endl;
    players_transform[id] = current_scene->new_transform();
    players_transform.at(id)->position = state.players.at(id).position;
    players_transform.at(id)->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	std::cout << "Spawn 2" << std::endl;
    // only spawn player mesh if not its own
    if (player_id != id) {
        Scene::Object *player_obj = current_scene->new_object(players_transform[id]);

        player_obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;

        MeshBuffer::Mesh const &mesh = meshes->lookup(player_mesh_name);
        player_obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        player_obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

        player_obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        player_obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
    }
	std::cout << "Spawn 3" << std::endl;
    {
        guns_transform[id] = current_scene->new_transform();
		std::cout << "Spawn 3.1" << std::endl;
        glm::mat4 gun_to_world =
            get_transform(get_own_player().position, get_own_player().rotation)
                * state.gun_offset_to_player;
		std::cout << "Spawn 3.1.1" << std::endl;
        guns_transform[id]->set_transform(gun_to_world);
		std::cout << "Spawn 3.2" << std::endl;
        Scene::Object *gun_obj = current_scene->new_object(guns_transform[id]);
		std::cout << "Spawn 3.3" << std::endl;
        gun_obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;
		std::cout << "Spawn 3.4" << std::endl;
        MeshBuffer::Mesh const &mesh = meshes->lookup(gun_mesh_name);
        gun_obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        gun_obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;
		std::cout << "Spawn 3.5" << std::endl;
        gun_obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        gun_obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
    }
	std::cout << "Spawn 4" << std::endl;
    {
        harpoons_transform[id] = current_scene->new_transform();
        harpoons_transform[id]->position = state.harpoons.at(player_id).position;
        harpoons_transform[id]->rotation = state.harpoons.at(player_id).rotation;

        Scene::Object *harpoon_obj = current_scene->new_object(harpoons_transform[id]);

        harpoon_obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;

        MeshBuffer::Mesh const &mesh = meshes->lookup(harpoon_mesh_name);
        harpoon_obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        harpoon_obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

        harpoon_obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        harpoon_obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
    }
	std::cout << "Spawn Done" << std::endl;
}

GameMode::GameMode(Client &client_, int pid, int player_count, std::vector<int> player_teams) : client(client_) {
    player_id = pid;
	std::cout << "Starting " << player_id << std::endl;
	state.player_count = player_count;

	spawn_player(player_id); //spawn ourselves first
    for (int i = 0; i < state.player_count; i++) {
		if (i != player_id) {
			spawn_player(i); //TODO: respect teams
		}
    }
	std::cout << "Starting 2" << std::endl;
    {
        camera->transform->set_parent(players_transform.at(player_id));
        camera->transform->set_transform(state.camera_offset_to_player);

        // camera rotated according to game state demands
        camera->transform->rotation *= get_own_player().rotation;
        elevation = glm::pitch(camera->transform->rotation);
        azimuth = glm::roll(camera->transform->rotation);
    }
	std::cout << "Starting 3" << std::endl;
    // spawn treasures on map
    treasures_transform[0]->position = state.treasures[0].position;
    treasures_transform[1]->position = state.treasures[1].position;

	std::cout << "Starting done" << std::endl;
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    //ignore any keys that are the result of automatic key repeat:
    if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
        return false;
    }
    //handle tracking the state of WSAD for movement control:
    if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
        if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
            controls.fwd = (evt.type == SDL_KEYDOWN);
            return true;
        }
        else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
            controls.back = (evt.type == SDL_KEYDOWN);
            return true;
        }
        else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
            controls.left = (evt.type == SDL_KEYDOWN);
            return true;
        }
        else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
            controls.right = (evt.type == SDL_KEYDOWN);
            return true;
        }
        else if (evt.key.keysym.scancode == SDL_SCANCODE_E) {
            controls.grab = (evt.type == SDL_KEYDOWN);
            return true;
        }
    }

    //handle tracking the mouse for rotation control:
    if (!mouse_captured) {
        if (evt.type == SDL_MOUSEBUTTONDOWN) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            mouse_captured = true;
            return true;
        }
    }
    else if (mouse_captured) {
        if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            //SDL_SetRelativeMouseMode(SDL_FALSE);
            //mouse_captured = false;
            show_pause_menu();
            return true;
        }
        if (evt.type == SDL_MOUSEBUTTONDOWN) {
            controls.fire = true;
            return true;
        }
        if (evt.type == SDL_MOUSEMOTION) {
            // Note: float(window_size.y) * camera->fovy is a pixels-to-radians
            // conversion factor
            float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
            float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;

            azimuth -= yaw;
            elevation -= pitch;

            /// Build a quaternion from euler angles (pitch, yaw, roll), in radians.
            camera->transform->rotation = glm::quat(glm::vec3(elevation, 0.0f, azimuth));
            return true;
        }
    }
    return false;
}

void GameMode::draw_message(std::string message, float y) {
    float height = 0.06f;
    float width = text_width(message, height);
    draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
}

//when in game:
void GameMode::send_action(Connection *c) {
    if (c) {
        c->send('p'); //player update
        //send player ID? or use sockets completely?
        //movement

        glm::vec3 pos = get_own_player().position;
        glm::vec3 vel = get_own_player().velocity;
        glm::quat rot = get_own_player().rotation;
        c->send(pos.x);
        c->send(pos.y);
        c->send(pos.z);
        c->send(vel.x);
        c->send(vel.y);
        c->send(vel.z);
        c->send(rot.w);
        c->send(rot.x);
        c->send(rot.y);
        c->send(rot.z);

        c->send(controls.fire);
        c->send(controls.grab);

        //std::cout << "Sending (" << pos.x << ", " << pos.y << ", " << pos.z << "), etc..." << std::endl;
    }
}

void GameMode::poll_server() {
    //poll server for current game state
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
                if (c->recv_buffer[0] == 'b') {
                    if (c->recv_buffer.size() < 1 + 2 * sizeof(int)) {
                        return; //wait for more data
                    }
                    else {
                        std::cout << "Enter the lobby." << std::endl;
                        memcpy(&state.player_count, c->recv_buffer.data() + 1 + 0 * sizeof(int), sizeof(int));
                        memcpy(&get_own_player().team, c->recv_buffer.data() + 1 + 1 * sizeof(int), sizeof(int));
                        c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 2 * sizeof(int));
                    }
                }
                    // receive info about other players
                else if (c->recv_buffer[0] == 't') {
                    if (c->recv_buffer.size() < 1 + state.player_count * sizeof(int)) {
                        return; //wait for more data
                    }
                    else {
                        std::cout << "receving the info about other players" << std::endl;
                        for (int i = 0; i < state.player_count; i++) {
                            memcpy(&state.players[i].team, c->recv_buffer.data() + 1 + i * sizeof(int), sizeof(int));
                        }
                        c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + state.player_count * sizeof(int));
                    }
                }

                    // in game
                else {
//                    std::cout << "Receiving info..." << std::endl;
                    assert(c->recv_buffer[0] == 's');
					size_t packet_len = 1 + 1 * sizeof(bool) + (state.player_count * 20 + 6) * sizeof(float) + (state.player_count + 2) * sizeof(int);
                    if (c->recv_buffer.size() < packet_len) {
//                        std::cout << "Num players " << state.player_count << std::endl;
//                        std::cout << "Buffer size " << c->recv_buffer.size() << ", should be " << (1 + 1 * sizeof(bool) + (state.player_count * 16 + 6) * sizeof(float) + (state.player_count + 2) * sizeof(int)) << std::endl;
                        return; //wait for more data
                    }
                    else {
//                        std::cout << "Copying state" << std::endl;
                        //if buffer length is more than twice the length of a full update, skip all but the last one
						while (c->recv_buffer.size() >= 2 * packet_len) {
							c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + packet_len);
						}

                        memcpy(&get_own_player().is_shot, c->recv_buffer.data() + 1, sizeof(bool));
                        // update the players and the harpoons
                        for (int i = 0; i < state.player_count; i++) {
                            //TODO: don't update position if it's self and close enough?
                            memcpy(&state.players[i].position.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 0) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].position.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 1) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].position.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 2) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].velocity.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 3) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].velocity.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 4) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].velocity.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 5) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].rotation.w, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 6) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].rotation.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 7) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].rotation.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 8) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
                            memcpy(&state.players[i].rotation.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 9) * sizeof(float) + (i + 0) * sizeof(int), sizeof(float));
//                            std::cout << "Player " << i << ": pos (" << state.players[i].position.x << ", " << state.players[i].position.y << ", " << state.players[i].position.z << "), etc..." << std::endl;

                            memcpy(&state.harpoons[i].state,      c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 10) * sizeof(float) + (i + 0) * sizeof(int), sizeof(int));
                            memcpy(&state.harpoons[i].position.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 10) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].position.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 11) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].position.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 12) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].velocity.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 13) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].velocity.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 14) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].velocity.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 15) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].rotation.w, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 16) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].rotation.x, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 17) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].rotation.y, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 18) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                            memcpy(&state.harpoons[i].rotation.z, c->recv_buffer.data() + 1 + 1 * sizeof(bool) + (i * 20 + 19) * sizeof(float) + (i + 1) * sizeof(int), sizeof(float));
                        }
                        // update treasure pos and state
                        for (int j = 0; j < 2; j++) {
                            memcpy(&state.treasures[j].position.x, c->recv_buffer.data() + 1 + sizeof(bool) + (state.player_count * 20 + j * 3 + 0) * sizeof(float) + (j + state.player_count) * sizeof(int), sizeof(float));
                            memcpy(&state.treasures[j].position.y, c->recv_buffer.data() + 1 + sizeof(bool) + (state.player_count * 20 + j * 3 + 1) * sizeof(float) + (j + state.player_count) * sizeof(int), sizeof(float));
                            memcpy(&state.treasures[j].position.z, c->recv_buffer.data() + 1 + sizeof(bool) + (state.player_count * 20 + j * 3 + 2) * sizeof(float) + (j + state.player_count) * sizeof(int), sizeof(float));
                            memcpy(&state.treasures[j].held_by,    c->recv_buffer.data() + 1 + sizeof(bool) + (state.player_count * 20 + j * 3 + 3) * sizeof(float) + (j + state.player_count) * sizeof(int), sizeof(int));
                        }

                        //1 for 's' char
                        //1 bool for is_shot
                        //player_count * 20 floats for pos(3), vel(3), rot(4), harpoon pos(3), harpoon vel(3), harpoon rotation(4), plus 6 for two treasure pos(3)
                        //player_count ints for harpoon states, plus 2 for two treasure held_by
                        c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + packet_len);
                    }
                }
            }
        }
    }, 0.01);
}

void GameMode::update(float elapsed) {

    glm::mat3 directions = camera->transform->make_local_to_world();

    // only process movement controls if player is not paralyzed
    if (!get_own_player().is_shot) {
        float amt;
        if (state.treasures[0].held_by == player_id || state.treasures[1].held_by == player_id) {
            amt = GameState::slowed_player_speed * elapsed;
        } else {
            amt = GameState::default_player_speed * elapsed;
        }

        if (controls.right) players_transform.at(player_id)->position += amt * directions[0];
        if (controls.left) players_transform.at(player_id)->position -= amt * directions[0];
        if (controls.back) players_transform.at(player_id)->position += amt * directions[2];
        if (controls.fwd) players_transform.at(player_id)->position -= amt * directions[2];
    }

    static glm::quat cam_to_player_rot = get_pos_rot(state.camera_offset_to_player).second;

    get_own_player().position = players_transform.at(player_id)->position;
    get_own_player().rotation =
        glm::inverse(cam_to_player_rot) * glm::quat(glm::vec3(elevation, -azimuth, 0.0f));

    // send player action and position to server
    if (client.connection) {
        send_action(&client.connection);
    }
    controls.fire = false;
    controls.grab = false;

    // server will call this
//    state.update(elapsed);
    poll_server(); //TODO: not every frame?

    // update gun position & rotation
    for (auto const &pair : state.players) {

        players_transform.at(pair.first)->position = state.players.at(pair.first).position;
        harpoons_transform.at(pair.first)->position = state.harpoons.at(pair.first).position;
        harpoons_transform.at(pair.first)->rotation = state.harpoons.at(pair.first).rotation;

        glm::mat4 gun_to_world =
            get_transform(state.players.at(pair.first).position, state.players.at(pair.first).rotation)
                * state.gun_offset_to_player;
        guns_transform.at(pair.first)->set_transform(gun_to_world);
    }

    treasures_transform[0]->position = state.treasures[0].position;
    treasures_transform[1]->position = state.treasures[1].position;

}

void GameMode::draw(glm::uvec2 const &drawable_size) {
    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //set up light position + color:
    glUseProgram(vertex_color_program->program);
    glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
    glUniform3fv(vertex_color_program->sun_direction_vec3, 1,
                 glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
    glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
    glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
    glUseProgram(0);

    //fix aspect ratio of camera
    camera->aspect = drawable_size.x / float(drawable_size.y);

    scene->draw(camera);

    glUseProgram(0);

    GL_ERRORS();
}

void GameMode::show_pause_menu()
{
    std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

    std::shared_ptr<Mode> game = shared_from_this();
    menu->background = game;

    menu->choices.emplace_back("PAUSED");
    menu->choices.emplace_back("");
    menu->choices.emplace_back("RESUME", [game]()
    {
        Mode::set_current(game);
    });
    menu->choices.emplace_back("QUIT", []()
    {
        Mode::set_current(nullptr);
    });

    menu->selected = 2;

    Mode::set_current(menu);
}
