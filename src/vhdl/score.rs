// Copyright (c) 2017 Fabian Schuiki

//! This module implements the scoreboard that drives the compilation of VHDL.

#![allow(dead_code)]
#![allow(unused_imports)]

use std;
use std::fmt::Debug;
use std::collections::HashMap;
use std::cell::{RefCell, Cell};
use moore_common;
use moore_common::Session;
use moore_common::name::*;
use moore_common::source::*;
use moore_common::errors::*;
use moore_common::NodeId;
use moore_common::score::{NodeStorage, NodeMaker, Result};
use syntax::ast;
use hir;
use typed_arena::Arena;
use llhd;


/// The VHDL scoreboard that keeps track of compilation results.
pub struct Scoreboard<'ast, 'ctx> {
	/// The compiler session which carries the options and is used to emit
	/// diagnostics.
	sess: &'ctx Session,
	/// A reference to the arenas where the scoreboard allocates nodes.
	arenas: &'ctx Arenas,
	/// An optional reference to a parent scoreboard.
	parent: Cell<Option<&'ctx moore_common::score::Scoreboard>>,
	/// A table of library nodes. This is a filtered version of what the global
	/// scoreboard has, with only the VHDL nodes remaining.
	libs: HashMap<LibRef, Vec<&'ast ast::DesignUnit>>,
	/// A table of AST nodes.
	ast_table: RefCell<AstTable<'ast>>,
	/// A table of HIR nodes.
	hir_table: RefCell<HirTable<'ctx>>,
	/// A table of definitions in each scope.
	def_table: RefCell<HashMap<ScopeRef, &'ctx Scope>>,
	/// A table of architecture per entity and library.
	arch_table: RefCell<HashMap<LibRef, &'ctx ArchTable>>,
	/// The LLHD module into which code is emitted.
	pub llmod: RefCell<llhd::Module>,
	/// A table of LLHD declarations (i.e. prototypes). These are useful for
	/// example when an entity needs so be instantiated, for which only the
	/// signature of the entity is required, but not its full definition with
	/// its interior.
	lldecl_table: RefCell<HashMap<NodeId, llhd::ValueRef>>,
	/// A table of LLHD definitions.
	lldef_table: RefCell<HashMap<NodeId, llhd::ValueRef>>,
}


impl<'ast, 'ctx> Scoreboard<'ast, 'ctx> {
	/// Creates a new empty VHDL scoreboard.
	///
	/// # Example
	/// ```
	/// use moore_vhdl::score::{Arenas, Scoreboard};
	///
	/// let arenas = Arenas::new();
	/// let sb = Scoreboard::new(&arenas);
	/// ```
	pub fn new(sess: &'ctx Session, arenas: &'ctx Arenas) -> Scoreboard<'ast, 'ctx> {
		Scoreboard {
			sess: sess,
			arenas: arenas,
			parent: Cell::new(None),
			libs: HashMap::new(),
			ast_table: RefCell::new(AstTable::new()),
			hir_table: RefCell::new(HirTable::new()),
			def_table: RefCell::new(HashMap::new()),
			arch_table: RefCell::new(HashMap::new()),
			llmod: RefCell::new(llhd::Module::new()),
			lldecl_table: RefCell::new(HashMap::new()),
			lldef_table: RefCell::new(HashMap::new()),
		}
	}

	/// Change the parent scoreboard.
	pub fn set_parent(&self, parent: &'ctx moore_common::score::Scoreboard) {
		self.parent.set(Some(parent));
	}

	/// Add a library of AST nodes. This function is called by the global
	/// scoreboard to add VHDL-specific AST nodes.
	pub fn add_library(&mut self, id: LibRef, lib: Vec<&'ast ast::DesignUnit>) {
		if self.libs.insert(id, lib).is_some() {
			panic!("library did already exist");
		}
	}

