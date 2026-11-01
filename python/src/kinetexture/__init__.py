"""KineTexture performance semantics and numerical primitives.

No source format, target skeleton, engine, or product policy is implied.
"""
from .semantics import ChannelSemantics, Provenance, Representation, Timebase, ValueType
from .scalar import ScalarSamples, ScalarTrack

__all__ = ["ChannelSemantics", "Provenance", "Representation", "Timebase",
           "ValueType", "ScalarSamples", "ScalarTrack"]
