#version 430 core

out vec4 frag_color;

in vec2 texcoord;
in vec4 color;
void main()
{	
	frag_color = color;
}