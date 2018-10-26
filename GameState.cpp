#include "GameState.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "data_path.hpp" //helper to get paths relative to executable
#include <iostream>

#include <glm/gtx/string_cast.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif // M_PI_2

Load<CollisionMeshBuffer> meshes_for_collision(LoadTagDefault, []()
{
    return new CollisionMeshBuffer(data_path("test_level_complex.collision"));
});

GameState::GameState()
{
    bt_collision_configuration = new btDefaultCollisionConfiguration();
    bt_dispatcher = new btCollisionDispatcher(bt_collision_configuration);

    auto sscene_size = (btScalar) scene_size;
    btVector3 worldAabbMin(-sscene_size, -sscene_size, -sscene_size);
    btVector3 worldAabbMax(sscene_size, sscene_size, sscene_size);
    //This is one type of broadphase, bullet has others that might be faster depending on the application
    bt_broadphase = new bt32BitAxisSweep3(worldAabbMin, worldAabbMax, max_objects, nullptr,
                                          true);  // true for disabling raycast accelerator

    bt_collision_world = new btCollisionWorld(bt_dispatcher, bt_broadphase, bt_collision_configuration);

    Scene level;
    //load all collision meshes and gameplay transforms
    level.load(data_path("test_level_complex.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m)
    {
        std::cout << t->name << ", " << m << std::endl;

        if (t->name == "Player") {
            auto *object = new btCollisionObject();
            object->setWorldTransform(
                btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                            btVector3(t->position.x, t->position.y, t->position.z)));
            auto *sphere = new btSphereShape((btScalar) player_sphere_radius);
            object->setCollisionShape(sphere);
            bt_collision_world->addCollisionObject(object);

        }
        else if (t->name.find("CL") != std::string::npos) {

            auto *object = new btCollisionObject();
            object->setWorldTransform(
                btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                            btVector3(t->position.x, t->position.y, t->position.z)));
            btScaledBvhTriangleMeshShape *scaled_mesh_shape;

            if (collision_meshes.find(m) == collision_meshes.end()) {
                CollisionMeshBuffer::CollisionMesh const &mesh = meshes_for_collision->lookup(m);
                btStridingMeshInterface *tri_array = new btTriangleIndexVertexArray((int) mesh.triangle_count,
                                                                                    (int *) &(meshes_for_collision
                                                                                        ->triangles[mesh.triangle_start]
                                                                                        .x),
                                                                                    (int) sizeof(glm::uvec3),
                                                                                    (int) meshes_for_collision->vertices
                                                                                        .size(),
                                                                                    (btScalar *) &(meshes_for_collision
                                                                                        ->vertices[0].x),
                                                                                    (int) sizeof(glm::vec3));
                auto *mesh_shape = new btBvhTriangleMeshShape(tri_array, true);
                collision_meshes[m] = mesh_shape;

                scaled_mesh_shape =
                    new btScaledBvhTriangleMeshShape(mesh_shape, btVector3(t->scale.x, t->scale.y, t->scale.z));
            }
            else {
                std::cout << "identical mesh found" << std::endl;
                auto *mesh_shape = collision_meshes.at(m);
                scaled_mesh_shape =
                    new btScaledBvhTriangleMeshShape(mesh_shape, btVector3(t->scale.x, t->scale.y, t->scale.z));
            }

            object->setCollisionShape(scaled_mesh_shape);
            object->setUserPointer(scaled_mesh_shape);
            bt_collision_world->addCollisionObject(object);

        }
        else if (t->name == "Gun") {
            gun_offset_to_player = t->make_local_to_parent();
        }
        else if (t->name == "Harpoon") {
            default_harpoon_offset_to_gun = t->make_local_to_parent();
        }
        else if (t->name.find("GM") != std::string::npos) {
            if (t->name == "GM_Spawn_Team1") {
                team_spawns_pos[0] = t->position;
                team_spawns_rot[0] = t->rotation;
            }
            if (t->name == "GM_Spawn_Team2") {
                team_spawns_pos[1] = t->position;
                team_spawns_rot[1] = t->rotation;
            }
        }
    });

    for (Scene::Transform *t = level.first_transform; t != nullptr; t = t->alloc_next) {
        if (t->name == "Treasure1") {
            treasure_spawns[0] = t->position;
            treasures[0].team = 0;
            treasures[0].position = t->position;
        }
        if (t->name == "Treasure2") {
            treasure_spawns[1] = t->position;
            treasures[1].team = 1;
            treasures[1].position = t->position;
        }
    }

    default_harpoon_to_player = gun_offset_to_player * default_harpoon_offset_to_gun;

    //look up the camera:
    for (Scene::Camera *c = level.first_camera; c != nullptr; c = c->alloc_next) {
        if (c->transform->name == "Camera") {
            camera_offset_to_player = c->transform->make_local_to_parent();
        }
    }

}