	/// Obtain the AST node corresponding to a node reference. The AST node must
	/// have previously been added to the `ast_table`, otherwise this function
	/// panics.
	pub fn ast<I>(&self, id: I) -> <AstTable<'ast> as NodeStorage<I>>::Node where
		I: 'ast + Copy + Debug,
		AstTable<'ast>: NodeStorage<I>,
		<AstTable<'ast> as NodeStorage<I>>::Node: Copy + Debug {
		match self.ast_table.borrow().get(&id) {
			Some(node) => node,
			None => panic!("AST for {:?} should exist", id),
		}
	}

	pub fn hir<I>(&self, id: I) -> Result<<HirTable<'ctx> as NodeStorage<I>>::Node> where
		I: 'ctx + Copy + Debug,
		HirTable<'ctx>: NodeStorage<I>,
		Scoreboard<'ast, 'ctx>: NodeMaker<I, <HirTable<'ctx> as NodeStorage<I>>::Node>,
		<HirTable<'ctx> as NodeStorage<I>>::Node: Copy + Debug {

		if let Some(node) = self.hir_table.borrow().get(&id) {
			return Ok(node);
		}
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] make hir for {:?}", id); }
		let node = self.make(id)?;
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] hir for {:?} is {:?}", id, node); }
		self.hir_table.borrow_mut().set(id, node);
		Ok(node)
	}

	pub fn defs(&self, id: ScopeRef) -> Result<&'ctx Scope> {
		if let Some(&node) = self.def_table.borrow().get(&id) {
			return Ok(node);
		}
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] make scope for {:?}", id); }
		let node = self.make(id)?;
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] scope for {:?} is {:?}", id, node); }
		if self.def_table.borrow_mut().insert(id, node).is_some() {
			panic!("node should not exist");
		}
		Ok(node)
	}

	pub fn archs(&self, id: LibRef) -> Result<&'ctx ArchTable> {
		if let Some(&node) = self.arch_table.borrow().get(&id) {
			return Ok(node);
		}
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] make arch for {:?}", id); }
		let node = self.make(id)?;
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] arch for {:?} is {:?}", id, node); }
		if self.arch_table.borrow_mut().insert(id, node).is_some() {
			panic!("node should not exist");
		}
		Ok(node)
	}

	pub fn lldecl<I>(&self, id: I) -> Result<llhd::ValueRef>
	where
		I: 'ctx + Copy + Debug + Into<NodeId>,
		Scoreboard<'ast, 'ctx>: NodeMaker<I, DeclValueRef>
	{
		if let Some(node) = self.lldecl_table.borrow().get(&id.into()).cloned() {
			return Ok(node);
		}
		if let Some(node) = self.lldef_table.borrow().get(&id.into()).cloned() {
			return Ok(node);
		}
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] make lldecl for {:?}", id); }
		let node = self.make(id)?.0;
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] lldecl for {:?} is {:?}", id, node); }
		if self.lldecl_table.borrow_mut().insert(id.into(), node.clone()).is_some() {
			panic!("node should not exist");
		}
		Ok(node)
	}

	pub fn lldef<I>(&self, id: I) -> Result<llhd::ValueRef>
	where
		I: 'ctx + Copy + Debug + Into<NodeId>,
		Scoreboard<'ast, 'ctx>: NodeMaker<I, DefValueRef>
	{
		if let Some(node) = self.lldef_table.borrow().get(&id.into()).cloned() {
			return Ok(node);
		}
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] make lldef for {:?}", id); }
		let node = self.make(id)?.0;
		if self.sess.opts.trace_scoreboard { println!("[SB][VHDL] lldef for {:?} is {:?}", id, node); }
		if self.lldef_table.borrow_mut().insert(id.into(), node.clone()).is_some() {
			panic!("node should not exist");
		}
		Ok(node)
	}
}


// Wrapper types around ValueRef such that we can distinguish in the
// scoreboard's implementations of the NodeMaker trait whether we're building a
// declaration or definition.
#[derive(Debug, Clone)]
pub struct DeclValueRef(pub llhd::ValueRef);
#[derive(Debug, Clone)]
pub struct DefValueRef(pub llhd::ValueRef);


// Library lowering to HIR.
impl<'ast, 'ctx> NodeMaker<LibRef, &'ctx hir::Lib> for Scoreboard<'ast, 'ctx> {
	fn make(&self, id: LibRef) -> Result<&'ctx hir::Lib> {
		let mut lib = hir::Lib::new();
		for du in &self.libs[&id] {
			match du.data {
				ast::DesignUnitData::EntityDecl(ref decl) => {
					let subid = EntityRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.entities.push(subid);
				}
				ast::DesignUnitData::CfgDecl(ref decl) => {
					let subid = CfgRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.cfgs.push(subid);
				}
				ast::DesignUnitData::PkgDecl(ref decl) => {
					let subid = PkgDeclRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.pkg_decls.push(subid);
				}
				ast::DesignUnitData::PkgInst(ref decl) => {
					let subid = PkgInstRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.pkg_insts.push(subid);
				}
				ast::DesignUnitData::CtxDecl(ref decl) => {
					let subid = CtxRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.ctxs.push(subid);
				}
				ast::DesignUnitData::ArchBody(ref decl) => {
					let subid = ArchRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.archs.push(subid);
				}
				ast::DesignUnitData::PkgBody(ref decl) => {
					let subid = PkgBodyRef(NodeId::alloc());
					self.ast_table.borrow_mut().set(subid, (id, du.ctx.as_slice(), decl));
					lib.pkg_bodies.push(subid);
				}
			}
		}
		Ok(self.arenas.hir.lib.alloc(lib))
	}
}


