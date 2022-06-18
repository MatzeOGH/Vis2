
#include <gvk.hpp>
#include <imgui.h>
#include <imfilebrowser.h>

#include <iostream>
#include <sstream>

#include <glm/gtx/string_cast.hpp>

#include "helper_functions.h"
#include "host_structures.h"
#include "Dataset.h"

#include <algorithm>

/// <summary>
/// The maximum number of Layers inside the k buffer
/// </summary>
constexpr size_t kBufferLayerCount = 16;

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
		mDataset->fillGPUReadyBuffer(gpuVertexData, mDrawCalls);

		// Create new GPU Buffer
		mNewVertexBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer, storage_buffer_meta::create_from_data(gpuVertexData), vertex_buffer_meta::create_from_data(gpuVertexData));
		mNewVertexBuffer.enable_shared_ownership();

		// Fill them and wait for completion
		auto fenc = context().record_and_submit_with_fence(
			{
				mNewVertexBuffer->fill(gpuVertexData.data(), 0)
			},
			mQueue
		);
		fenc->wait_until_signalled();

		mReplaceOldBufferWithNextFrame = true;
	}

	void resetCamera() {
		mQuakeCam->set_translation({ 1.1f, 1.1f, 1.1f });
		mQuakeCam->look_along({ -1.0f, -1.0f, -1.0f });
	}
		
	void initialize() override
	{
		using namespace avk;
		using namespace gvk;

		mDataset = std::make_unique<Dataset>();

		const auto resolution = context().main_window()->resolution();

		// Create an uniform buffer for per frame user input 
		mUniformBuffer = context().create_buffer(memory_usage::host_visible, {}, uniform_buffer_meta::create_from_size(sizeof(matrices_and_user_input)));

		// Create a descriptor cache that helps us to conveniently create descriptor sets:
		mDescriptorCache = gvk::context().create_descriptor_cache();

		std::vector<Vertex> gpuVertexData = { {} };
		mVertexBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer, storage_buffer_meta::create_from_data(gpuVertexData), vertex_buffer_meta::create_from_data(gpuVertexData));
		mVertexBuffer.enable_shared_ownership();

		// Create a storage buffer for the k buffer
		const size_t kBufferSize = resolution.x * resolution.y * kBufferLayerCount * sizeof(uint64_t);
		mkBuffer = context().create_buffer(memory_usage::device, vk::BufferUsageFlagBits::eStorageBuffer, storage_buffer_meta::create_from_size(kBufferSize));

		auto renderpass = context().create_renderpass(
			{
				attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load,  usage::color(0), on_store::store.in_layout(layout::attachment_optimal)),
				attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::load, usage::depth_stencil, on_store::store.in_layout(layout::attachment_optimal)),
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
			from_buffer_binding(0)->stream_per_vertex(&Vertex::pos)->to_location(0),
			from_buffer_binding(0)->stream_per_vertex(&Vertex::radius)->to_location(2),


			vertex_shader("shaders/passthrough.vert"),
			geometry_shader("shaders/billboard_creation.geom"),
			fragment_shader("shaders/ray_casting.frag"),

			cfg::primitive_topology::line_strip_with_adjacency,
			cfg::culling_mode::disabled(), // should be disabled for k buffer rendering
			cfg::depth_test::disabled(),
			cfg::depth_write::disabled(),
			//cfg::front_face::define_front_faces_to_be_clockwise(),
			cfg::color_blending_config::enable_alpha_blending_for_all_attachments(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			
			renderpass,

			push_constant_binding_data{ shader_type::vertex | shader_type::fragment | shader_type::geometry, 0, sizeof(int) },
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(2, 0, mkBuffer)
		);
		mPipeline.enable_shared_ownership();

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

			renderpass,
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(1, 0, mkBuffer)
		);
		mResolvePass.enable_shared_ownership();

		m2DLinePipeline = context().create_graphics_pipeline_for(
			from_buffer_binding(0)->stream_per_vertex(&Vertex::pos)->to_location(0),

			vertex_shader("shaders/2d_lines.vert"),
			fragment_shader("shaders/2d_lines.frag"),

			cfg::primitive_topology::line_strip_with_adjacency,
			cfg::depth_test::disabled(),
			cfg::depth_write::disabled(),
			cfg::culling_mode::disabled(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load, usage::color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::load, usage::depth_stencil, on_store::store),
			descriptor_binding(0, 0, mUniformBuffer),

			[](graphics_pipeline_t& p) {
				p.rasterization_state_create_info().setPolygonMode(vk::PolygonMode::eLine);
			}
		);

		// Create the pipeline for the background render pass
		mBackgroundPipeline = context().create_graphics_pipeline_for(
			vertex_shader("shaders/background.vert"),
			fragment_shader("shaders/background.frag"),

			// define attachments
			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::clear, usage::color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::clear, usage::depth_stencil, on_store::store),

			cfg::front_face::define_front_faces_to_be_counter_clockwise(),
			cfg::culling_mode::disabled(),
			cfg::depth_write::disabled(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),

			descriptor_binding(0, 0, mUniformBuffer) // Doesn't have to be the exact buffer, but one that describes the correct layout for the pipeline.
		);
		mBackgroundPipeline.enable_shared_ownership();

		// Set up an updater for shader hot reload and viewport resize
		mUpdater.emplace();
		m2DLinePipeline.enable_shared_ownership();
		mUpdater->on(swapchain_resized_event(context().main_window()))
			.update(mPipeline)
			.update(mResolvePass)
			.update(mBackgroundPipeline)
			.update(m2DLinePipeline)
			.update(mClearStoragePass)
			.invoke([this]() { mQuakeCam->set_aspect_ratio(gvk::context().main_window()->aspect_ratio()); });
		
		mUpdater->on(shader_files_changed_event(mPipeline)).update(mPipeline);
		mUpdater->on(shader_files_changed_event(mBackgroundPipeline)).update(mBackgroundPipeline);
		mUpdater->on(shader_files_changed_event(m2DLinePipeline)).update(m2DLinePipeline);
		mUpdater->on(shader_files_changed_event(mClearStoragePass)).update(mClearStoragePass);
		mUpdater->on(shader_files_changed_event(mResolvePass)).update(mResolvePass);

		auto imguiManager = current_composition()->element_by_type<imgui_manager>();

		if (nullptr != imguiManager) {
			mOpenFileDialog.SetTitle("Open Line-Data File");
			mOpenFileDialog.SetTypeFilters({ ".obj" });

			imguiManager->add_callback([this]() {
				ImGui::Begin("Line Renderer - Tool Box", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_AlwaysAutoResize);
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
					ImGui::ColorEdit4("Line-Color", mHelperLineColor);
				}
				ImGui::Separator();
				ImGui::Checkbox("Resolve K-Buffer", &mResolveKBuffer);
				ImGui::SliderInt("K-Buffer Layer", &mKBufferLayer, 0, kBufferLayerCount);
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Camera")) {
					ImGui::PushID("Cam");
					std::string camPos = std::format("Position: {}", vec3ToString(mQuakeCam->translation()));
					ImGui::Text(camPos.c_str());
					std::string camDir = std::format("Direction: {}", vec3ToString(mQuakeCam->rotation() * glm::vec3(0, 0, -1)));
					ImGui::Text(camDir.c_str());
					if (ImGui::Button("Reset Camera"))
						resetCamera();
					ImGui::PopID();
				}
				ImGui::End();

				// LIGHTING & MATERIALS WINDOW
				ImGui::Begin("Lighting & Material", NULL, ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::SetWindowPos(ImVec2(970.0F, 10.0F), ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Ambient Light", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit4("Color", mAmbLightColor);
				}
				if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::SliderFloat("Intensity", &mDirLightIntensity, 0.0F, 10.0F);
					ImGui::ColorEdit4("Color", mDirLightColor);
					ImGui::SliderFloat3("Direction", mDirLightDirection, -1.0F, 1.0F);
				}
				if (ImGui::CollapsingHeader("Material Constants", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::SliderFloat3("Amb/Dif/Spec", mMaterialReflectivity, 0.0F, 3.0F);
					ImGui::SliderFloat("Shininess", &mMaterialReflectivity[3], 0.0F, 128.0F);
				}
				
				if (ImGui::CollapsingHeader("Vertex Color", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("VC");
					const char* vertexColorModes[] = { "Static", "Velocity dependent", "Length dependent", "per Polyline", "per Line"};
					ImGui::Combo("Mode", &mVertexColorMode, vertexColorModes, IM_ARRAYSIZE(vertexColorModes));
					if (mVertexColorMode == 0) {
						ImGui::ColorEdit3("Color", mVertexColorStatic);
					}
					else if (mVertexColorMode < 3) {
						ImGui::ColorEdit3("Min-Color", mVertexColorMin);
						ImGui::ColorEdit3("Max-Color", mVertexColorMax);
					}
					if (ImGui::Button("Invert colors")) {
						float colorBuffer[3];
						std::copy(mVertexColorMin, mVertexColorMin + 3, colorBuffer);
						std::copy(mVertexColorMax, mVertexColorMax + 3, mVertexColorMin);
						std::copy(colorBuffer, colorBuffer + 3, mVertexColorMax);
					}
					ImGui::PopID();
				}
				if (ImGui::CollapsingHeader("Vertex Transparency", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("VT");
					const char* vertexAlphaModes[] = { "Static", "Velocity dependent", "Length dependent" };
					ImGui::Combo("Mode", &mVertexAlphaMode, vertexAlphaModes, IM_ARRAYSIZE(vertexAlphaModes));
					if (mVertexAlphaMode == 0) ImGui::SliderFloat("Alpha", &mVertexAlphaStatic, 0.0F, 1.0F);
					else ImGui::SliderFloat2("Bounds", mVertexAlphaBounds, 0.0F, 1.0F);
					ImGui::Checkbox("Invert", &mVertexAlphaInvert);
					ImGui::PopID();
				}
				if (ImGui::CollapsingHeader("Vertex Radius", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushID("VR");
					const char* vertexRadiusModes[] = { "Static", "Velocity dependent", "Length dependent" };
					ImGui::Combo("Mode", &mVertexRadiusMode, vertexRadiusModes, IM_ARRAYSIZE(vertexRadiusModes));
					if (mVertexRadiusMode == 0) ImGui::SliderFloat("Alpha", &mVertexRadiusStatic, 0.0F, 1.0F);
					else ImGui::SliderFloat2("Bounds", mVertexRadiusBounds, 0.0F, 0.02F, "%.4f");
					ImGui::Checkbox("Invert", &mVertexRadiusInvert);
					ImGui::PopID();
				}
				ImGui::End();

				mOpenFileDialog.Display();
				if (mOpenFileDialog.HasSelected()) {
					// ToDo Load Data-File into buffer
					std::string filename = mOpenFileDialog.GetSelected().string();
					mOpenFileDialog.ClearSelected();
					this->loadDatasetFromFile(filename);
				}
			});

			
		}

		mQuakeCam = std::make_shared<quake_camera>();
		resetCamera();
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

		// The ImGui-Context had to be created, thats why its here
		if (mRenderCallCount == 0) activateImGuiStyle(false, 0.7);

		// Update UBO:
		matrices_and_user_input uni;
		uni.mViewMatrix = mQuakeCam->view_matrix();
		uni.mProjMatrix = mQuakeCam->projection_matrix();
		uni.mCamPos = glm::vec4(mQuakeCam->translation(), 1.0f);
		uni.mCamDir = glm::vec4(mQuakeCam->rotation() * glm::vec3(0, 0, -1), 0.0f);
		uni.mClearColor = glm::make_vec4(mClearColor);
		uni.mHelperLineColor = glm::make_vec4(mHelperLineColor);
		uni.mkBufferInfo = glm::vec4(resolution.x, resolution.y, mKBufferLayer, 0);
		uni.mDirLightDirection = glm::normalize(glm::vec4(glm::make_vec3(mDirLightDirection), 0.0f));
		uni.mDirLightColor = mDirLightIntensity * glm::make_vec4(mDirLightColor);
		uni.mAmbLightColor = glm::make_vec4(mAmbLightColor);
		uni.mMaterialLightReponse = glm::make_vec4(mMaterialReflectivity);
		uni.mBillboardClippingEnabled = mBillboardClippingEnabled;
		uni.mVertexColorMode = mVertexColorMode;
		if (mVertexColorMode == 0)
			uni.mVertexColorMin = glm::vec4(glm::make_vec3(mVertexColorStatic), 0.0F);
		else {
			uni.mVertexColorMin = glm::vec4(glm::make_vec3(mVertexColorMin), 0.0F);
			uni.mVertexColorMax = glm::vec4(glm::make_vec3(mVertexColorMax), 0.0F);
		}

		uni.mVertexAlphaMode = mVertexAlphaMode;
		if (mVertexAlphaMode == 0)
			uni.mVertexAlphaBounds[0] = mVertexAlphaStatic;
		else
			uni.mVertexAlphaBounds = glm::vec4(glm::make_vec2(mVertexAlphaBounds), 0.0F, 0.0F);

		uni.mVertexRadiusMode = mVertexRadiusMode;
		if (mVertexRadiusMode == 0)
			uni.mVertexRadiusBounds[0] = mVertexRadiusStatic;
		else
			uni.mVertexRadiusBounds = glm::vec4(glm::make_vec2(mVertexRadiusBounds), 0.0F, 0.0F);
		uni.mDataMaxLineLength = mDataset->getMaxLineLength();
		uni.mDataMaxVertexAdjacentLineLength = mDataset->getMaxVertexAdjacentLineLength();
		uni.mVertexAlphaInvert = mVertexAlphaInvert;
		uni.mVertexRadiusInvert = mVertexRadiusInvert;

		buffer& cUBO = mUniformBuffer;
		cUBO->fill(&uni, 0);

		auto mainWnd = context().main_window();
		
		if (mReplaceOldBufferWithNextFrame) {
			mVertexBuffer = std::move(mNewVertexBuffer);
			mReplaceOldBufferWithNextFrame = false;
		}
		
		auto& commandPool = context().get_command_pool_for_single_use_command_buffers(*mQueue);
		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		
		cmdBfr->begin_recording();

		// clear the k buffer 
		cmdBfr->bind_pipeline(const_referenced(mClearStoragePass));
		cmdBfr->bind_descriptors(mClearStoragePass->layout(),
			mDescriptorCache.get_or_create_descriptor_sets(
				{
					descriptor_binding(0, 0, mUniformBuffer),
					descriptor_binding(0, 1, mkBuffer)
				}));
		constexpr auto WORKGROUP_SIZE = uint32_t{ 16u };
		cmdBfr->handle().dispatch((resolution.x + 15u) / WORKGROUP_SIZE, (resolution.y + 15u) / WORKGROUP_SIZE, 1);

		// Draw Skybox
		cmdBfr->begin_render_pass_for_framebuffer(mBackgroundPipeline->get_renderpass(), context().main_window()->current_backbuffer());
		cmdBfr->bind_pipeline(const_referenced(mBackgroundPipeline));
		cmdBfr->bind_descriptors(mBackgroundPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
			descriptor_binding(0, 0, mUniformBuffer)
			}));
		cmdBfr->handle().draw(6u, 2u, 0u, 0u);
		cmdBfr->end_render_pass();
			

		// Draw lines
		cmdBfr->begin_render_pass_for_framebuffer(mPipeline->get_renderpass(), context().main_window()->current_backbuffer());
		cmdBfr->bind_pipeline(avk::const_referenced(mPipeline));
		cmdBfr->bind_descriptors(mPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
			descriptor_binding(0, 0, mUniformBuffer),
			descriptor_binding(2, 0, mkBuffer)
		}));

		// Draw the line primitives
		if (mMainRenderPassEnabled && mVertexBuffer.has_value()) {
			int i = 0;
			for (draw_call_t draw_call : mDrawCalls)
			{
				i++;
				cmdBfr->push_constants(mPipeline->layout(), i);
				cmdBfr->draw_vertices(draw_call.numberOfPrimitives, 1u, draw_call.firstIndex, 1u, const_referenced(mVertexBuffer));
			}
		}
		cmdBfr->end_render_pass();

		// Resolve the k buffer and blend it with the rest of the framebuffer
		if (mResolveKBuffer)
		{
			cmdBfr->begin_render_pass_for_framebuffer(mResolvePass->get_renderpass(), context().main_window()->current_backbuffer());
			cmdBfr->bind_pipeline(const_referenced(mResolvePass));
			cmdBfr->bind_descriptors(mResolvePass->layout(), mDescriptorCache.get_or_create_descriptor_sets({
					descriptor_binding(0, 0, mUniformBuffer),
					descriptor_binding(1, 0, mkBuffer)
				}));
			cmdBfr->handle().draw(6u, 1u, 0u, 1u);
			cmdBfr->end_render_pass();
		}
			
		// Draw helper lines
		if (mDraw2DHelperLines && mVertexBuffer.has_value()) {
			cmdBfr->begin_render_pass_for_framebuffer(m2DLinePipeline->get_renderpass(), context().main_window()->current_backbuffer());
			cmdBfr->bind_pipeline(avk::const_referenced(m2DLinePipeline));
			cmdBfr->bind_descriptors(m2DLinePipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
				descriptor_binding(0, 0, mUniformBuffer)
				}));

			int i = 0;
			for (draw_call_t draw_call : mDrawCalls)
			{
				i++;
				cmdBfr->push_constants(mPipeline->layout(), i);
				cmdBfr->draw_vertices(draw_call.numberOfPrimitives, 1u, draw_call.firstIndex, 1u, const_referenced(mVertexBuffer));
			}
			cmdBfr->end_render_pass();
		}
		cmdBfr->end_recording();

		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(owned(cmdBfr));

		mRenderCallCount++;
	}

