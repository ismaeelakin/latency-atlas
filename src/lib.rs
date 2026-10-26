//! A mergeable base-2 histogram for nonnegative u64 observations.
//! Quantiles return bucket bounds, never a fictitious exact percentile.

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Histogram {
    buckets: [u64; 65],
    count: u64,
    min: u64,
    max: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    Overflow,
    InvalidPercentile,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Bounds {
    pub lower: u64,
    pub upper: u64,
}

impl Default for Histogram {
    fn default() -> Self {
        Self {
            buckets: [0; 65],
            count: 0,
            min: u64::MAX,
            max: 0,
        }
    }
}

impl Histogram {
    pub fn count(&self) -> u64 {
        self.count
    }
    pub fn buckets(&self) -> &[u64; 65] {
        &self.buckets
    }
    pub fn min(&self) -> Option<u64> {
        (self.count != 0).then_some(self.min)
    }
    pub fn max(&self) -> Option<u64> {
        (self.count != 0).then_some(self.max)
    }

    /// Record a weighted observation. A zero weight is a no-op.
    /// If count would overflow, the histogram remains unchanged.
    pub fn record_n(&mut self, value: u64, weight: u64) -> Result<(), Error> {
        if weight == 0 {
            return Ok(());
        }
        let count = self.count.checked_add(weight).ok_or(Error::Overflow)?;
        let bucket = (64 - value.leading_zeros()) as usize;
        self.buckets[bucket] += weight; // <= checked total count
        self.count = count;
        self.min = self.min.min(value);
        self.max = self.max.max(value);
        Ok(())
    }

    pub fn record(&mut self, value: u64) -> Result<(), Error> {
        self.record_n(value, 1)
    }

    /// Associative merge of compatible histograms. Failure is atomic.
    pub fn merge(&mut self, other: &Self) -> Result<(), Error> {
        let count = self.count.checked_add(other.count).ok_or(Error::Overflow)?;
        for (a, b) in self.buckets.iter_mut().zip(other.buckets.iter()) {
            *a += b;
        }
        self.count = count;
        self.min = self.min.min(other.min);
        self.max = self.max.max(other.max);
        Ok(())
    }

    /// Nearest-rank percentile in integer basis points (0..=10000).
    /// p0 and p100 use the exact observed minimum and maximum.
    pub fn quantile(&self, basis_points: u16) -> Result<Option<Bounds>, Error> {
        if basis_points > 10000 {
            return Err(Error::InvalidPercentile);
        }
        if self.count == 0 {
            return Ok(None);
        }
        if basis_points == 0 {
            return Ok(Some(Bounds {
                lower: self.min,
                upper: self.min,
            }));
        }
        if basis_points == 10000 {
            return Ok(Some(Bounds {
                lower: self.max,
                upper: self.max,
            }));
        }
        // u128 avoids overflow for all valid u64 counts and basis points.
        let rank = ((self.count as u128 * basis_points as u128).div_ceil(10000)) as u64;
        let mut cumulative = 0;
        for (index, weight) in self.buckets.iter().enumerate() {
            cumulative += weight;
            if cumulative >= rank {
                let bounds = bucket_bounds(index);
                return Ok(Some(Bounds {
                    lower: bounds.lower.max(self.min),
                    upper: bounds.upper.min(self.max),
                }));
            }
        }
        unreachable!("private histogram invariants keep bucket sum equal to count")
    }
}

fn bucket_bounds(index: usize) -> Bounds {
    match index {
        0 => Bounds { lower: 0, upper: 0 },
        64 => Bounds {
            lower: 1_u64 << 63,
            upper: u64::MAX,
        },
        n => Bounds {
            lower: 1_u64 << (n - 1),
            upper: (1_u64 << n) - 1,
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_zero_and_endpoints() {
        let mut h = Histogram::default();
        assert_eq!(h.quantile(5000), Ok(None));
        assert_eq!(h.quantile(10001), Err(Error::InvalidPercentile));
        h.record(0).unwrap();
        h.record(u64::MAX).unwrap();
        assert_eq!(h.quantile(0).unwrap().unwrap().lower, 0);
        assert_eq!(h.quantile(10000).unwrap().unwrap().upper, u64::MAX);
        assert_eq!(
            h.quantile(5000).unwrap().unwrap(),
            Bounds { lower: 0, upper: 0 }
        );
    }

    #[test]
    fn every_power_boundary_is_in_the_right_bucket() {
        for bit in 0..64 {
            let value = 1_u64 << bit;
            let mut h = Histogram::default();
            h.record(value).unwrap();
            assert_eq!(h.buckets()[bit + 1], 1);
            assert_eq!(
                h.quantile(9900).unwrap().unwrap(),
                Bounds {
                    lower: value,
                    upper: value
                }
            );
        }
    }

    #[test]
    fn quantile_bounds_contain_exact_order_statistics() {
        let mut values = Vec::new();
        let mut state = 0x12345678_u64;
        let mut h = Histogram::default();
        for _ in 0..10000 {
            state = state.wrapping_mul(6364136223846793005).wrapping_add(1);
            values.push(state);
            h.record(state).unwrap();
        }
        values.sort_unstable();
        for p in 1..10000 {
            let rank = (values.len() as u128 * p as u128).div_ceil(10000) as usize;
            let exact = values[rank - 1];
            let bounds = h.quantile(p).unwrap().unwrap();
            assert!(bounds.lower <= exact && exact <= bounds.upper);
        }
    }

    #[test]
    fn partition_merge_matches_single_pass() {
        let mut all = Histogram::default();
        let mut a = Histogram::default();
        let mut b = Histogram::default();
        for value in 0..10000 {
            all.record(value).unwrap();
            if value % 2 == 0 {
                a.record(value).unwrap();
            } else {
                b.record(value).unwrap();
            }
        }
        a.merge(&b).unwrap();
        assert_eq!(a, all);
    }

    #[test]
    fn overflow_does_not_mutate() {
        let mut h = Histogram::default();
        h.record_n(7, u64::MAX).unwrap();
        let before = h.clone();
        assert_eq!(h.record(1), Err(Error::Overflow));
        assert_eq!(h, before);
        assert_eq!(h.merge(&before), Err(Error::Overflow));
        assert_eq!(h, before);
        assert_eq!(
            h.quantile(9999).unwrap().unwrap(),
            Bounds { lower: 7, upper: 7 }
        );
    }

    #[test]
    fn zero_weight_is_noop_and_merge_is_associative() {
        let mut a = Histogram::default();
        a.record_n(20, 0).unwrap();
        assert_eq!(a, Histogram::default());
        a.record(1).unwrap();
        let mut b = Histogram::default();
        b.record(100).unwrap();
        let mut c = Histogram::default();
        c.record(10000).unwrap();
        let mut left = a.clone();
        left.merge(&b).unwrap();
        left.merge(&c).unwrap();
        b.merge(&c).unwrap();
        a.merge(&b).unwrap();
        assert_eq!(left, a);
    }
}
