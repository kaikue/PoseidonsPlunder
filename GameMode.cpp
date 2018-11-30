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
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffers
#include "vertex_color_program.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <array>
#include <type_traits>
#include <sstream>

glm::vec3 lerp(glm::vec3 start, glm::vec3 end, float t)
{
    return (1 - t) * start + t * end;
}

Load<MeshBuffer> meshes(LoadTagDefault, []()
{
    return new MeshBuffer(data_path("test_level_complex.pnc"));
});

Load<GLuint> meshes_for_vertex_color_program(LoadTagDefault, []()
{
    return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

//used for fullscreen passes:
Load< GLuint > empty_vao(LoadTagDefault, []() {
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindVertexArray(0);
	return new GLuint(vao);
});

Load< GLuint > hit_program(LoadTagDefault, []() {
	GLuint program = compile_program(
		//this draws a triangle that covers the entire screen:
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
		"}\n"
		,
		//NOTE on reading screen texture:
		//texelFetch() gives direct pixel access with integer coordinates, but accessing out-of-bounds pixel is undefined:
		//	vec4 color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
		//texture() requires using [0,1] coordinates, but handles out-of-bounds more gracefully (using wrap settings of underlying texture):
		//	vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex,0));

		"#version 330\n"
		"uniform sampler2D tex;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec2 at = (gl_FragCoord.xy - 0.5 * textureSize(tex, 0)) / textureSize(tex, 0);\n"
		//make tint amount more near the edges and less in the middle:
		"	float tint_amt = max(0.0, length(at));\n"
		"	float blur_amt = 2.0;\n"
		//pick a vector to move in for blur using function inspired by:
		//https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
		"	vec2 ofs = blur_amt * normalize(vec2(\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(12.9898,78.233))),\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(96.3869,-27.5796)))\n"
		"	));\n"
		//do a four-pixel average to blur:
		"	vec4 blur =\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.x,ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.y,ofs.x)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.x,-ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.y,-ofs.x)) / textureSize(tex, 0))\n"
		"	;\n"
		"	float tint_col = clamp(1.0 - tint_amt, 0.0, 1.0);\n"
		"	vec4 tint = vec4(1.0, tint_col, tint_col, 1.0);\n"
		"	fragColor = vec4(blur.rgb * tint.rgb, 1.0);\n"
		"}\n"
	);

	glUseProgram(program);

	glUniform1i(glGetUniformLocation(program, "tex"), 0);

	glUseProgram(0);

	return new GLuint(program);
});

Load< GLuint > nohit_program(LoadTagDefault, []() {
  GLuint program = compile_program(
    //this draws a triangle that covers the entire screen:
    "#version 330\n"
    "void main() {\n"
    "	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
    "}\n"
    ,
    //	vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex,0));

    "#version 330\n"
    "uniform sampler2D color_tex;\n"
		"uniform sampler2D depth_tex;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
     //TODO: Add some antialiasing, maybe?

    //Depth of field- blur when further away
    " float depth = texelFetch(depth_tex, ivec2(gl_FragCoord.xy), 0).r;\n"
    "	float blur_amt = max((depth - 0.99) * 100, 0);\n"
    //pick a vector to move in for blur using function inspired by:
    //https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
    "	vec2 ofs = blur_amt * normalize(vec2(\n"
    "		fract(dot(gl_FragCoord.xy ,vec2(12.9898,78.233))),\n"
    "		fract(dot(gl_FragCoord.xy ,vec2(96.3869,-27.5796)))\n"
    "	));\n"
    //do a four-pixel average to blur:
    "	vec4 blur =\n"
    "		+ 0.25 * texture(color_tex, (gl_FragCoord.xy + vec2(ofs.x,ofs.y)) / textureSize(color_tex, 0))\n"
    "		+ 0.25 * texture(color_tex, (gl_FragCoord.xy + vec2(-ofs.y,ofs.x)) / textureSize(color_tex, 0))\n"
    "		+ 0.25 * texture(color_tex, (gl_FragCoord.xy + vec2(-ofs.x,-ofs.y)) / textureSize(color_tex, 0))\n"
    "		+ 0.25 * texture(color_tex, (gl_FragCoord.xy + vec2(ofs.y,-ofs.x)) / textureSize(color_tex, 0))\n"
    "	;\n"

    //Vignette effect (darken around edges)
    "	vec2 at = (gl_FragCoord.xy - 0.5 * textureSize(color_tex, 0)) / textureSize(color_tex, 0);\n"
    "	float tint_amt = max(0.0, length(at) * 0.7);\n"
    " if (depth < 0.6) tint_amt = 0;\n" //don't vignette text- 0.6 seems to be a good cutoff for text vs. world geometry
    "	float tint_col = clamp(1.0 - tint_amt, 0.0, 1.0);\n"
    "	vec4 tint = vec4(tint_col, tint_col, tint_col, 1.0);\n"
    "	fragColor = vec4(blur.rgb * tint.rgb, 1.0);\n"
    //"	fragColor = vec4(0.1, blur_amt, 0.1, 1.0);\n" //sonar mode
    "}\n"
  );

  glUseProgram(program);

  glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
  glUniform1i(glGetUniformLocation(program, "depth_tex"), 1);

  glUseProgram(0);

  return new GLuint(program);
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

Load< Sound::Sample > sound_loop(LoadTagDefault, [](){
	return new Sound::Sample(data_path("loop.wav"));
});

Load<Scene> scene(LoadTagDefault, []()
{
    Scene *ret = new Scene;
    current_scene = ret;

    //pre-build some program info (material) blocks to assign to each object:
    vertex_color_program_info = new Scene::Object::ProgramInfo;
    vertex_color_program_info->program = vertex_color_program->program;
    vertex_color_program_info->vao = *meshes_for_vertex_color_program;
    vertex_color_program_info->mvp_mat4 = vertex_color_program->object_to_clip_mat4;
    vertex_color_program_info->mv_mat4 = vertex_color_program->object_to_light_mat4;
    vertex_color_program_info->itmv_mat3 = vertex_color_program->normal_to_light_mat3;

    //load transform hierarchy:
    ret->load(data_path("test_level_complex.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m)
    {
        // client doesn't need to load in gameplay objects
        if (t->name.find("GM") != std::string::npos) {
            return;
        }

        if (t->name == "Gun") {
            gun_mesh_name = m;
            return;
        }
        if (t->name == "Harpoon") {
            harpoon_mesh_name = m;
            return;
        }
        if (t->name == "Player") {
            player_mesh_name = m;
            return;
        }
        if (t->name == "Rope") {
            rope_mesh_name = m;
            return;
        }

        Scene::Object *obj = s.new_object(t);

        obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;

        MeshBuffer::Mesh const &mesh = meshes->lookup(m);
        obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

        obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
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

const glm::vec4 GameMode::team_colors[GameState::num_teams] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};

Player &GameMode::get_own_player()
{
    return state.players.at(player_id);
}

void GameMode::spawn_player(uint32_t id, int team, std::string nickname)
{
    state.players[id] = Player();
    state.harpoons[id] = Harpoon();
    state.players[id].team = team;
    memcpy(&state.players[id].nickname, &nickname, Player::NICKNAME_LENGTH * sizeof(char));
    players_transform[id] = current_scene->new_transform();
    players_transform.at(id)->position = state.players.at(id).position;
    players_transform.at(id)->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    sound_loop->play(players_transform.at(player_id)->position, 1.0f, Sound::LoopOrOnce::Loop);

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

    {
        guns_transform[id] = current_scene->new_transform();
        glm::mat4 gun_to_world =
            get_transform(get_own_player().position, get_own_player().rotation)
                * state.gun_offset_to_player;
        guns_transform[id]->set_transform(gun_to_world);

        Scene::Object *gun_obj = current_scene->new_object(guns_transform[id]);
        gun_obj->programs[Scene::Object::ProgramTypeDefault] = *vertex_color_program_info;
        MeshBuffer::Mesh const &mesh = meshes->lookup(gun_mesh_name);
        gun_obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
        gun_obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;
        gun_obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
        gun_obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
    }

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
}

GameMode::GameMode(Client &client_,
                   int pid,
                   int player_count,
                   std::vector<int> player_teams,
                   std::vector<std::string> nicknames)
    : underwater_skybox("textures/underwater_cube_map"), client(client_)
{
    player_id = pid;
    state.player_count = player_count;

    spawn_player(player_id, player_teams[player_id], nicknames[player_id]); //spawn ourselves first
    for (int i = 0; i < state.player_count; i++) {
        if (i != player_id) {
            spawn_player(i, player_teams[i], nicknames[i]);
        }
    }
    {
        camera->transform->set_parent(players_transform.at(player_id));
        camera->transform->set_transform(state.camera_offset_to_player);

        // camera rotated according to game state demands
        elevation = glm::pitch(camera->transform->rotation);
        azimuth = glm::roll(camera->transform->rotation);
    }
    // spawn treasures on map
    treasures_transform[0]->position = state.treasures[0].position;
    treasures_transform[1]->position = state.treasures[1].position;

    // OpenGL setup
    //set up light position + color:
    glUseProgram(vertex_color_program->program);
    glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
    glUniform3fv(vertex_color_program->sun_direction_vec3, 1,
                 glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
    glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2, 0.2, 0.3)));
    glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
    glUseProgram(0);
}

GameMode::~GameMode()
{
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
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
            elevation = glm::clamp(elevation - pitch, 0.0f, static_cast<float>(M_PI));
            /// Build a quaternion from euler angles (pitch, yaw, roll), in radians.
            players_transform.at(player_id)->rotation = glm::inverse(get_pos_rot(state.camera_offset_to_player).second)
                * glm::quat(glm::vec3(elevation, -azimuth, 0.0f));
            return true;
        }
    }
    return false;
}

void GameMode::draw_message(std::string message, float y)
{
    float height = 0.06f;
    float width = text_width(message, height);
    draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
}

//when in game:
void GameMode::send_action(Connection *c)
{
    static_assert(std::is_pod<glm::vec3>::value, "glm::vec3 must be tightly packed");
    static_assert(std::is_pod<glm::quat>::value, "glm::quat must be tightly packed");

    if (c) {
        c->send('p'); //player update
        //send player ID? or use sockets completely?
        //movement

        glm::vec3 pos = get_own_player().position;
        glm::vec3 vel = get_own_player().velocity;
        glm::quat rot = get_own_player().rotation;
        c->send(pos);
        c->send(vel);
        c->send(rot);

        c->send(controls.fire);
        c->send(controls.grab);

        //std::cout << "Sending (" << pos.x << ", " << pos.y << ", " << pos.z << "), etc..." << std::endl;
    }
}

void GameMode::poll_server()
{
    //poll server for current game state
    client.poll([&](Connection *c, Connection::Event event)
                {
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
                                    memcpy(&state.player_count,
                                           c->recv_buffer.data() + 1 + 0 * sizeof(int),
                                           sizeof(int));
                                    memcpy(&get_own_player().team,
                                           c->recv_buffer.data() + 1 + 1 * sizeof(int),
                                           sizeof(int));
                                    c->recv_buffer
                                        .erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 + 2 * sizeof(int));
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
                                        memcpy(&state.players[i].team,
                                               c->recv_buffer.data() + 1 + i * sizeof(int),
                                               sizeof(int));
                                    }
                                    c->recv_buffer.erase(c->recv_buffer.begin(),
                                                         c->recv_buffer.begin() + 1 + state.player_count * sizeof(int));
                                }
                            }

                                // in game
                            else {
//                    std::cout << "Receiving info..." << std::endl;
                                assert(c->recv_buffer[0] == 's');
                                size_t packet_len = 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                    + (state.player_count * 20 + 6) * sizeof(float)
                                    + (state.player_count + 2) * sizeof(int);
                                if (c->recv_buffer.size() < packet_len) {
//                        std::cout << "Num players " << state.player_count << std::endl;
//                        std::cout << "Buffer size " << c->recv_buffer.size() << ", should be " << (1 + 1 * sizeof(bool) + (state.player_count * 16 + 6) * sizeof(float) + (state.player_count + 2) * sizeof(int)) << std::endl;
                                    return; //wait for more data
                                }
                                else {
                                    //if buffer length is more than twice the length of a full update, skip all but the last one
                                    while (c->recv_buffer.size() >= 2 * packet_len) {
                                        c->recv_buffer
                                            .erase(c->recv_buffer.begin(), c->recv_buffer.begin() + packet_len);
                                    }

                                    // update if the player if shot
                                    memcpy(&get_own_player().is_shot, c->recv_buffer.data() + 1, sizeof(bool));

                                    // update current game points
                                    memcpy(&state.current_points,
                                           c->recv_buffer.data() + 1 + 1 * sizeof(bool),
                                           2 * sizeof(uint32_t));

                                    // update the players and the harpoons
                                    for (int i = 0; i < state.player_count; i++) {
                                        //TODO: don't update position if it's self and close enough?
                                        memcpy(&state.players[i].position,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 0) * sizeof(float) + (i + 0) * sizeof(int),
                                               sizeof(glm::vec3));
                                        memcpy(&state.players[i].velocity,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 3) * sizeof(float) + (i + 0) * sizeof(int),
                                               sizeof(glm::vec3));

                                        // only update player rotation if it's another player
                                        if (player_id != i) {
                                            memcpy(&state.players[i].rotation,
                                                   c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                       + (i * 20 + 6) * sizeof(float) + (i + 0) * sizeof(int),
                                                   sizeof(glm::quat));
                                        }

                                        memcpy(&state.harpoons[i].state,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 10) * sizeof(float) + (i + 0) * sizeof(int),
                                               sizeof(int));
                                        memcpy(&state.harpoons[i].position,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 10) * sizeof(float) + (i + 1) * sizeof(int),
                                               sizeof(glm::vec3));
                                        memcpy(&state.harpoons[i].velocity,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 13) * sizeof(float) + (i + 1) * sizeof(int),
                                               sizeof(glm::vec3));
                                        memcpy(&state.harpoons[i].rotation,
                                               c->recv_buffer.data() + 1 + 1 * sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (i * 20 + 16) * sizeof(float) + (i + 1) * sizeof(int),
                                               sizeof(glm::quat));
                                    }
                                    // update treasure pos and state
                                    for (int j = 0; j < 2; j++) {
                                        memcpy(&state.treasures[j].position,
                                               c->recv_buffer.data() + 1 + sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (state.player_count * 20 + j * 3 + 0) * sizeof(float)
                                                   + (j + state.player_count) * sizeof(int),
                                               sizeof(glm::vec3));
                                        memcpy(&state.treasures[j].held_by,
                                               c->recv_buffer.data() + 1 + sizeof(bool) + 2 * sizeof(uint32_t)
                                                   + (state.player_count * 20 + j * 3 + 3) * sizeof(float)
                                                   + (j + state.player_count) * sizeof(int),
                                               sizeof(int));
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

void GameMode::update(float elapsed)
{

    for (uint32_t team = 0; team < state.num_teams; team++) {
        if (state.current_points[team] >= state.max_points) {
            show_game_over_menu();
        }
    }

    glm::mat3 directions = camera->transform->make_local_to_world();

    // only process movement controls if player is not paralyzed
    if (!get_own_player().is_shot) {
        float spd;
        if (state.treasures[0].held_by == player_id || state.treasures[1].held_by == player_id) {
            spd = GameState::slowed_player_speed;
        }
        else {
            spd = GameState::default_player_speed;
        }

        glm::vec3 goalVel = glm::vec3(0, 0, 0);

        if (controls.right) goalVel += spd * directions[0];
        if (controls.left) goalVel -= spd * directions[0];
        if (controls.back) goalVel += spd * directions[2];
        if (controls.fwd) goalVel -= spd * directions[2];

        vel = lerp(vel, goalVel, FRICTION * elapsed);

        players_transform.at(player_id)->position += vel * elapsed;
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
    poll_server(); //TODO: not every frame?

    // update gun position & rotation
    for (auto const &pair : state.players) {

        players_transform.at(pair.first)->position = state.players.at(pair.first).position;
        players_transform.at(pair.first)->rotation = state.players.at(pair.first).rotation;

        // if own harpoon is held by player, update own harpoon
        if (pair.first == player_id && state.harpoons.at(pair.first).state == 0) {
            // held by player
            glm::mat4 harpoon_to_world = get_transform(get_own_player().position, get_own_player().rotation)
                * state.default_harpoon_to_player;
            auto harpoon_pos_rot = get_pos_rot(harpoon_to_world);

            harpoons_transform.at(pair.first)->position = harpoon_pos_rot.first;
            harpoons_transform.at(pair.first)->rotation = harpoon_pos_rot.second;
        }
        else {
            harpoons_transform.at(pair.first)->position = state.harpoons.at(pair.first).position;
            harpoons_transform.at(pair.first)->rotation = state.harpoons.at(pair.first).rotation;
        }

        glm::mat4 gun_to_world =
            get_transform(state.players.at(pair.first).position, state.players.at(pair.first).rotation)
                * state.gun_offset_to_player;
        guns_transform.at(pair.first)->set_transform(gun_to_world);
    }

    glm::vec3 player_pos = get_own_player().position;
    glm::quat player_rot = get_own_player().rotation;
    glm::mat4 treasure_to_world =
        get_transform(player_pos, player_rot) * state.treasure_offset_to_player;
    auto treasure_pos_rot = get_pos_rot(treasure_to_world);

    for (int team = 0; team < state.num_teams; team++) {
        if (state.treasures[team].held_by == player_id) {
            treasures_transform[team]->position = treasure_pos_rot.first;
//            treasures_transform[team]->rotation = treasure_pos_rot.second;
        }
        else {
            treasures_transform[team]->position = state.treasures[team].position;
        }
    }

}

//GameMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers {
	glm::uvec2 size = glm::uvec2(0, 0); //remember the size of the framebuffer

										//This framebuffer is used for fullscreen effects:
	GLuint color_tex = 0;
	GLuint depth_tex = 0;
	GLuint fb = 0;

	void allocate(glm::uvec2 const &new_size) {
		//allocate full-screen framebuffer:
		if (size != new_size) {
			size = new_size;

			if (color_tex == 0) glGenTextures(1, &color_tex);
			glBindTexture(GL_TEXTURE_2D, color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);

      //create a depth-format texture:
      if (depth_tex == 0) glGenTextures(1, &depth_tex);
      glBindTexture(GL_TEXTURE_2D, depth_tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glBindTexture(GL_TEXTURE_2D, 0);
      
      //to bind it to the framebuffer:
      if (fb == 0) glGenFramebuffers(1, &fb);
      glBindFramebuffer(GL_FRAMEBUFFER, fb);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
      check_fb();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}
	}
} fbs;

void GameMode::draw(glm::uvec2 const &drawable_size)
{
    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // setup camera position
    glUseProgram(vertex_color_program->program);
    auto cam_pos_rot = get_pos_rot(camera->transform->make_local_to_world());
    glUniform3fv(vertex_color_program->view_pos_vec3, 1, glm::value_ptr(cam_pos_rot.first));
    glUseProgram(0);

    //fix aspect ratio of camera
    camera->aspect = drawable_size.x / float(drawable_size.y);

	fbs.allocate(drawable_size);

	//Draw scene to off-screen framebuffer:
	glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);
	glViewport(0, 0, drawable_size.x, drawable_size.y);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    scene->draw(camera);

    // only draw score and skybox if this is the foreground
    if (Mode::current == shared_from_this()) {
        // draw ambient skybox
        underwater_skybox.draw(camera);

        std::stringstream score_stream;
        score_stream << "Team 1: " << state.current_points[0] << " * " << "Team 2: " << state.current_points[1];
        draw_message(score_stream.str(), 0.9f);

        {
            // temporary measure to be explicit about which team player is on
            std::stringstream team_stream;
            team_stream << "Team " << (get_own_player().team + 1);
            std::string message = team_stream.str();

            float height = 0.06f;
            draw_text(message, glm::vec2(-0.9f * camera->aspect, -0.9f), height, team_colors[get_own_player().team]);
            draw_text(message,
                      glm::vec2(-0.9f * camera->aspect, -0.9f + 0.01f),
                      height,
                      glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
        }
    }

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_ERRORS();

	//Copy scene from depth/color buffers to screen, performing post-processing effects:
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, fbs.depth_tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fbs.color_tex);
  
  //TODO: copy depth buffer somehow?
  //glBindRenderbuffer(GL_RENDERBUFFER, fbs.depth_rb);

	glUseProgram(get_own_player().is_shot ? *hit_program : *nohit_program);
	glBindVertexArray(*empty_vao);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
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

void GameMode::show_game_over_menu()
{
    std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

    std::shared_ptr<Mode> game = shared_from_this();
    menu->background = game;

    std::stringstream score_stream;
    score_stream << state.current_points[0] << " * " << state.current_points[1];

    menu->choices.emplace_back(score_stream.str());

    if (state.current_points[0] == std::max(state.current_points[0], state.current_points[1])) {
        menu->choices.emplace_back("TEAM 1 WINS");
    }
    else {
        menu->choices.emplace_back("TEAM 2 WINS");
    }

    menu->choices.emplace_back("");
    menu->choices.emplace_back("QUIT", []()
    {
        Mode::set_current(nullptr);
    });

    menu->selected = 3;

    Mode::set_current(menu);
}