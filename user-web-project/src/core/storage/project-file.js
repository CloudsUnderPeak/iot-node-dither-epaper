(function (app) {
    var PNG_SIGNATURE = [137, 80, 78, 71, 13, 10, 26, 10];
    var PROJECT_SUFFIX = '.dither.png';
    var PROJECT_FORMAT = 'embedded-web-dithering-project';
    var SCHEMA_VERSION = 1;
    var MAX_PROJECT_BYTES = 64 * 1024 * 1024;
    var MAX_SOURCE_BYTES = 50 * 1024 * 1024;
    var MAX_MANIFEST_BYTES = 256 * 1024;
    var PROJECT_CHUNKS = { diMF: true, diOR: true, diWK: true };
    var crcTable = null;

    function projectError(code, message) {
        var error = new Error(message);
        error.code = code;
        return error;
    }

    function isProjectName(name) {
        return String(name || '').toLowerCase().endsWith(PROJECT_SUFFIX);
    }

    function isPng(bytes) {
        return bytes.length >= PNG_SIGNATURE.length && PNG_SIGNATURE.every(function (value, index) {
            return bytes[index] === value;
        });
    }

    function makeCrcTable() {
        var table = new Uint32Array(256);
        for (var n = 0; n < 256; n += 1) {
            var value = n;
            for (var k = 0; k < 8; k += 1) {
                value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
            }
            table[n] = value >>> 0;
        }
        return table;
    }

    function crc32(bytes, start, end) {
        crcTable = crcTable || makeCrcTable();
        var crc = 0xffffffff;
        for (var i = start || 0; i < (end === undefined ? bytes.length : end); i += 1) {
            crc = crcTable[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
        }
        return (crc ^ 0xffffffff) >>> 0;
    }

    function uint32(bytes, offset) {
        return (((bytes[offset] << 24) >>> 0)
            | (bytes[offset + 1] << 16)
            | (bytes[offset + 2] << 8)
            | bytes[offset + 3]) >>> 0;
    }

    function writeUint32(bytes, offset, value) {
        bytes[offset] = value >>> 24;
        bytes[offset + 1] = value >>> 16;
        bytes[offset + 2] = value >>> 8;
        bytes[offset + 3] = value;
    }

    function ascii(bytes, offset, length) {
        var value = '';
        for (var i = 0; i < length; i += 1) {
            value += String.fromCharCode(bytes[offset + i]);
        }
        return value;
    }

    function parsePng(bytes, verifyCrc) {
        if (!isPng(bytes)) {
            throw projectError('not-png', 'File is not a PNG image.');
        }
        var chunks = [];
        var offset = 8;
        var sawIhdr = false;
        var sawIend = false;
        while (offset < bytes.length) {
            if (offset + 12 > bytes.length) {
                throw projectError('png-corrupt', 'PNG chunk is truncated.');
            }
            var length = uint32(bytes, offset);
            var dataStart = offset + 8;
            var dataEnd = dataStart + length;
            var end = dataEnd + 4;
            if (length > MAX_PROJECT_BYTES || end < dataStart || end > bytes.length) {
                throw projectError('png-corrupt', 'PNG chunk length is invalid.');
            }
            var type = ascii(bytes, offset + 4, 4);
            if (!/^[A-Za-z]{4}$/.test(type)) {
                throw projectError('png-corrupt', 'PNG chunk type is invalid.');
            }
            if (!sawIhdr && type !== 'IHDR') {
                throw projectError('png-corrupt', 'PNG does not begin with IHDR.');
            }
            if (type === 'IHDR') {
                if (sawIhdr || length !== 13) {
                    throw projectError('png-corrupt', 'PNG IHDR is invalid.');
                }
                sawIhdr = true;
            }
            if (verifyCrc && crc32(bytes, offset + 4, dataEnd) !== uint32(bytes, dataEnd)) {
                throw projectError('png-corrupt', 'PNG chunk checksum is invalid.');
            }
            chunks.push({ type: type, offset: offset, dataStart: dataStart, dataEnd: dataEnd, end: end });
            offset = end;
            if (type === 'IEND') {
                sawIend = true;
                break;
            }
            if (chunks.length > 4096) {
                throw projectError('png-corrupt', 'PNG contains too many chunks.');
            }
        }
        if (!sawIhdr || !sawIend || offset !== bytes.length) {
            throw projectError('png-corrupt', 'PNG stream is incomplete or has trailing data.');
        }
        return chunks;
    }

    function chunksByType(chunks, type) {
        return chunks.filter(function (chunk) { return chunk.type === type; });
    }

    function pngDimensions(bytes, chunks) {
        var ihdr = chunksByType(chunks, 'IHDR')[0];
        return {
            width: uint32(bytes, ihdr.dataStart),
            height: uint32(bytes, ihdr.dataStart + 4)
        };
    }

    function hasProjectMarker(bytes) {
        if (!isPng(bytes)) {
            return false;
        }
        return parsePng(bytes, false).some(function (chunk) {
            return PROJECT_CHUNKS[chunk.type];
        });
    }

    function validJsonValue(value, depth) {
        if (depth > 16) {
            return false;
        }
        if (value === null || typeof value === 'string' || typeof value === 'boolean') {
            return typeof value !== 'string' || value.length <= MAX_MANIFEST_BYTES;
        }
        if (typeof value === 'number') {
            return Number.isFinite(value);
        }
        if (Array.isArray(value)) {
            return value.length <= 128 && value.every(function (item) {
                return validJsonValue(item, depth + 1);
            });
        }
        if (!value || typeof value !== 'object') {
            return false;
        }
        return Object.keys(value).length <= 128 && Object.keys(value).every(function (key) {
            return key !== '__proto__' && key !== 'prototype' && key !== 'constructor'
                && validJsonValue(value[key], depth + 1);
        });
    }

    function decodeManifest(bytes) {
        if (bytes.length > MAX_MANIFEST_BYTES) {
            throw projectError('project-data-invalid', 'Project manifest is too large.');
        }
        var text;
        try {
            text = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
        } catch (error) {
            throw projectError('project-data-invalid', 'Project manifest is not valid UTF-8.');
        }
        var manifest;
        try {
            manifest = JSON.parse(text);
        } catch (error) {
            throw projectError('project-data-invalid', 'Project manifest is not valid JSON.');
        }
        if (!validJsonValue(manifest, 0) || manifest.format !== PROJECT_FORMAT) {
            throw projectError('project-data-invalid', 'Project manifest is invalid.');
        }
        if (manifest.schemaVersion !== SCHEMA_VERSION) {
            throw projectError('schema-unsupported', 'Project version is not supported.');
        }
        return manifest;
    }

    function sourceMimeType(manifest, bytes) {
        var declared = manifest.source && manifest.source.mimeType;
        if (isPng(bytes)) {
            return declared === 'image/png' ? declared : null;
        }
        if (bytes.length >= 3 && bytes[0] === 0xff && bytes[1] === 0xd8 && bytes[2] === 0xff) {
            return declared === 'image/jpeg' ? declared : null;
        }
        if (bytes.length >= 12 && ascii(bytes, 0, 4) === 'RIFF' && ascii(bytes, 8, 4) === 'WEBP') {
            return declared === 'image/webp' ? declared : null;
        }
        return null;
    }

    function readBytes(file) {
        if (!file || !file.size || file.size > MAX_PROJECT_BYTES) {
            return Promise.reject(projectError('file-too-large', 'File is empty or too large.'));
        }
        return file.arrayBuffer().then(function (buffer) { return new Uint8Array(buffer); });
    }

    function classify(file) {
        var pngName = String(file && file.name || '').toLowerCase().endsWith('.png');
        if (!pngName && !isProjectName(file && file.name)) {
            return Promise.resolve({ kind: 'image', file: file });
        }
        return readBytes(file).then(function (bytes) {
            var marked = hasProjectMarker(bytes);
            if (marked) {
                return { kind: 'project', file: file, bytes: bytes };
            }
            if (isProjectName(file.name)) {
                throw projectError('not-project', 'The selected .dither.png file does not contain project data.');
            }
            return { kind: 'image', file: file };
        });
    }

    function readProject(route) {
        var bytes = route.bytes;
        var chunks = parsePng(bytes, true);
        ['diMF', 'diOR', 'diWK'].forEach(function (type) {
            if (chunksByType(chunks, type).length !== 1) {
                throw projectError('project-data-missing', 'Project data is missing or duplicated.');
            }
        });
        var manifestChunk = chunksByType(chunks, 'diMF')[0];
        var sourceChunk = chunksByType(chunks, 'diOR')[0];
        var workingChunk = chunksByType(chunks, 'diWK')[0];
        var manifest = decodeManifest(bytes.slice(manifestChunk.dataStart, manifestChunk.dataEnd));
        var sourceBytes = bytes.slice(sourceChunk.dataStart, sourceChunk.dataEnd);
        var workingBytes = bytes.slice(workingChunk.dataStart, workingChunk.dataEnd);
        var resultSize = pngDimensions(bytes, chunks);
        if (!manifest.result || manifest.result.width !== resultSize.width
            || manifest.result.height !== resultSize.height) {
            throw projectError('project-data-invalid', 'Preview dimensions do not match the project manifest.');
        }
        if (!sourceBytes.length || sourceBytes.length > MAX_SOURCE_BYTES
            || !manifest.source || manifest.source.byteLength !== sourceBytes.length
            || !sourceMimeType(manifest, sourceBytes)) {
            throw projectError('source-invalid', 'Embedded source image is invalid.');
        }
        if (!workingBytes.length || !isPng(workingBytes)) {
            throw projectError('source-invalid', 'Embedded working image is invalid.');
        }
        var workingChunks = parsePng(workingBytes, true);
        var workingSize = pngDimensions(workingBytes, workingChunks);
        if (!manifest.working || manifest.working.width !== workingSize.width
            || manifest.working.height !== workingSize.height) {
            throw projectError('source-invalid', 'Working image dimensions do not match the project manifest.');
        }
        return {
            manifest: manifest,
            sourceBlob: new Blob([sourceBytes], { type: manifest.source.mimeType }),
            workingBlob: new Blob([workingBytes], { type: 'image/png' })
        };
    }

    function encodeChunk(type, data) {
        var output = new Uint8Array(data.length + 12);
        writeUint32(output, 0, data.length);
        for (var i = 0; i < 4; i += 1) {
            output[4 + i] = type.charCodeAt(i);
        }
        output.set(data, 8);
        writeUint32(output, output.length - 4, crc32(output, 4, output.length - 4));
        return output;
    }

    function createProject(previewBlob, sourceBlob, workingBlob, manifest) {
        if (!sourceBlob || sourceBlob.size > MAX_SOURCE_BYTES) {
            return Promise.reject(projectError('source-invalid', 'Original source image is unavailable or too large.'));
        }
        return Promise.all([
            previewBlob.arrayBuffer(),
            sourceBlob.arrayBuffer(),
            workingBlob.arrayBuffer()
        ]).then(function (buffers) {
            var preview = new Uint8Array(buffers[0]);
            var source = new Uint8Array(buffers[1]);
            var working = new Uint8Array(buffers[2]);
            var manifestBytes = new TextEncoder().encode(JSON.stringify(manifest));
            if (manifestBytes.length > MAX_MANIFEST_BYTES) {
                throw projectError('project-data-invalid', 'Project manifest is too large.');
            }
            var chunks = parsePng(preview, true);
            var iend = chunks[chunks.length - 1];
            var parts = [
                preview.slice(0, iend.offset),
                encodeChunk('diMF', manifestBytes),
                encodeChunk('diOR', source),
                encodeChunk('diWK', working),
                preview.slice(iend.offset)
            ];
            var blob = new Blob(parts, { type: 'image/png' });
            if (blob.size > MAX_PROJECT_BYTES) {
                throw projectError('file-too-large', 'Project file exceeds the size limit.');
            }
            return blob;
        });
    }

    app.core.projectFile = {
        format: PROJECT_FORMAT,
        schemaVersion: SCHEMA_VERSION,
        suffix: PROJECT_SUFFIX,
        classify: classify,
        read: readProject,
        create: createProject,
        isProjectName: isProjectName
    };
})(window.DitherApp);
