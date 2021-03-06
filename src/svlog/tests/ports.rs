// Copyright (c) 2017 Fabian Schuiki
#![allow(unused_variables)]

mod common;
use common::*;
use std::collections::HashMap;


#[test]
fn empty() {
	compile_to_hir(parse("module foo; endmodule"));
}

#[test]
fn syntax_ansi_1() {
	parse("
		module foo (
			// Interface Ports
			intf a,
			intf.modp b,
			interface c,
			interface.modp d,

			// Net Ports
			input wire e,
			output trireg logic [7:0] f = 12,
			inout supply0 [15:0] g,
			inout logic [7:0] h,

			// Variable Ports
			input logic j,
			output int k [3:0] = 1234,
			ref var l,
			ref logic [3:0] m [7:0],

			// Explicit Ports
			.n(),
			.o(bar),
			input .p(bar[0]),
			output .q(bar[3:1]),
			inout .r(bar * 2),
			ref .r()
		);
		endmodule
	");
}

/// `inout` ports must be of net type.
#[test]
#[should_panic]
fn var_inout_port() {
	let hir = compile_to_hir(parse("module foo (inout var x); endmodule"));
	let m = unwrap_single_module(&hir);
	// TODO: Verify that the compiler fails for the right reason.
}

/// `ref` ports must be of var type.
#[test]
#[should_panic]
fn net_ref_port() {
	let hir = compile_to_hir(parse("module foo (ref triand i = 42); endmodule"));
	let m = unwrap_single_module(&hir);
	// TODO: Verify that the compiler fails for the right reason.
}

/// This test should fail since the module has ports declared on it that are not
/// defined in the module body.
#[test]
#[should_panic]
fn impl_no_decl() {
	let hir = compile_to_hir(parse("
		module foo (a,b,c);
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Make sure the test fails for the right reason. Maybe inspect the
	// diagnostics emitted during the lowering.
}

#[test]
fn impl_ex1() {
	let hir = compile_to_hir(parse("
		module test (a,b,c,d,e,f,g,h);
			input [7:0] a;
			input [7:0] b;
			input signed [7:0] c;
			input signed [7:0] d;
			output [7:0] e;
			output [7:0] f;
			output signed [7:0] g;
			output signed [7:0] h;

			wire signed [7:0] b;
			wire [7:0] c;
			logic signed [7:0] f;
			logic [7:0] g;
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that net `a` is unsigned.
	// TODO: Verify that net port `b` inherits signed attribute from net decl.
	// TODO: Verify that net `c` inherits signed attribute from port.
	// TODO: Verify that net `d` is signed.
	// TODO: Verify that net `e` is unsigned.
	// TODO: Verify that port f inherits signed attribute from logic decl.
	// TODO: Verify that logic g inherits signed attribute from port.
	// TODO: Verify that net `h` is signed.
}

#[test]
fn expl_ex1() {
	let hir = compile_to_hir(parse("
		module test (
			input [7:0] a,
			input signed [7:0] b, c, d,
			output [7:0] e,
			output var signed [7:0] f, g,
			output signed [7:0] h
		);
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that the ports are of the correct size, direction, and type.
	// TODO: Verify that inside the module the appropriate variables and nets
	//       have been declared by the ports.
}

#[test]
#[should_panic]
fn mixed_ansi_and_nonansi() {
	let hir = compile_to_hir(parse("
		module test (input clk);
			output [7:0] data;
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that this fails for the right reason. Maybe look at the
	// diagnostics generated.
}

#[test]
fn nonansi_complex_ports() {
	let hir = compile_to_hir(parse("
		module complex_ports ({c,d}, .e(f));
			input c;
			output d;
			output [7:0] f;
		endmodule
	"));
	let m = unwrap_single_module(&hir);
	// TODO: Verify that port[0] is unnamed and has two slices.
	// TODO: Verify that port[1] is named and has one slice.
}

#[test]
fn nonansi_split_ports() {
	let hir = compile_to_hir(parse("
		module split_ports (a[7:4], a[3:0]);
		endmodule
	"));
	let m = unwrap_single_module(&hir);
	// TODO: Verify that port[0] and port[1] is unnamed, has one slice, and has one select.
}

#[test]
fn nonansi_same_port() {
	let hir = compile_to_hir(parse("
		module same_port (.a(i), .b(i));
			input logic i;
		endmodule
	"));
	let m = unwrap_single_module(&hir);
}

#[test]
#[should_panic]
fn nonansi_port_overdeclare() {
	let hir = compile_to_hir(parse("
		module overdeclare (a,b);
			input logic a;
			output wire b;
			logic a;
			wire b;
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that the lowering complains about a and b being redeclared.
}

#[test]
fn nonansi_partselect_port() {
	let hir = compile_to_hir(parse("
		module foo (.a(x[3].k.p[3:0]), y[8].n[3:0].p);
			input x[8];
			input y[10];
		endmodule
	"));
	let m = unwrap_single_module(&hir);
}

#[test]
#[should_panic]
fn nonansi_complete_redeclared() {
	let hir = compile_to_hir(parse("
		module foo (a,b);
			input logic a;
			input wire b;
			logic a;
			wire b;
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that the lowering complains about a and b being redeclared.
}

#[test]
#[should_panic]
fn nonansi_complete_undeclared() {
	let hir = compile_to_hir(parse("
		module foo (a);
		endmodule
	"));
	let m = unwrap_single_module(&hir);

	// TODO: Verify that the lowering complains about a being undeclared.
}

#[test]
#[should_panic]
fn nonansi_var_vs_net() {
	let hir = compile_to_hir(parse("
		module foo (a,b);
			input a;
			input var b;
			logic a;
			wire b;
		endmodule
	"));
	let m = unwrap_single_module(&hir);
}

#[test]
#[should_panic]
fn nonansi_var_and_net() {
	let hir = compile_to_hir(parse("
		module foo (a);
			input a;
			logic a;
			wire a;
		endmodule
	"));
	let m = unwrap_single_module(&hir);
}

#[test]
#[should_panic]
fn nonansi_contradicting_sign() {
	let hir = compile_to_hir(parse("
		module foo (a,b);
			input unsigned a;
			input signed b;
			logic signed a;
			logic unsigned b;
		endmodule
	"));
	let m = unwrap_single_module(&hir);
}


#[test]
fn inference_rules() {
	use common::moore_svlog::hir::*;
	use common::moore_svlog::ast;

	let hir = compile_to_hir(parse("
		module mh0  (wire x);                           endmodule
		module mh1  (integer x);                        endmodule
		module mh2  (inout integer x);                  endmodule
		module mh3  ([5:0] x);                          endmodule
		module mh5  (input x);                          endmodule
		module mh6  (input var x);                      endmodule
		module mh7  (input var integer x);              endmodule
		module mh8  (output x);                         endmodule
		module mh9  (output var x);                     endmodule
		module mh10 (output signed [5:0] x);            endmodule
		module mh11 (output integer x);                 endmodule
		module mh12 (ref [5:0] x);                      endmodule
		module mh13 (ref x [5:0]);                      endmodule
		module mh14 (wire x, y[7:0]);                   endmodule
		module mh15 (integer x, signed [5:0] y);        endmodule
		module mh16 ([5:0] x, wire y);                  endmodule
		module mh17 (input var integer x, wire y);      endmodule
		module mh18 (output var x, input y);            endmodule
		module mh19 (output signed [5:0] x, integer y); endmodule
		module mh20 (ref [5:0] x, y);                   endmodule
		module mh21 (ref x [5:0], y);                   endmodule
	"));

	let nt = common::moore_common::name::get_name_table();
	let nam = |x| nt.intern(x, true);
	let mods: HashMap<Name, Module> = hir.mods.into_iter().map(|m| (m.1.name, m.1)).collect();
	assert_eq!(mods.len(), 21);

	{
		let m = &mods[&nam("mh0")];
		assert_eq!(m.ports.len(), 1);
		{
			let p = &m.ports[0];
			assert_eq!(p.name, Some(nam("x")));
			assert_eq!(p.slices.len(), 1);
			assert_eq!(p.slices[0].name, nam("x"));
			assert_eq!(p.slices[0].selects.len(), 0);
			assert_eq!(p.slices[0].dir, ast::PortDir::Inout);
			assert_eq!(p.slices[0].kind, ast::PortKind::Net(ast::NetType::Wire));
			assert_eq!(p.slices[0].ty.as_ref().unwrap().data, ast::LogicType);
			assert_eq!(p.slices[0].ty.as_ref().unwrap().sign, ast::TypeSign::Unsigned);
			assert_eq!(p.slices[0].ty.as_ref().unwrap().dims.len(), 0);
		}
	}
}


#[test]
#[should_panic]
fn inference_rules_fail() {
	let hir = compile_to_hir(parse("
		module mh4 (var x);
	"));
	let m = unwrap_single_module(&hir);
}
