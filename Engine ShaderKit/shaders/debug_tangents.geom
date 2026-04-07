#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} gs_in[];

uniform mat4 projection;
const float MAGNITUDE = 4.0; // Adjust this to make lines longer/shorter

void GenerateLine(int index, vec3 vector, vec3 color) {
    // Start of the line (at the vertex)
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    
    // End of the line (offset by the vector)
    gl_Position = projection * (gl_in[index].gl_Position + vec4(vector, 0.0) * MAGNITUDE);
    EmitVertex();
    
    EndPrimitive();
}

void main() {
    for(int i = 0; i < 3; i++) {
        GenerateLine(i, gs_in[i].tangent,   vec3(1.0, 0.0, 0.0)); // Red
        GenerateLine(i, gs_in[i].bitangent, vec3(0.0, 1.0, 0.0)); // Green
        GenerateLine(i, gs_in[i].normal,    vec3(0.0, 0.0, 1.0)); // Blue
    }
}