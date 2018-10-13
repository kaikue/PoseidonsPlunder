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

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <time.h>

Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() {
  return new MeshBuffer(data_path("phone-bank.pnc"));
});

Load< GLuint > phonebank_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(phonebank_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< Sound::Sample > sample_ring(LoadTagDefault, [](){
	return new Sound::Sample(data_path("ring.wav"));
});
Load< Sound::Sample > sample_loop(LoadTagDefault, [](){
	return new Sound::Sample(data_path("music.wav"));
});

MainMode::MainMode() {
	//----------------
	//set up scene:

  mt = std::mt19937((unsigned int)time(0));
  responses.push_back("HELLO");
  responses.push_back("GOODBYE");
  responses.push_back("UHH");
  responses.push_back("WHAT");

	auto attach_object = [this](Scene::Transform *transform, std::string const &name) {
		Scene::Object *object = scene.new_object(transform);
		object->program = vertex_color_program->program;
		object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
		object->vao = *phonebank_meshes_for_vertex_color_program;
		MeshBuffer::Mesh const &mesh = phonebank_meshes->lookup(name);
		object->start = mesh.start;
		object->count = mesh.count;
		return object;
	};

  std::ifstream blob(data_path("phone-bank.scene"), std::ios::binary);
  //Scene file format:
  // str0 len < char > *[strings chunk]
  // xfh0 len < ... > *[transform hierarchy]
  // msh0 len < uint uint uint >[hierarchy point + mesh name]
  // cam0 len < uint params >[hierarchy point + camera params]
  // lmp0 len < uint params >[hierarchy point + light params]

  std::vector< char > names;
  read_chunk(blob, "str0", &names);

  struct NameRef {
    uint32_t begin; //index into names
    uint32_t end; //index into names
  };
  static_assert(sizeof(NameRef) == 8, "NameRef should be packed.");

  struct TransformInfo {
    int parent_ref; //index into transforms of parent
    NameRef name; //name of transform
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
  };
  static_assert(sizeof(TransformInfo) == 52, "TransformInfo should be packed.");

  std::vector<TransformInfo> transform_infos;
  read_chunk(blob, "xfh0", &transform_infos);

  struct MeshInfo {
    int hierarchy_ref; //index into transforms
    NameRef mesh_name; //name of mesh- index into names
  };
  static_assert(sizeof(MeshInfo) == 12, "MeshInfo should be packed.");

  std::vector< MeshInfo > mesh_infos;
  read_chunk(blob, "msh0", &mesh_infos);

  int phone_i = 0;
  std::map<int, Scene::Transform*> transforms;
  for (MeshInfo const &mesh_info : mesh_infos) {
    int current_ref = mesh_info.hierarchy_ref;
    TransformInfo transform_info = transform_infos[current_ref];
    Scene::Transform *transform = scene.new_transform();
    transforms[current_ref] = transform;
    transform->position = transform_info.position;
    transform->rotation = transform_info.rotation;
    transform->scale = transform_info.scale;
    if (transform_info.parent_ref >= 0) {
      transform->set_parent(transforms[transform_info.parent_ref]);
    }
    std::string mesh_name = std::string(names.begin() + mesh_info.mesh_name.begin, names.begin() + mesh_info.mesh_name.end);
    Scene::Object *obj_mesh = attach_object(transform, mesh_name);
    if (mesh_name.compare("Phone") == 0) {
      Phone phone;
      phone.obj = obj_mesh;
      char c = 'A' + phone_i;
      phone.name = std::string(1, c);
      phone_i++;
      phones.push_back(phone);
    }
  }

  { //Camera looking at the origin:
    Scene::Transform *transform = scene.new_transform();
    transform->position = glm::vec3(0.0f, -10.0f, 1.0f);
    //Cameras look along -z, so rotate view to look at origin:
    transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    camera = scene.new_camera(transform);
  }

	//start the 'loop' sample playing at the camera:
	loop = sample_loop->play(camera->transform->position, 1.0f, Sound::Loop);
  phone_ring();
}

MainMode::~MainMode() {
	if (loop) loop->stop();
}

bool MainMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//handle tracking the state of WSAD for movement control:
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}

  if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
    interact();
    return true;
  }

	//handle tracking the mouse for rotation control:
	if (!mouse_captured) {
		if (evt.type == SDL_MOUSEBUTTONDOWN) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
			return true;
		}
	} else if (mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      //SDL_SetRelativeMouseMode(SDL_FALSE);
      //mouse_captured = false;
      show_pause_menu();
			return true;
		}
		if (evt.type == SDL_MOUSEMOTION) {
			//Note: float(window_size.y) * camera->fovy is a pixels-to-radians conversion factor
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
			yaw = -yaw;
			pitch = -pitch;
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}
	return false;
}

void MainMode::interact() {
  for (Phone phone : phones) {
    if (close_to_player(phone)) {
      interact_phone(phone);
      break;
    }
  }
}

bool MainMode::close_to_player(Phone phone) {
  float distance = glm::distance(phone.obj->transform->position, camera->transform->position);
  return distance <= interact_distance;
}

void MainMode::interact_phone(Phone phone) {
  bool ringing = phone.name.compare(ringing_phone) == 0;
  if (ringing) {
    if (ring) ring->stop();
    bool hello = mt() % 2 == 0;
    if (hello) {
      num_merits++;
      show_phone_message("JUST CHECKING IN", "", true);
    }
    else {
      do {
        phone_to_call = get_random_phone();
      } while (phone_to_call.compare(ringing_phone) == 0);

      int r = mt() % responses.size();
      correct_response = responses[r];
      must_call = true;

      show_phone_message("CALL PHONE " + phone_to_call, "SAY " + correct_response, false);
    }

  }
  else {
    get_response(phone);
  }
}

