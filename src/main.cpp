#include <gvk.hpp>
#include <imgui.h>
#include <imfilebrowser.h>

#include <iostream>
#include <sstream>

#include <glm/gtx/string_cast.hpp>

#include "line_importer.h"
#include "host_structures.h"
#include "Dataset.h"


const size_t kBufferLayerCount = 16;

class line_renderer_app : public gvk::invokee 
{

public:

	line_renderer_app(avk::queue& aQueue)
		: mQueue{ &aQueue } {}
		
	void loadDatasetFromFile(std::string& filename) {
		using namespace avk;
		using namespace gvk;

		mDataset->importFromFile(filename);

		std::vector<Vertex> gpuVertexData;
		std::vector<uint32_t> gpuIndexData;
		mDataset->fillGPUReadyBuffer(gpuVertexData, gpuIndexData);

		// Create new GPU Buffer
		mNewVertexBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer, storage_buffer_meta::create_from_data(gpuVertexData), vertex_buffer_meta::create_from_data(gpuVertexData));
		mNewVertexBuffer.enable_shared_ownership();
		mNewIndexBuffer = context().create_buffer(memory_usage::device, {}, index_buffer_meta::create_from_data(gpuIndexData));
		mNewIndexBuffer.enable_shared_ownership();

		// Fill them and wait for completion
		auto fenc = context().record_and_submit_with_fence(
			{
				mNewVertexBuffer->fill(gpuVertexData.data(), 0),
				mNewIndexBuffer->fill(gpuIndexData.data(), 0)
			},
			mQueue
		);
		fenc->wait_until_signalled();

