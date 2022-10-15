in vec3 i_position;
in vec3 i_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_light_view;
uniform mat4 u_light_projection;

out vec3 io_normal;
out vec4 io_lvs_position;
out vec4 io_lcs_position;

void main(){
	vec3 ws_position = vec3(u_model * vec4(i_position, 1.0));
	gl_Position = u_projection * u_view * vec4(ws_position, 1.0);
	io_normal = mat3(inverse(transpose(u_model))) * i_normal;
	io_lvs_position = u_light_view * vec4(ws_position, 1.0);
	io_lcs_position = u_light_projection * io_lvs_position;
}
