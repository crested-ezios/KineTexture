"""Minimal in-memory channel contract; this is not a file format."""
from dataclasses import dataclass
from enum import Enum
import math


class ValueType(str, Enum):
    SCALAR = 'scalar'
    VECTOR3 = 'vector3'
    ROTATION = 'rotation_xyzw'


class Representation(str, Enum):
    ABSOLUTE = 'absolute'
    DELTA = 'delta'


@dataclass(frozen=True)
class Timebase:
    sample_rate_hz: float
    origin_seconds: float = 0.0

    def __post_init__(self):
        if not math.isfinite(self.sample_rate_hz) or self.sample_rate_hz <= 0:
            raise ValueError('sample_rate_hz must be finite and positive')
        if not math.isfinite(self.origin_seconds):
            raise ValueError('origin_seconds must be finite')

    def seconds(self, tick):
        return self.origin_seconds + tick / self.sample_rate_hz

    def ticks(self, seconds):
        return (seconds - self.origin_seconds) * self.sample_rate_hz


@dataclass(frozen=True)
class Provenance:
    source_id: str
    source_digest: str = ''
    source_channel: str = ''
    operation: str = 'source'

    def __post_init__(self):
        if not isinstance(self.source_id, str) or not self.source_id.strip():
            raise ValueError('source_id must identify the source, including synthetic sources')
        if not isinstance(self.operation, str) or not self.operation.strip():
            raise ValueError('operation must describe source or derivation')
        if not isinstance(self.source_digest, str) or not isinstance(self.source_channel, str):
            raise ValueError('provenance fields must be strings')


@dataclass(frozen=True)
class ChannelSemantics:
    name: str
    value_type: ValueType
    representation: Representation
    unit: str
    space: str
    reference_id: str = ''

    def __post_init__(self):
        if not all(isinstance(v, str) and v.strip() for v in (self.name, self.unit, self.space)):
            raise ValueError('channel name, unit and space must be explicit')
        if not isinstance(self.value_type, ValueType) or not isinstance(self.representation, Representation):
            raise ValueError('value type and absolute/delta representation must be explicit')
        if not isinstance(self.reference_id, str):
            raise ValueError('reference_id must be a string')
        if self.representation is Representation.DELTA and not self.reference_id.strip():
            raise ValueError('delta channels require an explicit reference_id')
