out vec4 o_color;

void main(){
	o_color = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
