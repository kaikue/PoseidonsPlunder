#include "GameState.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "data_path.hpp" //helper to get paths relative to executable
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/string_cast.hpp>

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
        std::cout << m << std::endl;

        if (t->name == "Player") {
            auto *object = new btCollisionObject();
            object->setWorldTransform(
                    btTransform(btQuaternion(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w),
                                btVector3(t->position.x, t->position.y, t->position.z)));
            auto *sphere = new btSphereShape(player_sphere_radius);
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
        }


    });

}

void GameState::add_player(uint32_t id) {
    glm::vec3 player_up = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 player_at = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 player_right = glm::vec3(1.0f, 0.0f, 0.0f);

    float elev_offset = std::atan2f(
            std::sqrtf(player_up.x * player_up.x + player_up.y * player_up.y),
            player_up.z);

    glm::vec3 position = player_at;
    glm::quat rotation = glm::angleAxis(elev_offset + float(M_PI_2), player_right);

    auto *object = new btCollisionObject();
    object->setWorldTransform(
            btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                        btVector3(position.x, position.y, position.z)));
    auto *sphere = new btSphereShape(player_sphere_radius);
    object->setCollisionShape(sphere);
    bt_collision_world->addCollisionObject(object);

    player_collisions[id] = object;
    players[id] = {position, glm::vec3(0.0f, 0.0f, 0.0f), rotation, 0, false, false, false, false, false, "test"};
}

void GameState::update(float time) {

    for (auto const &pair : player_collisions) {
        glm::vec3 position = players.at(pair.first).position;
        glm::quat rotation = players.at(pair.first).orientation;
        pair.second->setWorldTransform(
                btTransform(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w),
                            btVector3(position.x, position.y, position.z)));

        std::cout << "player position: " << glm::to_string(position) << std::endl;
    }

    //Perform collision detection
    bt_collision_world->performDiscreteCollisionDetection();

    int numManifolds = bt_collision_world->getDispatcher()->getNumManifolds();
//For each contact manifold
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold* contactManifold = bt_collision_world->getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject* obA = contactManifold->getBody0();
        const btCollisionObject* obB = contactManifold->getBody1();
        contactManifold->refreshContactPoints(obA->getWorldTransform(), obB->getWorldTransform());
        int numContacts = contactManifold->getNumContacts();
        //For each contact point in that manifold
        for (int j = 0; j < numContacts; j++) {
            //Get the contact information
            btManifoldPoint& pt = contactManifold->getContactPoint(j);
            btVector3 ptA = pt.getPositionWorldOnA();
            btVector3 ptB = pt.getPositionWorldOnB();
            double ptdist = pt.getDistance();
            std::cout << ptA.x() << ", " << ptA.y() << ", " << ptA.z() << ", " << ptB << ", " << ptdist << std::endl;
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
