(function (app) {
    app.i18n.en.helpMaxInputLongEdge = 'Large input is reduced to a working long edge of {value} px';
    app.i18n.en.helpMaxResizeOutputSize = '{value} px';
    app.i18n.en.helpBundle = {
        ui: {
            documents: 'Documentation',
            breadcrumbs: 'Breadcrumb',
            documentPagination: 'Document pagination',
            openDocuments: 'Browse documents',
            closeDocuments: 'Close documents',
            onThisPage: 'On this page',
            previous: 'Previous',
            next: 'Next',
            startGroup: 'Start here',
            algorithmsGroup: 'Algorithm guide',
            overviewGroup: 'Overview',
            before: 'Source',
            after: 'Result',
            comparisonHint: 'Drag to compare source and result',
            characteristics: 'Characteristics',
            bestFor: 'Best for',
            avoidWhen: 'Watch for',
            controls: 'Controls',
            structure: 'Structure',
            targetColor: 'Target color',
            nearestColor: 'Selected palette color',
            distanceExplorerHint: 'Change the target color to see how each metric chooses a palette color.',
            detailsPending: 'Detailed guidance has not been provided yet.',
            noExtraControls: 'No extra controls',
            listSeparator: ', '
        },
        visuals: {
            family: {
                title: 'Same source, three dither families',
                settings: 'Synthetic gradient 480x288 · E6 · Nearest Color · Euclidean BT.709 · 100%',
                floyd: 'Floyd-Steinberg',
                floydCaption: 'Fine, organic texture with quantization error carried to neighboring pixels.',
                bayer: 'Bayer 8x8',
                bayerCaption: 'A fast, deterministic threshold pattern with visible ordered structure.',
                dot: 'Dot Halftone',
                dotCaption: 'Clustered dots create a deliberate print-like texture.'
            },
            mapping: {
                title: 'Compare Palette Mapping',
                settings: 'Synthetic gradient 480x288 · Bayer 8x8 · E6 · Euclidean BT.709 · 100%',
                nearest: 'Nearest Color',
                nearestCaption: 'Each sample is represented by one closest palette color.',
                pair: 'Pair Mix',
                pairCaption: 'Two palette colors share pixels to approximate intermediate color.',
                tri: 'Tri Mix',
                triCaption: 'Three palette colors can preserve more varied color relationships.'
            },
            distance: {
                title: 'Compare Color Distance',
                settings: 'Synthetic gradient 480x288 · Floyd-Steinberg · E6 · Nearest Color · 100%',
                bt709: 'Euclidean BT.709',
                bt709Caption: 'Weighted RGB emphasizes differences in green.',
                rgb: 'Euclidean RGB',
                rgbCaption: 'All RGB channels contribute equally to squared distance; this is the project default.',
                ciede: 'CIEDE2000',
                ciedeCaption: 'A perceptual Lab-space comparison can choose a different palette neighbor.'
            }
        },
        documents: {
            home: {
                title: 'Help center',
                eyebrow: 'Embedded Web Dithering documentation',
                lead: 'Learn the complete workflow, then understand how Dither Algorithm, Palette Mapping, and Color Distance work together.',
                sections: [
                    {
                        id: 'choose-a-path',
                        title: 'Choose a path',
                        cards: [
                            { title: 'New to the project', body: 'Start with the purpose, supported formats, privacy, and project limits.', linkId: 'introduction' },
                            { title: 'Make an image now', body: 'Follow the editor from image input and crop through palette, dither, and PNG export.', linkId: 'quick-start' },
                            { title: 'Understand the output', body: 'Compare algorithm families and learn why mapping and distance settings change pixels.', linkId: 'dithering' }
                        ]
                    },
                    {
                        id: 'mental-model',
                        title: 'A useful mental model',
                        paragraphs: [
                            'The Palette defines which colors may appear. Color Distance measures which palette choices are close to an input color. Palette Mapping decides whether one, two, or three palette colors should represent it. The Dither Algorithm distributes those choices across pixels.'
                        ],
                        visual: { type: 'pipeline' }
                    },
                    {
                        id: 'recommended-order',
                        title: 'Recommended reading order',
                        bullets: [
                            'Read Project introduction to confirm the tool fits your display workflow.',
                            'Complete Quick start once with the bundled demo.',
                            'Use Dithering algorithms to choose a texture family.',
                            'Fine-tune Palette Mapping and Color Distance only after the palette is stable.'
                        ]
                    }
                ]
            },
            introduction: {
                title: 'Project introduction',
                eyebrow: 'Start here',
                lead: 'Embedded Web Dithering prepares ordinary images for e-paper and other limited-color displays entirely inside the browser.',
                sections: [
                    {
                        id: 'what-it-does',
                        title: 'What the project does',
                        paragraphs: [
                            'Load a local image, crop it to the target shape, resize it, adjust tone, select a palette, apply dithering, inspect the pixels, and export a PNG.',
                            'The fixed processing order is Crop, Resize, Adjust, Palette, then Dither. This keeps the meaning of every color setting predictable.'
                        ]
                    },
                    {
                        id: 'good-fit',
                        title: 'Where it fits',
                        table: {
                            headers: ['Use case', 'Why it helps'],
                            rows: [
                                ['E-paper displays', 'Previews a fixed color set before sending an image to a device.'],
                                ['Embedded LCD or LED panels', 'Matches exact output dimensions and reduces color complexity.'],
                                ['Pixel and print-inspired art', 'Provides ordered, noise-like, diffusion, and halftone textures.']
                            ]
                        }
                    },
                    {
                        id: 'local-and-offline',
                        title: 'Local and offline by design',
                        bullets: [
                            'The source image is processed in your browser and is not uploaded to a server.',
                            'The original project can be opened directly from index.html without npm, a backend, or a CDN.',
                            'Runtime resources are bundled with the project.'
                        ],
                        note: 'Your current image and editor settings survive SPA page changes, but they are not restored after refreshing or closing the page.'
                    },
                    {
                        id: 'formats',
                        title: 'Input and output',
                        table: {
                            headers: ['Area', 'Support'],
                            rows: [
                                ['Input', 'PNG, JPEG/JPG, and WebP'],
                                ['Output', 'PNG'],
                                ['Transparency', 'Transparent input pixels are composed over white'],
                                ['Maximum editor working edge', { key: 'helpMaxInputLongEdge', fact: 'maxInputLongEdge' }],
                                ['Maximum configured output side', { key: 'helpMaxResizeOutputSize', fact: 'maxResizeOutputSize' }]
                            ]
                        }
                    }
                ]
            },
            'quick-start': {
                title: 'Quick start',
                eyebrow: 'Start here',
                lead: 'Complete one image from input to export. The bundled demo is the fastest way to learn the controls.',
                sections: [
                    {
                        id: 'load-and-prepare',
                        title: '1. Load and prepare the image',
                        steps: [
                            { title: 'Load a source', body: 'Drop a supported file on the empty preview, choose Browse File, or select Load Demo from Image Input.' },
                            { title: 'Crop', body: 'Choose a fixed ratio, then zoom, pan, rotate, flip, and set the fill color. Press OK or collapse Crop to enter editing.' },
                            { title: 'Resize', body: 'Enter the target pixel width or height. The other side remains linked to the crop ratio.' }
                        ]
                    },
                    {
                        id: 'adjust-colors',
                        title: '2. Adjust tone and colors',
                        steps: [
                            { title: 'Adjust', body: 'Tune brightness, contrast, and saturation before color reduction.' },
                            { title: 'Palette', body: 'Keep Original for extracted representative colors, choose a device preset, or edit swatches to create Custom.' },
                            { title: 'Dither', body: 'Begin with Floyd-Steinberg, Nearest Color, and Euclidean RGB. Change one setting at a time.' }
                        ]
                    },
                    {
                        id: 'inspect-and-export',
                        title: '3. Inspect and export',
                        bullets: [
                            'Original shows the prepared image before edit effects.',
                            'Result fits the processed output into the preview area.',
                            'Expand shows the result at one CSS pixel per output pixel for close inspection.',
                            'Export PNG reruns the formal pipeline at the configured output size.'
                        ]
                    },
                    {
                        id: 'starter-recipe',
                        title: 'Safe starter recipe',
                        table: {
                            headers: ['Setting', 'Start with'],
                            rows: [
                                ['Palette', 'Original, 8 colors'],
                                ['Dither Algorithm', 'Floyd-Steinberg'],
                                ['Palette Mapping', 'Nearest Color'],
                                ['Color Distance', 'Euclidean RGB'],
                                ['Error Strength', '100%'],
                                ['Serpentine', 'Off; enable it if one-way streaks are distracting']
                            ]
                        },
                        note: 'Judge the exported PNG or Expand view at its intended display size. A fit preview can resample single-pixel patterns.'
                    }
                ]
            },
            dithering: {
                title: 'Dithering algorithms',
                eyebrow: 'Algorithm guide',
                lead: 'Dithering simulates unavailable tones by arranging only the colors in the current palette. Choose a family by texture and use case.',
                sections: [
                    {
                        id: 'what-is-dithering',
                        title: 'What changes when you dither',
                        paragraphs: [
                            'Without dithering, a limited palette creates flat regions and abrupt bands. Dithering trades those bands for controlled texture that the eye blends into intermediate tone and color.',
                            'Every algorithm in this project outputs palette colors only. It changes their spatial distribution; it does not invent extra colors.'
                        ],
                        visual: { type: 'comparison', set: 'family' }
                    },
                    {
                        id: 'family-comparison',
                        title: 'Choose an algorithm family',
                        table: {
                            headers: ['Family', 'Visual character', 'Typical use'],
                            rows: [
                                ['Error Diffusion', 'Organic fine grain; follows image detail', 'Photos, gradients, general default'],
                                ['Ordered / Blue Noise', 'Deterministic matrix or high-frequency mask', 'Pixel art, UI assets, fast predictable output'],
                                ['Dot-based', 'Class-ordered noise or clustered print dots', 'Special texture, posters, print-inspired output']
                            ]
                        },
                        cards: [
                            { title: 'Error Diffusion', body: 'Six algorithms that spread quantization error forward.', linkId: 'error-diffusion' },
                            { title: 'Ordered / Blue Noise', body: 'Bayer matrices and a deterministic blue-noise-like mask.', linkId: 'ordered' },
                            { title: 'Dot-based Algorithms', body: 'Dot Diffusion and clustered-dot Halftone use very different dot structures.', linkId: 'dot' }
                        ]
                    },
                    {
                        id: 'strength-controls',
                        title: 'Strength controls',
                        bullets: [
                            'Error Strength scales propagated error for Error Diffusion and Dot Diffusion.',
                            'Dither Strength scales threshold influence for Bayer and Blue Noise.',
                            'Dot Density changes the sampled clustered-dot density for Dot Halftone.',
                            'Switching algorithms resets the shared strength value to 100%.'
                        ]
                    }
                ]
            },
            'error-diffusion': {
                title: 'Error Diffusion',
                eyebrow: 'Dithering algorithms',
                lead: 'Each pixel is mapped to a palette color, then its RGB quantization error is carried to selected future neighbors.',
                sections: [
                    {
                        id: 'how-it-works',
                        title: 'How error moves',
                        paragraphs: [
                            'The kernel determines which unprocessed pixels receive the error and in what proportion. Small kernels are fast and crisp; wider kernels usually look smoother but touch more neighbors.'
                        ],
                        visual: {
                            type: 'kernel',
                            title: 'Floyd-Steinberg kernel',
                            rows: [['', 'current', '7/16'], ['3/16', '5/16', '1/16']]
                        },
                        note: 'Serpentine alternates scan direction on each row. It can reduce directional streaking on algorithms that support it.'
                    },
                    {
                        id: 'algorithms',
                        title: 'Available algorithms',
                        capabilityFamily: 'error-diffusion',
                        algorithms: [
                            { id: 'floyd-steinberg', name: 'Floyd-Steinberg', summary: 'Compact 2-row kernel and the safest general-purpose default.', characteristics: 'Fine grain, balanced detail, moderate cost', bestFor: 'Photos, gradients, first comparison', avoid: 'Can form worm-like texture in flat areas', controls: 'Error Strength; optional Serpentine', structure: '4 recipients, divisor 16' },
                            { id: 'atkinson', name: 'Atkinson', summary: 'Diffuses only six eighths of the error, preserving stronger contrast.', characteristics: 'Crisp, lighter midtones, retro character', bestFor: 'Line art, portraits, classic Macintosh-like output', avoid: 'Can lose shadow or highlight tone', controls: 'Error Strength', structure: '6 recipients over 3 rows' },
                            { id: 'jarvis', name: 'Jarvis-Judice-Ninke', summary: 'A dense 5x3 kernel spreads error broadly.', characteristics: 'Smooth gradients, soft texture, higher work', bestFor: 'Photos and broad tonal transitions', avoid: 'May soften small detail', controls: 'Error Strength; optional Serpentine', structure: '12 recipients, divisor 48' },
                            { id: 'sierra-lite', name: 'Sierra Lite', summary: 'A very small three-recipient kernel prioritizes speed and edge clarity.', characteristics: 'Fast, sharp, more directional', bestFor: 'Small assets and quick previews', avoid: 'More visible streaks or grain', controls: 'Error Strength; optional Serpentine', structure: '3 recipients, divisor 4' },
                            { id: 'stevenson-arce', name: 'Stevenson-Arce', summary: 'A sparse, wide kernel distributes error across a large neighborhood.', characteristics: 'Organic texture, low local directionality, higher cost', bestFor: 'Large artistic output and smooth fields', avoid: 'Speckling and slower processing', controls: 'Error Strength', structure: '12 sparse recipients across 11x3' },
                            { id: 'adaptive-fs-3x3', name: 'Adaptive FS 3x3', summary: 'Adds a local 3x3 luminance mean to Floyd-Steinberg-style diffusion.', characteristics: 'Local tone adaptation, edge-aware bias', bestFor: 'Textured photos and uneven local contrast', avoid: 'Possible micro halos around strong local changes', controls: 'Error Strength; optional Serpentine', structure: 'Integral luminance map plus FS diffusion' }
                        ]
                    },
                    {
                        id: 'choosing',
                        title: 'How to choose',
                        bullets: [
                            'Start with Floyd-Steinberg.',
                            'Try Atkinson when contrast and a crisp retro texture matter more than full tone conservation.',
                            'Try Jarvis-Judice-Ninke or Stevenson-Arce for smoother large images.',
                            'Try Sierra Lite when speed and sharpness matter.',
                            'Try Adaptive FS 3x3 when local lighting varies strongly.'
                        ]
                    }
                ]
            },
            ordered: {
                title: 'Ordered and Blue Noise',
                eyebrow: 'Dithering algorithms',
                lead: 'Threshold algorithms compare each pixel against a repeatable spatial mask. They are deterministic, parallel-friendly, and visually structured.',
                sections: [
                    {
                        id: 'bayer-matrix',
                        title: 'Bayer threshold structure',
                        paragraphs: [
                            'A Bayer matrix ranks positions inside a repeating tile. Lower and higher ranks switch palette choices at different tone levels, producing a stable pattern.'
                        ],
                        visual: {
                            type: 'matrix',
                            title: 'Bayer 4x4 rank matrix',
                            rows: [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
                        }
                    },
                    {
                        id: 'algorithms',
                        title: 'Available algorithms',
                        capabilityFamily: 'ordered',
                        algorithms: [
                            { id: 'bayer-4', name: 'Bayer 4x4', summary: 'Small repeating tile with an obvious pixel-friendly pattern.', characteristics: 'Fast, crisp, strongly ordered', bestFor: 'Icons, pixel art, low-resolution output', avoid: 'Visible 4x4 tiling in smooth photos', controls: 'Dither Strength', structure: '4x4 threshold matrix' },
                            { id: 'bayer-8', name: 'Bayer 8x8', summary: 'A larger matrix creates finer tonal steps with less obvious repetition.', characteristics: 'Fast, predictable, finer texture', bestFor: 'General ordered dithering and gradients', avoid: 'Low-frequency 8x8 pattern can remain visible', controls: 'Dither Strength', structure: '8x8 recursively generated matrix' },
                            { id: 'blue-noise-64', name: 'Blue Noise 64', summary: 'A deterministic 64x64 blue-noise-like ranking mask reduces grid-like repetition.', characteristics: 'High-frequency, less tiled, stable', bestFor: 'Gradients and color palettes where Bayer looks too regular', avoid: 'Can look grainier and still has a repeating 64x64 mask', controls: 'Dither Strength', structure: 'Procedural void-and-cluster-style 64x64 rank mask' }
                        ]
                    },
                    {
                        id: 'strength',
                        title: 'Dither Strength',
                        paragraphs: [
                            'At 100%, each algorithm uses its configured threshold influence. Lower values reduce spatial variation and move toward direct color mapping; higher values exaggerate the mask. Check Expand or the exported PNG because fit-preview resampling can hide or amplify single-pixel structure.'
                        ]
                    }
                ]
            },
            dot: {
                title: 'Dot-based algorithms',
                eyebrow: 'Dithering algorithms',
                lead: 'Dot Diffusion and Dot Halftone both show dot structure, but one diffuses error while the other grows clustered print-like dots.',
                sections: [
                    {
                        id: 'not-the-same',
                        title: 'Two different mechanisms',
                        table: {
                            headers: ['Algorithm', 'Mechanism', 'Control'],
                            rows: [
                                ['Dot Diffusion 8x8', 'Processes pixels by an 8x8 class order and distributes error to later 3x3 neighbors.', 'Error Strength'],
                                ['Dot Halftone', 'Samples a center-out clustered-dot threshold cell.', 'Dot Density']
                            ]
                        }
                    },
                    {
                        id: 'algorithms',
                        title: 'Available algorithms',
                        capabilityFamily: 'dot',
                        algorithms: [
                            { id: 'dot-diffusion-simple', name: 'Dot Diffusion 8x8', summary: 'Combines class-ordered processing with local RGB error distribution.', characteristics: 'Parallel structure, textured dots, multi-color aware', bestFor: 'A structured alternative to scan-line diffusion', avoid: '8x8 class texture may become visible', controls: 'Error Strength', structure: '8x8 class matrix; 3x3 later-class recipients' },
                            { id: 'pattern-dots', name: 'Dot Halftone', summary: 'Uses clustered dots that grow outward from cell centers.', characteristics: 'Print-like, bold, graphic', bestFor: 'Posters, editorial graphics, retro print effects', avoid: 'Moiré, cell seams, and reduced fine detail', controls: 'Dot Density', structure: 'Center-out clustered threshold cell' }
                        ]
                    },
                    {
                        id: 'density',
                        title: 'Reading Dot Density',
                        bullets: [
                            '50% is approximately half the default sampled dot density.',
                            '100% preserves the standard cell sampling density.',
                            '150% is approximately twice the sampled density.',
                            'Dot Density changes cell sampling, not a global black/white cutoff.'
                        ]
                    }
                ]
            },
            'palette-mapping': {
                title: 'Palette Mapping',
                eyebrow: 'Algorithm guide',
                lead: 'Palette Mapping decides how one input color is represented by the fixed colors in the current palette before the dither pattern distributes pixels.',
                sections: [
                    {
                        id: 'compare',
                        title: 'One color, one to three palette choices',
                        visual: { type: 'comparison', set: 'mapping' },
                        table: {
                            headers: ['Mode', 'Representation', 'Minimum useful palette'],
                            rows: [
                                ['Nearest Color', 'One closest palette color', '1 color'],
                                ['Pair Mix', 'A best-fit line between two palette colors', '2 colors'],
                                ['Tri Mix', 'A best-fit triangle from three nearby candidates', '3 colors']
                            ]
                        }
                    },
                    {
                        id: 'modes',
                        title: 'How each mode behaves',
                        algorithms: [
                            { name: 'Nearest Color', summary: 'Maps directly to the single palette entry with the lowest selected Color Distance.', characteristics: 'Stable, clean, fast', bestFor: 'Default use, small palettes, sharp assets', avoid: 'Flat bands when the palette lacks intermediate colors', controls: 'Color Distance', structure: 'Nearest-neighbor search over palette colors' },
                            { name: 'Pair Mix', summary: 'Projects the input color onto every palette-color pair and selects the closest mixed point.', characteristics: 'Richer intermediate tone with moderate texture', bestFor: 'Two-color ramps and ordered masks', avoid: 'More color noise and pair search cost', controls: 'Color Distance and algorithm threshold/error behavior', structure: 'Best-fit line segment plus mix ratio' },
                            { name: 'Tri Mix', summary: 'Tests triangles among the six closest candidates and derives normalized barycentric weights.', characteristics: 'Broad color approximation, highest local variety', bestFor: 'Colorful palettes where pair ramps miss hues', avoid: 'More noise and CPU cost; threshold GPU path falls back to CPU', controls: 'Color Distance and algorithm threshold/error behavior', structure: '20 candidate triangles from top six colors' }
                        ]
                    },
                    {
                        id: 'algorithm-interaction',
                        title: 'Interaction with the Dither Algorithm',
                        paragraphs: [
                            'Ordered, Blue Noise, and Dot Halftone algorithms pass their threshold to Pair Mix and Tri Mix, so the spatial mask chooses colors according to the calculated proportions.',
                            'Error Diffusion has no threshold mask. Pair Mix or Tri Mix first emits the color with the greatest calculated weight, then the processor diffuses the error from that actual output color.'
                        ],
                        note: 'Every mapping mode outputs palette colors only. If Pair Mix has fewer than two colors, or Tri Mix fewer than three, the implementation safely falls back to Nearest Color.'
                    }
                ]
            },
            'color-distance': {
                title: 'Color Distance',
                eyebrow: 'Algorithm guide',
                lead: 'Color Distance is the ruler used to compare an input RGB color with palette colors or candidate mixes. Changing the ruler can change which palette pixel wins.',
                sections: [
                    {
                        id: 'interactive',
                        title: 'Try the distance metrics',
                        visual: { type: 'distance-explorer' }
                    },
                    {
                        id: 'metrics',
                        title: 'Available metrics',
                        table: {
                            headers: ['Metric', 'Meaning', 'Use it when'],
                            rows: [
                                ['Euclidean BT.709', 'Squared RGB distance weighted 0.2126 red, 0.7152 green, 0.0722 blue.', 'Balanced luminance-sensitive matching is more important than equal channel weight.'],
                                ['Euclidean RGB', 'Squared distance with equal channel weight.', 'You want the project default and equal numeric RGB changes to count equally.'],
                                ['Manhattan BT.709', 'Weighted absolute channel differences.', 'You want a simpler, more angular BT.709-weighted decision boundary.'],
                                ['Manhattan RGB', 'Equal-weight absolute channel differences.', 'You want predictable channel-by-channel matching.'],
                                ['CIEDE2000', 'Perceptual difference after sRGB to Lab conversion.', 'Perceptual similarity matters more than processing cost.']
                            ]
                        }
                    },
                    {
                        id: 'visual-comparison',
                        title: 'The same pixels under different rulers',
                        visual: { type: 'comparison', set: 'distance' }
                    },
                    {
                        id: 'choosing',
                        title: 'Practical choice',
                        bullets: [
                            'Keep Euclidean RGB as the first choice.',
                            'Compare Euclidean BT.709 when luminance-sensitive matching is more important than equal channel weight.',
                            'Use Manhattan variants for a deliberately different, channel-linear boundary.',
                            'Try CIEDE2000 when a small palette contains perceptually close colors and preview latency remains acceptable.',
                            'Evaluate distance only after fixing the palette and Palette Mapping; otherwise several variables change at once.'
                        ]
                    }
                ]
            }
        }
    };
})(window.DitherApp);