// Definitions per library.
impl<'ast, 'ctx> NodeMaker<ScopeRef, &'ctx Scope> for Scoreboard<'ast, 'ctx> {
	fn make(&self, id: ScopeRef) -> Result<&'ctx Scope> {
		match id {
			ScopeRef::Lib(id) => {
				// Approach:
				// 1) Get the HIR of the library
				// 2) Gather definitions from the HIR.
				// 3) Create scope and return.
				let lib = self.hir(id)?;

				// Assemble an uber-iterator that will iterate over each
				// definition in the library. We do this by obtaining an
				// iterator for every design unit type in the library, then
				// mapping each ID to the corresponding name and definition.
				// The name is determined by looking up the AST node of the
				// design unit to be defined.
				let iter = lib.entities.iter().map(|&n| (self.ast(n).2.name, Def::Entity(n)));
				let iter = iter.chain(lib.cfgs.iter().map(|&n| (self.ast(n).2.name, Def::Cfg(n))));
				let iter = iter.chain(lib.pkg_decls.iter().map(|&n| (self.ast(n).2.name, Def::Pkg(n))));
				let iter = iter.chain(lib.pkg_insts.iter().map(|&n| (self.ast(n).2.name, Def::PkgInst(n))));
				let iter = iter.chain(lib.ctxs.iter().map(|&n| (self.ast(n).2.name, Def::Ctx(n))));

				// For every element the iterator produces, add it to the map of
				// definitions.
				let mut defs = HashMap::new();
				for (name, def) in iter {
					defs.entry(name.value).or_insert_with(|| Vec::new()).push(Spanned::new(def, name.span));
				}

				// Warn the user about duplicate definitions.
				let mut had_dups = false;
				for (name, defs) in &defs {
					if defs.len() <= 1 {
						continue;
					}
					let mut d = DiagBuilder2::error(format!("`{}` declared multiple times", name));
					for def in defs {
						d = d.span(def.span);
					}
					self.sess.emit(d);
					had_dups = true;
				}
				if had_dups {
					return Err(());
				}

				// Return the scope of definitions.
				Ok(self.arenas.scope.alloc(defs))
			}
		}
	}
}


// Group the architectures declared in a library by entity.
impl<'ast, 'ctx> NodeMaker<LibRef, &'ctx ArchTable> for Scoreboard<'ast, 'ctx> {
	fn make(&self, id: LibRef) -> Result<&'ctx ArchTable> {
		let lib = self.hir(id)?;
		let defs = self.defs(ScopeRef::Lib(id.into()))?;
		let mut res = ArchTable::new();
		res.by_entity = lib.entities.iter().map(|&id| (id, EntityArchTable::new())).collect();
		let mut had_fails = false;
		for &arch_ref in &lib.archs {
			let arch = self.ast(arch_ref).2;

			// Extract a simple entity name for now. Maybe we need to support
			// the full-blown compound names at some point?
			let entity_name = match if arch.target.parts.is_empty() {
				match arch.target.primary.kind {
					ast::PrimaryNameKind::Ident(n) => Some(n),
					_ => None,
				}
			} else {
				None
			}{
				Some(n) => n,
				None => {
					self.sess.emit(
						DiagBuilder2::error(format!("`{}` is not a valid entity name", arch.target.span.extract()))
						.span(arch.target.span)
					);
					had_fails = true;
					continue;
				}
			};

			// Try to find the entity with the name.
			let entity = match defs.get(&entity_name) {
				Some(e) => {
					let last = e.last().unwrap();
					match last.value {
						Def::Entity(e) => e,
						_ => {
							self.sess.emit(
								DiagBuilder2::error(format!("`{}` is not an entity", entity_name))
								.span(arch.target.span)
								.add_note(format!("`{}` defined here:", entity_name))
								.span(last.span)
							);
							had_fails = true;
							continue;
						}
					}
				}
				None => {
					self.sess.emit(
						DiagBuilder2::error(format!("Unknown entity `{}`", entity_name))
						.span(arch.target.span)
					);
					had_fails = true;
					continue;
				}
			};

			// Insert the results into the table of architectures for the found
			// entity.
			let entry = res.by_entity.get_mut(&entity).unwrap();
			entry.ordered.push(arch_ref);
			entry.by_name.insert(arch.name.value, arch_ref);
			res.by_arch.insert(arch_ref, entity);
		}
		if had_fails {
			Err(())
		} else {
			Ok(self.arenas.archs.alloc(res))
		}
	}
}