		mReplaceOldBufferWithNextFrame = true;
	}

	void initialize() override
	{
		using namespace avk;
		using namespace gvk;

		mDataset = std::make_unique<Dataset>();

		const auto resolution = context().main_window()->resolution();

		mUniformBuffer = context().create_buffer(memory_usage::host_visible, {}, uniform_buffer_meta::create_from_size(sizeof(matrices_and_user_input)));

		// Initialize vertex and index buffer with empty datasets
		std::vector<Vertex> gpuVertexData = { {} };
		std::vector<uint32_t> gpuIndexData = { 0 };
		mVertexBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer, storage_buffer_meta::create_from_data(gpuVertexData), vertex_buffer_meta::create_from_data(gpuVertexData));
		mVertexBuffer.enable_shared_ownership();
		mIndexBuffer = context().create_buffer(memory_usage::device, {}, index_buffer_meta::create_from_data(gpuIndexData));
		mIndexBuffer.enable_shared_ownership();

		mDrawCalls.push_back({0, 2});

		// Create a descriptor cache that helps us to conveniently create descriptor sets:
		mDescriptorCache = gvk::context().create_descriptor_cache();

		// Create the buffer for the k buffer
		const size_t kBufferSize = resolution.x * resolution.y * kBufferLayerCount * sizeof(uint64_t);
		mkBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer, storage_buffer_meta::create_from_size(kBufferSize));

		auto kBufferAttachment = context().create_image(resolution.x, resolution.y, context().main_window()->swap_chain_image_format(), 1, memory_usage::device, image_usage::color_attachment | image_usage::input_attachment | image_usage::sampled);
		auto minOpacityMipmap = context().create_image(resolution.x, resolution.y, vk::Format::eR32Sfloat, 1, memory_usage::device, image_usage::shader_storage | image_usage::mip_mapped);
		mAlphaCopyImage = context().create_image(resolution.x, resolution.y, context().main_window()->swap_chain_image_format(), 1, memory_usage::device, image_usage::input_attachment | image_usage::sampled);

		auto fen = context().record_and_submit_with_fence(context().gather_commands(
			sync::image_memory_barrier(minOpacityMipmap, stage::none >> stage::none).with_layout_transition(layout::undefined >> layout::general),
			sync::image_memory_barrier(kBufferAttachment, stage::none >> stage::none).with_layout_transition(layout::undefined >> layout::shader_read_only_optimal),

			context().main_window()->layout_transitions_for_all_backbuffer_images()
		), mQueue);
		fen->wait_until_signalled();
		
		kBufferAttachment.enable_shared_ownership();

		// sampler for reducing the alpha in the mip map generation
		mAlphaReduceSampler = context().create_sampler(
			filter_mode::nearest_neighbor,
			border_handling_mode::clamp_to_edge,
			16.f
		);

		auto kBufferRenderpass = context().create_renderpass(
			{
				attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load,  usage::color(0), on_store::store),
			},
			{
				subpass_dependency(subpass::external >> subpass::index(0),
									stage::compute_shader >> stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output,
									access::shader_storage_write >> access::depth_stencil_attachment_read | access::depth_stencil_attachment_write | access::color_attachment_write
								  ),
				subpass_dependency(subpass::index(0) >> subpass::external,
									stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output >> stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output,
									access::depth_stencil_attachment_write | access::color_attachment_write >> access::depth_stencil_attachment_read | access::color_attachment_read
								  )

			}

			);
		kBufferRenderpass.enable_shared_ownership();

		auto view0 = context().create_image_view(owned(kBufferAttachment));

		mKBufferFramebuffer = context().create_framebuffer(
			avk::shared(kBufferRenderpass),
			avk::make_vector(
				avk::shared(view0)
			)
		);

		auto renderpass = context().create_renderpass(
			{
				attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load,  usage::color(0), on_store::store),
				attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::load, usage::depth_stencil, on_store::store),
			},
			{ 
				subpass_dependency(subpass::external >> subpass::index(0),
									stage::compute_shader >> stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output,
									access::shader_storage_write >> access::depth_stencil_attachment_read | access::depth_stencil_attachment_write | access::color_attachment_write
								  ),
				subpass_dependency(subpass::index(0) >> subpass::external,
									stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output >> stage::early_fragment_tests | stage::late_fragment_tests | stage::color_attachment_output,
									access::depth_stencil_attachment_write | access::color_attachment_write >> access::depth_stencil_attachment_read | access::color_attachment_read
								  )

			}

		);
		renderpass.enable_shared_ownership();

		mClearStoragePass = context().create_compute_pipeline_for(
			compute_shader("shaders/clear_storage_pass.comp"),
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(0, 1, mkBuffer)
		);
		mClearStoragePass.enable_shared_ownership();

		mPipeline = context().create_graphics_pipeline_for(
			//from_buffer_binding(0)->stream_per_vertex(&Vertex::pos)->to_location(0),

			vertex_shader("shaders/passthrough.vert"),
			geometry_shader("shaders/billboard_creation.geom"),
			fragment_shader("shaders/ray_casting.frag"),

			cfg::primitive_topology::lines,
			cfg::culling_mode::disabled(), // should be disabled for k buffer rendering
			cfg::depth_test::disabled(),
			cfg::depth_write::disabled(),
			//cfg::front_face::define_front_faces_to_be_clockwise(),
			cfg::color_blending_config::enable_alpha_blending_for_all_attachments(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			
			kBufferRenderpass,

			push_constant_binding_data{ shader_type::vertex | shader_type::fragment | shader_type::geometry, 0, sizeof(int) },
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(0, 3, mVertexBuffer),
			descriptor_binding(2, 0, mkBuffer)
		);

		mResolvePass = context().create_graphics_pipeline_for(
			vertex_shader("shaders/kbuffer_resolve.vert"),
			fragment_shader("shaders/kbuffer_resolve.frag"),

			// Configuration parameters for this graphics pipeline:
			cfg::front_face::define_front_faces_to_be_counter_clockwise(),
			cfg::viewport_depth_scissors_config::from_framebuffer(
				context().main_window()->backbuffer_at_index(0) // Just use any compatible framebuffer here
			),
			cfg::depth_test::disabled(),
			cfg::color_blending_config::enable_alpha_blending_for_attachment(0),

			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load, usage::color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::clear, usage::depth_stencil, on_store::store),

			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(1, 0, mkBuffer)
		);
		mResolvePass.enable_shared_ownership();
		/*
		auto mDownsamplePipeline = context().create_compute_pipeline_for(
			compute_shader("shader/downsample_alpha"),

		);
		mDownsamplePipeline.enable_shared_ownership();
		*/
		m2DLinePipeline = context().create_graphics_pipeline_for(
			vertex_shader("shaders/2d_lines.vert"),
			fragment_shader("shaders/2d_lines.frag"),

			cfg::primitive_topology::lines,
			cfg::depth_test::disabled(),
			cfg::depth_write::disabled(),
			cfg::culling_mode::disabled(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load, usage::color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::load, usage::depth_stencil, on_store::store),
			//push_constant_binding_data{ shader_type::vertex | shader_type::fragment | shader_type::geometry, 0, sizeof(push_constants) },
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(0, 3, mVertexBuffer),

			[](graphics_pipeline_t& p) {
				p.rasterization_state_create_info().setPolygonMode(vk::PolygonMode::eLine);
			}
		);

		mSkyboxPipeline = context().create_graphics_pipeline_for(
			vertex_shader("shaders/sky_gradient.vert"),
			fragment_shader("shaders/sky_gradient.frag"),
			
			attachment::declare(context().main_window()->swap_chain_image_format(), on_load::clear, usage::color(0), on_store::store),

			cfg::front_face::define_front_faces_to_be_counter_clockwise(),
			cfg::culling_mode::disabled(),
			//cfg::depth_test::enabled().set_compare_operation(cfg::compare_operation::less_or_equal),
			cfg::depth_write::disabled(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			descriptor_binding(0, 0, mUniformBuffer) // Doesn't have to be the exact buffer, but one that describes the correct layout for the pipeline.
		);

		// Set up an updater for shader hot reload and viewport resize
		mUpdater.emplace();
		mSkyboxPipeline.enable_shared_ownership();
		m2DLinePipeline.enable_shared_ownership();
		mPipeline.enable_shared_ownership();
		mUpdater->on(swapchain_resized_event(context().main_window()))
			.update(mPipeline)
			.update(mResolvePass)
			.update(mSkyboxPipeline)
			.update(m2DLinePipeline)
			.update(mClearStoragePass)
			//.update(mSortLinesPipeline)
			.invoke([this]() { mQuakeCam->set_aspect_ratio(gvk::context().main_window()->aspect_ratio()); });
		
		mUpdater->on(shader_files_changed_event(mPipeline)).update(mPipeline);
		mUpdater->on(shader_files_changed_event(mSkyboxPipeline)).update(mSkyboxPipeline);
		mUpdater->on(shader_files_changed_event(m2DLinePipeline)).update(m2DLinePipeline);
		mUpdater->on(shader_files_changed_event(mClearStoragePass)).update(mClearStoragePass);
		mUpdater->on(shader_files_changed_event(mResolvePass)).update(mResolvePass);
		//mUpdater->on(shader_files_changed_event(mSortLinesPipeline)).update(mSortLinesPipeline);

		auto imguiManager = current_composition()->element_by_type<imgui_manager>();
		if (nullptr != imguiManager) {
			mOpenFileDialog.SetTitle("Open Line-Data File");
			mOpenFileDialog.SetTypeFilters({ ".obj" });

			imguiManager->add_callback([this]() {
				ImGui::Begin("Line Renderer - Tool Box", &mOpenToolbox, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::TextColored(ImVec4(0.f, .6f, .8f, 1.f), "[F1]: Toggle input-mode");
				if (ImGui::BeginMenuBar()) {
					if (ImGui::BeginMenu("File")) {
						if (ImGui::MenuItem("Load Data-File")) {
							mOpenFileDialog.Open();
						}
						if (ImGui::MenuItem("Exit Application", "Esc")) {
							// Exit
							current_composition()->stop(); // stop the current composition
						}
						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}
				ImGui::SetWindowPos(ImVec2(10.0F, 10.0F), ImGuiCond_Once);

				std::string fps = std::format("{:.0f} FPS", ImGui::GetIO().Framerate);
				static std::vector<float> values;
				values.push_back(ImGui::GetIO().Framerate);
				if (values.size() > 90) values.erase(values.begin());
				ImGui::PlotLines(fps.c_str(), values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 20.0f));

				ImGui::ColorEdit4("Background", mClearColor);
				ImGui::Separator();
				ImGui::Checkbox("Main Render Pass Enabled", &mMainRenderPassEnabled);
				if (mMainRenderPassEnabled) {
					ImGui::Checkbox("Billboard-Clipping", &mBillboardClippingEnabled);
				}
				ImGui::Separator();
				ImGui::Checkbox("Show Helper Lines", &mDraw2DHelperLines);
				if (mDraw2DHelperLines) {
					ImGui::Checkbox("Use vertex color for Lines", &mUseVertexColorForHelperLines);
					ImGui::ColorEdit4("Line-Color", mHelperLineColor);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("Cam");
					std::string camPos = std::format("Position:\n{}", glm::to_string(mQuakeCam->translation()));
					ImGui::Text(camPos.c_str());
					std::string camDir = std::format("Direction:\n{}", glm::to_string(mQuakeCam->rotation() * glm::vec3(0, 0, -1)));
					ImGui::Text(camDir.c_str());
					ImGui::PopID();
				}
				ImGui::End();

				// LIGHTING & MATERIALS WINDOW
				ImGui::Begin("Lighting & Material", &mOpenToolbox, ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::SetWindowPos(ImVec2(970.0F, 10.0F), ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("AL");
					ImGui::ColorEdit4("Color", mAmbLightColor);
					ImGui::PopID();
				}
				if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("DL");
					ImGui::SliderFloat("Intensity", &mDirLightIntensity, 0.0F, 10.0F);
					ImGui::ColorEdit4("Color", mDirLightColor);
					ImGui::SliderFloat3("Direction", mDirLightDirection, -1.0F, 1.0F);
					ImGui::PopID();
				}
				if (ImGui::CollapsingHeader("Material Constants", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("MC");
					ImGui::SliderFloat("Ambient", &mMaterialAmbient, 0.0F, 3.0F);
					ImGui::SliderFloat("Diffuse", &mMaterialDiffuse, 0.0F, 3.0F);
					ImGui::SliderFloat("Specular", &mMaterialSpecular, 0.0F, 3.0F);
					ImGui::SliderFloat("Shininess", &mMaterialShininess, 0.0F, 128.0F);
					ImGui::PopID();
				}
				ImGui::End();

				mOpenFileDialog.Display();
				if (mOpenFileDialog.HasSelected()) {
					// ToDo Load Data-File into buffer
					std::string filename = mOpenFileDialog.GetSelected().string();
					mOpenFileDialog.ClearSelected();
					loadDatasetFromFile(filename);
				}
			});
		}

		mQuakeCam = std::make_shared<quake_camera>();
		mQuakeCam->set_translation({ 1.1f, 1.1f, 1.1f });
		mQuakeCam->look_along({ -1.0f, -1.0f, -1.0f });
		mQuakeCam->set_move_speed(0.5);
		mQuakeCam->set_perspective_projection(glm::radians(60.0f), context().main_window()->aspect_ratio(), 0.001f, 100.0f);
		mQuakeCam->disable();
		current_composition()->add_element(*mQuakeCam);
	}

	void update() override
	{
		using namespace avk;
		using namespace gvk;
		// Escape tears everything down:
		if (gvk::input().key_pressed(key_code::escape) || glfwWindowShouldClose(context().main_window()->handle()->mHandle)) {
			current_composition()->stop(); // stop the current composition
		}

		// F1 toggles between "quake_cam mode" and "UI mode":
		if (gvk::input().key_pressed(key_code::f1)) {
			auto* quakeCam = current_composition()->element_by_type<quake_camera>();
			auto* imguiManager =  current_composition()->element_by_type<imgui_manager>();
			if (nullptr != quakeCam && nullptr != imguiManager) {
				if (quakeCam->is_enabled()) {
					quakeCam->disable();
					imguiManager->enable_user_interaction(true);
				}
				else {
					quakeCam->enable();
					imguiManager->enable_user_interaction(false);
				}
			}
		}
	}
		
	void render() override
	{
		using namespace avk;
		using namespace gvk;

		const auto resolution = context().main_window()->resolution();

		// Update UBO:
		matrices_and_user_input uni;
		uni.mViewMatrix = mQuakeCam->view_matrix();
		uni.mProjMatrix = mQuakeCam->projection_matrix();
		uni.mCamPos = glm::vec4(mQuakeCam->translation(), 1.0f);
		uni.mCamDir = glm::vec4(mQuakeCam->rotation() * glm::vec3(0, 0, -1), 0.0f);
		uni.mClearColor = glm::vec4(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]);
		uni.mHelperLineColor = glm::vec4(mHelperLineColor[0], mHelperLineColor[1], mHelperLineColor[2], mHelperLineColor[3]);
		uni.mkBufferInfo = glm::vec4(resolution.x, resolution.y, kBufferLayerCount, 0);
		uni.mUseVertexColorForHelperLines = mUseVertexColorForHelperLines;
		uni.mBillboardClippingEnabled = mBillboardClippingEnabled;

		uni.mDirLightDirection = glm::normalize(glm::vec4(mDirLightDirection[0], mDirLightDirection[1], mDirLightDirection[2], 0.0F));
		uni.mDirLightColor = mDirLightIntensity * glm::vec4(mDirLightColor[0], mDirLightColor[1], mDirLightColor[2], mDirLightColor[3]);
		uni.mAmbLightColor = glm::vec4(mAmbLightColor[0], mAmbLightColor[1], mAmbLightColor[2], mAmbLightColor[3]);
		uni.mMaterialLightReponse = glm::vec4(mMaterialAmbient, mMaterialDiffuse, mMaterialSpecular, mMaterialShininess);
		uni.mNumberOfLines = mNumberOfLines;

		buffer& cUBO = mUniformBuffer;
		cUBO->fill(&uni, 0);

		auto mainWnd = context().main_window();
		
		if (mReplaceOldBufferWithNextFrame) {
			mVertexBuffer = std::move(mNewVertexBuffer);
			mIndexBuffer = std::move(mNewIndexBuffer);
			mReplaceOldBufferWithNextFrame = false;
		}
		
		auto& commandPool = context().get_command_pool_for_single_use_command_buffers(*mQueue);
		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		
		cmdBfr->begin_recording();

			// clear k buffer
			cmdBfr->bind_pipeline(const_referenced(mClearStoragePass));
			cmdBfr->bind_descriptors(mClearStoragePass->layout(),
				mDescriptorCache.get_or_create_descriptor_sets(
					{
						descriptor_binding(0, 0, mUniformBuffer),
						descriptor_binding(0, 1, mkBuffer)
					}));
			constexpr auto WORKGROUP_SIZE = uint32_t{ 16u };
			cmdBfr->handle().dispatch((resolution.x + 15u) / WORKGROUP_SIZE, (resolution.y + 15u) / WORKGROUP_SIZE, 1);

			//sort line segments
			/*
			if (mMainRenderPassEnabled && mVertexBuffer.has_value()) {
				cmdBfr->bind_pipeline(const_referenced(mSortLinesPipeline));
				cmdBfr->bind_descriptors(mSortLinesPipeline->layout(),
					mDescriptorCache.get_or_create_descriptor_sets(
						{
							descriptor_binding(0, 0, mUniformBuffer),
							descriptor_binding(0, 1, mLineDst),
							descriptor_binding(0, 2, mSourceIndexBuffer),
							descriptor_binding(0, 3, mVertexBuffer),
							descriptor_binding(0, 4, mLineBuffer1),
							descriptor_binding(0, 5, mLineBuffer2)
						}));
				cmdBfr->handle().dispatch((mNumberOfLines + 15u) / WORKGROUP_SIZE, 1, 1);
			}/**/
			
			// Draw Skybox
			cmdBfr->begin_render_pass_for_framebuffer(mSkyboxPipeline->get_renderpass(), mKBufferFramebuffer);
			cmdBfr->bind_pipeline(const_referenced(mSkyboxPipeline));
			cmdBfr->bind_descriptors(mSkyboxPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
				descriptor_binding(0, 0, mUniformBuffer)
				}));
			cmdBfr->handle().draw(6u, 2u, 0u, 0u);
			cmdBfr->end_render_pass();
			

			// Draw tubes
			cmdBfr->begin_render_pass_for_framebuffer(mPipeline->get_renderpass(), mKBufferFramebuffer);
			cmdBfr->bind_pipeline(avk::const_referenced(mPipeline));
			cmdBfr->bind_descriptors(mPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
				descriptor_binding(0, 0, mUniformBuffer),
				descriptor_binding(0, 3, mVertexBuffer),
				descriptor_binding(2, 0, mkBuffer)
			}));
			// NOTE: We can't completely skip the renderpass as it initialices the back and depth buffer! So I'll just skip drawing the vertices.
			if (mMainRenderPassEnabled && mVertexBuffer.has_value()) {

				cmdBfr->draw_indexed(const_referenced(mIndexBuffer), const_referenced(mVertexBuffer));
				
				// TODO: generate mips
				/*
				sync::image_memory_barrier(context().main_window()->current_backbuffer()->image_at(0),
					stage::color_attachment_output >> stage::blit,
					access::color_attachment_write >> access::transfer_read
				).with_layout_transition(layout::general >> layout::transfer_src),

				sync::image_memory_barrier(context().main_window()->current_backbuffer()->image_at(0), // Color attachment
					stage::none >> stage::blit,
					access::none >> access::transfer_write
				).with_layout_transition(layout::present_src >> layout::transfer_dst),

				blit_image(
					context().main_window()->current_backbuffer()->image_at(0), layout::transfer_src,
					context().main_window()->current_backbuffer()->image_at(0), layout::transfer_dst,
					vk::ImageAspectFlagBits::e
				),
				*/
			}

			cmdBfr->end_render_pass();

			cmdBfr->copy_image(mKBufferFramebuffer->image_at(0).get(), context().main_window()->current_backbuffer()->image_at(0)->handle());
			
			cmdBfr->begin_render_pass_for_framebuffer(mResolvePass->get_renderpass(), context().main_window()->current_backbuffer());
			cmdBfr->bind_pipeline(const_referenced(mResolvePass));
			cmdBfr->bind_descriptors(mResolvePass->layout(), mDescriptorCache.get_or_create_descriptor_sets({
					descriptor_binding(0, 0, mUniformBuffer),
					descriptor_binding(1, 0, mkBuffer)
			}));
			cmdBfr->handle().draw(6u, 1u, 0u, 1u);

			cmdBfr->end_render_pass();
			/**/
			// Draw helper lines
			if (mDraw2DHelperLines && mVertexBuffer.has_value()) {
				cmdBfr->begin_render_pass_for_framebuffer(m2DLinePipeline->get_renderpass(), context().main_window()->current_backbuffer());
				cmdBfr->bind_pipeline(avk::const_referenced(m2DLinePipeline));
				cmdBfr->bind_descriptors(m2DLinePipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
					descriptor_binding(0, 0, mUniformBuffer),
					descriptor_binding(0, 3, mVertexBuffer)
				}));
				cmdBfr->draw_indexed(const_referenced(mIndexBuffer), const_referenced(mVertexBuffer));
				/*
				int i = 0;
				for (draw_call_t draw_call : mDrawCalls)
				{
					i++;
					cmdBfr->push_constants(mPipeline->layout(), i);
					cmdBfr->draw_vertices(draw_call.numberOfPrimitives, 1u, draw_call.firstIndex, 1u, const_referenced(mVertexBuffer));
				}*/
				cmdBfr->end_render_pass();
			}
			
		cmdBfr->end_recording();

		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(owned(cmdBfr));
	}

