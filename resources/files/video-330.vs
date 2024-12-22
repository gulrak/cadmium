#version 330

// Input vertex attributes (from vertex buffer)
in vec3 vertexPosition;
in vec2 vertexTexCoord;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;

void main()
{
    // Pass through the texture coordinates
    fragTexCoord = vertexTexCoord;

    // Set the vertex position
    gl_Position = vec4(vertexPosition, 1.0);
}
