"""VMD source adapter preview. No game installation or third-party runtime imports."""

from .parser import VmdError, parse_vmd
from .sampling import sample_track

__all__ = ["VmdError", "parse_vmd", "sample_track"]