private:

	std::unique_ptr<Dataset> mDataset;

	avk::queue* mQueue;
	avk::graphics_pipeline mPipeline;
	avk::graphics_pipeline mResolvePass;
	avk::graphics_pipeline m2DLinePipeline;
	avk::graphics_pipeline mSkyboxPipeline;
	avk::compute_pipeline mSortLinesPipeline;
	avk::compute_pipeline mClearStoragePass;

	avk::image mAlphaCopyImage;
	avk::sampler mAlphaReduceSampler;

	avk::buffer mVertexBuffer;
	avk::buffer mNewVertexBuffer;
	avk::buffer mkBuffer;
	avk::buffer mIndexBuffer;
	avk::buffer mNewIndexBuffer;
	bool mReplaceOldBufferWithNextFrame = false;

	avk::framebuffer mKBufferFramebuffer;

	std::shared_ptr<gvk::quake_camera> mQuakeCam;
	avk::buffer mUniformBuffer;
	std::vector<draw_call_t> mDrawCalls;

	/** One descriptor cache to use for allocating all the descriptor sets from: */
	avk::descriptor_cache mDescriptorCache;

	float mClearColor[4] = { 1.0F, 1.0F, 1.0F, 0.8F };
	ImGui::FileBrowser mOpenFileDialog;
	bool mOpenToolbox = true;

	bool mDraw2DHelperLines = false;
	bool mUseVertexColorForHelperLines = false;
	bool mBillboardClippingEnabled = true;
	float mHelperLineColor[4] = { 64.0f / 255.0f, 224.0f / 255.0f, 208.0f / 255.0f, 1.0f };

	bool mMainRenderPassEnabled = true;

	float mDirLightDirection[3] = { -0.7F, -0.6F, -0.3F };
	float mDirLightColor[4] = { 1.0F, 1.0F, 1.0F, 1.0F };
	float mDirLightIntensity = 1.0F;
	float mAmbLightColor[4] = { 0.05F, 0.05F, 0.05F };
	float mMaterialAmbient = 0.5;
	float mMaterialDiffuse = 1.0;
	float mMaterialSpecular = 0.5;
	float mMaterialShininess = 32.0;
	int mNumberOfLines = 0;

};

