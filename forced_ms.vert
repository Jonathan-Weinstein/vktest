#version 450
layout(location=0) in vec2 pos;
void main()
{
   vec2 tmp = pos*2 - 1;
   gl_Position = vec4(tmp.x, -tmp.y, 0, 1);
}
/*
glslc --target-env=vulkan1.1 ./forced_mutilisample.vert -O -S -o -
; SPIR-V
; Version: 1.3
; Generator: Google Shaderc over Glslang; 10
; Bound: 41
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %4 "main" %11 %24
               OpDecorate %11 Location 0
               OpMemberDecorate %_struct_22 0 BuiltIn Position
               OpMemberDecorate %_struct_22 1 BuiltIn PointSize
               OpMemberDecorate %_struct_22 2 BuiltIn ClipDistance
               OpMemberDecorate %_struct_22 3 BuiltIn CullDistance
               OpDecorate %_struct_22 Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
%_ptr_Input_v2float = OpTypePointer Input %v2float
         %11 = OpVariable %_ptr_Input_v2float Input
    %float_2 = OpConstant %float 2
    %float_1 = OpConstant %float 1
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
%_arr_float_uint_1 = OpTypeArray %float %uint_1
 %_struct_22 = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output__struct_22 = OpTypePointer Output %_struct_22
         %24 = OpVariable %_ptr_Output__struct_22 Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %float_0 = OpConstant %float 0
%_ptr_Output_v4float = OpTypePointer Output %v4float
         %40 = OpConstantComposite %v2float %float_1 %float_1
          %4 = OpFunction %void None %3
          %5 = OpLabel
         %12 = OpLoad %v2float %11
         %14 = OpVectorTimesScalar %v2float %12 %float_2
         %17 = OpFSub %v2float %14 %40
         %30 = OpCompositeExtract %float %17 0
         %32 = OpCompositeExtract %float %17 1
         %33 = OpFNegate %float %32
         %35 = OpCompositeConstruct %v4float %30 %33 %float_0 %float_1
         %37 = OpAccessChain %_ptr_Output_v4float %24 %int_0
               OpStore %37 %35
               OpReturn
               OpFunctionEnd
*/
