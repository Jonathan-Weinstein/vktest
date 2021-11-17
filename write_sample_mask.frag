#version 450
layout(location=0) out uint rtv0;
void main()
{
   rtv0 = gl_SampleMaskIn[0];
}