int main()
{

	int result = EXIT_FAILURE;

	static const char* titel = "Vis2 - Line Renderer";

	try {
		// Create a window, set some configuration parameters (also relevant for its swap chain), and open it:
		auto mainWnd = gvk::context().create_window(titel);
		mainWnd->set_resolution({ 1280, 800 });
		mainWnd->set_additional_back_buffer_attachments({
			avk::attachment::declare(vk::Format::eD32Sfloat, avk::on_load::clear, avk::usage::depth_stencil, avk::on_store::dont_care)
		});
		mainWnd->enable_resizing(true);
		mainWnd->request_srgb_framebuffer(true);
		mainWnd->set_presentaton_mode(gvk::presentation_mode::fifo);
		mainWnd->set_number_of_concurrent_frames(1u); // lets not make it more complex than it has to be xD
		mainWnd->open();

		// Create one single queue which we will submit all command buffers to:
		// (We pass the mainWnd because also presentation shall be submitted to this queue)
		auto& singleQueue = gvk::context().create_queue({}, avk::queue_selection_preference::versatile_queue, mainWnd);
		mainWnd->add_queue_family_ownership(singleQueue);
		mainWnd->set_present_queue(singleQueue);

		auto app = line_renderer_app(singleQueue);
		auto ui = gvk::imgui_manager(singleQueue);

		// Pass everything to gvk::start and off we go:
		start(
			gvk::application_name(titel),
			mainWnd,
			app,
			ui
		);


		result = EXIT_SUCCESS;
	}
	catch (gvk::logic_error&) {}
	catch (gvk::runtime_error&) {}
	catch (avk::logic_error&) {}
	catch (avk::runtime_error&) {}

	return result;
}
