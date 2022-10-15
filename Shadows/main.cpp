#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#ifndef _WIN
#include <windows.h>
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 1;
	_declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static const int ONE_SECOND = 1000 * 1000 * 1000;

static const int MODE_NORMAL = 0;
static const int MODE_PCF = 1;
static const int MODE_PCSS = 2;
static const int MODE_VSM = 3;

static const int SAMPLING_MODE_GRID = 0;
static const int SAMPLING_MODE_POISSON = 1;
static const int SAMPLING_MODE_VOGEL = 2;

static const glm::vec4 NDC_FRUSTUM_CORNER_POINTS[] = {
	glm::vec4(-1, 1, 1, 1),
	glm::vec4(1, 1, 1, 1),
	glm::vec4(-1, -1, 1, 1),
	glm::vec4(1, -1, 1, 1),
	glm::vec4(-1, 1, -1, 1),
	glm::vec4(1, 1, -1, 1),
	glm::vec4(-1, -1, -1, 1),
	glm::vec4(1, -1, -1, 1)
};

struct time_handler_type {
	double current_frame_count = 0.0;
	double fps = 0.0;
	double frame_time_sum = 0.0;
	double frame_time = 0.0;
	double average_frame_time = 0.0;
	std::chrono::time_point<std::chrono::high_resolution_clock> last_moment = std::chrono::high_resolution_clock::now();
	double delta_time = 0.0;
};

struct mesh_type {
	GLuint vao = 0;
	GLsizei index_count = 0;
};

struct renderable_type {
	mesh_type mesh;
	glm::vec3 position = glm::vec3(0.0);
	glm::quat rotation = glm::angleAxis(0.0f, glm::vec3(1.0, 0.0, 0.0));
	glm::vec3 scale = glm::vec3(1.0);
	glm::vec3 diffuse_color = glm::vec3(0.5);
};

struct player_type {
	glm::vec3 position = glm::vec3(0.0, 30.0, 0.0);
	glm::quat rotation = glm::angleAxis(0.0f, glm::vec3(1.0, 0.0, 0.0));
	glm::dvec2 cursor_position = glm::vec2(0.0);
	double move_speed = 50.0;
	double rotation_speed = 0.75;
	glm::vec3 forward = glm::vec3(0.0, 0.0, -1.0);
	glm::vec3 right = glm::vec3(1.0, 0.0, 0.0);
	glm::vec3 up = glm::vec3(0.0, 1.0, 0.0);
	glm::mat4 view = glm::mat4(1.0);
	glm::mat4 projection = glm::mat4(1.0);
};

struct light_type {
	glm::vec3 direction = glm::normalize(glm::vec3(-1.0, -1.0, 1.0));
	glm::vec3 color = glm::vec3(1.0);
	float size = 1.0;
	float distance = 500.0;
	glm::mat4 view = glm::mat4(1.0);
	glm::mat4 projection = glm::mat4(1.0);
};

struct window_type {
	GLFWwindow* handler;
	glm::ivec2 size;
};

struct shadow_map_settings_type {
	int mode = MODE_NORMAL;
	int resolution = 1024;
	float bias = 0.003f;
	float intensity = 0.5;
	bool match_frustums = false;
	float scale = 1.0;
	//sampling
	int sampling_mode = 0;
	int grid_kernel_size = 5;
	int poisson_sample_count = 25;
	int vogel_sample_count = 25;
	int gaussian_kernel_size = 5;
	bool rotate_samples = false;
	//pcss
	float near_plane = 1.0;
	float far_plane = 100.0;
	float frustum_width = 100.0;
	//vsm
	bool vsm_smoothstep_fix = false;
	float vsm_smoothstep_fix_lower_bound = 0.1f;
};

time_handler_type time_handler;
player_type player;
light_type light;
window_type window;
shadow_map_settings_type shadow_map_settings;

GLuint lambertian_program = 0;
GLuint shadow_map_program = 0;
GLuint gaussian_blur_program = 0;

mesh_type quad_mesh;
std::vector<renderable_type> renderables;

GLuint shadow_map_fbo = 0;

GLuint shadow_color_texture = 0;
GLuint shadow_color_texture_2 = 0;
GLuint shadow_depth_texture = 0;