private:

	std::unique_ptr<Dataset> mDataset;

	avk::queue* mQueue;
	avk::graphics_pipeline mPipeline;
	avk::graphics_pipeline mResolvePass;
	avk::graphics_pipeline m2DLinePipeline;
	avk::graphics_pipeline mBackgroundPipeline;
	avk::compute_pipeline mClearStoragePass;

	avk::buffer mVertexBuffer;
	avk::buffer mNewVertexBuffer;
	avk::buffer mkBuffer;
	bool mReplaceOldBufferWithNextFrame = false;

	std::shared_ptr<gvk::quake_camera> mQuakeCam;
	avk::buffer mUniformBuffer;
	std::vector<draw_call_t> mDrawCalls;

	/** One descriptor cache to use for allocating all the descriptor sets from: */
	avk::descriptor_cache mDescriptorCache;

	float mClearColor[4] = { 1.0F, 1.0F, 1.0F, 0.8F };
	ImGui::FileBrowser mOpenFileDialog;

	bool mDraw2DHelperLines = false;
	bool mBillboardClippingEnabled = true;
	float mHelperLineColor[4] = { 64.0f / 255.0f, 224.0f / 255.0f, 208.0f / 255.0f, 1.0f };

	bool mMainRenderPassEnabled = true;

	float mDirLightDirection[3] = { -0.7F, -0.6F, -0.3F };
	float mDirLightColor[4] = { 1.0F, 1.0F, 1.0F, 1.0F };
	float mDirLightIntensity = 1.0F;
	float mAmbLightColor[4] = { 0.05F, 0.05F, 0.05F };
	float mMaterialReflectivity[4] = { 0.5, 1.0, 0.5, 32.0 };

	int mVertexColorMode = 0;
	float mVertexColorStatic[3] = { 65.0F / 255.0F, 105.0F / 255.0F, 225.0F / 255.0F };
	float mVertexColorMin[3] = { 1.0F / 255.0F, 169.0F / 255.0F, 51.0F / 255.0F };
	float mVertexColorMax[3] = { 1.0F, 0.0F, 0.0F };

	int mVertexAlphaMode = 0;
	bool mVertexAlphaInvert = false;
	float mVertexAlphaStatic = 0.5;
	float mVertexAlphaBounds[2] = { 0.2, 0.8 };

	int mVertexRadiusMode = 0;
	bool mVertexRadiusInvert = false;
	float mVertexRadiusStatic = 0.001;
	float mVertexRadiusBounds[2] = { 0.0005, 0.002 };

	long mRenderCallCount = 0;

	bool mResolveKBuffer = true;
	int mKBufferLayer = 8;
};

