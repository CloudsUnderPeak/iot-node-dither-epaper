(function (app) {
    // Dither matrices 是演算法參數資料，不含執行邏輯。
    // error diffusion 使用一維 offset/factor；ordered dithering 使用二維 Bayer threshold map。
    app.pages.ditherEditor = app.pages.ditherEditor || {};

    function buildBayer(size) {
        var matrix = [[0]];
        var currentSize = 1;
        while (currentSize < size) {
            var nextSize = currentSize * 2;
            var next = new Array(nextSize);
            for (var y = 0; y < nextSize; y += 1) {
                next[y] = new Array(nextSize);
            }
            for (var row = 0; row < currentSize; row += 1) {
                for (var col = 0; col < currentSize; col += 1) {
                    var value = matrix[row][col];
                    next[row][col] = 4 * value;
                    next[row][col + currentSize] = 4 * value + 2;
                    next[row + currentSize][col] = 4 * value + 3;
                    next[row + currentSize][col + currentSize] = 4 * value + 1;
                }
            }
            matrix = next;
            currentSize = nextSize;
        }
        return matrix;
    }

    function mulberry32(seed) {
        return function random() {
            var t = seed += 0x6D2B79F5;
            t = Math.imul(t ^ (t >>> 15), t | 1);
            t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        };
    }

    function buildBlueNoiseKernel() {
        var offsets = [];
        var radius = 5;
        var sigma = 2.1;
        var sigmaScale = 2 * sigma * sigma;

        for (var y = -radius; y <= radius; y += 1) {
            for (var x = -radius; x <= radius; x += 1) {
                var distance = x * x + y * y;
                if (distance > radius * radius) {
                    continue;
                }
                offsets.push({
                    x: x,
                    y: y,
                    weight: Math.exp(-distance / sigmaScale)
                });
            }
        }

        return offsets;
    }

    function wrapCoordinate(value, size) {
        if (value < 0) {
            return value + size;
        }
        if (value >= size) {
            return value - size;
        }
        return value;
    }

    function shuffledIndexes(total, rng) {
        var indexes = new Uint16Array(total);
        for (var i = 0; i < total; i += 1) {
            indexes[i] = i;
        }
        for (var end = total - 1; end > 0; end -= 1) {
            var swapIndex = (rng() * (end + 1)) | 0;
            var swap = indexes[end];
            indexes[end] = indexes[swapIndex];
            indexes[swapIndex] = swap;
        }
        return indexes;
    }

    function createBlueNoiseMask(size) {
        var total = size * size;
        var half = total / 2;
        var rng = mulberry32(0xB1);
        var kernel = buildBlueNoiseKernel();
        var selected = new Uint8Array(total);
        var scores = new Float32Array(total);
        var jitter = new Float32Array(total);
        var indexes = shuffledIndexes(total, rng);
        var ranking = new Uint16Array(total);

        for (var i = 0; i < half; i += 1) {
            selected[indexes[i]] = 1;
        }
        for (var jitterIndex = 0; jitterIndex < total; jitterIndex += 1) {
            jitter[jitterIndex] = rng() * 0.000001;
        }

        function adjustScores(index, sign, targetScores) {
            var x = index % size;
            var y = (index / size) | 0;
            for (var offsetIndex = 0; offsetIndex < kernel.length; offsetIndex += 1) {
                var offset = kernel[offsetIndex];
                var nx = wrapCoordinate(x + offset.x, size);
                var ny = wrapCoordinate(y + offset.y, size);
                targetScores[ny * size + nx] += sign * offset.weight;
            }
        }

        function buildScores(targetSelected, targetScores) {
            for (var point = 0; point < total; point += 1) {
                if (targetSelected[point]) {
                    adjustScores(point, 1, targetScores);
                }
            }
        }

        function findTightestCluster(targetSelected, targetScores) {
            var bestIndex = 0;
            var bestScore = -Infinity;
            for (var point = 0; point < total; point += 1) {
                if (!targetSelected[point]) {
                    continue;
                }
                var score = targetScores[point] + jitter[point];
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = point;
                }
            }
            return bestIndex;
        }

        function findLargestVoid(targetSelected, targetScores) {
            var bestIndex = 0;
            var bestScore = Infinity;
            for (var point = 0; point < total; point += 1) {
                if (targetSelected[point]) {
                    continue;
                }
                var score = targetScores[point] - jitter[point];
                if (score < bestScore) {
                    bestScore = score;
                    bestIndex = point;
                }
            }
            return bestIndex;
        }

        function addPoint(targetSelected, targetScores, index) {
            targetSelected[index] = 1;
            adjustScores(index, 1, targetScores);
        }

        function removePoint(targetSelected, targetScores, index) {
            targetSelected[index] = 0;
            adjustScores(index, -1, targetScores);
        }

        buildScores(selected, scores);

        for (var settle = 0; settle < 512; settle += 1) {
            var cluster = findTightestCluster(selected, scores);
            removePoint(selected, scores, cluster);
            var voidIndex = findLargestVoid(selected, scores);
            addPoint(selected, scores, voidIndex);
        }

        var baseSelected = new Uint8Array(selected);
        var baseScores = new Float32Array(scores);

        for (var lowRank = half - 1; lowRank >= 0; lowRank -= 1) {
            var lowCluster = findTightestCluster(selected, scores);
            ranking[lowCluster] = lowRank;
            removePoint(selected, scores, lowCluster);
        }

        selected = baseSelected;
        scores = baseScores;

        for (var highRank = half; highRank < total; highRank += 1) {
            var highVoid = findLargestVoid(selected, scores);
            ranking[highVoid] = highRank;
            addPoint(selected, scores, highVoid);
        }

        return ranking;
    }

    function blueNoise64() {
        var size = 64;
        var mask = null;
        return {
            size: size,
            levels: size * size,
            cell: function cell(x, y) {
                if (!mask) {
                    mask = createBlueNoiseMask(size);
                }
                return mask[(y % size) * size + (x % size)];
            }
        };
    }

    app.pages.ditherEditor.ditherMatrices = {
        floydSteinberg: [
            { x: 1, y: 0, factor: 7 / 16 },
            { x: -1, y: 1, factor: 3 / 16 },
            { x: 0, y: 1, factor: 5 / 16 },
            { x: 1, y: 1, factor: 1 / 16 }
        ],
        atkinson: [
            { x: 1, y: 0, factor: 1 / 8 },
            { x: 2, y: 0, factor: 1 / 8 },
            { x: -1, y: 1, factor: 1 / 8 },
            { x: 0, y: 1, factor: 1 / 8 },
            { x: 1, y: 1, factor: 1 / 8 },
            { x: 0, y: 2, factor: 1 / 8 }
        ],
        jarvis: [
            { x: 1, y: 0, factor: 7 / 48 },
            { x: 2, y: 0, factor: 5 / 48 },
            { x: -2, y: 1, factor: 3 / 48 },
            { x: -1, y: 1, factor: 5 / 48 },
            { x: 0, y: 1, factor: 7 / 48 },
            { x: 1, y: 1, factor: 5 / 48 },
            { x: 2, y: 1, factor: 3 / 48 },
            { x: -2, y: 2, factor: 1 / 48 },
            { x: -1, y: 2, factor: 3 / 48 },
            { x: 0, y: 2, factor: 5 / 48 },
            { x: 1, y: 2, factor: 3 / 48 },
            { x: 2, y: 2, factor: 1 / 48 }
        ],
        sierraLite: [
            { x: 1, y: 0, factor: 2 / 4 },
            { x: -1, y: 1, factor: 1 / 4 },
            { x: 0, y: 1, factor: 1 / 4 }
        ],
        stevensonArce: [
            { x: 2, y: 0, factor: 32 / 200 },
            { x: -5, y: 1, factor: 12 / 200 },
            { x: -3, y: 1, factor: 26 / 200 },
            { x: -1, y: 1, factor: 30 / 200 },
            { x: 1, y: 1, factor: 30 / 200 },
            { x: 3, y: 1, factor: 26 / 200 },
            { x: 5, y: 1, factor: 12 / 200 },
            { x: -4, y: 2, factor: 12 / 200 },
            { x: -2, y: 2, factor: 26 / 200 },
            { x: 0, y: 2, factor: 12 / 200 },
            { x: 2, y: 2, factor: 26 / 200 },
            { x: 4, y: 2, factor: 12 / 200 }
        ],
        bayer4: [
            [0, 8, 2, 10],
            [12, 4, 14, 6],
            [3, 11, 1, 9],
            [15, 7, 13, 5]
        ],
        bayer8: buildBayer(8),
        blueNoise64: blueNoise64()
    };
})(window.DitherApp);
