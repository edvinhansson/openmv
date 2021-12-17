#begin VERTEX

#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;
layout (location = 3) in float texture_id;

uniform mat4 camera = mat4(1.0);
uniform mat4 view = mat4(1.0);

out VS_OUT {
	vec4 color;
	vec2 uv;
	float texture_id;
} vs_out;

void main() {
	vs_out.color = color;
	vs_out.uv = uv;
	vs_out.texture_id = texture_id;

	gl_Position = camera * view * vec4(position, 0.0, 1.0);
}

#end VERTEX

#begin FRAGMENT

#version 330 core

out vec4 result;

in VS_OUT {
	vec4 color;
	vec2 uv;
	float texture_id;
} fs_in;

uniform sampler2D textures[32];

void main() {
	vec4 texture_color = vec4(1.0);

	switch (int(fs_in.texture_id)) {
	case 0:  texture_color = texture(textures[0],  fs_in.uv); break;
	case 1:  texture_color = texture(textures[1],  fs_in.uv); break;
	case 2:  texture_color = texture(textures[2],  fs_in.uv); break;
	case 3:  texture_color = texture(textures[3],  fs_in.uv); break;
	case 4:  texture_color = texture(textures[4],  fs_in.uv); break;
	case 5:  texture_color = texture(textures[5],  fs_in.uv); break;
	case 6:  texture_color = texture(textures[6],  fs_in.uv); break;
	case 7:  texture_color = texture(textures[7],  fs_in.uv); break;
	case 8:  texture_color = texture(textures[8],  fs_in.uv); break;
	case 9:  texture_color = texture(textures[9],  fs_in.uv); break;
	case 10: texture_color = texture(textures[10], fs_in.uv); break;
	case 11: texture_color = texture(textures[11], fs_in.uv); break;
	case 12: texture_color = texture(textures[12], fs_in.uv); break;
	case 13: texture_color = texture(textures[13], fs_in.uv); break;
	case 14: texture_color = texture(textures[14], fs_in.uv); break;
	case 15: texture_color = texture(textures[15], fs_in.uv); break;
	case 16: texture_color = texture(textures[16], fs_in.uv); break;
	case 17: texture_color = texture(textures[17], fs_in.uv); break;
	case 18: texture_color = texture(textures[18], fs_in.uv); break;
	case 19: texture_color = texture(textures[19], fs_in.uv); break;
	case 20: texture_color = texture(textures[20], fs_in.uv); break;
	case 21: texture_color = texture(textures[21], fs_in.uv); break;
	case 22: texture_color = texture(textures[22], fs_in.uv); break;
	case 23: texture_color = texture(textures[23], fs_in.uv); break;
	case 24: texture_color = texture(textures[24], fs_in.uv); break;
	case 25: texture_color = texture(textures[25], fs_in.uv); break;
	case 26: texture_color = texture(textures[26], fs_in.uv); break;
	case 27: texture_color = texture(textures[27], fs_in.uv); break;
	case 28: texture_color = texture(textures[28], fs_in.uv); break;
	case 29: texture_color = texture(textures[29], fs_in.uv); break;
	case 30: texture_color = texture(textures[30], fs_in.uv); break;
	case 31: texture_color = texture(textures[31], fs_in.uv); break;
	default: texture_color = vec4(1.0); break;
	}

	result = fs_in.color * texture_color;
}

#end FRAGMENT
