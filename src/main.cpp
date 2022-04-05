#include <gvk.hpp>

#include <iostream>

class Application : public gvk::invokee 
{
public:

	Application(avk::queue& aQueue)
		: mQueue{ &aQueue } {}
		
	void initialize() override
	{

	}

	void update() override
	{
		// Escape tears everything down:
		if (gvk::input().key_pressed(gvk::key_code::escape) || glfwWindowShouldClose(gvk::context().main_window()->handle()->mHandle)) {
			gvk::current_composition()->stop(); // stop the current composition
		}
	}
		
	void render() override
	{

	}

private:

	avk::queue* mQueue{};
};

int main()
{
	using namespace avk;
	using namespace gvk;

	int result = EXIT_FAILURE;

	static const char* titel = "Vis2";

	try {
		// Create a window, set some configuration parameters (also relevant for its swap chain), and open it:
		auto mainWnd = context().create_window(titel);
		mainWnd->set_resolution({ 640, 480 });
		mainWnd->set_additional_back_buffer_attachments({
			attachment::declare(vk::Format::eD32Sfloat, on_load::clear, depth_stencil(), on_store::dont_care)
			});
		mainWnd->enable_resizing(true);
		mainWnd->request_srgb_framebuffer(true);
		mainWnd->set_presentaton_mode(presentation_mode::fifo);
		mainWnd->set_number_of_concurrent_frames(1u); // lets not make it more complex than it has to be xD
		mainWnd->open();

		// Create one single queue which we will submit all command buffers to:
		// (We pass the mainWnd because also presentation shall be submitted to this queue)
		auto& singleQueue = context().create_queue({}, queue_selection_preference::versatile_queue, mainWnd);
		mainWnd->add_queue_family_ownership(singleQueue);
		mainWnd->set_present_queue(singleQueue);

		auto app = Application(singleQueue);
		auto ui = gvk::imgui_manager(singleQueue);

		// Pass everything to gvk::start and off we go:
		start(
			application_name(titel),
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
