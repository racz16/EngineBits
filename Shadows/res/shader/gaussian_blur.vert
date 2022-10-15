layout(location = 0) in vec3 i_position;
layout(location = 2) in vec2 i_texture_coordinates;

out vec2 io_texture_coordinates;

void main(){
    io_texture_coordinates = i_texture_coordinates;
    gl_Position = vec4(i_position, 1.0);
}
