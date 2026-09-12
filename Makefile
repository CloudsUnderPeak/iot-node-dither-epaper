.DEFAULT_GOAL := help

PIO ?= pio
PIO_ENV ?= firebeetle2_esp32c6
PYTHON ?= python3
PORT ?=
IMAGE ?=
WEB ?= user
WEB_PROCESS ?= auto
RELEASE_TOOL := tools/release-build/build_release.py
WEB_TOOL := tools/web-build/build_web.py
USER_WEB_PROJECT := user-web-project
USER_WEB_LATEST := $(USER_WEB_PROJECT)/build/latest
USER_WEB_REMOTE ?= embedded-web-dithering
USER_WEB_BRANCH ?= six-color-epaper
USER_WEB_ACTION := $(filter pull push,$(MAKECMDGOALS))
USER_WEB_OTHER_GOALS := $(filter-out user-web pull push,$(MAKECMDGOALS))

ifneq ($(origin COMPONENT),undefined)
$(error COMPONENT was removed; use direct targets: make web, make demo, or make esp)
endif

.PHONY: help build prepare-user-web user-web pull push web demo esp verify verify-web clean all test test-native test-tools test-web test-all deploy flash

help:
	@printf '%s\n' \
		'make build                         Rebuild user web, firmware, and a snapshot.' \
		'make build WEB=builtin             Build builtin web plus firmware.' \
		'make build WEB=user                Rebuild user web plus firmware.' \
		'make build WEB=none                Build firmware without a frontend.' \
		'make prepare-user-web              Rebuild and import user-web-project.' \
		'make user-web pull                 Pull the configured user-web subtree branch.' \
		'make user-web push                 Push committed user-web subtree changes.' \
		'make web [WEB=...] [WEB_PROCESS=...] Build production web only.' \
		'make demo [WEB_PROCESS=...]        Build builtin mock demo only.' \
		'make esp                           Build firmware from current production web.' \
		'make verify [IMAGE=...]            Verify latest or historical full snapshot.' \
		'make verify-web                    Verify current build/latest/web only.' \
		'make flash PORT=<port> [IMAGE=...] Flash an existing verified snapshot.' \
		'make deploy PORT=<port> [WEB=...]  Clean, build, verify, and flash.' \
		'make deploy WEB=none PORT=<port>   Build and flash without a frontend.' \
		'make clean                         Remove latest, transients, and imported user web.' \
		'make clean all                     Also remove timestamp snapshots.' \
		'make test                          Run native, tools, and browser tests without firmware build.' \
		'make test-all                      Build, verify, and run all tests.' \
		'' \
		'WEB: user (default), auto, builtin, none' \
		'WEB_PROCESS: auto (default), minify-gzip, none' \
		'auto processing: builtin=minify-gzip, user=none, none=none'

build:
	+@if [ "$(WEB)" = "user" ]; then \
		$(MAKE) --no-print-directory prepare-user-web; \
	fi
	+@$(MAKE) --no-print-directory web WEB="$(WEB)" WEB_PROCESS="$(WEB_PROCESS)"
	+@$(MAKE) --no-print-directory esp

prepare-user-web:
	+@$(MAKE) -C $(USER_WEB_PROJECT) --no-print-directory build
	$(PYTHON) $(WEB_TOOL) import-user --source "$(USER_WEB_LATEST)"

user-web:
	@test -z "$(USER_WEB_OTHER_GOALS)" || { \
		echo "Unexpected user-web arguments: $(USER_WEB_OTHER_GOALS)" >&2; \
		echo 'Usage: make user-web pull | make user-web push' >&2; exit 2; \
	}
	@case "$(USER_WEB_ACTION)" in \
		pull) \
			git remote get-url "$(USER_WEB_REMOTE)" >/dev/null 2>&1 || { \
				echo "Missing Git remote '$(USER_WEB_REMOTE)'; see README for setup." >&2; exit 2; \
			}; \
			test -z "$$(git status --porcelain --untracked-files=normal)" || { \
				echo 'Commit, stash, or remove worktree changes before pulling the user-web subtree.' >&2; exit 2; \
			}; \
			git subtree pull --prefix="$(USER_WEB_PROJECT)" "$(USER_WEB_REMOTE)" "$(USER_WEB_BRANCH)" --squash ;; \
		push) \
			git remote get-url "$(USER_WEB_REMOTE)" >/dev/null 2>&1 || { \
				echo "Missing Git remote '$(USER_WEB_REMOTE)'; see README for setup." >&2; exit 2; \
			}; \
			test -z "$$(git status --porcelain --untracked-files=normal -- "$(USER_WEB_PROJECT)")" || { \
				echo 'Commit, stash, or remove user-web-project changes before pushing its subtree.' >&2; exit 2; \
			}; \
			git subtree push --prefix="$(USER_WEB_PROJECT)" "$(USER_WEB_REMOTE)" "$(USER_WEB_BRANCH)" ;; \
		*) \
			echo 'Usage: make user-web pull | make user-web push' >&2; exit 2 ;; \
	esac

pull push:
	@test "$(filter user-web,$(MAKECMDGOALS))" = user-web || { \
		echo 'Use: make user-web pull | make user-web push' >&2; exit 2; \
	}

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
	$(PYTHON) $(WEB_TOOL) clean-user

all:
	@if [ "$(firstword $(MAKECMDGOALS))" != "clean" ]; then \
		echo "'all' is only a cleanup modifier; use: make clean all" >&2; \
		exit 2; \
	fi

test:
	+@$(MAKE) --no-print-directory test-native
	+@$(MAKE) --no-print-directory test-tools
	+@$(MAKE) --no-print-directory test-web

test-native:
	PIO_ENV="$(PIO_ENV)" bash tools/native-test/test_native.sh

test-tools:
	$(PYTHON) -m unittest discover -s tests/tools -p 'test_*.py' -v

test-all:
	+@$(MAKE) --no-print-directory build
	+@$(MAKE) --no-print-directory verify
	+@$(MAKE) --no-print-directory test

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
