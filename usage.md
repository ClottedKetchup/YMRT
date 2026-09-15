# YMRT Usage

**English** | [中文](usage.zh-CN.md)

## Basic Layout

Launch `YMRT.exe` to open the editor window:

- **Editor view** — a live OpenGL preview of the scene. Use WASD to move the camera and Q/E to adjust its height. Hold the left mouse button and drag to rotate the view, and left-click an object to select it.
- **Path tracing view** — CUDA progressive rendering that accumulates until convergence
- **Editing panels** — numbered in the screenshot below:

![Editor panels](ui_usage_tags.png)

1. **Load scene** — load a scene in any format supported by assimp, such as OBJ or FBX (just enter the absolute file path).
2. **Temp files** — import or save temporary scene files (the extension must be `.ymtmp`, e.g. `src/model/CornellBox_Test.ymtmp`).
3. **Render setting** — set the maximum accumulated frame count, samples per pixel, sampler, ray depth and other properties.
4. **Resource list** — all geometry, materials, textures and other resources in the scene. Entries are highlighted in red when selected in this list, in purple when referenced by the object selected in the Editor view, and in gray-white when both. Clicking Delete removes the red-highlighted entry.
5. **Outline view** — all objects in the scene, including the default distant light.
6. **Properties** — the selected primitive's Geometry / Transform / Material / Volume bindings and parameters (the Material tab is active here).
7. **Tree view** — the entries referenced by the current resource, which may be transforms or textures. Left-click an entry to expand it (the little arrow in front points down when expanded).
8. **Resource editing window** — pops up when you right-click an expanded entry in the tree view.

**Setting lights**: select an object, then change its Material type to Light material in the material editor.

Example scenes ship with the repository in `src/model/` (CornellBox, Sponza, pool test, and more) as `.ymtmp` scene files.

## Rendering and Screenshots

![Begin rendering](screenshot_0_tagged.png)

Click Begin rendering in the Render setting dropdown list, and a small window pops up prompting you to set the image's maximum accumulated frame count and resolution. After entering the parameters, click Apply to start rendering.

![Standalone window](screenshot_1_tagged.png)

During rendering, the Path tracing view is shown as a standalone window and cannot be docked; it stays that way until rendering finishes. When the accumulated frame count reaches the set maximum, a "Render finish" prompt appears in the Render setting. You can then right-click the Path tracing view to open a screenshot window; enter a path to save the screenshot (currently only PNG is supported).

Note that screenshots can only be taken while the Path tracing view is a standalone window — if you dock it back right after rendering finishes, you won't be able to take a screenshot.