GLFWwindow* create_glfw_window(const std::string& title) {
	glfwSetErrorCallback([](int type, const char* message) {
		std::string error = "";
		switch(type) {
			case GLFW_NOT_INITIALIZED:
				error = "NOT_INITIALIZED";
				break;
			case GLFW_NO_CURRENT_CONTEXT:
				error = "NO_CURRENT_CONTEXT";
				break;
			case GLFW_INVALID_ENUM:
				error = "INVALID_ENUM";
				break;
			case GLFW_INVALID_VALUE:
				error = "INVALID_VALUE";
				break;
			case GLFW_OUT_OF_MEMORY:
				error = "OUT_OF_MEMORY";
				break;
			case GLFW_API_UNAVAILABLE:
				error = "API_UNAVAILABLE";
				break;
			case GLFW_VERSION_UNAVAILABLE:
				error = "VERSION_UNAVAILABLE";
				break;
			case GLFW_PLATFORM_ERROR:
				error = "PLATFORM_ERROR";
				break;
			case GLFW_FORMAT_UNAVAILABLE:
				error = "FORMAT_UNAVAILABLE";
				break;
			default:
				error = "UNKNOWN";
		}
		std::cout << "GLFW, ERROR, HIGH, " << error << " : " << message << std::endl;
	});
	glfwInit();
	auto window_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	auto screen_size = glm::ivec2(window_mode->width, window_mode->height);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	window.size = glm::ivec2(1280, 720);
	auto window_handler = glfwCreateWindow(window.size.x, window.size.y, title.c_str(), nullptr, nullptr);
#else
	window.size = screen_size;
	auto window_handler = glfwCreateWindow(window.size.x, window.size.y, title.c_str(), glfwGetPrimaryMonitor(), nullptr);
#endif
	glfwMakeContextCurrent(window_handler);
	glfwSwapInterval(0);
	return window_handler;
}

void create_window() {
	window.handler = create_glfw_window("Shadow maps example");
}

std::string get_message_source(const GLenum source) {
	switch(source) {
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
		case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER: return "OTHER";
		default: return "UNKNOWN";
	}
}

std::string get_message_type(const GLenum type) {
	switch(type) {
		case GL_DEBUG_TYPE_ERROR: return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
		case GL_DEBUG_TYPE_MARKER: return "MARKER";
		case GL_DEBUG_TYPE_OTHER: return "OTHER";
		default: return "UNKNOWN";
	}
}

std::string get_message_severity(const GLenum severity) {
	switch(severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW: return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
		default: return "UNKNOWN";
	}
}

void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* user_param) {
	auto src_str = get_message_source(source);
	auto type_str = get_message_type(type);
	auto severity_str = get_message_severity(severity);
	std::cout << src_str << ", " << type_str << ", " << severity_str << ", " << id << ": " << message << std::endl;
}

