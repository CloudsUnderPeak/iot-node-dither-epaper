(function (app) {
    // AdjustProcessor 嘗試用 WebGL 加速亮度、對比、飽和度。
    // 若瀏覽器不支援 WebGL 或 shader 失敗，就永久降級到呼叫端提供的 CPU fallback。
    // WebGL 樣板（context/shader/quad/texture/讀回）共用 gpu/gl-helpers.js。
    var processor = null;
    var disabled = false;

    function apply(imageData, settings, fallback) {
        // 失敗後設為 disabled，避免每次滑桿調整都重複建立 WebGL context。
        if (disabled) {
            return fallback();
        }
        try {
            processor = processor || createProcessor();
            if (!processor) {
                disabled = true;
                return fallback();
            }
            return processor.run(imageData, settings);
        } catch (error) {
            disabled = true;
            return fallback();
        }
    }

    function createProcessor() {
        // 建立一個離畫面 canvas，將整張圖當 texture 丟到 fragment shader 逐像素處理。
        var glHelpers = app.pages.ditherEditor.glHelpers;
        var context = glHelpers.createContext();
        if (!context) {
            return null;
        }
        var canvas = context.canvas;
        var gl = context.gl;

        var program = glHelpers.createProgram(gl, glHelpers.fullscreenVertexShader(), fragmentShaderSource());
        var positionLocation = gl.getAttribLocation(program, 'a_position');
        var texCoordLocation = gl.getAttribLocation(program, 'a_texCoord');
        var uniforms = {
            image: gl.getUniformLocation(program, 'u_image'),
            brightnessFactor: gl.getUniformLocation(program, 'u_brightnessFactor'),
            contrastFactor: gl.getUniformLocation(program, 'u_contrastFactor'),
            saturationFactor: gl.getUniformLocation(program, 'u_saturationFactor')
        };
        var buffers = glHelpers.createQuadBuffers(gl);
        var texture = glHelpers.createTexture(gl);

        return {
            run: function run(imageData, settings) {
                var width = imageData.width;
                var height = imageData.height;
                canvas.width = width;
                canvas.height = height;
                gl.viewport(0, 0, width, height);
                gl.useProgram(program);

                gl.activeTexture(gl.TEXTURE0);
                gl.bindTexture(gl.TEXTURE_2D, texture);
                gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, imageData);
                gl.uniform1i(uniforms.image, 0);
                gl.uniform1f(uniforms.brightnessFactor, cssFactor(Number(settings.brightness || 0)));
                gl.uniform1f(uniforms.contrastFactor, cssFactor(Number(settings.contrast || 0)));
                gl.uniform1f(uniforms.saturationFactor, cssFactor(Number(settings.saturation || 0)));

                glHelpers.drawQuad(gl, buffers, positionLocation, texCoordLocation);
                return glHelpers.readImageData(gl, width, height);
            }
        };
    }

    // 將 UI 百分比轉成 shader 使用的倍率。
    function cssFactor(value) {
        return Math.max(0.01, 1 + value / 100);
    }

    // Fragment shader 逐像素套用 brightness/contrast/saturation。
    function fragmentShaderSource() {
        return [
            'precision highp float;',
            'uniform sampler2D u_image;',
            'uniform float u_brightnessFactor;',
            'uniform float u_contrastFactor;',
            'uniform float u_saturationFactor;',
            'varying vec2 v_texCoord;',
            'void main() {',
            '  vec4 color = texture2D(u_image, v_texCoord);',
            '  vec3 rgb = color.rgb;',
            '  rgb *= u_brightnessFactor;',
            '  rgb = (rgb - 0.5) * u_contrastFactor + 0.5;',
            '  float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));',
            '  rgb = vec3(gray) + (rgb - vec3(gray)) * u_saturationFactor;',
            '  rgb = clamp(rgb, 0.0, 1.0);',
            '  gl_FragColor = vec4(rgb, 1.0);',
            '}'
        ].join('\n');
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.adjustProcessor = {
        apply: apply
    };
})(window.DitherApp);