// Generate the prototype for an architecture.
impl<'ast, 'ctx> NodeMaker<ArchRef, DeclValueRef> for Scoreboard<'ast, 'ctx> {
	fn make(&self, id: ArchRef) -> Result<DeclValueRef> {
		unimplemented!("llhd decl for {:?}", id);
	}
}


// Generate the definition for an architecture.
impl<'ast, 'ctx> NodeMaker<ArchRef, DefValueRef> for Scoreboard<'ast, 'ctx> {
	fn make(&self, id: ArchRef) -> Result<DefValueRef> {
		// Approach:
		// 1) Get ref to arch's entity
		// 2) Get lltype of entity
		// 3) Add to module and return
		let arch = self.ast(id);
		let entity_id = *self.archs(arch.0)?.by_arch.get(&id).unwrap();
		println!("arch {:?} has entity {:?}", id, entity_id);

		// TODO: Actually get the lltype of the entity for this.
		let ty = llhd::entity_ty(vec![], vec![]);

		// Create a new entity into which we will generate all the code.
		let name = format!("{}_{}", self.ast(entity_id).2.name.value, arch.2.name.value);
		let entity = llhd::Entity::new(name, ty);

		// Add the entity to the module and return a reference to it.
		Ok(DefValueRef(self.llmod.borrow_mut().add_entity(entity).into()))
	}
}


/// A collection of arenas that the scoreboard uses to allocate its nodes.
pub struct Arenas {
	pub hir: hir::Arenas,
	pub scope: Arena<Scope>,
	pub archs: Arena<ArchTable>,
}


impl Arenas {
	/// Create a new set of arenas.
	pub fn new() -> Arenas {
		Arenas {
			hir: hir::Arenas::new(),
			scope: Arena::new(),
			archs: Arena::new(),
		}
	}
}


/// A table of the architectures in a library, and how they relate to the
/// entities.
#[derive(Debug)]
pub struct ArchTable {
	pub by_arch: HashMap<ArchRef, EntityRef>,
	pub by_entity: HashMap<EntityRef, EntityArchTable>,
}

/// A table of the architectures associated with an entity.
#[derive(Debug)]
pub struct EntityArchTable {
	pub ordered: Vec<ArchRef>,
	pub by_name: HashMap<Name, ArchRef>,
}

impl ArchTable {
	pub fn new() -> ArchTable {
		ArchTable {
			by_arch: HashMap::new(),
			by_entity: HashMap::new(),
		}
	}
}

impl EntityArchTable {
	pub fn new() -> EntityArchTable {
		EntityArchTable {
			ordered: Vec::new(),
			by_name: HashMap::new(),
		}
	}
}


/// A set of names and definitions.
pub type Scope = HashMap<Name, Vec<Spanned<Def>>>;
pub type Archs = HashMap<EntityRef, (Vec<ArchRef>, HashMap<Name, ArchRef>)>;


// Declare the node references.
node_ref!(ArchRef);
node_ref!(CfgRef);
node_ref!(CtxRef);
node_ref!(DesignUnitRef);
node_ref!(EntityRef);
node_ref!(LibRef);
node_ref!(PkgBodyRef);
node_ref!(PkgDeclRef);
node_ref!(PkgInstRef);

// Declare the node reference groups.
node_ref_group!(Def:
	Arch(ArchRef),
	Cfg(CfgRef),
	Ctx(CtxRef),
	Entity(EntityRef),
	Lib(LibRef),
	Pkg(PkgDeclRef),
	PkgInst(PkgInstRef),
);
node_ref_group!(ScopeRef:
	Lib(LibRef),
);


// Declare the node tables.
node_storage!(AstTable<'ast>,
	design_units: DesignUnitRef => &'ast ast::DesignUnit,
	// The design units are tuples that also carry the list of context items
	// that were defined before them.
	entity_decls: EntityRef  => (LibRef, &'ast [ast::CtxItem], &'ast ast::EntityDecl),
	cfg_decls:    CfgRef     => (LibRef, &'ast [ast::CtxItem], &'ast ast::CfgDecl),
	pkg_decls:    PkgDeclRef => (LibRef, &'ast [ast::CtxItem], &'ast ast::PkgDecl),
	pkg_insts:    PkgInstRef => (LibRef, &'ast [ast::CtxItem], &'ast ast::PkgInst),
	ctx_decls:    CtxRef     => (LibRef, &'ast [ast::CtxItem], &'ast ast::CtxDecl),
	arch_bodies:  ArchRef    => (LibRef, &'ast [ast::CtxItem], &'ast ast::ArchBody),
	pkg_bodies:   PkgBodyRef => (LibRef, &'ast [ast::CtxItem], &'ast ast::PkgBody),
);

node_storage!(HirTable<'ctx>,
	libs: LibRef => &'ctx hir::Lib,
);