void initialize_opengl() {
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	int flags;
	glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if(flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(message_callback, nullptr);
	}
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void initialize_imgui() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window.handler, true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

GLuint create_shader(const std::string& path, const GLenum type, const std::vector<std::string> defines = {}) {
	std::stringstream stringstream;
	try {
		std::fstream filestream;
		filestream.open(path);
		stringstream << filestream.rdbuf();
		filestream.close();
	} catch(std::fstream::failure& ex) {
		std::cout << "code: " << ex.code() << ", what: " << ex.what() << std::endl;
		exit(1);
	}

	std::string source = "#version 460 core\n";
	for(auto& define : defines) {
		source += "#define " + define + "\n";
	}
	source += stringstream.str();
	auto code = source.c_str();

	GLint shader = glCreateShader(type);
	glObjectLabel(GL_SHADER, shader, path.length(), ("<" + path + ">").c_str());
	glShaderSource(shader, 1, &code, nullptr);
	glCompileShader(shader);
	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if(!result) {
		GLint length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		auto log = new char[length];
		glGetShaderInfoLog(shader, length, nullptr, log);
		std::cout << log << std::endl;
		delete[] log;
	}
	return shader;
}

GLuint create_shader_program(const std::string& vertex_path, const std::string& fragment_path, const std::string& name, const std::vector<std::string> additional_shaders_paths = {}, const std::vector<std::string> defines = {}) {
	auto vertex_shader = create_shader(vertex_path, GL_VERTEX_SHADER, defines);
	auto fragment_shader = create_shader(fragment_path, GL_FRAGMENT_SHADER, defines);
	std::vector<GLuint> additional_shaders = {};
	if(!additional_shaders_paths.empty()) {
		for(auto& shader_path : additional_shaders_paths) {
			auto shader = create_shader(shader_path.c_str(), GL_FRAGMENT_SHADER, defines);
			additional_shaders.push_back(shader);
		}
	}

	auto program = glCreateProgram();
	glObjectLabel(GL_PROGRAM, program, name.length(), name.c_str());
	if(!additional_shaders_paths.empty()) {
		for(auto shader : additional_shaders) {
			glAttachShader(program, shader);
		}
	}
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if(!result) {
		GLint length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		auto log = new char[length];
		glGetProgramInfoLog(program, length, nullptr, log);
		std::cout << log << std::endl;
		delete[] log;
	}
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	for(auto additional_shader : additional_shaders) {
		glDeleteShader(additional_shader);
	}
	return program;
}

void create_shader_programs() {
	glDeleteProgram(lambertian_program);
	glDeleteProgram(shadow_map_program);
	glDeleteProgram(gaussian_blur_program);
	std::vector<std::string> additional_shaders_paths;
	std::vector<std::string> defines = {};
	if(shadow_map_settings.mode == MODE_NORMAL) {
		additional_shaders_paths = {"res/shader/normal_shadow_map.frag"};
	} else if(shadow_map_settings.mode == MODE_PCF || shadow_map_settings.mode == MODE_PCSS) {
		if(shadow_map_settings.mode == MODE_PCF) {
			additional_shaders_paths = {"res/shader/sampling.frag", "res/shader/pcf_shadow_map.frag"};
		} else {
			additional_shaders_paths = {"res/shader/sampling.frag", "res/shader/pcss_shadow_map.frag"};
		}
		if(shadow_map_settings.sampling_mode == SAMPLING_MODE_GRID) {
			defines.push_back("SAMPLING_MODE_GRID 1");
		} else if(shadow_map_settings.sampling_mode == SAMPLING_MODE_POISSON) {
			defines.push_back("SAMPLING_MODE_POISSON 1");
			defines.push_back("POISSON_" + std::to_string(shadow_map_settings.poisson_sample_count) + " 1");
		} else if(shadow_map_settings.sampling_mode == SAMPLING_MODE_VOGEL) {
			defines.push_back("SAMPLING_MODE_VOGEL 1");
		}
	} else if(shadow_map_settings.mode == MODE_VSM) {
		additional_shaders_paths = {"res/shader/sampling.frag", "res/shader/vsm_shadow_map.frag"};
	}
	lambertian_program = create_shader_program("res/shader/lambertian.vert", "res/shader/lambertian.frag", "<lambertian>", additional_shaders_paths, defines);
	auto shadow_map_frag = shadow_map_settings.mode == MODE_VSM ? "res/shader/shadow_map_vsm.frag" : "res/shader/shadow_map.frag";
	shadow_map_program = create_shader_program("res/shader/shadow_map.vert", shadow_map_frag, "<shadow map>");
	gaussian_blur_program = create_shader_program("res/shader/gaussian_blur.vert", "res/shader/gaussian_blur.frag", "<gaussian blur>", {"res/shader/sampling.frag"}, {"GAUSSIAN_" + std::to_string(shadow_map_settings.gaussian_kernel_size) + " 1"});
}

GLuint create_and_attach_vbo(const GLuint vao, const GLuint index, const std::vector<float> data, const std::string& name, const GLuint vertex_size = 3) {
	GLuint vbo;
	glCreateBuffers(1, &vbo);
	glObjectLabel(GL_BUFFER, vbo, name.length(), name.c_str());
	glNamedBufferStorage(vbo, data.size() * sizeof(float), &data[0], GL_NONE);
	glEnableVertexArrayAttrib(vao, index);
	glVertexArrayVertexBuffer(vao, index, vbo, 0, vertex_size * sizeof(float));
	glVertexArrayAttribFormat(vao, index, vertex_size, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, index, index);
	return vbo;
}

GLuint create_and_attach_ebo(const GLuint vao, const std::vector<GLuint> indices, const std::string& name) {
	GLuint ebo;
	glCreateBuffers(1, &ebo);
	glObjectLabel(GL_BUFFER, ebo, name.length(), name.c_str());
	glNamedBufferStorage(ebo, indices.size() * sizeof(GLuint), &indices[0], GL_NONE);
	glVertexArrayElementBuffer(vao, ebo);
	return ebo;
}

GLuint create_vao(const std::string name) {
	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glObjectLabel(GL_VERTEX_ARRAY, vao, name.length(), name.c_str());
	return vao;
}

mesh_type create_quad() {
	std::vector<float> vertices = {
		-1.0, 1.0, 0.0,		//top-left
		1.0, 1.0, 0.0,		//top-right
		-1.0, -1.0, 0.0,	//bottom-left
		1.0, -1.0, 0.0		//bottom-right
	};
	std::vector<float> normals = {
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0
	};
	std::vector<float> uvs = {
		0.0, 1.0,
		1.0, 1.0,
		0.0, 0.0,
		1.0, 0.0
	};
	std::vector<GLuint> indices = {
		0, 2, 3,
		1, 0, 3
	};

	auto vao = create_vao("<quad>");
	create_and_attach_vbo(vao, 0, vertices, "<quad vertex position>");
	create_and_attach_vbo(vao, 1, normals, "<quad vertex normals>");
	create_and_attach_vbo(vao, 2, uvs, "<quad vertex uvs>", 2);
	create_and_attach_ebo(vao, indices, "<quad indices>");

	mesh_type quad;
	quad.vao = vao;
	quad.index_count = 6;
	return quad;
}

mesh_type create_mesh_from_file(const std::string& path) {
	Assimp::Importer importer;
	auto scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
	if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cout << "ASSIMP, ERROR, HIGH, " << path << ": " << importer.GetErrorString() << std::endl;
		exit(1);
	}

	auto ai_meshes = scene->mMeshes;
	auto ai_mesh = ai_meshes[0];
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<GLuint> indices;
	for(unsigned int i = 0; i < ai_mesh->mNumVertices; i++) {
		auto vertex = ai_mesh->mVertices[i];
		vertices.push_back(vertex.x);
		vertices.push_back(vertex.y);
		vertices.push_back(vertex.z);
		if(ai_mesh->HasNormals()) {
			auto normal = ai_mesh->mNormals[i];
			normals.push_back(normal.x);
			normals.push_back(normal.y);
			normals.push_back(normal.z);
		}
	}
	for(int i = 0; i < ai_mesh->mNumFaces; i++) {
		auto face = ai_mesh->mFaces[i];
		for(int j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

	auto vao = create_vao("<" + path + ">");
	create_and_attach_vbo(vao, 0, vertices, "<" + path + " vertex positions>");
	create_and_attach_vbo(vao, 1, normals, "<" + path + " vertex normals>");
	create_and_attach_ebo(vao, indices, "<" + path + " indices>");

	mesh_type mesh;
	mesh.vao = vao;
	mesh.index_count = indices.size();
	return mesh;
}

void create_renderables() {
	auto box_mesh = create_mesh_from_file("res/mesh/box.glb");
	renderable_type box;
	box.mesh = box_mesh;
	box.position = glm::vec3(0.0, 0.0, -30.0);
	renderables.push_back(box);

	renderable_type helmet;
	helmet.mesh = create_mesh_from_file("res/mesh/DamagedHelmet.glb");
	helmet.position = glm::vec3(0.0, 10.0, -50.0);
	helmet.scale = glm::vec3(10.0);
	helmet.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0, 1.0, 0.0)) * glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0, 0.0, 0.0));
	renderables.push_back(helmet);

	renderable_type camera;
	camera.mesh = create_mesh_from_file("res/mesh/AntiqueCamera.glb");
	camera.position = glm::vec3(0.0, 10.0, -65.0);
	renderables.push_back(camera);

	renderable_type camera_2;
	camera_2.mesh = camera.mesh;
	camera_2.position = glm::vec3(-10.0, 0.0, -70.0);
	renderables.push_back(camera_2);

	renderable_type camera_3;
	camera_3.mesh = camera.mesh;
	camera_3.position = glm::vec3(-19.0, -9.0, -75.0);
	renderables.push_back(camera_3);

	renderable_type quad;
	quad_mesh = create_quad();
	quad.mesh = quad_mesh;
	quad.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
	quad.scale = glm::vec3(500.0);
	quad.diffuse_color = glm::vec3(1.0, 0.7, 0.4);
	renderables.push_back(quad);
}

