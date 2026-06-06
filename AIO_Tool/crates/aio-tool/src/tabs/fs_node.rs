//! Filesystem tree node + path utilities for the File Manager tab.

/// One node in the remote SD card tree (file or directory).
#[derive(Debug, Clone)]
pub struct FsNode {
    /// Basename (e.g. `"foo.txt"`, `"sd"`).
    pub name: String,
    /// Absolute path on the device (e.g. `"/sd/foo.txt"`).
    pub path: String,
    /// True if directory, false if file.
    pub is_dir: bool,
    /// Child nodes — only populated for directories after their DirList
    /// reply arrives.
    pub children: Vec<FsNode>,
    /// True once we've received a DirList for this directory (so we don't
    /// re-fetch every frame).
    pub loaded: bool,
}

impl FsNode {
    /// Root node for the tree.
    pub fn root() -> Self {
        Self {
            name: "/".to_owned(),
            path: "/".to_owned(),
            is_dir: true,
            children: Vec::new(),
            loaded: false,
        }
    }

    /// Heuristic: entries with a `.` in the basename are files, others are
    /// dirs. See Plan 8 D7 — revise if firmware uses a different convention.
    pub fn is_dir_heuristic(name: &str) -> bool {
        !name.contains('.')
    }

    /// Build a child node from a basename and the parent's path.
    pub fn new_child(parent_path: &str, name: &str) -> Self {
        let path = if parent_path.ends_with('/') {
            format!("{parent_path}{name}")
        } else {
            format!("{parent_path}/{name}")
        };
        Self {
            name: name.to_owned(),
            path,
            is_dir: Self::is_dir_heuristic(name),
            children: Vec::new(),
            loaded: false,
        }
    }

    /// Replace this node's children with the given entries (used on DirList).
    ///
    /// Surviving entries (matched by basename) keep their cached subtree and
    /// `loaded` state so re-populate after a sibling delete doesn't collapse
    /// every expanded folder.
    pub fn populate(&mut self, entries: &[String]) {
        let mut old: std::collections::HashMap<String, FsNode> = self
            .children
            .drain(..)
            .map(|c| (c.name.clone(), c))
            .collect();
        self.children = entries
            .iter()
            .map(|name| {
                old.remove(name)
                    .unwrap_or_else(|| Self::new_child(&self.path, name))
            })
            .collect();
        self.loaded = true;
    }

    /// Find a mutable reference to the node at `path` in the tree.
    pub fn find_mut<'a>(&'a mut self, path: &str) -> Option<&'a mut FsNode> {
        if self.path == path {
            return Some(self);
        }
        for child in &mut self.children {
            if let Some(found) = child.find_mut(path) {
                return Some(found);
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn root_is_dir_unloaded() {
        let r = FsNode::root();
        assert!(r.is_dir);
        assert!(!r.loaded);
        assert!(r.children.is_empty());
        assert_eq!(r.path, "/");
    }

    #[test]
    fn new_child_appends_to_parent_path_without_double_slash() {
        let a = FsNode::new_child("/", "sd");
        assert_eq!(a.path, "/sd");
        let b = FsNode::new_child("/sd", "foo.txt");
        assert_eq!(b.path, "/sd/foo.txt");
        let c = FsNode::new_child("/sd/", "bar.txt");
        assert_eq!(c.path, "/sd/bar.txt"); // no double-slash
    }

    #[test]
    fn is_dir_heuristic_basics() {
        assert!(FsNode::is_dir_heuristic("sd"));
        assert!(FsNode::is_dir_heuristic("photos"));
        assert!(!FsNode::is_dir_heuristic("foo.txt"));
        assert!(!FsNode::is_dir_heuristic("README.md"));
    }

    #[test]
    fn populate_marks_loaded_and_sets_children() {
        let mut r = FsNode::root();
        r.populate(&["sd".to_owned(), "boot.txt".to_owned()]);
        assert!(r.loaded);
        assert_eq!(r.children.len(), 2);
        assert!(r.children[0].is_dir);
        assert!(!r.children[1].is_dir);
        assert_eq!(r.children[0].path, "/sd");
        assert_eq!(r.children[1].path, "/boot.txt");
    }

    #[test]
    fn find_mut_walks_descendants() {
        let mut r = FsNode::root();
        r.populate(&["sd".to_owned()]);
        let sd = r.find_mut("/sd").expect("find /sd");
        sd.populate(&["foo.txt".to_owned()]);
        assert!(r.find_mut("/sd/foo.txt").is_some());
        assert!(r.find_mut("/nonexistent").is_none());
    }

    #[test]
    fn populate_preserves_existing_subtrees_when_repopulating() {
        let mut r = FsNode::root();
        r.populate(&["sd".to_owned(), "boot.txt".to_owned()]);
        // Expand /sd and populate its subtree.
        let sd = r.find_mut("/sd").expect("find /sd");
        sd.populate(&["photos".to_owned()]);
        // Now re-populate root (simulating delete + refresh): boot.txt gone.
        r.populate(&["sd".to_owned()]);
        // /sd should still be there AND still have photos as a child.
        let sd = r.find_mut("/sd").expect("find /sd after re-populate");
        assert!(sd.loaded, "subtree state preserved");
        assert_eq!(sd.children.len(), 1);
        assert_eq!(sd.children[0].name, "photos");
        // boot.txt is gone.
        assert!(r.find_mut("/boot.txt").is_none());
    }
}
