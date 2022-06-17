# Setup and Installation
<img style="float: right; width: 30%;" src="img/screenshot1.png">
The Advanced Line Renderer is a Windows-Application based on the paper ["Advanced Rendering of Line Data with Ambient Occlusion and Transparency"](https://ieeexplore.ieee.org/document/9216549) by David Groß and Stefan Gumhold.

The Project was implemented by Mathias Hürbe and Gerald Kimmersdorfer for the Visualization 2 course of the Technical University Vienna in the summer semester 2022.

## Download
An already compiled version for *Windows 10 x64* of the Line Renderer can be downloaded [here](down/line_renderer_x64.zip).

If you have a different operating system you have to compile the application by yourself. Please feel free to download the source code from our [repository on github](https://github.com/MatzeOGH/Vis2.git). You may wanna follow the instructions in the [following chapter](#setup-dev-environment).


## Setup Dev-Environment {#setup-dev-environment}
As our application utilizes [Gears-Vk](https://github.com/cg-tuwien/Gears-Vk), therefore all of the requirements for this framework have to be fullfilled:

* [Visual Studio 2019](https://visualstudio.microsoft.com/de/downloads/) with [Windows 10 SDK (10.0.18362.0)](https://stackoverflow.com/questions/50590700/how-do-i-install-windows-10-sdk-for-use-with-visual-studio-2017)
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (1.2.189.2)

If you have all of the requirements installed you can continue to check out our source code with:
```
git clone https://github.com/MatzeOGH/Vis2.git --recurse-submodules 
```

As we use some functionality of a different branch of Gears-VK which is not yet integrated in the main branch you have to manually switch to this branch with the following commands:
```
cd Vis2/Gears-Vk
git fetch --all
git checkout remotes/origin/artr2022_assignment3
cd ..
git submodule update --remote --merge
```

If everything worked fine you are now ready to open the project:
```
start Vis2.sln
```

Now make the **Vis2**-Project the startup project and you should be good to go.
> **INFO**: The first (ever) build-process usually gets stuck during the resolve of some NuGet-package. If this happens to you just cancel the build and restart the process.