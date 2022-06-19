# Technical Documentation
This documentation is supposed to give a brief overview of what we implemented and which tools we used.

## Implementation
After importing the dataset, the line primitives are extracted, and the vertices are stored in a vertex buffer, with one position property and one attribute property per vertex. 

To render the lines, multiple discrete compute and render passes are necessary.

The first compute pass takes the k-Buffer and resets all elements to the largest possible uint64 value; This step is necessary to clear out old entries from the previous frame. 
In the next step, a fullscreen quad render pass draws the background to the frame buffer, setting its alpha channel to 0;
Then, the line primitives are drawn as line strips with adjacency in successive draw calls. The vertex shader passes the primitives to a geometry shader. Then the geometry shader creates a view-aligned billboard for each line segment. A [rounded cone intersection test](https://www.shadertoy.com/view/MlKfzm) returns whether or not a line segment is hit in the fragment shader. 
To fix overlapping geometry of line segments, the geometry shader passes the vector from the start point, of the line segment, to the previous line point and the endpoint, of the line segment, to the next point. With that information, it can be determined if a fragment is on the inside or outside of the line segment by constructing two plains and testing if the fragment is inside both. To account for the start and end caps of lines, where the overlapping test is not necessary, the x component of the vertex position is set to a negative number. 
Every fragment that fails one of the tests gets discarded. In the next phase, the fragment's depth and color values are packed into an uint64_t. The depth value is stored in the MSB of the uint64_t to enable fast minimum comparisons without unpacking it. Fragments premultiplied color is stored in ascending depth order in the K-Buffer. If an entry overflows the K-Buffer, it gets blended with the frame buffer. 
The last phase of the frame resolves the K-Buffer. The resolve pass loads the fragments from the K-Buffer, alpha blends them manually, and outputs the final fragment color. 


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

## Disclaimer
The application was tested on Nvidia GTX 1080Ti and Nvidia RTX 3060 Laptop GPUs