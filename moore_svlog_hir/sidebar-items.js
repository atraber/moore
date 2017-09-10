initSidebarItems({"enum":[["Node",""],["PortSelect",""]],"fn":[["is_type_empty","Check if a type is empty, i.e. it is an implicit type with no sign or packed dimensions specified."],["lower",""]],"struct":[["Expr",""],["GenerateBlock",""],["GenerateFor",""],["GenerateIf",""],["HierarchyBody","A hierarchy body represents the contents of a module, interface, or package. Generate regions and nested modules introduce additional bodies. The point of hierarchy bodies is to take a level of the design hierarchy and group all declarations by type, rather than having them in a single array in declaration order."],["Interface","An interface."],["Module","A module."],["Name","A name is a lightweight 32 bit tag that refers to a string in a name table. During parsing, encountered strings are inserted into the name table and only the corresponding tag is kept in the token. Names which have their most significant bit set represent case sensitive names, such as for extended identifiers."],["NodeId","A positive, small ID assigned to each node in the AST. Used as a lightweight way to refer to individual nodes, e.g. during symbol table construction and name resolution."],["NodeIndex","An search index of all nodes in a HIR tree."],["Package","A package."],["Port",""],["PortDecl",""],["PortSlice","A port slice refers to a port declaration within the module. It consists of the name of the declaration and a list of optional member and index accesses that select individual parts of the declaration."],["Root","The root of the HIR tree. This represents one elaborated design."],["Span","A span of locations within a source file, expressed as a half-open interval of bytes `[begin,end)`."]]});