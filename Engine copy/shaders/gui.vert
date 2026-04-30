#version 330 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

out vec4 v_Color;
out vec2 v_TexCoord;

uniform mat4 u_Projection;

void main()
{
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * vec4(a_Position, 0.0, 1.0);
}