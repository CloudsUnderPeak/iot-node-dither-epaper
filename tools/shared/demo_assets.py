from pathlib import Path


DEMO_ASSET_DIR = Path("assets") / "demo"
DEMO_MANIFEST_NAME = "demo-manifest.js"
DEMO_DATA_NAME = "demo-data.js"
LEGACY_DEMO_DATA_NAME = "demo-16x9-data.js"
SUPPORTED_DEMO_EXTENSIONS = (".png", ".jpg", ".jpeg", ".webp")


def demo_manifest_path(root: Path) -> Path:
    return root / DEMO_ASSET_DIR / DEMO_MANIFEST_NAME


def demo_data_path(root: Path) -> Path:
    return root / DEMO_ASSET_DIR / DEMO_DATA_NAME


def generated_demo_data_paths() -> set[Path]:
    return {
        DEMO_ASSET_DIR / DEMO_DATA_NAME,
        DEMO_ASSET_DIR / LEGACY_DEMO_DATA_NAME,
    }


def discover_demo_source(root: Path) -> Path:
    demo_dir = root / DEMO_ASSET_DIR
    candidates = [
        path
        for path in demo_dir.iterdir()
        if is_demo_source_candidate(path)
    ]
    candidates.sort(key=lambda path: path.name.lower())

    if not candidates:
        extensions = ", ".join(SUPPORTED_DEMO_EXTENSIONS)
        raise ValueError(
            f"No demo image found in {DEMO_ASSET_DIR}. "
            f"Add exactly one supported image ({extensions})."
        )
    if len(candidates) > 1:
        names = ", ".join(path.name for path in candidates)
        raise ValueError(
            f"Multiple demo images found in {DEMO_ASSET_DIR}: {names}. "
            "Keep exactly one demo source image."
        )
    return candidates[0]


def is_demo_source_candidate(path: Path) -> bool:
    return (
        path.is_file()
        and not path.name.startswith(".")
        and path.suffix.lower() in SUPPORTED_DEMO_EXTENSIONS
    )
