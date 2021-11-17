#version 450

const vec2 positions[6] = {
   vec2(  0,    1 ),
   vec2(  1,    0 ),
   vec2( -0.5, -1 ),

   vec2( -0.75,  1 ),
   vec2(  1, -1 ),
   vec2( -1, -0.75 ),
};

void main()
{
   uint vid = gl_VertexIndex;
   vid = vid < 6u ? vid : 6u;
   gl_Position = vec4(positions[vid], 0, 1);
}