void GameState::add_player(uint32_t id, uint32_t team)
{
    glm::vec3 player_at = team_spawns_pos[team];

    glm::vec3 position = player_at;
    glm::quat rotation = team_spawns_rot[team];

    players[id] = {position, glm::vec3(0.0f, 0.0f, 0.0f), rotation, 0, false, false, false, false, false, "test"};

    // add player collision mesh
    auto *player_object = new btCollisionObject();
    {
        player_object->setWorldTransform(
            btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                        btVector3(position.x, position.y, position.z)));
        auto *sphere = new btSphereShape((btScalar) player_sphere_radius);
        player_object->setCollisionShape(sphere);

        player_object->setUserIndex(id);
        player_object->setUserPointer((void *) &players.at(id));
        bt_collision_world->addCollisionObject(player_object);
    }

    glm::mat4 harpoon_to_world = get_transform(position, rotation) * default_harpoon_to_player;
    auto harpoon_pos_rot = get_pos_rot(harpoon_to_world);
    harpoons[id] = {id, 0, harpoon_pos_rot.first, harpoon_pos_rot.second, glm::vec3(0.0f)};

    // add harpoon collision mesh
    auto *harpoon_object = new btCollisionObject();
    {
        harpoon_object->setWorldTransform(
            btTransform(btQuaternion(harpoon_pos_rot.second.x,
                                     harpoon_pos_rot.second.y,
                                     harpoon_pos_rot.second.z,
                                     harpoon_pos_rot.second.w),
                        btVector3(harpoon_pos_rot.first.x, harpoon_pos_rot.first.y, harpoon_pos_rot.first.z)));
        auto
            *capsule = new btCylinderShapeZ(btVector3((btScalar) harpoon_radius, 0, (btScalar) (0.5 * harpoon_length)));
        harpoon_object->setCollisionShape(capsule);

        // set additional pointer to harpoon for identification
        harpoon_object->setUserIndex(id);
        harpoon_object->setUserPointer((void *) &harpoons.at(id));
        bt_collision_world->addCollisionObject(harpoon_object);
    }

    player_collisions[id] = {player_object, harpoon_object};
    harpoons_grab_timer[id] = 0.0f;
}

void GameState::handle_harpoon_collision(const btCollisionObject *harpoon_obj,
                                         const btCollisionObject *other_obj,
                                         const HarpoonCollision type,
                                         const btPersistentManifold *manifold)
{
    auto harpoon = ((Harpoon *) harpoon_obj->getUserPointer());

    if (harpoon->state == 0 || harpoon->state == 3) {
        // harpoon held in gun or retracting, so collision is ignored
        return;
    }

    int numContacts = manifold->getNumContacts();

    bool intersects = false;

    for (int i = 0; i < numContacts; i++) {
        //Get the contact information
        const btManifoldPoint &pt = manifold->getContactPoint(i);
        if (pt.getDistance() < 0) {
            intersects = true;
        }
    }

    if (!intersects) {
        // if there is no true collision, return;
        return;
    }

    switch (type) {
        case HarpoonCollision::Harpoon: {
            // harpoon on harpoon collision is ignored
        }
            break;
        case HarpoonCollision::Player: {
            if (other_obj->getUserIndex() == harpoon_obj->getUserIndex()) {
                // harpoon on self collision is ignored
                return;
            }
            else {
                auto player = ((Player *) other_obj->getUserPointer());
                player->is_shot = true;
                // harpoon retract since it hit another player
                harpoon->state = 3;
            }
        }
            break;
        case HarpoonCollision::Other: {
            // harpoon landed on static object
            harpoon->state = 2;
        }
            break;
    }
}

