from functools import lru_cache
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


@lru_cache(maxsize=1)
def ensure_build() -> None:
    subprocess.run(["make", "-j"], cwd=ROOT, check=True)
