// Copyright (c) 2017 Fabian Schuiki
use std;

/// A positive, small ID assigned to nodes in the AST and derived data
/// structures. Used as a lightweight way to refer to individual nodes, e.g.
/// during symbol table construction and name resolution.
#[derive(Clone, Copy, PartialEq, PartialOrd, Eq, Ord, Hash, Debug, RustcEncodable, RustcDecodable)]
pub struct NodeId(u32);

impl NodeId {
	pub fn new(x: usize) -> NodeId {
		use std::u32;
		assert!(x < (u32::MAX as usize));
		NodeId(x as u32)
	}

	pub fn from_u32(x: u32) -> NodeId {
		NodeId(x)
	}

	pub fn as_usize(&self) -> usize {
		self.0 as usize
	}

	pub fn as_u32(&self) -> u32 {
		self.0
	}
}

impl std::fmt::Display for NodeId {
	fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
		write!(f, "{}", self.0)
	}
}
