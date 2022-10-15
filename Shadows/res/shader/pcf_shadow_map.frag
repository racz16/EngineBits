in vec4 io_lcs_position;

uniform sampler2D u_shadow_map;
uniform float u_intensity;
uniform float u_light_size;
uniform int u_kernel_size;
uniform int u_vogel_sample_count;
uniform bool u_rotate_samples;
uniform float u_scale;

float get_bias();
vec2[25] get_poisson_25();
vec2[32] get_poisson_32();
vec2[64] get_poisson_64();
vec2[128] get_poisson_128();
float interleaved_gradient_noise();
vec2 vogel_disk_sample(int sample_index, int samples_count, float angle);

#ifdef POISSON_25
	#define POISSON_SIZE 25
	#define POISSON() get_poisson_25()
#elif POISSON_32
	#define POISSON_SIZE 32
	#define POISSON() get_poisson_32()
#elif POISSON_64
	#define POISSON_SIZE 64
	#define POISSON() get_poisson_64()
#elif POISSON_128
	#define POISSON_SIZE 128
	#define POISSON() get_poisson_128()
#else
	#define POISSON_SIZE 25
	#define POISSON() get_poisson_25()
#endif

float compute_shadow() {
	vec3 uv = io_lcs_position.xyz / io_lcs_position.w;
	uv = uv * 0.5 + 0.5;
	float real_depth = uv.z;
	if(real_depth > 1.0) {
		return 1.0;
	}
	float result = 0.0;
	float angle = mix(0.0, interleaved_gradient_noise(), u_rotate_samples);
	float rotation_cos = cos(angle);
	float rotation_sin = sin(angle);
	mat2 rotator = mat2(
		mix(vec2(1.0, 0.0), vec2(rotation_cos, rotation_sin), u_rotate_samples), 
		mix(vec2(0.0, 1.0), vec2(-rotation_sin, rotation_cos), u_rotate_samples)
	);

#ifdef SAMPLING_MODE_GRID
	const int subtract = u_kernel_size / 2;
	for(int i = 0; i < u_kernel_size; i++){
		for(int j = 0; j < u_kernel_size; j++){
			vec2 offset = vec2(i - subtract, j - subtract) / u_kernel_size * rotator * u_light_size * u_scale;
			float depth = texture(u_shadow_map, uv.xy + offset).r + get_bias();
			result += mix(u_intensity, 1.0, depth > real_depth);
		}
	}
	return result / (u_kernel_size * u_kernel_size);
#elif SAMPLING_MODE_POISSON
    vec2[POISSON_SIZE] poisson = POISSON();
	for(int i = 0; i < poisson.length(); i++) {
		vec2 offset = poisson[i] * rotator * u_light_size * u_scale;
		float depth = texture(u_shadow_map, uv.xy + offset).r + get_bias();
		result += mix(u_intensity, 1.0, depth > real_depth);
	}
	return result / poisson.length();
#elif SAMPLING_MODE_VOGEL
	for(int i = 0; i < u_vogel_sample_count; i++) {
		vec2 offset = vogel_disk_sample(i, u_vogel_sample_count, angle) * u_light_size * u_scale;
		float depth = texture(u_shadow_map, uv.xy + offset).r + get_bias();
		result += mix(u_intensity, 1.0, depth > real_depth);
	}
	return result / u_vogel_sample_count;
#endif
	return 1.0;
}
