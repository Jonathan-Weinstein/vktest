void main(in float2 udc : in0xy,
          out float4 clipspace : SV_Position)
{
   float2 tmp = udc*2 - 1;
   clipspace = float4(tmp.x, -tmp.y, 0, 1);
}
/*
dxc -spirv -fspv-target-env=vulkan1.1 -O3 -T vs_6_7 forced_ms_vs.hlsl
; SPIR-V
; Version: 1.3
; Generator: Google spiregg; 0
; Bound: 23
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %in_var_in0xy %gl_Position
               OpSource HLSL 670
               OpName %in_var_in0xy "in.var.in0xy"
               OpName %main "main"
               OpDecorate %gl_Position BuiltIn Position
               OpDecorate %in_var_in0xy Location 0
      %float = OpTypeFloat 32
    %float_2 = OpConstant %float 2
    %float_1 = OpConstant %float 1
    %v2float = OpTypeVector %float 2
          %8 = OpConstantComposite %v2float %float_1 %float_1
    %float_0 = OpConstant %float 0
%_ptr_Input_v2float = OpTypePointer Input %v2float
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %14 = OpTypeFunction %void
%in_var_in0xy = OpVariable %_ptr_Input_v2float Input
%gl_Position = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %14
         %15 = OpLabel
         %16 = OpLoad %v2float %in_var_in0xy
         %17 = OpVectorTimesScalar %v2float %16 %float_2
         %18 = OpFSub %v2float %17 %8
         %19 = OpCompositeExtract %float %18 0
         %20 = OpCompositeExtract %float %18 1
         %21 = OpFNegate %float %20
         %22 = OpCompositeConstruct %v4float %19 %21 %float_0 %float_1
               OpStore %gl_Position %22
               OpReturn
               OpFunctionEnd
*/
