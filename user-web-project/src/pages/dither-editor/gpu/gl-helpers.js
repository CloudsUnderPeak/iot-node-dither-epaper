(function (app) {
    // GPU processor 共用的 WebGL 樣板：context、shader 編譯、全屏 quad、texture 與讀回。
    // 兩個 processor（adjust、threshold dither）只保留各自的 fragment shader 與 uniform 邏輯。
    // 這裡的函式失敗時直接 throw，由 processor 的 apply 層捕捉並永久降級到 CPU fallback。

    // 建立離畫面 canvas 與 WebGL context；環境不支援（含 worker）時回 null。
    function createContext() {
        if (typeof document === 'undefined') {
            return null;
        }
        var canvas = document.createElement('canvas');
        var options = { antialias: false, preserveDrawingBuffer: true };
        var gl = canvas.getContext('webgl', options) ||
            canvas.getContext('experimental-webgl', options);
        if (!gl) {
            return null;
        }
        return { canvas: canvas, gl: gl };
    }

    // 編譯並連結 vertex/fragment shader。
    function createProgram(gl, vertexSource, fragmentSource) {
        var vertexShader = createShader(gl, gl.VERTEX_SHADER, vertexSource);
        var fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, fragmentSource);
        var program = gl.createProgram();
        gl.attachShader(program, vertexShader);
        gl.attachShader(program, fragmentShader);
        gl.linkProgram(program);
        if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
            throw new Error(gl.getProgramInfoLog(program));
        }
        return program;
    }

    // 建立單支 shader，若編譯失敗就丟出錯誤讓外層 fallback。
    function createShader(gl, type, source) {
        var shader = gl.createShader(type);
        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
            throw new Error(gl.getShaderInfoLog(shader));
        }
        return shader;
    }

    // 建立像素紋理：CLAMP_TO_EDGE + NEAREST，避免取樣插值污染像素運算。
    function createTexture(gl) {
        var texture = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        return texture;
    }

    // 覆蓋整個畫布的兩個三角形（position 與 texCoord 各一個 STATIC buffer）。
    function createQuadBuffers(gl) {
        var position = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, position);
        gl.bufferData(
            gl.ARRAY_BUFFER,
            new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]),
            gl.STATIC_DRAW
        );
        var texCoord = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, texCoord);
        gl.bufferData(
            gl.ARRAY_BUFFER,
            new Float32Array([0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1]),
            gl.STATIC_DRAW
        );
        return { position: position, texCoord: texCoord };
    }

    // 全屏 quad 的 vertex shader；fragment shader 由各 processor 自行提供。
    function fullscreenVertexShader() {
        return [
            'attribute vec2 a_position;',
            'attribute vec2 a_texCoord;',
            'varying vec2 v_texCoord;',
            'void main() {',
            '  gl_Position = vec4(a_position, 0.0, 1.0);',
            '  v_texCoord = a_texCoord;',
            '}'
        ].join('\n');
    }

    // 綁定 quad attribute 並繪製。
    function drawQuad(gl, buffers, positionLocation, texCoordLocation) {
        gl.bindBuffer(gl.ARRAY_BUFFER, buffers.position);
        gl.enableVertexAttribArray(positionLocation);
        gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, buffers.texCoord);
        gl.enableVertexAttribArray(texCoordLocation);
        gl.vertexAttribPointer(texCoordLocation, 2, gl.FLOAT, false, 0, 0);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
    }

    // readPixels 由左下角開始，翻回 ImageData 座標系後包成 ImageData。
    function readImageData(gl, width, height) {
        var pixels = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        var rowSize = width * 4;
        var output = new Uint8ClampedArray(pixels.length);
        for (var y = 0; y < height; y += 1) {
            var sourceStart = (height - y - 1) * rowSize;
            output.set(pixels.subarray(sourceStart, sourceStart + rowSize), y * rowSize);
        }
        return new ImageData(output, width, height);
    }

    app.pages.ditherEditor = app.pages.ditherEditor || {};
    app.pages.ditherEditor.glHelpers = {
        createContext: createContext,
        createProgram: createProgram,
        createShader: createShader,
        createTexture: createTexture,
        createQuadBuffers: createQuadBuffers,
        fullscreenVertexShader: fullscreenVertexShader,
        drawQuad: drawQuad,
        readImageData: readImageData
    };
})(window.DitherApp);
