XserverX Tremded
==================
Many of the XserverX community mods require a customized tremded which supplies support for MySQL, among other items.

This is a modern idTech 3 Tremulous engine aimed to be fast, secure, and (hopefully) compatible with all existing Tremulous mods. It is based on the latest non-SDL source dump of [ioquake3](https://github.com/ioquake/ioq3) with latest upstream fixes applied, and is a continuation of the [work](https://github.com/AlienHoboken/Tremulous-xserverx-tremded) uploaded by [AlienHoboken](https://github.com/AlienHoboken).

*This repository does not contain any game content so in order to play you must download the [original Tremulous files](https://sourceforge.net/projects/tremulous/files/tremulous/1.1/) and set it up the same way you would any other 1.1 server.*

**Key features**:
* integrated ACEBot navigation logic through syscalls
* up to 128 clients, including bots
* optimized OpenGL renderer
* optimized Vulkan renderer
* raw mouse input support, enabled automatically instead of DirectInput(**\in_mouse 1**) if available
* unlagged mouse events processing, can be reverted by setting **\in_lagged 1**
* **\in_minimize** - hotkey for minimize/restore main window (win32-only, direct replacement for Q3Minimizer)
* **\video-pipe** - to use external ffmpeg binary as an encoder for better quality and smaller output files
* significantly reworked QVM (Quake Virtual Machine)
* improved server-side DoS protection, much reduced memory usage
* raised filesystem limits (up to 20,000 maps can be handled in a single directory)
* reworked Zone memory allocator, no more out-of-memory errors
* non-intrusive support for SDL2 backend (video, audio, input), selectable at compile time
* network handling has been greatly optimized
* much improved state for server downloads
* tons of bug fixes and other improvements

## Vulkan renderer

Based on [Quake-III-Arena-Kenny-Edition](https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition) with many additions:

* high-quality per-pixel dynamic lighting
* very fast flares (**\r_flares 1**)
* anisotropic filtering (**\r_ext_texture_filter_anisotropic**)
* greatly reduced API overhead (call/dispatch ratio)
* flexible vertex buffer memory management to allow loading huge maps
* multiple command buffers to reduce processing bottlenecks
* [reversed depth buffer](https://developer.nvidia.com/content/depth-precision-visualized) to eliminate z-fighting on big maps
* merged lightmaps (atlases)
* multitexturing optimizations
* static world surfaces cached in VBO (**\r_vbo 1**)
* useful debug markers for tools like [RenderDoc](https://renderdoc.org/)
* fixed framebuffer corruption on some Intel iGPUs
* offscreen rendering, enabled with **\r_fbo 1**, all following requires it enabled:
* `screenMap` texture rendering - to create realistic environment reflections
* multisample anti-aliasing (**\r_ext_multisample**)
* supersample anti-aliasing (**\r_ext_supersample**)
* per-window gamma-correction which is important for screen-capture tools like OBS
* you can minimize game window any time during **\video**|**\video-pipe** recording
* high dynamic range render targets (**\r_hdr 1**) to avoid color banding
* bloom post-processing effect
* arbitrary resolution rendering
* greyscale mode

In general, not counting offscreen rendering features you might expect from 10% to 200%+ FPS increase comparing to KE's original version

Highly recommended to use on modern systems

## OpenGL renderer

Based on classic OpenGL renderers from [idq3](https://github.com/id-Software/Quake-III-Arena)/[ioquake3](https://github.com/ioquake/ioq3)/[cnq3](https://bitbucket.org/CPMADevs/cnq3)/[openarena](https://github.com/OpenArena/engine), features:

* OpenGL 1.1 compatible, uses features from newer versions whenever available
* high-quality per-pixel dynamic lighting, can be triggered by **\r_dlightMode** cvar
* merged lightmaps (atlases)
* static world surfaces cached in VBO (**\r_vbo 1**)
* all set of offscreen rendering features mentioned in Vulkan renderer, plus:
* bloom reflection post-processing effect

Performance is usually greater or equal to other opengl1 renderers

## OpenGL2 renderer

Original ioquake3 renderer, performance is very poor on non-nvidia systems, unmaintained

Extra Dependencies
-----------------------
The tremded depends on MySQL/MySQL development libraries. Ensure these are installed before attempting to build.

Building
-----------------------
You can build the tremded the same way you build normally.

Make sure that the `BUILD_SERVER` flag is set in the Makefile before building.

As normal there are shell scripts for building on Windows and Mac OSX. Windows requires MingW be used.

Running
-----------------------
The tremded can be run the same as any other tremulous dedicated server, eg:
`./tremded.x86 +set dedicated 2 +exec server.cfg +set net_port 30721 +set net_ip 127.0.0.1 +set fs_game "base"`

Branches
-----------------------
The development branch contains upcoming features that have not been deemed stable for use in the server yet.

Contributing
-----------------------
If you wish to contribute, please fork the branch you wish to work on, make your changes, and submit a pull request for review.

Also, please report all bugs you encounter!

## Links

* https://bitbucket.org/CPMADevs/cnq3
* https://github.com/ec-/Quake3e
* https://github.com/ioquake/ioq3
* https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition
* https://github.com/OpenArena/engine