GLuint create_fbo(const std::string& name) {
	GLuint fbo;
	glCreateFramebuffers(1, &fbo);
	glObjectLabel(GL_FRAMEBUFFER, fbo, name.length(), name.c_str());
	return fbo;
}

GLuint create_and_attach_texture(const GLuint fbo, const GLenum attachment, const glm::ivec2 size, const GLenum internal_format, const std::string& name, const bool border) {
	GLuint texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glObjectLabel(GL_TEXTURE, texture, name.length(), name.c_str());
	glTextureStorage2D(texture, 1, internal_format, size.x, size.y);
	if(border) {
		glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		auto border_color = glm::vec4(1.0);
		glTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, &border_color.x);
	} else {
		glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(fbo, attachment, texture, 0);
	return texture;
}

std::string get_fbo_error(const GLenum type) {
	switch(type) {
		case GL_FRAMEBUFFER_UNDEFINED: return "FRAMEBUFFER_UNDEFINED";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: return "FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: return "FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
		case GL_FRAMEBUFFER_UNSUPPORTED: return "FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: return "FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
		default: return "UNKNOWN";
	}
}

void create_render_targets() {
	glDeleteTextures(1, &shadow_color_texture);
	glDeleteTextures(1, &shadow_color_texture_2);
	glDeleteTextures(1, &shadow_depth_texture);
	glDeleteFramebuffers(1, &shadow_map_fbo);
	shadow_color_texture_2 = 0;
	shadow_map_fbo = create_fbo("<shadow map fbo>");
	auto internal_format = shadow_map_settings.mode == MODE_VSM ? GL_RG32F : GL_R32F;
	if(shadow_map_settings.mode == MODE_VSM) {
		shadow_color_texture_2 = create_and_attach_texture(shadow_map_fbo, GL_COLOR_ATTACHMENT0, glm::ivec2(shadow_map_settings.resolution), internal_format, "<shadow map color texture 2>", false);
	}
	shadow_color_texture = create_and_attach_texture(shadow_map_fbo, GL_COLOR_ATTACHMENT0, glm::ivec2(shadow_map_settings.resolution), internal_format, "<shadow map color texture>", shadow_map_settings.mode != MODE_VSM);
	shadow_depth_texture = create_and_attach_texture(shadow_map_fbo, GL_DEPTH_ATTACHMENT, glm::ivec2(shadow_map_settings.resolution), GL_DEPTH_COMPONENT32F, "<shadow map depth texture>", true);
	auto status = glCheckNamedFramebufferStatus(shadow_map_fbo, GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << get_fbo_error(status) << std::endl;
	}
}

void load_uniform_float(const GLuint program, const float value, const std::string& name) {
	auto location = glGetUniformLocation(program, name.c_str());
	if(location == -1) {
		std::cout << name << " doesn't found" << std::endl;
	}
	glUniform1f(location, value);
}

