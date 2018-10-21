#include "GameState.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "data_path.hpp" //helper to get paths relative to executable
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif // M_PI_2

Load<CollisionMeshBuffer> meshes_for_collision(LoadTagDefault, []()
{
    return new CollisionMeshBuffer(data_path("test_level.collision"));
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
    //load all collision meshes
    level.load(data_path("test_level.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m)
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

            CollisionMeshBuffer::CollisionMesh const &mesh = meshes_for_collision->lookup(m);

            auto *object = new btCollisionObject();
            object->setWorldTransform(
                btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                            btVector3(t->position.x, t->position.y, t->position.z)));
            btStridingMeshInterface *tri_array = new btTriangleIndexVertexArray((int) mesh.triangle_count,
                                                                                (int *) &(meshes_for_collision
                                                                                    ->triangles[mesh.triangle_start].x),
                                                                                (int) sizeof(glm::uvec3),
                                                                                (int) mesh.vertex_count,
                                                                                (btScalar *) &(meshes_for_collision
                                                                                    ->vertices[mesh.vertex_start].x),
                                                                                (int) sizeof(glm::vec3));
            auto *mesh_shape = new btBvhTriangleMeshShape(tri_array, true);
            object->setCollisionShape(mesh_shape);
            bt_collision_world->addCollisionObject(object);
        }
        else if (t->name == "Gun") {
            gun_offset_to_player = t->make_local_to_parent();
        }
        else if (t->name == "Harpoon") {
            default_harpoon_offset_to_gun = t->make_local_to_parent();
        }
    });

    default_harpoon_to_player = gun_offset_to_player * default_harpoon_offset_to_gun;

    //look up the camera:
    for (Scene::Camera *c = level.first_camera; c != nullptr; c = c->alloc_next) {
        if (c->transform->name == "Camera") {
            camera_offset_to_player = c->transform->make_local_to_parent();
        }
    }

}

void GameState::add_player(uint32_t id)
{
    glm::vec3 player_at = glm::vec3(0.0f, -14.0f, 2.0f);

    glm::vec3 position = player_at;
    glm::quat rotation = glm::quat(glm::vec3(0.0f, float(M_PI_2), 0.0f));

    glm::mat4 rot = glm::toMat4(rotation);
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 final = trans * rot;

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

    glm::mat4 harpoon_to_world = final * default_harpoon_to_player;
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
        auto *capsule = new btCylinderShapeZ(btVector3(harpoon_radius, 0, 0.5 * harpoon_length));
        harpoon_object->setCollisionShape(capsule);

        // set additional pointer to harpoon for identification
        harpoon_object->setUserIndex(id);
        harpoon_object->setUserPointer((void *) &harpoons.at(id));
        bt_collision_world->addCollisionObject(harpoon_object);
    }

    player_collisions[id] = {player_object, harpoon_object};
    harpoon_time[id] = 0.0f;
}

void GameState::handle_harpoon_collision(const btCollisionObject *harpoon_obj,
                                         const btCollisionObject *other_obj,
                                         const HarpoonCollision type,
                                         const btPersistentManifold *manifold)
{
    auto harpoon = ((Harpoon *) harpoon_obj->getUserPointer());

    if (harpoon->state == 0) {
        // harpoon held in gun, so collision is ignored
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
        if (pair.second.state == 0) {
            // held by player
            glm::vec3 position = players.at(pair.first).position;
            glm::quat rotation = players.at(pair.first).rotation;

            glm::mat4 rot = glm::toMat4(rotation);
            glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 final = trans * rot;

            glm::mat4 harpoon_to_world = final * default_harpoon_to_player;
            auto harpoon_pos_rot = get_pos_rot(harpoon_to_world);

            pair.second.position = harpoon_pos_rot.first;
            pair.second.rotation = harpoon_pos_rot.second;
        }
        else if (pair.second.state == 1) {
            // fired
            pair.second.position += pair.second.velocity * time;
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
