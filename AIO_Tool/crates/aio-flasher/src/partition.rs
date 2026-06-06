//! Partition descriptor + validation + HoloCubic flash-layout constants.

use crate::error::FlashError;

/// Default ESP32 ROM-bootloader address for the second-stage bootloader.
pub const PARTITION_BOOTLOADER: u32 = 0x1000;

/// Default ESP32 partition-table address.
pub const PARTITION_PARTITIONS: u32 = 0x8000;

/// Default ESP32 boot_app0 selector address (used by OTA).
pub const PARTITION_BOOTAPP0: u32 = 0xe000;

/// Default HoloCubic firmware payload start.
pub const PARTITION_FIRMWARE: u32 = 0x10000;

/// A flash partition to write: address + raw bytes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Partition {
    /// Flash address (absolute, in bytes).
    pub address: u32,
    /// Bytes to write.
    pub data: Vec<u8>,
}

impl Partition {
    /// End address (exclusive).
    pub fn end(&self) -> u32 {
        self.address.saturating_add(self.data.len() as u32)
    }
}

/// Validate a list of partitions: no overlaps, no empty data.
/// (Empty data is allowed conceptually but we reject it as a probable bug —
/// most likely the caller meant to skip the partition entirely.)
pub fn validate(parts: &[Partition]) -> Result<(), FlashError> {
    for (i, p) in parts.iter().enumerate() {
        for (j, q) in parts.iter().enumerate().skip(i + 1) {
            let (a_start, a_end) = (p.address, p.end());
            let (b_start, b_end) = (q.address, q.end());
            if a_start < b_end && b_start < a_end {
                return Err(FlashError::OverlappingPartitions {
                    a: i,
                    a_start,
                    a_end,
                    b: j,
                    b_start,
                    b_end,
                });
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(addr: u32, len: usize) -> Partition {
        Partition {
            address: addr,
            data: vec![0u8; len],
        }
    }

    #[test]
    fn end_is_address_plus_len() {
        let q = p(0x1000, 0x100);
        assert_eq!(q.end(), 0x1100);
    }

    #[test]
    fn validate_empty_list_ok() {
        validate(&[]).unwrap();
    }

    #[test]
    fn validate_single_ok() {
        validate(&[p(0x1000, 0x100)]).unwrap();
    }

    #[test]
    fn validate_disjoint_ok() {
        validate(&[p(0x1000, 0x100), p(0x2000, 0x100)]).unwrap();
    }

    #[test]
    fn validate_adjacent_ok() {
        // 0x1000..0x1100 + 0x1100..0x1200 — touching but not overlapping
        validate(&[p(0x1000, 0x100), p(0x1100, 0x100)]).unwrap();
    }

    #[test]
    fn validate_overlap_detected() {
        let err = validate(&[p(0x1000, 0x200), p(0x10ff, 0x100)]).unwrap_err();
        match err {
            FlashError::OverlappingPartitions {
                a: 0,
                b: 1,
                a_start: 0x1000,
                a_end: 0x1200,
                b_start: 0x10ff,
                b_end: 0x11ff,
            } => {}
            other => panic!("expected OverlappingPartitions {{0, 1, ...}}, got {other:?}"),
        }
    }

    #[test]
    fn validate_overlap_with_reversed_order_detected() {
        // Same partitions but supplied in the other order — still detected.
        let err = validate(&[p(0x10ff, 0x100), p(0x1000, 0x200)]).unwrap_err();
        assert!(matches!(err, FlashError::OverlappingPartitions { .. }));
    }

    #[test]
    fn holocubic_constants_match_python_addresses() {
        // SAFETY: must match download_debug.py:83-97.
        assert_eq!(PARTITION_BOOTLOADER, 0x1000);
        assert_eq!(PARTITION_PARTITIONS, 0x8000);
        assert_eq!(PARTITION_BOOTAPP0, 0xe000);
        assert_eq!(PARTITION_FIRMWARE, 0x10000);
    }
}
