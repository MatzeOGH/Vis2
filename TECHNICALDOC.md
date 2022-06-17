# Technical Documentation
This documentation is supposed to give a brief overview of what we implemented and which tools we used.


## External Libraries

* [Gears-Vk](https://github.com/cg-tuwien/Gears-Vk) see [next chapter](#aboutgearsvk) for more details
* [imgui-filebrowser](https://github.com/AirGuanZ/imgui-filebrowser)
* [fast_float library](https://github.com/fastfloat/fast_float) for the importer


### About Gears-Vk {#aboutgearsvk}
Our implementation uses [Gears-Vk](https://github.com/cg-tuwien/Gears-Vk) which can be seen as a productivity layer between the verbose Vulkan-API and the programmer. It offers full Vulkan functionality but with a lot of convenience functions to make our programming life easier. It furthermore also offers out of the box, which is relevant for our project:

* Window Management (GLFW)
* Render-Loop and Input Handling
* A ready to use controllable camera class

and comes already integrated with [Dear ImGui](https://github.com/ocornut/imgui) which is a convenient user interface library.