void GameState::handle_player_collision(const btCollisionObject *player_obj,
                                        const btCollisionObject *other_obj,
                                        const GameState::PlayerCollision type,
                                        const btPersistentManifold *manifold,
                                        bool A_is_player)
{
    int numContacts = manifold->getNumContacts();

    //For each contact point in that manifold
    for (int j = 0; j < numContacts; j++) {
        //Get the contact information
        const btManifoldPoint &pt = manifold->getContactPoint(j);
        btVector3 ptA = pt.getPositionWorldOnA();
        btVector3 ptB = pt.getPositionWorldOnB();
        double ptdist = pt.getDistance();
        btVector3 rebound_vec;

        if (ptdist < 0) {
            if (A_is_player) {
                rebound_vec = (ptA - ptB) * (btScalar) ((ptdist > 0) - (ptdist < 0));
            }
            else {
                rebound_vec = (ptB - ptA) * (btScalar) ((ptdist > 0) - (ptdist < 0));
            }

//            std::cout << "before collision: " << glm::to_string(players.at(collision_player_id).position) << std::endl;
            ((Player *) player_obj->getUserPointer())->position +=
                glm::vec3(rebound_vec.x(), rebound_vec.y(), rebound_vec.z());
//            std::cout << rebound_vec.x() << ", " << rebound_vec.y() << ", " << rebound_vec.z() << std::endl;
//            std::cout << "after collision: " << glm::to_string(players.at(collision_player_id).position) << std::endl;

            if (type == PlayerCollision::Player) {
                // handle player on player collision
            }
        }
    }
}

