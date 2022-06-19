# Usage Manual
The following manual should help get you started with the Line Renderer App.

## First run & Loading Files
<img style="float: right; width: 30%;" src="../img/manual1.png">
After the start of `Vis2.exe` you should be presented with and empty viewport and the main interface of the APP. By clicking **Load Data-File** in the Main-Menu you can open the **shroom.obj**. It is located in the `data`-folder next to the Application-File and is one of the very small linedata-files that we used during development. This dataset is especially good to test the functionality of the program.

## About Datasets
Our importer only supports OBJ files with line primitives and one data field. Example:
```
v 0 0 0
vt 0.1
v 0 0 1
vt 0.2
v 0 0 2
vt 0.3
v 0 0 3
vt 0.4
l 1 2 3 4
```
Lines starting with a *v* define vertices, lines with a *vt* define the data-field of the previous vertex. (is often a velocity, radius or vorticity) A line starting with an *l* defines a polyline. The following integers are the id-values of the vertices in the order they present themselves in the file.

This file would result in one line being shown from (0,0,1) to (0,0,2), as the first and last vertex gets ommited because of the vulkan primitives we are working with.

> **WARNING**: In our datasets those *l*-lines where always exactly pointing to the vertices before them. We therefore optimized our line import in a way that those indices are not even read.

The VIS2 Line Renderer comes with two test datasets which you are free to play with. The huge datasets that we also tested our Application with can be found [here](https://zenodo.org/record/3637625).

## Useful Keyboard-Shortcuts

 * `F1` toggles between camera- and UI-mode. In camera-mode you can move around via:
   * `Mouse-Drag`: Looking around
   * `W`,`A`,`S`,`D`,`E`,`Q`: Forward, Backward, Left, Right, Down, Up respectively
   * Holding `Shift` allows for quicker movement while holding `Strg` makes it slower
 * `F2` hides/shows the user interface
 * `F3` hides/shows the statistics panel on the right side
 * `F4` activates/deactivates borderless Fullscreen-Mode (only with 1920x1080 resolution so far)
 * `ESC` closes the APP

All of those actions can also be triggered via buttons in the Main-Menu.


## Debug Properties

<img style="float: right; width: 20%;" src="../img/manual2.png">
 * **Main Render Pass Enabled**: If you deactivate the main renderpass there will be no output as the main rendering pipeline will not be invoked.
 * **Billboard-Clipping**: If billboard-clipping is not activated you will be able to see the billboards as they leave the geometry shader.
 * **Billboard-Shading**: This deactivates the illumination calculation. The raycasting will still be executed though! (So dont expect a performance increase)
 * **Show Helper Lines**: If activated another rendering pipeline will overlay the line data shown as simple 2D lines.
 * **Resolve K-Buffer**: If this is deactivated you'll just see the fragments which didn't fit into the k-buffer anymore. The resolve-pipeline will not be invoked.
 * **Layer (K-Buffer)**: This slider lets you select how many of the k-buffer layers the APP should use. More active layers result in better accuracy if many lines are on top of each other, but comes with the cost of higher processing power and probably the loss of framerate. A value of **8** should be sufficient for most datasets and settings.
 
## Vertex Color/Tramsparency/Radius modes

<img style="float: right; width: 20%;" src="../img/manual3.png">
 * **Static**: You can set a fixed value for all vertices
 * **Data dependent**: The value will be calculated for every vertex depending on the data that comes with the dataset (often velocity/verticity).
 * **Length dependent**: The value will be calculated for every line depending on this lines length.
 * **Curvature dependent**: The value will be calculated for every vertex depending on the angle of the two adjacend lines.
 * **per Polyline** *(only for Color)*: Every polyline will have the same random color.
 * **per Line** *(only for Color)*: Every individual line segment will have a different random color.
 
## Lighting & Material

A simple Blinn-Phong implementation will calculate the illumination based on one directional light and one ambient light. The attributes of those light primitves can be changed in the Group "Lighting & Material". Furthermore with some simple material properties you can disable or enhance ambient, diffuse or specular lighting to your needs
