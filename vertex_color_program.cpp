#include "vertex_color_program.hpp"

#include "compile_program.hpp"

VertexColorProgram::VertexColorProgram() {
	program = compile_program(R"(
#version 330
uniform mat4 object_to_clip;
uniform mat4 object_to_light;
uniform mat3 normal_to_light;
layout(location=0) in vec4 Position; //note: layout keyword used to make sure that the location-0 attribute is always bound to something
in vec3 Normal;
in vec4 Color;
out vec4 position;
out vec3 normal;
out vec4 color;
void main() {
	gl_Position = object_to_clip * Position;
	position = object_to_light * Position;
	normal = normal_to_light * Normal;
	color = Color;
}
)",
		R"(
#version 330
uniform vec3 sun_direction;
uniform vec3 sun_color;
uniform vec3 sky_direction;
uniform vec3 sky_color;
uniform vec3 view_pos;
in vec4 position;
in vec3 normal;
in vec4 color;
out vec4 fragColor;

const vec3 fogColor = vec3(0.11, 0.26, 0.42);
const float FogDensity = 0.05;

const vec3 waterGradient = vec3(0.91, 1, 1);
const float total_water_depth = 10;
const float gradient_bias = 0.5;
const float min_light = 0.3;

void main() {
	vec3 total_light = vec3(0.0, 0.0, 0.0);
	vec3 n = normalize(normal);
	{ //sky (hemisphere) light:
		vec3 l = sky_direction;
		float nl = 0.5 + 0.5 * dot(n,l);
		total_light += nl * sky_color;
	}
	{ //sun (directional) light:
		vec3 l = sun_direction;
		float nl = max(min_light, dot(n,l));
		total_light += nl * sun_color;
	}
	vec3 light_color = color.rgb * total_light;

	// underwater fog effect
	float dist = length(view_pos - position.xyz);
	float fogFactor = 1.0 /exp( (dist * FogDensity) * (dist * FogDensity));
	fogFactor = clamp( fogFactor, 0.0, 1.0 );
	vec3 final_color = mix(fogColor, light_color, fogFactor);

    // underwater gradient
    vec3 gradient_color = waterGradient * ((position.z / total_water_depth) + gradient_bias);
    final_color = final_color * gradient_color;

	fragColor = vec4(final_color, color.a);
}
)"
	);

	object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");
	object_to_light_mat4 = glGetUniformLocation(program, "object_to_light");
	normal_to_light_mat3 = glGetUniformLocation(program, "normal_to_light");

	sun_direction_vec3 = glGetUniformLocation(program, "sun_direction");
	sun_color_vec3 = glGetUniformLocation(program, "sun_color");
	sky_direction_vec3 = glGetUniformLocation(program, "sky_direction");
	sky_color_vec3 = glGetUniformLocation(program, "sky_color");
	view_pos_vec3 = glGetUniformLocation(program, "view_pos");
}

Load< VertexColorProgram > vertex_color_program(LoadTagInit, [](){
	return new VertexColorProgram();
});
