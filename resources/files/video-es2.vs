// OpenGL ES 2.0 shaders use GLSL ES 1.0
precision mediump float;

// Input vertex attributes
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;

// Output to fragment shader
varying vec2 fragTexCoord;

void main()
{
    // Pass through the texture coordinates
    fragTexCoord = vertexTexCoord;

    // Set the vertex position
    gl_Position = vec4(vertexPosition, 1.0);
}
