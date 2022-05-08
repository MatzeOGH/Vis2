#include <gvk.hpp>
#include <imgui.h>
#include "utils/imfilebrowser.h"

#include <iostream>
#include <sstream>


class line_renderer_app : public gvk::invokee 
{

	struct Vertex {
		glm::vec3 pos;
		glm::vec4 color;
		float radius;
	};

	const std::vector<Vertex> mVertexData = {
		{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f},
		{{10.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 2.0f}
	};

	struct matrices_and_user_input {
		glm::mat4 mViewMatrix;
		glm::mat4 mProjMatrix;
		glm::vec4 mCamPos;
		glm::vec4 mClearColor;
	};

public:

	line_renderer_app(avk::queue& aQueue)
		: mQueue{ &aQueue } {}
		
	void initialize() override
	{
		using namespace avk;
		using namespace gvk;

		mVertexBuffer = context().create_buffer(memory_usage::device, {}, vertex_buffer_meta::create_from_data(mVertexData));
		mVertexBuffer->fill(mVertexData.data(), 0, sync::wait_idle());

		mUniformBuffer = context().create_buffer(memory_usage::host_visible, {}, uniform_buffer_meta::create_from_size(sizeof(matrices_and_user_input)));
		mUniformBuffer->fill(mVertexData.data(), 0, sync::wait_idle());

		// Create a descriptor cache that helps us to conveniently create descriptor sets:
		mDescriptorCache = gvk::context().create_descriptor_cache();


		mPipeline = context().create_graphics_pipeline_for(
			from_buffer_binding(0)->stream_per_vertex(&Vertex::pos)->to_location(0),
			from_buffer_binding(0)->stream_per_vertex(&Vertex::color)->to_location(1),
			from_buffer_binding(0)->stream_per_vertex(&Vertex::radius)->to_location(2),

			vertex_shader("shaders/passthrough.vert"),
			geometry_shader("shaders/billboard_creation.geom"),
			fragment_shader("shaders/ray_casting.frag"),

			cfg::primitive_topology::lines,

			cfg::culling_mode::disabled(), // should be enabled, just for debugging
			//cfg::front_face::define_front_faces_to_be_clockwise(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::clear, color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::clear, depth_stencil(0), on_store::store),
			//push_constant_binding_data{ shader_type::vertex | shader_type::fragment | shader_type::geometry, 0, sizeof(push_constants) },
			descriptor_binding(0, 0, mUniformBuffer)
		);

		mSkyboxPipeline = context().create_graphics_pipeline_for(
			vertex_shader("shaders/sky_gradient.vert"),
			fragment_shader("shaders/sky_gradient.frag"),
			attachment::declare(format_from_window_color_buffer(context().main_window()), on_load::load, color(0), on_store::store),
			attachment::declare(format_from_window_depth_buffer(context().main_window()), on_load::load, depth_stencil(0), on_store::store),
			cfg::culling_mode::disabled(),
			cfg::depth_test::enabled().set_compare_operation(cfg::compare_operation::less_or_equal),
			cfg::depth_write::disabled(),
			cfg::viewport_depth_scissors_config::from_framebuffer(context().main_window()->backbuffer_at_index(0)),
			descriptor_binding(0, 0, mUniformBuffer) // Doesn't have to be the exact buffer, but one that describes the correct layout for the pipeline.
		);


		//mIndexBuffer = context().create_buffer(memory_usage::device, {}, index_buffer_meta::create_from_data(mIndices));
		//mIndexBuffer->fill(mIndices.data(), 0, sync::wait_idle());

		// Set up an updater for shader hot reload and viewport resize
		mUpdater.emplace();
		mSkyboxPipeline.enable_shared_ownership();
		mPipeline.enable_shared_ownership();
		mUpdater->on(swapchain_resized_event(context().main_window()))
			.update(mPipeline)
			.update(mSkyboxPipeline)
			.invoke([this]() { mQuakeCam->set_aspect_ratio(gvk::context().main_window()->aspect_ratio()); });
		
		mUpdater->on(shader_files_changed_event(mPipeline)).update(mPipeline);
		mUpdater->on(shader_files_changed_event(mSkyboxPipeline)).update(mSkyboxPipeline);

		auto imguiManager = current_composition()->element_by_type<imgui_manager>();
		if (nullptr != imguiManager) {
			mOpenFileDialog.SetTitle("Open Line-Data File");
			mOpenFileDialog.SetTypeFilters({ ".dat" });

			imguiManager->add_callback([this]() {
				ImGui::Begin("Line Renderer - Tool Box", &mOpenToolbox, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
				ImGui::TextColored(ImVec4(0.f, .6f, .8f, 1.f), "[F1]: Toggle input-mode");
				if (ImGui::BeginMenuBar()) {
					if (ImGui::BeginMenu("File")) {
						if (ImGui::MenuItem("Load Data-File", "Ctrl+O")) {
							mOpenFileDialog.Open();
						}
						if (ImGui::MenuItem("Exit Application", "Alt+F4")) {
							// Exit
							current_composition()->stop(); // stop the current composition
						}
						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}
				ImGui::SetWindowPos(ImVec2(1.0F, 1.0F), ImGuiCond_FirstUseEver);

				std::string fps = std::format("{:.0f} FPS", ImGui::GetIO().Framerate);
				static std::vector<float> values;
				values.push_back(ImGui::GetIO().Framerate);
				if (values.size() > 90) values.erase(values.begin());
				ImGui::PlotLines(fps.c_str(), values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 20.0f));

				ImGui::ColorEdit4("Background", mClearColor);

				ImGui::Separator();
				ImGui::End();

				mOpenFileDialog.Display();
				if (mOpenFileDialog.HasSelected()) {
					// ToDo Load Data-File into buffer
					std::cout << "Selected Filename: " << mOpenFileDialog.GetSelected().string() << std::endl;
					mOpenFileDialog.ClearSelected();
				}
			});
		}

		mQuakeCam = std::make_shared<quake_camera>();
		mQuakeCam->set_translation({ 0.0f, 0.0f, 5.0f });
		mQuakeCam->look_along({ 0.0f, 0.0f, -1.0f });
		mQuakeCam->set_perspective_projection(glm::radians(60.0f), context().main_window()->aspect_ratio(), 0.1f, 10000.0f);
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
		// Update UBO:
		matrices_and_user_input uni;
		uni.mViewMatrix = mQuakeCam->view_matrix();
		uni.mProjMatrix = mQuakeCam->projection_matrix();
		uni.mCamPos = glm::vec4(mQuakeCam->translation(), 1.0f);
		uni.mClearColor = glm::vec4(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]);

		buffer& cUBO = mUniformBuffer;
		cUBO->fill(&uni, 0, sync::not_required());

		auto mainWnd = context().main_window();
		
		
		auto& commandPool = context().get_command_pool_for_single_use_command_buffers(*mQueue);
		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		
		cmdBfr->begin_recording();

			// Draw lines
			cmdBfr->begin_render_pass_for_framebuffer(mPipeline->get_renderpass(), context().main_window()->current_backbuffer());
			cmdBfr->bind_pipeline(avk::const_referenced(mPipeline));
			cmdBfr->bind_descriptors(mPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
				descriptor_binding(0, 0, mUniformBuffer)
			}));
			cmdBfr->draw_vertices(const_referenced(mVertexBuffer));
			cmdBfr->end_render_pass();

