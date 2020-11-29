#version 150

in vec3 aPos;
in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
   TexCoords = aTexCoords;
   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
