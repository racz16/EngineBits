in vec4 io_lcs_position;

uniform sampler2D u_shadow_map;
uniform float u_intensity;
uniform bool u_smoothstep_fix;
uniform float u_smoothstep_fix_lower_bound;

float compute_shadow(){
	vec3 uv = io_lcs_position.xyz / io_lcs_position.w;
    uv = uv * 0.5 + 0.5;
    float real_depth = uv.z;
	if(real_depth > 1.0) {
		return 1.0;
	}
    vec2 moments = texture(u_shadow_map, uv.xy).xy;
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.00002);
	float d = real_depth - moments.x;
	float p_max = variance / (variance + d * d);
	p_max = mix(p_max, smoothstep(u_smoothstep_fix_lower_bound, 1.0, p_max), u_smoothstep_fix);
	return mix(p_max * (1.0 - u_intensity) + u_intensity, 1.0, real_depth <= moments.x);
}