int main()
{

	int result = EXIT_FAILURE;

	static const char* titel = "Vis2 - Line Renderer";

	try {
		// Create a window, set some configuration parameters (also relevant for its swap chain), and open it:
		auto mainWnd = gvk::context().create_window(titel);
		mainWnd->set_resolution({ 1280, 768 });
		//mainWnd->set_resolution({ 1920, 1080 });

		mainWnd->set_additional_back_buffer_attachments({
			avk::attachment::declare(vk::Format::eD32Sfloat, avk::on_load::clear, avk::usage::depth_stencil, avk::on_store::dont_care)
		});
		mainWnd->enable_resizing(true);
		mainWnd->request_srgb_framebuffer(true);
		mainWnd->set_presentaton_mode(gvk::presentation_mode::fifo);
		mainWnd->set_number_of_concurrent_frames(1u); // lets not make it more complex than it has to be xD
		mainWnd->open();

		// Change Icon of the Window
		if (std::filesystem::exists({ "icon_small.png" })) {
			GLFWimage images[1];
			images[0].pixels = stbi_load("icon_small.png", &images[0].width, &images[0].height, 0, 4);
			glfwSetWindowIcon(mainWnd->handle()->mHandle, 1, images);
			stbi_image_free(images[0].pixels);
		}

		//mainWnd->switch_to_fullscreen_mode();

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
