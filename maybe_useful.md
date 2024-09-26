Stuff that may be useful eventually

Code to power on display
```cpp
#if POWER_ON_DISPLAY
    static bool poweredOn = false;
    if (false && !poweredOn) { // power display on, seems unnecessary
        poweredOn = true;
        // power on display
        DEBUG("Turning display on...");
        VkDisplayPowerInfoEXT powerInfo{
            .sType = VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT,
            .pNext = VK_NULL_HANDLE,
            .powerState = VkDisplayPowerStateEXT::VK_DISPLAY_POWER_STATE_ON_EXT
        };
        PFN_vkDisplayPowerControlEXT fnPtr = reinterpret_cast<PFN_vkDisplayPowerControlEXT>(
            vkGetInstanceProcAddr(_instance, "vkDisplayPowerControlEXT")
        );
        ASSERT(fnPtr);
        VK_CHECK_RESULT(fnPtr(_device->logicalDevice, _mainProjectorDisplay.display, &powerInfo));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
#endif // POWER_ON_DISPLAY

```


## MoltenVK supported device extensions
```
Device Extensions: count = 94
        VK_AMD_gpu_shader_half_float              : extension revision 2
        VK_AMD_negative_viewport_height           : extension revision 1
        VK_AMD_shader_image_load_store_lod        : extension revision 1
        VK_AMD_shader_trinary_minmax              : extension revision 1
        VK_EXT_4444_formats                       : extension revision 1
        VK_EXT_buffer_device_address              : extension revision 2
        VK_EXT_calibrated_timestamps              : extension revision 2
        VK_EXT_debug_marker                       : extension revision 4
        VK_EXT_descriptor_indexing                : extension revision 2
        VK_EXT_extended_dynamic_state             : extension revision 1
        VK_EXT_extended_dynamic_state2            : extension revision 1
        VK_EXT_extended_dynamic_state3            : extension revision 2
        VK_EXT_external_memory_host               : extension revision 1
        VK_EXT_fragment_shader_interlock          : extension revision 1
        VK_EXT_hdr_metadata                       : extension revision 2
        VK_EXT_host_image_copy                    : extension revision 1
        VK_EXT_host_query_reset                   : extension revision 1
        VK_EXT_image_robustness                   : extension revision 1
        VK_EXT_inline_uniform_block               : extension revision 1
        VK_EXT_memory_budget                      : extension revision 1
        VK_EXT_metal_objects                      : extension revision 2
        VK_EXT_pipeline_creation_cache_control    : extension revision 3
        VK_EXT_pipeline_creation_feedback         : extension revision 1
        VK_EXT_post_depth_coverage                : extension revision 1
        VK_EXT_private_data                       : extension revision 1
        VK_EXT_robustness2                        : extension revision 1
        VK_EXT_sample_locations                   : extension revision 1
        VK_EXT_scalar_block_layout                : extension revision 1
        VK_EXT_separate_stencil_usage             : extension revision 1
        VK_EXT_shader_atomic_float                : extension revision 1
        VK_EXT_shader_demote_to_helper_invocation : extension revision 1
        VK_EXT_shader_stencil_export              : extension revision 1
        VK_EXT_shader_subgroup_ballot             : extension revision 1
        VK_EXT_shader_subgroup_vote               : extension revision 1
        VK_EXT_shader_viewport_index_layer        : extension revision 1
        VK_EXT_subgroup_size_control              : extension revision 2
        VK_EXT_swapchain_maintenance1             : extension revision 1
        VK_EXT_texel_buffer_alignment             : extension revision 1
        VK_EXT_texture_compression_astc_hdr       : extension revision 1
        VK_EXT_vertex_attribute_divisor           : extension revision 3
        VK_GOOGLE_display_timing                  : extension revision 1
        VK_IMG_format_pvrtc                       : extension revision 1
        VK_INTEL_shader_integer_functions2        : extension revision 1
        VK_KHR_16bit_storage                      : extension revision 1
        VK_KHR_8bit_storage                       : extension revision 1
        VK_KHR_bind_memory2                       : extension revision 1
        VK_KHR_buffer_device_address              : extension revision 1
        VK_KHR_calibrated_timestamps              : extension revision 1
        VK_KHR_copy_commands2                     : extension revision 1
        VK_KHR_create_renderpass2                 : extension revision 1
        VK_KHR_dedicated_allocation               : extension revision 3
        VK_KHR_deferred_host_operations           : extension revision 4
        VK_KHR_depth_stencil_resolve              : extension revision 1
        VK_KHR_descriptor_update_template         : extension revision 1
        VK_KHR_device_group                       : extension revision 4
        VK_KHR_driver_properties                  : extension revision 1
        VK_KHR_dynamic_rendering                  : extension revision 1
        VK_KHR_external_fence                     : extension revision 1
        VK_KHR_external_memory                    : extension revision 1
        VK_KHR_external_semaphore                 : extension revision 1
        VK_KHR_format_feature_flags2              : extension revision 2
        VK_KHR_fragment_shader_barycentric        : extension revision 1
        VK_KHR_get_memory_requirements2           : extension revision 1
        VK_KHR_image_format_list                  : extension revision 1
        VK_KHR_imageless_framebuffer              : extension revision 1
        VK_KHR_incremental_present                : extension revision 2
        VK_KHR_maintenance1                       : extension revision 2
        VK_KHR_maintenance2                       : extension revision 1
        VK_KHR_maintenance3                       : extension revision 1
        VK_KHR_map_memory2                        : extension revision 1
        VK_KHR_multiview                          : extension revision 1
        VK_KHR_portability_subset                 : extension revision 1
        VK_KHR_push_descriptor                    : extension revision 2
        VK_KHR_relaxed_block_layout               : extension revision 1
        VK_KHR_sampler_mirror_clamp_to_edge       : extension revision 3
        VK_KHR_sampler_ycbcr_conversion           : extension revision 14
        VK_KHR_separate_depth_stencil_layouts     : extension revision 1
        VK_KHR_shader_draw_parameters             : extension revision 1
        VK_KHR_shader_float16_int8                : extension revision 1
        VK_KHR_shader_float_controls              : extension revision 4
        VK_KHR_shader_integer_dot_product         : extension revision 1
        VK_KHR_shader_non_semantic_info           : extension revision 1
        VK_KHR_shader_subgroup_extended_types     : extension revision 1
        VK_KHR_spirv_1_4                          : extension revision 1
        VK_KHR_storage_buffer_storage_class       : extension revision 1
        VK_KHR_swapchain                          : extension revision 70
        VK_KHR_swapchain_mutable_format           : extension revision 1
        VK_KHR_synchronization2                   : extension revision 1
        VK_KHR_timeline_semaphore                 : extension revision 2
        VK_KHR_uniform_buffer_standard_layout     : extension revision 1
        VK_KHR_variable_pointers                  : extension revision 1
        VK_KHR_vertex_attribute_divisor           : extension revision 1
        VK_NV_fragment_shader_barycentric         : extension revision 1
        VK_NV_glsl_shader                         : extension revision 1
```