void load_uniform_int(const GLuint program, const int value, const std::string& name) {
	auto location = glGetUniformLocation(program, name.c_str());
	if(location == -1) {
		std::cout << name << " doesn't found" << std::endl;
	}
	glUniform1i(location, value);
}

void load_uniform_vec3(const GLuint program, const glm::vec3 value, const std::string& name) {
	auto location = glGetUniformLocation(program, name.c_str());
	if(location == -1) {
		std::cout << name << " doesn't found" << std::endl;
	}
	glUniform3fv(location, 1, &value[0]);
}

void load_uniform_mat(const GLuint program, const glm::mat4 value, const std::string& name) {
	auto location = glGetUniformLocation(program, name.c_str());
	if(location == -1) {
		std::cout << name << " doesn't found" << std::endl;
	}
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void load_uniform_texture(const GLuint program, const GLuint texture, const std::string& name) {
	auto location = glGetUniformLocation(program, name.c_str());
	if(location == -1) {
		std::cout << name << " doesn't found" << std::endl;
	}
	glBindTextureUnit(0, texture);
	glUniform1i(location, 0);
}

void compute_matrices() {
	player.view = glm::toMat4(player.rotation);
	player.view = glm::translate(player.view, -player.position);

	player.projection = glm::perspective(glm::radians(70.0), 1.0 * window.size.x / window.size.y, 1.0, 100.0);

	if(shadow_map_settings.match_frustums) {
		auto light_rotation = glm::quatLookAt(light.direction, glm::vec3(0.0, 1.0, 0.0));
		auto inverse_view = glm::inverse(player.view);
		auto inverse_projection = glm::inverse(player.projection);
		light.view = glm::inverse(glm::translate(glm::mat4(1.0), -light.direction * light.distance + player.position) * glm::toMat4(light_rotation));
		auto min_distances = glm::vec3(INFINITY);
		auto max_distances = glm::vec3(-INFINITY);
		for(auto ndc_corner_point : NDC_FRUSTUM_CORNER_POINTS) {
			auto light_view_corner_point = inverse_projection * ndc_corner_point;
			light_view_corner_point /= light_view_corner_point.w;
			light_view_corner_point = light.view * inverse_view * light_view_corner_point;
			for(int i = 0; i < 3; i++) {
				min_distances[i] = min(min_distances[i], light_view_corner_point[i]);
				max_distances[i] = max(max_distances[i], light_view_corner_point[i]);
			}
		}
		shadow_map_settings.near_plane = -max_distances[2] - light.distance;
		shadow_map_settings.far_plane = -min_distances[2];
		shadow_map_settings.frustum_width = max_distances[0] - min_distances[0];
		light.projection = glm::ortho(min_distances[0], max_distances[0], min_distances[1], max_distances[1], shadow_map_settings.near_plane, shadow_map_settings.far_plane);
	} else {
		light.view = glm::lookAt(glm::vec3(0.0), light.direction, glm::vec3(0.0, 1.0, 0.0));
		light.view = glm::translate(light.view, glm::vec3(-30.0, -50.0, 90.0));
		shadow_map_settings.frustum_width = 100.0;
		shadow_map_settings.near_plane = 1.0;
		shadow_map_settings.far_plane = 100.0;

		light.projection = glm::ortho(-30.0, 30.0, -30.0, 30.0, 1.0, 130.0);
	}
}

void load_uniforms() {
	load_uniform_mat(lambertian_program, player.view, "u_view");
	load_uniform_mat(lambertian_program, player.projection, "u_projection");

	auto shadow_map = shadow_map_settings.mode == MODE_VSM ? shadow_color_texture : shadow_depth_texture;
	load_uniform_texture(lambertian_program, shadow_map, "u_shadow_map");

	load_uniform_mat(lambertian_program, light.view, "u_light_view");
	load_uniform_mat(lambertian_program, light.projection, "u_light_projection");
	load_uniform_vec3(lambertian_program, light.direction, "u_light_direction");

	load_uniform_vec3(lambertian_program, light.color, "u_light_color");
	load_uniform_float(lambertian_program, shadow_map_settings.intensity, "u_intensity");
	if(shadow_map_settings.mode == MODE_PCF || shadow_map_settings.mode == MODE_PCSS) {
		load_uniform_float(lambertian_program, light.size, "u_light_size");
		load_uniform_float(lambertian_program, shadow_map_settings.rotate_samples, "u_rotate_samples");
		load_uniform_float(lambertian_program, shadow_map_settings.scale, "u_scale");
		if(shadow_map_settings.sampling_mode == SAMPLING_MODE_GRID) {
			load_uniform_int(lambertian_program, shadow_map_settings.grid_kernel_size, "u_kernel_size");
		} else if(shadow_map_settings.sampling_mode == SAMPLING_MODE_VOGEL) {
			load_uniform_int(lambertian_program, shadow_map_settings.vogel_sample_count, "u_vogel_sample_count");
		}
	}
	if(shadow_map_settings.mode == MODE_VSM) {
		load_uniform_float(lambertian_program, shadow_map_settings.vsm_smoothstep_fix, "u_smoothstep_fix");
		load_uniform_float(lambertian_program, shadow_map_settings.vsm_smoothstep_fix_lower_bound, "u_smoothstep_fix_lower_bound");
	} else {
		load_uniform_float(lambertian_program, shadow_map_settings.bias, "u_bias");
	}
	if(shadow_map_settings.mode == MODE_PCSS) {
		load_uniform_float(lambertian_program, shadow_map_settings.near_plane, "u_near_plane");
		load_uniform_float(lambertian_program, shadow_map_settings.far_plane, "u_far_plane");
		load_uniform_float(lambertian_program, shadow_map_settings.frustum_width, "u_frustum_width");
	}
}

void load_shadow_map_uniforms() {
	load_uniform_mat(shadow_map_program, light.view, "u_view");
	load_uniform_mat(shadow_map_program, light.projection, "u_projection");
}

void load_renderable_uniforms(const GLuint program, const renderable_type renderable, const bool color = true) {
	auto model = glm::mat4(1.0);
	model = glm::translate(model, renderable.position);
	model = glm::rotate(model, glm::angle(renderable.rotation), glm::axis(renderable.rotation));
	model = glm::scale(model, renderable.scale);
	load_uniform_mat(program, model, "u_model");
	if(color) {
		load_uniform_vec3(program, renderable.diffuse_color, "u_diffuse_color");
	}
}

void load_gaussian_blur_uniforms(const bool horizontal, const GLuint texture) {
	load_uniform_texture(gaussian_blur_program, texture, "u_image");
	load_uniform_float(gaussian_blur_program, horizontal, "u_horizontal");
	load_uniform_float(gaussian_blur_program, light.size, "u_light_size");
	load_uniform_float(gaussian_blur_program, shadow_map_settings.rotate_samples, "u_rotate_samples");
	load_uniform_float(gaussian_blur_program, shadow_map_settings.scale, "u_scale");
}

void render_shadow_map() {
	glBindFramebuffer(GL_FRAMEBUFFER, shadow_map_fbo);
	glViewport(0, 0, shadow_map_settings.resolution, shadow_map_settings.resolution);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shadow_map_program);
	load_shadow_map_uniforms();
	for(auto& renderable : renderables) {
		load_renderable_uniforms(shadow_map_program, renderable, false);
		glBindVertexArray(renderable.mesh.vao);
		glDrawElements(GL_TRIANGLES, renderable.mesh.index_count, GL_UNSIGNED_INT, 0);
	}

	if(shadow_map_settings.mode == MODE_VSM) {
		glUseProgram(gaussian_blur_program);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		glNamedFramebufferTexture(shadow_map_fbo, GL_COLOR_ATTACHMENT0, shadow_color_texture_2, 0);
		load_gaussian_blur_uniforms(true, shadow_color_texture);
		glBindVertexArray(quad_mesh.vao);
		glDrawElements(GL_TRIANGLES, quad_mesh.index_count, GL_UNSIGNED_INT, 0);

		glNamedFramebufferTexture(shadow_map_fbo, GL_COLOR_ATTACHMENT0, shadow_color_texture, 0);
		load_gaussian_blur_uniforms(false, shadow_color_texture_2);
		glBindVertexArray(quad_mesh.vao);
		glDrawElements(GL_TRIANGLES, quad_mesh.index_count, GL_UNSIGNED_INT, 0);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
}

void render_geometry() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window.size.x, window.size.y);
	glClearColor(0.5, 0.8, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(lambertian_program);
	load_uniforms();
	for(auto& renderable : renderables) {
		load_renderable_uniforms(lambertian_program, renderable);
		glBindVertexArray(renderable.mesh.vao);
		glDrawElements(GL_TRIANGLES, renderable.mesh.index_count, GL_UNSIGNED_INT, 0);
	}
}

