/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_AST_H
#define OZNI_AST_H

#include "ctx.h"

#define OZNI_NODE_KINDS(X)                                                                              \
  X(Error) X(File) X(Local) X(Dimension) X(Unit) X(Base) X(Axis) X(Enum) X(Value) X(Finer) X(Trait)       \
  X(Ingredient) X(Kind) X(Vessel) X(Dirt) X(Resource) X(Tool) X(Alias) X(Rule) X(RuleCase) X(AnyUse) X(Property)      \
  X(Source) X(Action) X(Extend) X(Where) X(Param) X(Default) X(Output) X(Recipe) X(Requires) X(Menu)    \
  X(Kitchen) X(Type) X(TypeField) X(Check) X(Macro) X(MacroParam) X(Package) X(Module) X(Bundle)        \
  X(BundleFrom) X(Phrases) X(Import) X(ImportName) X(ImportAlias) X(Uses) X(For) X(Takes) X(Variant)    \
  X(Steps) X(Field) X(On) X(Section) X(Marker) X(TypeRef) X(CallArgs) X(TypeArgs) X(Alt) X(StateArg)    \
  X(BareArg) X(UnionType) X(OptionalType) X(ListType) X(RecordType) X(TupleType) X(RangedType) X(Block) \
  X(Let) X(LetTuple) X(BindName) X(Parallel) X(Branch) X(If) X(ForIn) X(Repeat) X(Serve) X(After)       \
  X(Plate) X(Ingredients) X(Item) X(Annotation) X(Binary) X(Unary) X(Not) X(Range) X(RangeFrom)        \
  X(RangeTo) X(Call) X(NamedArg) X(Member) X(Quantity) X(QtyPart) X(Number) X(NumberRange) X(String)    \
  X(MlString) X(Bool) X(Splice) X(Vague) X(List) X(Record) X(Entry) X(Construct) X(Name) X(Quote)       \
  X(Make) X(Buy) X(Choose) X(Assemble) X(PerServing)

typedef enum ozni_node_kind : uint8_t {
#define OZNI_NODE_ENUM(k) OZNI_N_##k,
  OZNI_NODE_KINDS(OZNI_NODE_ENUM)
#undef OZNI_NODE_ENUM
  OZNI_N_COUNT
} ozni_node_kind;

typedef struct ozni_node {
  ozni_node_kind kind;
  ozni_span span;
  ozni_index text;
  ozni_index text2;
  ozni_index text3;
  uint32_t name_start;
  uint32_t name_end;
  ozni_index first;
  ozni_index last;
  ozni_index next;
} ozni_node;

typedef enum ozni_rule : uint8_t {
#define OZNI_RULE(r) OZNI_R_##r,
#include "rules.def"
#undef OZNI_RULE
  OZNI_R_COUNT
} ozni_rule;

typedef struct ozni_ast {
  ozni_table *nodes;
  ozni_index root;
  uint8_t rules[(OZNI_R_COUNT + 7) / 8];
} ozni_ast;

const char *ozni_node_kind_name(ozni_node_kind k);
const char *ozni_rule_name(ozni_rule r);
const ozni_node *ozni_node_get(const ozni_ast *ast, ozni_index i);

[[nodiscard]] bool ozni_parse(ozni_ctx *c, ozni_index file, ozni_ast *ast);

#endif
