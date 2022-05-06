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
		{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 0.1f},
		{{0.5f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f}
	};

	struct matrices_and_user_input {
		glm::mat4 mViewMatrix;
		glm::mat4 mProjMatrix;
		glm::vec4 mCamPos;
	};

public:

	line_renderer_app(avk::queue& aQueue)
		: mQueue{ &aQueue } {}
		
	void initialize() override
	{
		

		mPipeline = gvk::context().create_graphics_pipeline_for(
			avk::from_buffer_binding(0)->stream_per_vertex(&Vertex::pos)->to_location(0),
			avk::from_buffer_binding(0)->stream_per_vertex(&Vertex::color)->to_location(1),
			avk::from_buffer_binding(0)->stream_per_vertex(&Vertex::radius)->to_location(2),

			avk::vertex_shader("shaders/passthrough.vert"),
			avk::geometry_shader("shaders/billboard_creation.geom"),
			avk::fragment_shader("shaders/color.frag"),

			avk::cfg::primitive_topology::lines,

			avk::cfg::front_face::define_front_faces_to_be_clockwise(),
			avk::cfg::viewport_depth_scissors_config::from_framebuffer(gvk::context().main_window()->backbuffer_at_index(0)),
			avk::attachment::declare(
				gvk::format_from_window_color_buffer(gvk::context().main_window()),
				avk::on_load::clear,
				avk::color(0),
				avk::on_store::store
			)
		);

		mVertexBuffer = gvk::context().create_buffer(avk::memory_usage::device, {}, avk::vertex_buffer_meta::create_from_data(mVertexData));
		mVertexBuffer->fill(mVertexData.data(), 0, avk::sync::wait_idle());

		mUniformBuffer = gvk::context().create_buffer(avk::memory_usage::host_visible, {}, avk::uniform_buffer_meta::create_from_size(sizeof(matrices_and_user_input)));

		//mIndexBuffer = gvk::context().create_buffer(avk::memory_usage::device, {}, avk::index_buffer_meta::create_from_data(mIndices));
		//mIndexBuffer->fill(mIndices.data(), 0, avk::sync::wait_idle());

		// Set up an updater for shader hot reload and viewport resize
		mUpdater.emplace();
		mPipeline.enable_shared_ownership();
		mUpdater->on(
			gvk::swapchain_resized_event(gvk::context().main_window()),
			gvk::shader_files_changed_event(mPipeline)
		).update(mPipeline);

		auto imguiManager = gvk::current_composition()->element_by_type<gvk::imgui_manager>();
		if (nullptr != imguiManager) {
			mOpenFileDialog.SetTitle("Open Line-Data File");
			mOpenFileDialog.SetTypeFilters({ ".dat" });

			imguiManager->add_callback([this]() {
				ImGui::Begin("Line Renderer - Tool Box", &mOpenToolbox, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
				if (ImGui::BeginMenuBar()) {
					if (ImGui::BeginMenu("File")) {
						if (ImGui::MenuItem("Load Data-File", "Ctrl+O")) {
							mOpenFileDialog.Open();
						}
						if (ImGui::MenuItem("Exit Application", "Alt+F4")) {
							// Exit
							gvk::current_composition()->stop(); // stop the current composition
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

		mQuakeCam = std::make_shared<gvk::quake_camera>();
		mQuakeCam->set_translation({ 0.0f, 0.0f, 1.0f });
		mQuakeCam->look_along({ 0.0f, 0.0f, -1.0f });
		mQuakeCam->set_perspective_projection(glm::radians(60.0f), gvk::context().main_window()->aspect_ratio(), 0.1f, 100.0f);
		mQuakeCam->disable();
		gvk::current_composition()->add_element(*mQuakeCam);
	}

	void update() override
	{
		// Escape tears everything down:
		if (gvk::input().key_pressed(gvk::key_code::escape) || glfwWindowShouldClose(gvk::context().main_window()->handle()->mHandle)) {
			gvk::current_composition()->stop(); // stop the current composition
		}

		// F1 toggles between "quake_cam mode" and "UI mode":
		if (gvk::input().key_pressed(gvk::key_code::f1)) {
			auto* quakeCam = gvk::current_composition()->element_by_type<gvk::quake_camera>();
			auto* imguiManager = gvk:: current_composition()->element_by_type<gvk::imgui_manager>();
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
		// Update UBO:
		matrices_and_user_input uni;
		uni.mViewMatrix = mQuakeCam->view_matrix();
		uni.mProjMatrix = mQuakeCam->projection_matrix();
		uni.mCamPos = glm::vec4(mQuakeCam->translation(), 1.0f);

		mUniformBuffer->fill(&uni, 0);

		auto mainWnd = gvk::context().main_window();

		auto& commandPool = gvk::context().get_command_pool_for_single_use_command_buffers(*mQueue);

		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		cmdBfr->begin_recording();
		cmdBfr->begin_render_pass_for_framebuffer(mPipeline->get_renderpass(), gvk::context().main_window()->current_backbuffer());
		cmdBfr->handle().bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline->handle());
		cmdBfr->draw_vertices(avk::const_referenced(mVertexBuffer));
		cmdBfr->end_render_pass();
		cmdBfr->end_recording();

		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(avk::owned(cmdBfr));
	}

private:

	avk::queue* mQueue;
	avk::graphics_pipeline mPipeline;
	avk::buffer mVertexBuffer;
	//avk::buffer mIndexBuffer;

	std::shared_ptr<gvk::quake_camera> mQuakeCam;
	avk::buffer mUniformBuffer;

	float mClearColor[4] = { 1.0F, 0.0F, 0.0F, 1.0F };
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
		//mainWnd->set_additional_back_buffer_attachments({
		//	attachment::declare(vk::Format::eD32Sfloat, on_load::clear, depth_stencil(), on_store::dont_care)
		//	});
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
