//
// Created by Eric Fang on 11/7/18.
//

#include "Skybox.hpp"
#include "load_save_png.hpp"
#include "Load.hpp"
#include "data_path.hpp"

#include "compile_program.hpp"

#include <array>

#include <glm/gtc/type_ptr.hpp>

static const GLfloat skyboxVertices[108] = {
    // Positions
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

GLuint load_cube_map(std::string const &file_prefix)
{
    static std::array<std::string, 6> faces = {"_right", "_left", "_top", "_bottom", "_front", "_back"};
    static std::string png = ".png";

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    for (uint32_t i = 0; i < faces.size(); i++)
    {
        glm::uvec2 size;
        std::vector<glm::u8vec4> data;
        load_png(file_prefix + faces[i] + png, &size, &data, UpperLeftOrigin);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB,
                     size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

Skybox::SkyboxProgram::SkyboxProgram()
{
    program = compile_program(R"(
#version 330
uniform mat4 object_to_clip;
layout(location=0) in vec4 Position; //note: layout keyword used to make sure that the location-0 attribute is always bound to something
out vec3 TexCoords;

void main() {
	vec4 pos = object_to_clip * Position;
    gl_Position = pos.xyww;
	TexCoords = Position.xyz;
}
)",
                              R"(
#version 330

uniform samplerCube skybox;

in vec3 TexCoords;
out vec4 fragColor;

void main() {
    fragColor = texture(skybox, TexCoords);
}
)"
    );

    object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");

    glUseProgram(program);

    skybox_samplerCube = glGetUniformLocation(program, "skybox");
    glUniform1i(skybox_samplerCube, 0);

    glUseProgram(0);

}

Load<Skybox::SkyboxProgram> skybox_program(LoadTagInit, [](){
    return new Skybox::SkyboxProgram();
});

Skybox::Skybox(std::string const &name)
{
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    skybox = load_cube_map(data_path(name));
}

void Skybox::draw(Scene::Camera const *camera)
{
    // Draw skybox as last
    glDepthFunc(GL_LEQUAL);  // Change depth function so depth test passes when
    // values are equal to depth buffer's content
    glUseProgram(skybox_program->program);
    glm::mat4 world_to_camera =
        glm::mat4(glm::mat3(camera->transform->make_world_to_local()));  // Remove any translation
    // component of the view
    // matrix

    glm::mat4 world_to_clip = camera->make_projection() * glm::rotate(world_to_camera, float(M_PI_2), glm::vec3(1, 0, 0));

    glUniformMatrix4fv(skybox_program->object_to_clip_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip));

    // skybox cube
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    // glUniform1i(glGetUniformLocation(shader.Program, "skybox"), 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS); // Set depth function back to default
}