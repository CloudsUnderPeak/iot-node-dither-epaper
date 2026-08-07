.DEFAULT_GOAL := help

PIO ?= pio
PIO_ENV ?= firebeetle2_esp32c6
PYTHON ?= python3
PORT ?=
IMAGE ?=
WEB ?= auto
WEB_PROCESS ?= auto
RELEASE_TOOL := tools/release-build/build_release.py
WEB_TOOL := tools/web-build/build_web.py

ifneq ($(origin COMPONENT),undefined)
$(error COMPONENT was removed; use direct targets: make web, make demo, or make esp)
endif

.PHONY: help build web demo esp verify verify-web clean all test test-web deploy flash

help:
	@printf '%s\n' \
		'make build                         Build selected web, firmware, and a snapshot.' \
		'make build WEB=builtin             Build builtin web plus firmware.' \
		'make build WEB=user                Build user web plus firmware.' \
		'make build WEB=none                Build firmware without a frontend.' \
		'make web [WEB=...] [WEB_PROCESS=...] Build production web only.' \
		'make demo [WEB_PROCESS=...]        Build builtin mock demo only.' \
		'make esp                           Build firmware from current production web.' \
		'make verify [IMAGE=...]            Verify latest or historical full snapshot.' \
		'make verify-web                    Verify current build/latest/web only.' \
		'make flash PORT=<port> [IMAGE=...] Flash an existing verified snapshot.' \
		'make deploy PORT=<port> [WEB=...]  Clean, build, verify, and flash.' \
		'make deploy WEB=none PORT=<port>   Build and flash without a frontend.' \
		'make clean                         Remove latest and transient outputs.' \
		'make clean all                     Also remove timestamp snapshots.' \
		'make test                          Build and run native/browser tests.' \
		'' \
		'WEB: auto (default), builtin, user, none' \
		'WEB_PROCESS: auto (default), minify-gzip, none' \
		'auto processing: builtin=minify-gzip, user=none, none=none'

build:
	+@$(MAKE) --no-print-directory web WEB="$(WEB)" WEB_PROCESS="$(WEB_PROCESS)"
	+@$(MAKE) --no-print-directory esp

web:
	$(PYTHON) $(WEB_TOOL) build --target production --web "$(WEB)" --process "$(WEB_PROCESS)"

demo:
	$(PYTHON) $(WEB_TOOL) build --target demo --web builtin --process "$(WEB_PROCESS)"

esp:
	$(PYTHON) $(WEB_TOOL) verify --target production
	$(PYTHON) $(RELEASE_TOOL) build --pio "$(PIO)" --environment "$(PIO_ENV)"

verify:
	@if [ -z "$(strip $(IMAGE))" ]; then \
		$(PYTHON) $(WEB_TOOL) verify --target production; \
	fi
	$(PYTHON) $(RELEASE_TOOL) verify $(if $(strip $(IMAGE)),--image "$(IMAGE)")

verify-web:
	$(PYTHON) $(WEB_TOOL) verify

clean:
	$(PYTHON) $(RELEASE_TOOL) clean $(if $(filter all,$(MAKECMDGOALS)),--all)

all:
	@if [ "$(firstword $(MAKECMDGOALS))" != "clean" ]; then \
		echo "'all' is only a cleanup modifier; use: make clean all" >&2; \
		exit 2; \
	fi

test:
	+@$(MAKE) --no-print-directory build
	+@$(MAKE) --no-print-directory test-web
	PIO_ENV="$(PIO_ENV)" bash tools/native-test/test_native.sh

test-web:
	$(PYTHON) tools/web-test/run.py

deploy:
	@if [ -z "$(strip $(PORT))" ]; then \
		echo 'PORT is required. Confirm the current port, then run: make deploy PORT=<port>' >&2; \
		exit 2; \
	fi
	+@$(MAKE) --no-print-directory clean
	+@$(MAKE) --no-print-directory build WEB="$(WEB)" WEB_PROCESS="$(WEB_PROCESS)"
	+@$(MAKE) --no-print-directory flash PORT="$(PORT)"

flash:
	@if [ -z "$(strip $(PORT))" ]; then \
		echo 'PORT is required. Confirm the current port, then run: make flash PORT=<port>' >&2; \
		exit 2; \
	fi
	+@$(MAKE) --no-print-directory verify IMAGE="$(IMAGE)"
	$(PYTHON) $(RELEASE_TOOL) flash --pio "$(PIO)" --port "$(PORT)" $(if $(strip $(IMAGE)),--image "$(IMAGE)")
