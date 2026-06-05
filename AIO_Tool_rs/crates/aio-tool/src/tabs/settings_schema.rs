//! Settings schema — parsed at startup from `cubictool.json`.

use std::sync::OnceLock;

/// One row of the settings UI.
#[derive(Debug, Clone)]
pub struct SettingKey {
    /// The key name (e.g. `"ssid_1"`).
    pub key: String,
    /// Firmware namespace (e.g. `"sys"`, `"zhixin"`, `"tianqi"`, `"other"`).
    pub namespace: String,
    /// Wire value type — `"String"` or `"UChar"` per cubictool.json.
    pub value_type: String,
}

/// Embedded copy of `AIO_Tool/cubictool.json`. Path is relative to this
/// source file; at v3 cutover (Plan 10) this collapses to a sibling
/// `cubictool.json` next to `i18n/`.
const SCHEMA_RAW: &str = include_str!("../../../../../AIO_Tool/cubictool.json");

/// Lazily-parsed schema. Returns the keys in JSON file order (we treat
/// that as the canonical display order for the UI sections).
pub fn schema() -> &'static [SettingKey] {
    static SCHEMA: OnceLock<Vec<SettingKey>> = OnceLock::new();
    SCHEMA.get_or_init(|| {
        let raw: serde_json::Map<String, serde_json::Value> =
            serde_json::from_str(SCHEMA_RAW).expect("cubictool.json must parse");
        raw.into_iter()
            .map(|(key, value)| SettingKey {
                key,
                namespace: value["namespace"].as_str().unwrap_or("").to_owned(),
                value_type: value["type"].as_str().unwrap_or("").to_owned(),
            })
            .collect()
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn schema_has_15_keys_across_4_namespaces() {
        let keys = schema();
        assert_eq!(keys.len(), 15);
        let namespaces: std::collections::HashSet<&str> =
            keys.iter().map(|k| k.namespace.as_str()).collect();
        assert!(namespaces.contains("sys"));
        assert!(namespaces.contains("zhixin"));
        assert!(namespaces.contains("tianqi"));
        assert!(namespaces.contains("other"));
        assert_eq!(namespaces.len(), 4);
    }

    #[test]
    fn ssid_is_first_and_in_sys_namespace() {
        let keys = schema();
        assert_eq!(keys[0].key, "ssid");
        assert_eq!(keys[0].namespace, "sys");
        assert_eq!(keys[0].value_type, "String");
    }

    #[test]
    fn types_are_only_string_or_uchar() {
        for k in schema() {
            assert!(
                matches!(k.value_type.as_str(), "String" | "UChar"),
                "key {} has unexpected type {:?}",
                k.key,
                k.value_type
            );
        }
    }
}
