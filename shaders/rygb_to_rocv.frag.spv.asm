; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 11
; Bound: 70
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragUV %outColor
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpSourceExtension "GL_GOOGLE_cpp_style_line_directive"
               OpSourceExtension "GL_GOOGLE_include_directive"
               OpName %main "main"
               OpName %colorRYGB "colorRYGB"
               OpName %rygbFrameBufferSampler "rygbFrameBufferSampler"
               OpName %SystemUBOStatic "SystemUBOStatic"
               OpMemberName %SystemUBOStatic 0 "transformMat"
               OpMemberName %SystemUBOStatic 1 "frameBufferIdx"
               OpMemberName %SystemUBOStatic 2 "toRGB"
               OpName %systemUBOStatic "systemUBOStatic"
               OpName %fragUV "fragUV"
               OpName %toRGB "toRGB"
               OpName %colorROCV "colorROCV"
               OpName %outColor "outColor"
               OpDecorate %rygbFrameBufferSampler DescriptorSet 0
               OpDecorate %rygbFrameBufferSampler Binding 2
               OpMemberDecorate %SystemUBOStatic 0 ColMajor
               OpMemberDecorate %SystemUBOStatic 0 Offset 0
               OpMemberDecorate %SystemUBOStatic 0 MatrixStride 16
               OpMemberDecorate %SystemUBOStatic 1 Offset 64
               OpMemberDecorate %SystemUBOStatic 2 Offset 68
               OpDecorate %SystemUBOStatic Block
               OpDecorate %systemUBOStatic DescriptorSet 0
               OpDecorate %systemUBOStatic Binding 1
               OpDecorate %fragUV Location 1
               OpDecorate %outColor Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
         %10 = OpTypeImage %float 2D 0 0 0 1 Unknown
         %11 = OpTypeSampledImage %10
       %uint = OpTypeInt 32 0
    %uint_10 = OpConstant %uint 10
%_arr_11_uint_10 = OpTypeArray %11 %uint_10
%_ptr_UniformConstant__arr_11_uint_10 = OpTypePointer UniformConstant %_arr_11_uint_10
%rygbFrameBufferSampler = OpVariable %_ptr_UniformConstant__arr_11_uint_10 UniformConstant
%mat4v4float = OpTypeMatrix %v4float 4
        %int = OpTypeInt 32 1
%SystemUBOStatic = OpTypeStruct %mat4v4float %int %int
%_ptr_Uniform_SystemUBOStatic = OpTypePointer Uniform %SystemUBOStatic
%systemUBOStatic = OpVariable %_ptr_Uniform_SystemUBOStatic Uniform
      %int_1 = OpConstant %int 1
%_ptr_Uniform_int = OpTypePointer Uniform %int
%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11
    %v2float = OpTypeVector %float 2
%_ptr_Input_v2float = OpTypePointer Input %v2float
     %fragUV = OpVariable %_ptr_Input_v2float Input
       %bool = OpTypeBool
%_ptr_Function_bool = OpTypePointer Function %bool
      %int_2 = OpConstant %int 2
      %int_0 = OpConstant %int 0
%_ptr_Uniform_mat4v4float = OpTypePointer Uniform %mat4v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
   %outColor = OpVariable %_ptr_Output_v4float Output
     %uint_0 = OpConstant %uint 0
%_ptr_Function_float = OpTypePointer Function %float
    %float_0 = OpConstant %float 0
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
       %main = OpFunction %void None %3
          %5 = OpLabel
  %colorRYGB = OpVariable %_ptr_Function_v4float Function
      %toRGB = OpVariable %_ptr_Function_bool Function
  %colorROCV = OpVariable %_ptr_Function_v4float Function
         %24 = OpAccessChain %_ptr_Uniform_int %systemUBOStatic %int_1
         %25 = OpLoad %int %24
         %27 = OpAccessChain %_ptr_UniformConstant_11 %rygbFrameBufferSampler %25
         %28 = OpLoad %11 %27
         %32 = OpLoad %v2float %fragUV
         %33 = OpImageSampleImplicitLod %v4float %28 %32
               OpStore %colorRYGB %33
         %38 = OpAccessChain %_ptr_Uniform_int %systemUBOStatic %int_2
         %39 = OpLoad %int %38
         %40 = OpIEqual %bool %39 %int_1
               OpStore %toRGB %40
         %44 = OpAccessChain %_ptr_Uniform_mat4v4float %systemUBOStatic %int_0
         %45 = OpLoad %mat4v4float %44
         %46 = OpLoad %v4float %colorRYGB
         %47 = OpMatrixTimesVector %v4float %45 %46
               OpStore %colorROCV %47
         %48 = OpLoad %bool %toRGB
               OpSelectionMerge %50 None
               OpBranchConditional %48 %49 %59
         %49 = OpLabel
         %55 = OpAccessChain %_ptr_Function_float %colorROCV %uint_0
         %56 = OpLoad %float %55
         %58 = OpCompositeConstruct %v4float %56 %float_0 %float_0 %float_0
               OpStore %outColor %58
               OpBranch %50
         %59 = OpLabel
         %61 = OpAccessChain %_ptr_Function_float %colorROCV %uint_1
         %62 = OpLoad %float %61
         %64 = OpAccessChain %_ptr_Function_float %colorROCV %uint_2
         %65 = OpLoad %float %64
         %67 = OpAccessChain %_ptr_Function_float %colorROCV %uint_3
         %68 = OpLoad %float %67
         %69 = OpCompositeConstruct %v4float %float_0 %62 %65 %68
               OpStore %outColor %69
               OpBranch %50
         %50 = OpLabel
               OpReturn
               OpFunctionEnd
