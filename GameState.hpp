#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <btBulletDynamicsCommon.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

#include "read_chunk.hpp"

struct Translation
{
    //TODO: use this, static_assert the size somewhere
    //Contains position, velocity, and rotation in one convenient struct
    glm::vec3 position;
    glm::vec3 velocity;
    glm::quat rotation;
};

struct Player
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::quat rotation;
    int team;
    bool has_treasure_1;
    bool has_treasure_2;
    bool is_shot;
    bool shot_harpoon;
    bool grab;

    static constexpr int NICKNAME_LENGTH = 16;

    std::string nickname;
};

struct Harpoon
{
    uint32_t player_id; //who fired it- can this just be determined from index in list?
    //int team; //can be determined from player_id
    int state; //0: held, 1: firing, 2: landed, 3: retracting
    glm::vec3 position; //used when firing, landed, retracting
    glm::quat rotation; //used when firing, landed, retracting
    glm::vec3 velocity; //used when firing, retracting
};

struct Treasure
{
    int team;
    glm::vec3 position;
    glm::quat rotation;
    int held_by = -1; //player_id of holding player, or -1 if not held
};

struct CollisionMeshBuffer
{
    std::vector<glm::uvec3> triangles;
    std::vector<glm::vec3> vertices;

    struct CollisionMesh
    {
        uint32_t vertex_start = 0;
        uint32_t vertex_count = 0;
        uint32_t triangle_start = 0;
        uint32_t triangle_count = 0;
    };

    std::unordered_map<std::string, CollisionMesh> meshes;

    CollisionMeshBuffer(std::string filename)
    {
        std::ifstream file(filename, std::ios::binary);

        static_assert(sizeof(glm::vec3) == 3 * 4, "vec3 is packed.");
        static_assert(sizeof(glm::uvec3) == 3 * 4, "uvec3 is packed.");

        std::vector<glm::vec3> normals;

        read_chunk(file, "p...", &vertices);
        read_chunk(file, "n...", &normals);
        read_chunk(file, "tri0", &triangles);

        std::vector<char> strings;
        read_chunk(file, "str0", &strings);

        { //read index chunk, add to meshes:
            struct IndexEntry
            {
                uint32_t name_begin, name_end;
                uint32_t vertex_begin, vertex_end;
                uint32_t triangle_begin, triangle_end;
            };

            std::vector<IndexEntry> index;
            read_chunk(file, "idxA", &index);

            for (auto const &entry : index) {
                if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
                    throw std::runtime_error("index entry has out-of-range name begin/end");
                }
                if (!(entry.vertex_begin <= entry.vertex_end && entry.vertex_end <= vertices.size())) {
                    throw std::runtime_error("index entry has out-of-range vertex start/count");
                }
                if (!(entry.triangle_begin <= entry.triangle_end && entry.triangle_end <= triangles.size())) {
                    throw std::runtime_error("index entry has out-of-range triangle start/count");
                }
                std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
                CollisionMesh mesh;
                mesh.vertex_start = entry.vertex_begin;
                mesh.vertex_count = entry.vertex_end - entry.vertex_begin;
                mesh.triangle_start = entry.triangle_begin;
                mesh.triangle_count = entry.triangle_end - entry.triangle_begin;
                bool inserted = meshes.insert(std::make_pair(name, mesh)).second;
                if (!inserted) {
                    std::cerr << "WARNING: mesh name '" + name + "' in filename '" + filename
                        + "' collides with existing mesh." << std::endl;
                }
            }
        }

        if (file.peek() != EOF) {
            std::cerr << "WARNING: trailing data in mesh file '" << filename << "'" << std::endl;
        }
    }

    const CollisionMesh &lookup(std::string const &name) const
    {
        auto f = meshes.find(name);
        if (f == meshes.end()) {
            throw std::runtime_error("Looking up mesh '" + name + "' that doesn't exist.");
        }
        return f->second;
    }
};

/**
 *
 * @param transform
 * @return position and rotation normalized with scale
 */
inline std::pair<glm::vec3, glm::quat> get_pos_rot(const glm::mat4 transform)
{
    glm::vec3 position, scale, skew;
    glm::vec4 persp;
    glm::quat rotation;
    glm::decompose(transform, scale, rotation, position, skew, persp);
    return {position / scale, rotation};
}