void GameState::update(float time)
{

    // handling harpoon firing event
    for (auto &pair : players) {
        if (pair.second.shot_harpoon) {
            harpoons.at(pair.first).state = 1;
            harpoons.at(pair.first).velocity = glm::normalize(glm::toMat3(pair.second.rotation)[1]) * harpoon_vel;
            pair.second.shot_harpoon = false;
        }
    }

    for (auto const &pair : player_collisions) {
        glm::vec3 position = players.at(pair.first).position;
        glm::quat rotation = players.at(pair.first).rotation;

        // first update player
        pair.second.first->setWorldTransform(
            btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                        btVector3(position.x, position.y, position.z)));

        position = harpoons.at(pair.first).position;
        rotation = harpoons.at(pair.first).rotation;

        // then update harpoon
        pair.second.second->setWorldTransform(
            btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                        btVector3(position.x, position.y, position.z)));

    }

    //Perform collision detection
    bt_collision_world->performDiscreteCollisionDetection();

    int numManifolds = bt_collision_world->getDispatcher()->getNumManifolds();
    //For each contact manifold
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold *contactManifold = bt_collision_world->getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject *obA = contactManifold->getBody0();
        const btCollisionObject *obB = contactManifold->getBody1();
        contactManifold->refreshContactPoints(obA->getWorldTransform(), obB->getWorldTransform());

        bool A_is_player = false;
        bool B_is_player = false;
        bool A_is_harpoon = false;
        bool B_is_harpoon = false;

        if (players.find(obA->getUserIndex()) != players.end()) {
            if (obA->getUserPointer() == &harpoons.at(obA->getUserIndex())) {
                A_is_harpoon = true;
            }
            else {
                A_is_player = true;
            }
        }
        if (players.find(obB->getUserIndex()) != players.end()) {
            if (obB->getUserPointer() == &harpoons.at(obB->getUserIndex())) {
                B_is_harpoon = true;
            }
            else {
                B_is_player = true;
            }
        }

        if (A_is_harpoon || B_is_harpoon) {
            if (A_is_harpoon) {
                if (B_is_harpoon) {
                    handle_harpoon_collision(obA, obB, HarpoonCollision::Harpoon, contactManifold);
                }
                else if (B_is_player) {
                    handle_harpoon_collision(obA, obB, HarpoonCollision::Player, contactManifold);
                }
                else {
                    handle_harpoon_collision(obA, obB, HarpoonCollision::Other, contactManifold);
                }
            }
            else {
                if (A_is_harpoon) {
                    handle_harpoon_collision(obB, obA, HarpoonCollision::Harpoon, contactManifold);
                }
                else if (A_is_player) {
                    handle_harpoon_collision(obB, obA, HarpoonCollision::Player, contactManifold);
                }
                else {
                    handle_harpoon_collision(obB, obA, HarpoonCollision::Other, contactManifold);
                }
            }
        }
        else if (A_is_player || B_is_player) {
            if (A_is_player) {
                if (B_is_player) {
                    handle_player_collision(obA, obB, PlayerCollision::Player, contactManifold, A_is_player);
                }
                else {
                    handle_player_collision(obA, obB, PlayerCollision::Other, contactManifold, A_is_player);
                }
            }
            else {
                if (A_is_player) {
                    handle_player_collision(obB, obA, PlayerCollision::Player, contactManifold, A_is_player);
                }
                else {
                    handle_player_collision(obB, obA, PlayerCollision::Other, contactManifold, A_is_player);
                }
            }
        }
    }

    // handle harpoon position update
    for (auto &pair : harpoons) {
//        std::cout << "harpoon state: " << pair.second.state << ", position: " << glm::to_string(pair.second.position)
//                  << ", rotation: " << glm::to_string(glm::eulerAngles(pair.second.rotation))
//                  << ", velocity: " << glm::to_string(pair.second.velocity) << std::endl;
        if (pair.second.state == 0) {
            // held by player
            glm::mat4 harpoon_to_world = get_transform(players.at(pair.first).position, players.at(pair.first).rotation)
                * default_harpoon_to_player;
            auto harpoon_pos_rot = get_pos_rot(harpoon_to_world);

            pair.second.position = harpoon_pos_rot.first;
            pair.second.rotation = harpoon_pos_rot.second;
        }
        else if (pair.second.state == 1) {
            // fired
            if (glm::distance(pair.second.position, players.at(pair.first).position) >= dist_before_retract) {
                // retract if too far away from player
                pair.second.state = 3;
            }
            else {
                pair.second.position += pair.second.velocity * time;
            }
        }
        else if (pair.second.state == 2) {
            // landed, update timer for grab mechanic
            if (harpoons_grab_timer.at(pair.first) >= time_before_grab_retract) {
                pair.second.state = 3;
                harpoons_grab_timer.at(pair.first) = 0.0f;
            }
            else {
                harpoons_grab_timer.at(pair.first) += time;
            }
        }
        else if (pair.second.state == 3) {
            //retracting state
            glm::mat4 default_gun_tip =
                get_transform(players.at(pair.first).position, players.at(pair.first).rotation)
                    * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f * harpoon_length, 0.0f))
                    * default_harpoon_to_player;
            auto default_gun_tip_pos_rot = get_pos_rot(default_gun_tip);

            glm::mat4 harpoon_back_to_world = get_transform(pair.second.position, pair.second.rotation)
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f * harpoon_length));
            auto harpoon_back_pos_rot = get_pos_rot(harpoon_back_to_world);

            if (glm::distance(default_gun_tip_pos_rot.first, harpoon_back_pos_rot.first) < 0.1) {
                pair.second.state = 0;
            }
            else {
                pair.second.velocity =
                    glm::normalize(default_gun_tip_pos_rot.first - harpoon_back_pos_rot.first) * harpoon_vel;
                pair.second.position += pair.second.velocity * time;
            }
        }
    }
}

GameState::~GameState()
{

    for (int i = 0; i < bt_collision_world->getNumCollisionObjects(); i++) {

        delete bt_collision_world->getCollisionObjectArray()[i]->getCollisionShape();
        delete bt_collision_world->getCollisionObjectArray()[i];
    }

    delete bt_collision_world;
    delete bt_broadphase;
    delete bt_dispatcher;
    delete bt_collision_configuration;
}
