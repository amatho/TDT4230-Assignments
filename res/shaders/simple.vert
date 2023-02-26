#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 1) vec3 normal_in;
in layout(location = 2) vec2 textureCoordinates_in;
in layout(location = 3) vec3 tangent_in;
in layout(location = 4) vec3 bitangent_in;

uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model;
uniform layout(location = 5) mat3 normalTransform;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 textureCoordinates_out;
out layout(location = 2) vec3 position_out;
out layout(location = 3) mat3 tbn_out;

void main()
{
    // Transform the normals
    normal_out = normalize(normalTransform * normal_in);

    textureCoordinates_out = textureCoordinates_in;
    gl_Position = MVP * vec4(position, 1.0f);

    // Pass vertex positions to fragment shader
    position_out = vec3(model * vec4(position, 1.0));

    // Calculate the TBN matrix
    vec3 t = normalize(vec3(model * vec4(tangent_in, 0.0)));
    vec3 b = normalize(vec3(model * vec4(bitangent_in, 0.0)));
    vec3 n = normalize(vec3(model * vec4(normal_in, 0.0)));
    tbn_out = mat3(t, b, n);
}