/**
 * inverse of the above
 * @param position
 * @param rotation
 * @return transform matrix
 */
inline glm::mat4 get_transform(const glm::vec3 position, const glm::quat rotation)
{
    glm::mat4 rot = glm::toMat4(rotation);
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
    return trans * rot;
}

struct GameState
{
public:
    static constexpr uint32_t num_teams = 2;

    int player_count = 0;
    std::unordered_map<uint32_t, uint32_t> ready_to_start;
    std::unordered_map<uint32_t, Player> players;
    std::unordered_map<uint32_t, Harpoon> harpoons;
    Treasure treasures[num_teams];
    uint32_t current_points[num_teams] = {0, 0};

    static constexpr float default_player_speed = 6.0f;
    static constexpr float slowed_player_speed = 4.0f;
    static constexpr uint32_t max_points = 3;
    glm::mat4 gun_offset_to_player, default_harpoon_offset_to_gun, camera_offset_to_player, default_harpoon_to_player,
        treasure_offset_to_player;

    GameState();

    void add_player(uint32_t id, uint32_t team, std::string nickname);

    void add_treasure(uint32_t team);

    ~GameState();

    void update(float time);


private:
    // private game state members
    static constexpr float dist_before_retract = 15.0f;
    static constexpr float time_before_grab_retract = 1.0f;
    static constexpr float harpoon_vel = 12.0f;
    static constexpr float treasure_spawn_radius = 1.0f;
    static constexpr float time_before_treasure_return = 30.0f;
    static constexpr float player_shot_timeout = 5.0f;
    static constexpr float water_depth = 10.0f;

    // determines how close the player has to be to be able to grab the treasure
    static constexpr double player_reach = 1.5;

    std::unordered_map<uint32_t, float> harpoons_grab_timer;
    std::unordered_map<uint32_t, float> player_shot_timer;
    glm::vec3 team_spawns_pos[num_teams];
    glm::quat team_spawns_rot[num_teams];
    glm::vec3 treasure_spawns[num_teams];
    btVector3 treasure_dims = {0.5f, 0.5f, 0.5f};
    float treasure_timeout[num_teams] = {0.0f, 0.0f};
    // float test_treasure_drop_time = 5.0f;
    // bullet related members
    static constexpr double scene_size = 500;
    static constexpr unsigned int max_objects = 16000;
    static constexpr double player_sphere_radius = 0.9;
    static constexpr double player_capsule_radius = 0.2;
    static constexpr double player_capsule_height = 1.2;
    static constexpr double harpoon_length = 0.5;
    static constexpr double harpoon_radius = 0.01;

    btCollisionConfiguration *bt_collision_configuration;
    btCollisionDispatcher *bt_dispatcher;
    btBroadphaseInterface *bt_broadphase;
    btCollisionWorld *bt_collision_world;

    std::unordered_map<std::string, btBvhTriangleMeshShape *> collision_meshes;
    std::unordered_map<uint32_t, std::pair<btCollisionObject *, btCollisionObject *>> player_collisions;
    btCollisionObject *treasure_collisions[2];
    btVector3 bounds_min, bounds_max;

    enum class HarpoonCollision
    {
        Harpoon, Player, Other
    };
    enum class PlayerCollision
    {
        Player, Other
    };

    void generate_bounds();

    void handle_harpoon_collision(const btCollisionObject *harpoon_obj,
                                  const btCollisionObject *other_obj,
                                  const HarpoonCollision type,
                                  const btPersistentManifold *manifold);

    void handle_player_collision(const btCollisionObject *player_obj,
                                 const btCollisionObject *other_obj,
                                 const GameState::PlayerCollision type,
                                 const btPersistentManifold *manifold,
                                 bool A_is_player);
    void treasure_drop_collision(const btCollisionObject *treasure_obj,
                                 const btCollisionObject *other_obj,
                                 const btPersistentManifold *manifold,
                                 bool treasure_is_0,
                                 bool A_is_treasure);
    bool treasure_0_is_dropping = false;
    bool treasure_1_is_dropping = false;
    bool treasure_0_collide = false;
    bool treasure_1_collide = false;
};
