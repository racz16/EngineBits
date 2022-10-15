in vec2 io_texture_coordinates;

uniform sampler2D u_image;
uniform bool u_horizontal;
uniform float u_scale;
uniform float u_light_size;
uniform bool u_rotate_samples;

out vec4 o_color;

float interleaved_gradient_noise();

const float[] weights_3 = float[](2.0 / 4.0, 1.0 / 4.0);
const float[] weights_5 = float[](6.0 / 16.0, 4.0 / 16.0, 1.0 / 16.0);
const float[] weights_7 = float[](20.0 / 64.0, 15.0 / 64.0, 6.0 / 64.0, 1.0 / 64.0);
const float[] weights_9 = float[](70.0 / 256.0, 56.0 / 256.0, 28.0 / 256.0, 8.0 / 256.0, 1.0 / 256.0);
const float[] weights_11 = float[](252.0 / 1024.0, 210.0 / 1024.0, 120.0 / 1024.0, 45.0 / 1024.0, 10.0 / 1024.0, 1.0 / 1024.0);
const float[] weights_13 = float[](924.0 / 4096.0, 792.0 / 4096.0, 495.0 / 4096.0, 220.0 / 4096.0, 66.0 / 4096.0, 12.0 / 4096.0, 1.0 / 4096.0);

#ifdef GAUSSIAN_3
    #define WEIGHTS weights_3
#elif GAUSSIAN_5
    #define WEIGHTS weights_5
#elif GAUSSIAN_7
    #define WEIGHTS weights_7
#elif GAUSSIAN_9
    #define WEIGHTS weights_9
#elif GAUSSIAN_11
    #define WEIGHTS weights_11
#elif GAUSSIAN_13
    #define WEIGHTS weights_13
#else
    #define WEIGHTS weights_5
#endif

void main() {
    vec3 result = texture(u_image, io_texture_coordinates).rgb * WEIGHTS[0];
    vec2 offset_vector = mix(vec2(0.0, 1.0), vec2(1.0, 0.0), u_horizontal);
    float angle = mix(0.0, interleaved_gradient_noise(), u_rotate_samples);
	float rotation_cos = cos(angle);
	float rotation_sin = sin(angle);
	mat2 rotator = mat2(
		mix(vec2(1.0, 0.0), vec2(rotation_cos, rotation_sin), u_rotate_samples), 
		mix(vec2(0.0, 1.0), vec2(-rotation_sin, rotation_cos), u_rotate_samples)
	);
    for(int i = 1; i < WEIGHTS.length(); i++) {
        vec2 real_offset = offset_vector * float(i) / (WEIGHTS.length() - 1) * rotator * u_light_size * u_scale;
        result += texture(u_image, io_texture_coordinates + real_offset).rgb * WEIGHTS[i];
        result += texture(u_image, io_texture_coordinates - real_offset).rgb * WEIGHTS[i];
    }
    o_color = vec4(result, 1.0);
}
