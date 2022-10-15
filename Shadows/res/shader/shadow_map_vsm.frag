out vec4 o_color;

void main(){
    float depth = gl_FragCoord.z;
    float depth_squared = depth * depth;
    o_color = vec4(depth, depth_squared, 0.0, 1.0);
}