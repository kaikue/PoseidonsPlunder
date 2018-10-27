#include "MainMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <array>
#include <cstddef>
#include <random>
#include <time.h>

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
        if (t->name.find("GM") == std::string::npos){
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
        }
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

void MainMode::spawn_player(uint32_t id)
{
    players_transform[id] = current_scene->new_transform();
    players_transform.at(id)->position = state.players.at(id).position;
    players_transform.at(id)->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

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
            get_transform(state.players.at(player_id).position, state.players.at(player_id).rotation)
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

MainMode::MainMode()
    : state()
{
    player_id = 0;

    state.add_player(player_id, 0);

    for (auto const &elem : state.players) {
        spawn_player(elem.first);
    }

    {
        camera->transform->set_parent(players_transform.at(player_id));
        camera->transform->set_transform(state.camera_offset_to_player);

        // camera rotated according to game state demands
        camera->transform->rotation *= state.players.at(player_id).rotation;
        elevation = glm::pitch(camera->transform->rotation);
        azimuth = glm::roll(camera->transform->rotation);
    }

    // spawn treasures on map
    treasures_transform[0]->position = state.treasures[0].position;
    treasures_transform[1]->position = state.treasures[1].position;
}

MainMode::~MainMode()
{
}

bool MainMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
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
        if (evt.type == SDL_MOUSEBUTTONUP) {
            controls.fire = false;
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

void MainMode::update(float elapsed)
{
    glm::mat3 directions = camera->transform->make_local_to_world();
    float amt = 5.0f * elapsed;
    if (controls.right) players_transform.at(player_id)->position += amt * directions[0];
    if (controls.left) players_transform.at(player_id)->position -= amt * directions[0];
    if (controls.back) players_transform.at(player_id)->position += amt * directions[2];
    if (controls.fwd) players_transform.at(player_id)->position -= amt * directions[2];
    if (controls.fire) {
        state.players.at(player_id).shot_harpoon = true;
    }

    static glm::quat cam_to_player_rot = get_pos_rot(state.camera_offset_to_player).second;

    state.players.at(player_id).position = players_transform.at(player_id)->position;
    state.players.at(player_id).rotation =
        glm::inverse(cam_to_player_rot) * glm::quat(glm::vec3(elevation, -azimuth, 0.0f));

    state.update(elapsed);

    // update gun position & rotation
    for (auto const &pair : state.players) {
        {
            glm::mat4 gun_to_world =
                get_transform(state.players.at(pair.first).position, state.players.at(pair.first).rotation)
                    * state.gun_offset_to_player;
            guns_transform.at(pair.first)->set_transform(gun_to_world);
        }

        players_transform.at(pair.first)->position = state.players.at(pair.first).position;
        harpoons_transform.at(pair.first)->position = state.harpoons.at(pair.first).position;
        harpoons_transform.at(pair.first)->rotation = state.harpoons.at(pair.first).rotation;
    }

    treasures_transform[0]->position = state.treasures[0].position;
    treasures_transform[1]->position = state.treasures[1].position;

}

void MainMode::draw(glm::uvec2 const &drawable_size)
{
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

void MainMode::draw_message(std::string message, float y)
{
    float height = 0.06f;
    float width = text_width(message, height);
    draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void MainMode::show_pause_menu()
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