void MainMode::show_phone_message(std::string message1, std::string message2, bool ring_next) {
  std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();
  std::shared_ptr< Mode > game = shared_from_this();
  menu->background = game;

  menu->choices.emplace_back(message1);
  menu->choices.emplace_back(message2);

  menu->choices.emplace_back("OK", [this, ring_next, game]() {
    Mode::set_current(game);
    if (ring_next) {
      phone_ring();
    }
    check_score();
  });

  menu->selected = 2;

  Mode::set_current(menu);
}

void MainMode::get_response(Phone phone) {
  
  std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();
  std::shared_ptr< Mode > game = shared_from_this();
  menu->background = game;

  for (std::string &response : responses) {
    menu->choices.emplace_back(response, [this, response, phone, game]() {
      if (phone.name.compare(phone_to_call) == 0 && response.compare(correct_response) == 0) {
        num_merits++;
      }
      else {
        num_strikes++;
      }
      Mode::set_current(game);

      must_call = false;

      check_score();

      phone_ring();
    });
  }

  menu->choices.emplace_back("");
  menu->choices.emplace_back("CANCEL", [this, game]() {
    Mode::set_current(game);
    phone_ring();
  });

  menu->selected = 0;

  Mode::set_current(menu);
}

std::string MainMode::get_random_phone() {
  size_t num_phones = phones.size();
  int phone_i = mt() % num_phones;
  char c = 'A' + phone_i;
  return std::string(1, c);
}

void MainMode::check_score() {
  if (num_merits >= max_merits) {
    end_game(true);
  }
  else if (num_strikes >= max_strikes) {
    end_game(false);
  }
}

void MainMode::end_game(bool won) {
  std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();
  std::shared_ptr< Mode > game = shared_from_this();
  menu->background = game;

  std::string message = won ? "CONGRATULATIONS" : "YOU LOSE";

  menu->choices.emplace_back(message);
  menu->choices.emplace_back("");

  menu->choices.emplace_back("EXIT", [game]() {
    Mode::set_current(nullptr);
  });

  menu->selected = 2;

  Mode::set_current(menu);

}

void MainMode::update(float elapsed) {
	glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
	float amt = 5.0f * elapsed;
	if (controls.right) camera->transform->position += amt * directions[0];
	if (controls.left) camera->transform->position -= amt * directions[0];
	if (controls.backward) camera->transform->position += amt * directions[2];
	if (controls.forward) camera->transform->position -= amt * directions[2];

	{ //set sound positions:
		glm::mat4 cam_to_world = camera->transform->make_local_to_world();
		Sound::listener.set_position( cam_to_world[3] );
		//camera looks down -z, so right is +x:
		Sound::listener.set_right( glm::normalize(cam_to_world[0]) );

		if (loop) {
			glm::mat4 camera_to_world = camera->transform->make_local_to_world();
			loop->set_position(camera_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}
	}

  if (Mode::current.get() == this) {
    if (!must_call) {
      phone_countdown -= elapsed;
      if (phone_countdown <= 0.0f) {
        num_strikes++;
        check_score();

        phone_ring();
      }
    }
  }
}

void MainMode::phone_ring() {
  phone_countdown = ring_time;

  //make random phone ring (not phone to call or previously ringing phone)

  Phone phone;
  do {
    size_t num_phones = phones.size();
    int phone_i = mt() % num_phones;
    phone = phones[phone_i];
  } while (phone.name.compare(ringing_phone) == 0 || phone.name.compare(phone_to_call) == 0);

  ringing_phone = phone.name;
  glm::mat4 phone_to_world = phone.obj->transform->make_local_to_world();

  if (ring) ring->stop();
  ring = sample_ring->play(phone_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void MainMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene.draw(camera);

	if (Mode::current.get() == this) {
    
		glDisable(GL_DEPTH_TEST);
		if (!mouse_captured) {
      std::string message = "CLICK TO GRAB MOUSE";
      draw_message(message, -1.0f);
		}
    
    std::string strikes_message = "STRIKES REMAINING ";
    for (uint32_t i = 0; i < max_strikes - num_strikes; i++) {
      strikes_message += "X";
    }
    for (uint32_t i = 0; i < num_strikes; i++) {
      strikes_message += "O";
    }
    draw_message(strikes_message, -0.9f);
    
    std::string merits_message = "MERITS ";
    for (uint32_t i = 0; i < max_merits - num_merits; i++) {
      merits_message += "X";
    }
    for (uint32_t i = 0; i < num_merits; i++) {
      merits_message += "O";
    }
    draw_message(merits_message, 0.9f);

    for (Phone phone : phones) {
      if (close_to_player(phone)) {
        std::string phone_message = "PHONE " + phone.name;
        draw_message(phone_message, -0.5f);
        break;
      }
    }

		glUseProgram(0);
	}

	GL_ERRORS();
}

void MainMode::draw_message(std::string message, float y) {
  float height = 0.06f;
  float width = text_width(message, height);
  draw_text(message, glm::vec2(-0.5f * width, y + 0.01f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
  draw_text(message, glm::vec2(-0.5f * width, y), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void MainMode::show_pause_menu() {
	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;

	menu->choices.emplace_back("PAUSED");
	menu->choices.emplace_back("");
	menu->choices.emplace_back("RESUME", [game](){
		Mode::set_current(game);
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 2;

	Mode::set_current(menu);
}
