#include "GameState.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "data_path.hpp" //helper to get paths relative to executable
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif // M_PI_2

Load<CollisionMeshBuffer> meshes_for_collision(LoadTagDefault, []() {
    return new CollisionMeshBuffer(data_path("test_level.collision"));
});

GameState::GameState() {
    bt_collision_configuration = new btDefaultCollisionConfiguration();
    bt_dispatcher = new btCollisionDispatcher(bt_collision_configuration);

    auto sscene_size = (btScalar) scene_size;
    btVector3 worldAabbMin(-sscene_size, -sscene_size, -sscene_size);
    btVector3 worldAabbMax(sscene_size, sscene_size, sscene_size);
    //This is one type of broadphase, bullet has others that might be faster depending on the application
    bt_broadphase = new bt32BitAxisSweep3(worldAabbMin, worldAabbMax, max_objects, 0,
                                          true);  // true for disabling raycast accelerator

    bt_collision_world = new btCollisionWorld(bt_dispatcher, bt_broadphase, bt_collision_configuration);

    Scene level;
    //load all collision meshes
    level.load(data_path("test_level.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m) {
        std::cout << t->name << ", " << m << std::endl;

        if (t->name == "Player") {
            auto *object = new btCollisionObject();
            object->setWorldTransform(
                    btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                                btVector3(t->position.x, t->position.y, t->position.z)));
            auto *sphere = new btSphereShape((btScalar)player_sphere_radius);
            object->setCollisionShape(sphere);
            bt_collision_world->addCollisionObject(object);

        } else if (t->name.find("CL") != std::string::npos) {

            CollisionMeshBuffer::CollisionMesh const &mesh = meshes_for_collision->lookup(m);

            auto *object = new btCollisionObject();
            object->setWorldTransform(
                    btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                                btVector3(t->position.x, t->position.y, t->position.z)));
            btStridingMeshInterface *tri_array = new btTriangleIndexVertexArray((int) mesh.triangle_count,
                                                                                (int *) &(meshes_for_collision->triangles[mesh.triangle_start].x),
                                                                                (int) sizeof(glm::uvec3),
                                                                                (int) mesh.vertex_count,
                                                                                (btScalar *) &(meshes_for_collision->vertices[mesh.vertex_start].x),
                                                                                (int) sizeof(glm::vec3));
            auto *mesh_shape = new btBvhTriangleMeshShape(tri_array, true);
            object->setCollisionShape(mesh_shape);
            bt_collision_world->addCollisionObject(object);
        } else if (t->name == "Gun") {
            gun_offset_to_player = t->make_local_to_parent();
        } else if (t->name == "Harpoon") {
            default_harpoon_offset_to_gun = t->make_local_to_parent();
        }
    });

    //look up the camera:
    for (Scene::Camera *c = level.first_camera; c != nullptr; c = c->alloc_next) {
        if (c->transform->name == "Camera") {
            camera_offset_to_player = c->transform->make_local_to_parent();
        }
    }

}

void GameState::add_player(uint32_t id) {
    glm::vec3 player_at = glm::vec3(0.0f, -14.0f, 2.0f);

    glm::vec3 position = player_at;
    glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);

    // add player collision mesh
    {
        auto *object = new btCollisionObject();
        object->setWorldTransform(
                btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                            btVector3(position.x, position.y, position.z)));
        auto *sphere = new btSphereShape((btScalar)player_sphere_radius);
        object->setCollisionShape(sphere);
        object->setUserIndex(id);
        bt_collision_world->addCollisionObject(object);

        player_collisions[id] = object;
    }

    players[id] = {position, glm::vec3(0.0f, 0.0f, 0.0f), rotation, 0, false, false, false, false, false, "test"};
    player_controls[id] = {false, false, false, false, false, false};
}

void GameState::update(float time) {

    for (auto const &pair : player_collisions) {
        glm::vec3 position = players.at(pair.first).position;
        glm::quat rotation = players.at(pair.first).orientation;

        pair.second->setWorldTransform(
                btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                            btVector3(position.x, position.y, position.z)));

//        std::cout << "player position: " << glm::to_string(position) << std::endl;
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
        int numContacts = contactManifold->getNumContacts();

        uint32_t collision_player_id;
        const btCollisionObject * player_obj;
        bool player_is_A;

        if (players.find(obA->getUserIndex()) != players.end()) {
            collision_player_id = obA->getUserIndex();
            player_obj = obA;
            player_is_A = true;
        } else if (players.find(obB->getUserIndex()) != players.end()) {
            collision_player_id = obB->getUserIndex();
            player_obj = obB;
            player_is_A = false;
        } else {
            continue;
        }

        //For each contact point in that manifold
        for (int j = 0; j < numContacts; j++) {
            //Get the contact information
            btManifoldPoint &pt = contactManifold->getContactPoint(j);
            btVector3 ptA = pt.getPositionWorldOnA();
            btVector3 ptB = pt.getPositionWorldOnB();
            double ptdist = pt.getDistance();
            btVector3 rebound_vec;

            if (player_is_A) {
                rebound_vec = (ptA - ptB) * (btScalar)((ptdist > 0) - (ptdist < 0));
            } else {
                rebound_vec = (ptB - ptA) * (btScalar)((ptdist > 0) - (ptdist < 0));
            }

//            std::cout << "before collision: " << glm::to_string(players.at(collision_player_id).position) << std::endl;
            players.at(collision_player_id).position += glm::vec3(rebound_vec.x(), rebound_vec.y(), rebound_vec.z());
//            std::cout << rebound_vec.x() << ", " << rebound_vec.y() << ", " << rebound_vec.z() << std::endl;
//            std::cout << "after collision: " << glm::to_string(players.at(collision_player_id).position) << std::endl;

        }
    }
}

GameState::~GameState() {

    for (int i = 0; i < bt_collision_world->getNumCollisionObjects(); i++) {

        delete bt_collision_world->getCollisionObjectArray()[i]->getCollisionShape();
        delete bt_collision_world->getCollisionObjectArray()[i];
    }

    delete bt_collision_world;
    delete bt_broadphase;
    delete bt_dispatcher;
    delete bt_collision_configuration;
}
