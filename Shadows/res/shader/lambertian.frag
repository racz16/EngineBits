in vec3 io_normal;
in vec4 io_lvs_position;

uniform float u_bias;
uniform vec3 u_light_direction;
uniform vec3 u_light_color;
uniform vec3 u_diffuse_color;

out vec4 o_color;

float bias;

float compute_shadow();

float get_bias() {
	return bias;
}

void main() {
	vec3 normal = normalize(io_normal);
	vec3 light_direction = -normalize(u_light_direction);
	bias = (1.0 - dot(normal, light_direction)) * u_bias;
	float shadow = compute_shadow();
	o_color = vec4(vec3(0.1), 1.0) + vec4(u_diffuse_color * dot(normal, light_direction) * u_light_color, 1.0) * shadow;
}
