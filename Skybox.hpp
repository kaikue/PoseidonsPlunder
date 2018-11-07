//
// Created by Eric Fang on 11/7/18.
//

#ifndef POSEIDONS_PLUNDER_SKYBOX_HPP
#define POSEIDONS_PLUNDER_SKYBOX_HPP

#include "GL.hpp"
#include "Scene.hpp"
#include <iostream>

GLuint load_cube_map(std::string const &file_prefix);

class Skybox
{
public:
    struct SkyboxProgram {
        //opengl program object:
        GLuint program = 0;

        //uniform locations:
        GLuint object_to_clip_mat4 = -1U;

        //texture
        GLuint skybox_samplerCube = -1U;

        SkyboxProgram();
    };

    explicit Skybox(std::string const &name);

    void draw(Scene::Camera const *camera);

private:
    GLuint skybox, skyboxVAO, skyboxVBO;
};


#endif //POSEIDONS_PLUNDER_SKYBOX_HPP