void handle_time() {
	auto current_moment = std::chrono::high_resolution_clock::now();
	time_handler.frame_time = std::chrono::duration_cast<std::chrono::duration<float, std::nano>>(current_moment - time_handler.last_moment).count();
	time_handler.delta_time = time_handler.frame_time / ONE_SECOND;
	time_handler.last_moment = current_moment;
	time_handler.frame_time_sum += time_handler.frame_time;
	time_handler.current_frame_count++;
	if(time_handler.frame_time_sum >= ONE_SECOND) {
		time_handler.fps = time_handler.current_frame_count / time_handler.frame_time_sum * ONE_SECOND;
		time_handler.average_frame_time = time_handler.frame_time_sum / time_handler.current_frame_count;
		time_handler.frame_time_sum = 0;
		time_handler.current_frame_count = 0;
	}
}

void handle_rotation_input() {
	glm::dvec2 position;
	glfwGetCursorPos(window.handler, &position.x, &position.y);
	position.x = position.x / window.size.x - 0.5;
	position.y = position.y / window.size.y - 0.5;
	if(glfwGetMouseButton(window.handler, GLFW_MOUSE_BUTTON_RIGHT)) {
		glfwSetInputMode(window.handler, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		float cursor_movement_x = (position.x - player.cursor_position.x) * player.rotation_speed;
		float cursor_movement_y = (position.y - player.cursor_position.y) * player.rotation_speed;
		player.rotation = glm::normalize(player.rotation * glm::angleAxis(cursor_movement_y, player.right) * glm::angleAxis(cursor_movement_x, player.up));
		auto rotation = glm::toMat4(player.rotation);
		player.forward = -glm::normalize(glm::vec3(rotation[0][2], rotation[1][2], rotation[2][2]));
		player.right = glm::normalize(glm::vec3(rotation[0][0], rotation[1][0], rotation[2][0]));
		player.up = glm::normalize(glm::vec3(rotation[0][1], rotation[1][1], rotation[2][1]));
		player.rotation = glm::lookAt(glm::vec3(0.0), player.forward, glm::vec3(0.0, 1.0, 0.0));
	} else {
		glfwSetInputMode(window.handler, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	player.cursor_position.x = position.x;
	player.cursor_position.y = position.y;
}

void handle_movement_input() {
	float delta_speed = player.move_speed * time_handler.delta_time;
	glm::vec3 forward_movement = glm::normalize(player.forward * glm::vec3(1.0, 0.0, 1.0)) * delta_speed;
	glm::vec3 right_movement = glm::normalize(player.right * glm::vec3(1.0, 0.0, 1.0)) * delta_speed;
	glm::vec3 up_movement = glm::vec3(0.0, 1.0, 0.0) * delta_speed;
	if(glfwGetKey(window.handler, GLFW_KEY_W)) {
		player.position += forward_movement;
	}
	if(glfwGetKey(window.handler, GLFW_KEY_A)) {
		player.position -= right_movement;
	}
	if(glfwGetKey(window.handler, GLFW_KEY_S)) {
		player.position -= forward_movement;
	}
	if(glfwGetKey(window.handler, GLFW_KEY_D)) {
		player.position += right_movement;
	}
	if(glfwGetKey(window.handler, GLFW_KEY_R)) {
		player.position += up_movement;
	}
	if(glfwGetKey(window.handler, GLFW_KEY_F)) {
		player.position -= up_movement;
	}
}

void handle_input() {
	if(glfwGetKey(window.handler, GLFW_KEY_ESCAPE)) {
		glfwSetWindowShouldClose(window.handler, GLFW_TRUE);
	}
	handle_rotation_input();
	handle_movement_input();
}

void set_scale() {
	if(shadow_map_settings.mode == MODE_PCF) {
		shadow_map_settings.scale = shadow_map_settings.match_frustums ? 1.0 / 1024.0 : 1.0 / 256.0;
	} else if(shadow_map_settings.mode == MODE_PCSS) {
		shadow_map_settings.scale = shadow_map_settings.match_frustums ? 4.0 : 1.0 / 2.0;
	} else if(shadow_map_settings.mode == MODE_VSM) {
		shadow_map_settings.scale = shadow_map_settings.match_frustums ? 1.0 / 768.0 : 1.0 / 256.0;
	}
}

void render_ui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowBgAlpha(0.35f);
	bool overlay = true;
	ImGui::Begin("Stats", &overlay, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
	ImGui::Text("FPS: %.2f", time_handler.fps);
	ImGui::Text("Frame time: %.2f ms", time_handler.average_frame_time / 1000 / 1000);
	ImGui::End();

	ImGui::Begin("Shadow map settings");
	const char* items[] = {"normal", "PCF", "PCSS", "VSM"};
	if(ImGui::Combo("Type", &shadow_map_settings.mode, items, 4, -1)) {
		set_scale();
		create_shader_programs();
		create_render_targets();
	}
	static int shadow_map_resolution_index = 3;
	const char* shadow_map_resolutions[] = {"128", "256", "512", "1024", "2048", "4096"};
	if(ImGui::Combo("Resolution", &shadow_map_resolution_index, shadow_map_resolutions, 6, -1)) {
		shadow_map_settings.resolution = pow(2, shadow_map_resolution_index + 7);
		create_render_targets();
	}
	ImGui::SliderFloat("Intensity", &shadow_map_settings.intensity, 0.0, 1.0);
	if(shadow_map_settings.mode != MODE_VSM) {
		ImGui::SliderFloat("Bias", &shadow_map_settings.bias, 0.0, 1.0);
	}
	if(ImGui::Checkbox("Match frustums", &shadow_map_settings.match_frustums)) {
		set_scale();
	}
	if(shadow_map_settings.mode != MODE_NORMAL) {
		ImGui::Text("Sampling");
		if(shadow_map_settings.mode == MODE_PCF || shadow_map_settings.mode == MODE_PCSS) {
			if(ImGui::RadioButton("Grid", &shadow_map_settings.sampling_mode, 0)) {
				create_shader_programs();
			}
			ImGui::SameLine();
			if(ImGui::RadioButton("Poisson", &shadow_map_settings.sampling_mode, 1)) {
				create_shader_programs();
			}
			ImGui::SameLine();
			if(ImGui::RadioButton("Vogel", &shadow_map_settings.sampling_mode, 2)) {
				create_shader_programs();
			}
			if(shadow_map_settings.sampling_mode == SAMPLING_MODE_GRID) {
				static int shadow_map_grid_kernel_size_index = 2;
				const char* shadow_map_grid_kernel_sizes[] = {"1x1", "3x3", "5x5", "7x7", "9x9", "11x11", "13x13"};
				if(ImGui::Combo("Kernel size", &shadow_map_grid_kernel_size_index, shadow_map_grid_kernel_sizes, 7, -1)) {
					shadow_map_settings.grid_kernel_size = 2 * shadow_map_grid_kernel_size_index + 1;
				}
			} else if(shadow_map_settings.sampling_mode == SAMPLING_MODE_POISSON) {
				static int shadow_map_poisson_sample_count_index = 0;
				const char* shadow_map_poisson_sample_counts[] = {"25", "32", "64", "128"};
				if(ImGui::Combo("Sample count", &shadow_map_poisson_sample_count_index, shadow_map_poisson_sample_counts, 4, -1)) {
					switch(shadow_map_poisson_sample_count_index) {
						case 0: shadow_map_settings.poisson_sample_count = 25; break;
						case 1: shadow_map_settings.poisson_sample_count = 32; break;
						case 2: shadow_map_settings.poisson_sample_count = 64; break;
						case 3: shadow_map_settings.poisson_sample_count = 128; break;
					}
					create_shader_programs();
				}
			} else if(shadow_map_settings.sampling_mode == SAMPLING_MODE_VOGEL) {
				ImGui::SliderInt("Sample count", &shadow_map_settings.vogel_sample_count, 1, 128);
			}
		} else if(shadow_map_settings.mode == MODE_VSM) {
			static int shadow_map_gaussian_kernel_size_index = 1;
			const char* shadow_map_gaussian_kernel_sizes[] = {"3x3", "5x5", "7x7", "9x9", "11x11", "13x13"};
			if(ImGui::Combo("Kernel size", &shadow_map_gaussian_kernel_size_index, shadow_map_gaussian_kernel_sizes, 6, -1)) {
				switch(shadow_map_gaussian_kernel_size_index) {
					case 0: shadow_map_settings.gaussian_kernel_size = 3; break;
					case 1: shadow_map_settings.gaussian_kernel_size = 5; break;
					case 2: shadow_map_settings.gaussian_kernel_size = 7; break;
					case 3: shadow_map_settings.gaussian_kernel_size = 9; break;
					case 4: shadow_map_settings.gaussian_kernel_size = 11; break;
					case 5: shadow_map_settings.gaussian_kernel_size = 13; break;
				}
				create_shader_programs();
			}
		}
		ImGui::Checkbox("Rotate samples", &shadow_map_settings.rotate_samples);
	}
	if(shadow_map_settings.mode == MODE_VSM) {
		ImGui::Checkbox("Smoothstep fix", &shadow_map_settings.vsm_smoothstep_fix);
		if(shadow_map_settings.vsm_smoothstep_fix) {
			ImGui::SliderFloat("Smoothstep fix lower bound", &shadow_map_settings.vsm_smoothstep_fix_lower_bound, 0.0, 1.0);
		}
	}
	ImGui::End();

	ImGui::Begin("Light source settings");
	ImGui::ColorEdit3("Color", (float*) &light.color, ImGuiColorEditFlags_Float);
	if(shadow_map_settings.mode != MODE_NORMAL) {
		ImGui::SliderFloat("Light size", &light.size, 0.0, 10.0);
	}
	ImGui::End();

	ImGui::Begin("Shadow map");
	if(shadow_map_settings.mode == MODE_VSM) {
		ImGui::Image((ImTextureID) shadow_color_texture, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::Image((ImTextureID) shadow_color_texture_2, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
	} else {
		ImGui::Image((ImTextureID) shadow_depth_texture, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void run() {
	while(!glfwWindowShouldClose(window.handler)) {
		handle_time();
		handle_input();
		compute_matrices();
		render_shadow_map();
		render_geometry();
		render_ui();
		glfwSwapBuffers(window.handler);
		glfwPollEvents();
	}
}

void initialize() {
	create_window();
	initialize_opengl();
	initialize_imgui();
	create_shader_programs();
	create_renderables();
	create_render_targets();
}

void destroy_imgui() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void destroy_opengl() {
	glDeleteTextures(1, &shadow_color_texture);
	glDeleteTextures(1, &shadow_color_texture_2);
	glDeleteTextures(1, &shadow_depth_texture);
	glDeleteFramebuffers(1, &shadow_map_fbo);
	for(auto& renderable : renderables) {
		glDeleteVertexArrays(1, &renderable.mesh.vao);
	}
	glDeleteProgram(shadow_map_program);
	glDeleteProgram(lambertian_program);
	glDeleteProgram(gaussian_blur_program);
}

void destroy_window() {
	glfwDestroyWindow(window.handler);
	glfwTerminate();
}

void destroy() {
	destroy_imgui();
	destroy_opengl();
	destroy_window();
}

int main() {
	initialize();
	run();
	destroy();
}
