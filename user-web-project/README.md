# Embedded Web Dithering

[繁體中文](README.zh-TW.md)

Embedded Web Dithering is an image dithering tool you can use directly in the browser. It is made for preparing images for e-paper and other limited-color displays: load an image, crop it, adjust size and tone, choose colors and dithering, then export the result as PNG.

## Try It Online

Live app: [https://cloudsunderpeak.github.io/embedded-web-dithering/](https://cloudsunderpeak.github.io/embedded-web-dithering/)

You can also download the project and open `index.html` locally.

## Replace the Demo Image

Put exactly one supported image file in `assets/demo/` to change the bundled demo image. PNG, JPEG/JPG, and WebP are supported, and the image does not need to be 16:9. After replacing or renaming the image, regenerate the demo metadata and `file://` fallback:

```bash
python3 tools/generate-demo-data/run.py
```

Server/GitHub Pages loads the selected image directly. The generated `assets/demo/demo-data.js` fallback is only needed when opening `index.html` directly with `file://`.

## Why This Project

Many e-paper and embedded displays can only show a small set of colors. If you place a normal image on them directly, details can disappear or colors can look wrong. This project brings the common preparation steps into one simple page, so you can tune an image for device display without opening a large graphics app.

## Highlights

- Open the app directly from `index.html`.
- Load local images or try the bundled demo.
- Crop to fixed ratios, resize proportionally, and adjust brightness, contrast, and saturation.
- Choose or fine-tune the colors used by the image.
- Apply dithering and preview how the image looks with limited colors.
- Export the processed result as PNG.

## Help Center

Open [the built-in Help center](https://cloudsunderpeak.github.io/embedded-web-dithering/#/help) for the project introduction, a complete quick-start workflow, and guides to Dithering Algorithms, Palette Mapping, and Color Distance. Each document has a shareable route, and the algorithm guides include engine-rendered comparisons, matrix and kernel diagrams, and interactive examples.

Algorithm availability and image-size limits are read from the same runtime configuration used by the editor. After changing an algorithm or Help content, validate both language variants with:

```bash
python3 tools/help-validate/run.py
```

## Supported Input Formats

You can upload PNG, JPEG/JPG, and WebP images.