			// Draw Skybox
			cmdBfr->begin_render_pass_for_framebuffer(mSkyboxPipeline->get_renderpass(), context().main_window()->current_backbuffer());
			cmdBfr->bind_pipeline(const_referenced(mSkyboxPipeline));
			cmdBfr->bind_descriptors(mPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
				descriptor_binding(0, 0, mUniformBuffer)
				}));
			cmdBfr->handle().draw(6u, 2u, 0u, 0u);
			cmdBfr->end_render_pass();

		cmdBfr->end_recording();

		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(owned(cmdBfr));
	}

private:

	avk::queue* mQueue;
	avk::graphics_pipeline mPipeline;
	avk::graphics_pipeline mSkyboxPipeline;
	avk::buffer mVertexBuffer;

	std::shared_ptr<gvk::quake_camera> mQuakeCam;
	avk::buffer mUniformBuffer;

	/** One descriptor cache to use for allocating all the descriptor sets from: */
	avk::descriptor_cache mDescriptorCache;

	float mClearColor[4] = { 0.0F, 0.0F, 0.0F, 1.0F };
	ImGui::FileBrowser mOpenFileDialog;
	bool mOpenToolbox = true;

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
			avk::attachment::declare(vk::Format::eD32Sfloat, avk::on_load::clear, avk::depth_stencil(), avk::on_store::dont_care)
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
