# Terrain Generator

This tool lets you create and modify procedural terrains using Perlin noise.

![terrain](https://i.imgur.com/8m8BsGo.png)

This tool was build with C++, OpenGL and ImGUI for the UI. The project is part of a final year project for my undergraduate degree.

Features
- Plenty of sliders to edit Perlin noise settings, colours, lighting and terrain features.
- Fully realised 3D scene using Blinn-Phong lighting with a moveable camera.
- Shadow mapping.
- Chunk based world.
- Customizable level of detail (LOD) generation for chunks.
- Landscape features such as trees and rocks.
- Option to export terrain with settings to bake in colours/shadows and include LODS/trees/rocks.
- Terrain setting presets that can be saved, loaded and renamed.

## Building

To build this project you can use the provided Visual Studio 2019 solution, a copy of ImGUI is present in the `code` folder and there are no other 3rd party dependancies. This project only runs on a Windows operating system and I have only tested it on two windows 10 machines both running intel processors, one with a dedicated Nvidia GPU and the other using the integrated intel graphics processor.
