"""Linear scalar tracks with explicit clock and missing-sample state."""
from dataclasses import dataclass
import numpy as np

from .semantics import ChannelSemantics, Provenance, Timebase, ValueType


@dataclass(frozen=True)
class ScalarSamples:
    values: np.ndarray
    valid: np.ndarray


class ScalarTrack:
    def __init__(self, ticks, values, *, timebase: Timebase,
                 semantics: ChannelSemantics, provenance: Provenance, valid=None):
        if not isinstance(timebase, Timebase) or not isinstance(semantics, ChannelSemantics):
            raise ValueError('timebase and channel semantics are required')
        if semantics.value_type is not ValueType.SCALAR or not isinstance(provenance, Provenance):
            raise ValueError('scalar semantics and provenance are required')
        t = np.array(ticks, dtype=np.float64, copy=True)
        v = np.array(values, dtype=np.float64, copy=True)
        mask = np.ones(v.shape, dtype=bool) if valid is None else np.asarray(valid)
        if t.ndim != 1 or v.shape != t.shape or mask.shape != t.shape:
            raise ValueError('ticks, values and valid must be equal-length one-dimensional arrays')
        if not np.isin(mask, (0, 1)).all():
            raise ValueError('valid must contain only 0 or 1')
        mask = mask.astype(bool, copy=True)
        with np.errstate(over='ignore', invalid='ignore'):
            spans = np.diff(t)
        if not np.isfinite(t).all() or not np.isfinite(spans).all() or np.any(spans <= 0):
            raise ValueError('ticks must be finite and strictly increasing; adapters resolve duplicate keys')
        if not np.isfinite(v[mask]).all():
            raise ValueError('available scalar values must be finite')
        v[~mask] = 0.0  # Sentinel, not authored zero. Consumers must read valid.
        t.setflags(write=False)
        v.setflags(write=False)
        mask.setflags(write=False)
        self.ticks, self.values, self.valid = t, v, mask
        self.timebase, self.semantics, self.provenance = timebase, semantics, provenance

    def sample_ticks(self, ticks):
        """Evaluate explicit clock coordinates; hold endpoints, never bridge missing keys."""
        query = np.asarray(ticks, dtype=np.float64)
        if not np.isfinite(query).all():
            raise ValueError('sample times must be finite')
        shape = query.shape
        query = query.reshape(-1)
        if not len(self.ticks):
            return ScalarSamples(np.zeros(shape), np.zeros(shape, dtype=bool))
        low = np.clip(np.searchsorted(self.ticks, query, side='right') - 1, 0, len(self.ticks)-1)
        high = np.minimum(low + 1, len(self.ticks)-1)
        # Exact samples and endpoint holds require only that key. A missing
        # neighbour must not erase an explicitly present zero at a key time.
        high = np.where((query <= self.ticks[low]) | (query >= self.ticks[-1]), low, high)
        mask = self.valid[low] & self.valid[high]
        with np.errstate(over='ignore', invalid='ignore'):
            values = np.interp(query, self.ticks, self.values)
        if not np.isfinite(values[mask]).all():
            raise ValueError('scalar interpolation exceeds the finite numeric range')
        values[~mask] = 0.0
        return ScalarSamples(values.reshape(shape), mask.reshape(shape))

    def sample(self, seconds):
        query = np.asarray(seconds, dtype=np.float64)
        if not np.isfinite(query).all():
            raise ValueError('sample times must be finite')
        if not len(self.ticks):
            return ScalarSamples(np.zeros(query.shape), np.zeros(query.shape, dtype=bool))
        with np.errstate(over='ignore', invalid='ignore'):
            ticks = self.timebase.ticks(query)
        # Saturate finite extreme seconds before the low-level sampler. Positive
        # rate preserves the overflow sign, so endpoint holding remains defined.
        return self.sample_ticks(np.clip(ticks, self.ticks[0], self.ticks[-1]))
