PYTHON ?= python3
BUILD_DIR ?= build
BUILD_FLAGS ?=

.PHONY: build demo minify gzip clean

build:
	$(PYTHON) tools/build/run.py --output $(BUILD_DIR) $(BUILD_FLAGS)

demo:
	$(PYTHON) tools/build/run.py --output $(BUILD_DIR) --demo $(BUILD_FLAGS)

minify:
	$(PYTHON) tools/build/run.py --output $(BUILD_DIR) --no-gzip $(BUILD_FLAGS)

gzip:
	$(PYTHON) tools/build/run.py --output $(BUILD_DIR) --no-minify $(BUILD_FLAGS)

clean:
	rm -rf $(BUILD_DIR)

TEST_FLAGS ?=
.PHONY: test test-production
test:
	$(PYTHON) tools/test/run.py $(TEST_FLAGS)

test-production:
	$(PYTHON) tools/test/run.py --production $(TEST_FLAGS)
