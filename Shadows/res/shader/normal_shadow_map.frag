in vec4 io_lcs_position;

uniform sampler2D u_shadow_map;
uniform float u_intensity;

float get_bias();

float compute_shadow(){
	vec3 uv = io_lcs_position.xyz;// / io_lcs_position.w;
	uv = uv * 0.5 + 0.5;
	float real_depth = uv.z;
	if(real_depth > 1.0) {
		return 1.0;
	}
	float depth = texture(u_shadow_map, uv.xy).r;
	return real_depth > depth + get_bias() ? u_intensity : 1.0;
}
