#include <cassert>
#include <strstream>

#include "ast.h"
#include "dom-to-string.h"

namespace ast {

using dom::TypeInfo;
using dom::CppClassType;
using dom::TypeWithFills;
using dom::CppField;
using dom::Kind;

own<dom::Dom> cpp_dom;
own<TypeWithFills> Ast::dom_type_;

own<TypeWithFills> Var::dom_type_;

own<TypeWithFills> ConstInt64::dom_type_;
own<TypeWithFills> ConstDouble::dom_type_;
own<TypeWithFills> ConstVoid::dom_type_;
own<TypeWithFills> ConstBool::dom_type_;
own<TypeWithFills> MkLambda::dom_type_;
own<TypeWithFills> Call::dom_type_;
own<TypeWithFills> GetAtIndex::dom_type_;
own<TypeWithFills> SetAtIndex::dom_type_;
own<TypeWithFills> MakeDelegate::dom_type_;
own<TypeWithFills> MakeFnPtr::dom_type_;
own<TypeWithFills> Get::dom_type_;
own<TypeWithFills> Set::dom_type_;
own<TypeWithFills> GetField::dom_type_;
own<TypeWithFills> SetField::dom_type_;
own<TypeWithFills> MkInstance::dom_type_;

own<TypeWithFills> ToIntOp::dom_type_;
own<TypeWithFills> ToFloatOp::dom_type_;
own<TypeWithFills> NotOp::dom_type_;
own<TypeWithFills> NegOp::dom_type_;
own<TypeWithFills> RefOp::dom_type_;
own<TypeWithFills> Block::dom_type_;
own<TypeWithFills> CastOp::dom_type_;
own<TypeWithFills> AddOp::dom_type_;
own<TypeWithFills> SubOp::dom_type_;
own<TypeWithFills> MulOp::dom_type_;
own<TypeWithFills> DivOp::dom_type_;
own<TypeWithFills> ModOp::dom_type_;
own<TypeWithFills> AndOp::dom_type_;
own<TypeWithFills> OrOp::dom_type_;
own<TypeWithFills> XorOp::dom_type_;
own<TypeWithFills> ShlOp::dom_type_;
own<TypeWithFills> ShrOp::dom_type_;
own<TypeWithFills> EqOp::dom_type_;
own<TypeWithFills> LtOp::dom_type_;
own<TypeWithFills> If::dom_type_;
own<TypeWithFills> LAnd::dom_type_;
own<TypeWithFills> Else::dom_type_;
own<TypeWithFills> LOr::dom_type_;
own<TypeWithFills> Loop::dom_type_;
own<TypeWithFills> CopyOp::dom_type_;
own<TypeWithFills> MkWeakOp::dom_type_;
own<TypeWithFills> DerefWeakOp::dom_type_;

own<TypeWithFills> TpInt64::dom_type_;
own<TypeWithFills> TpDouble::dom_type_;
own<TypeWithFills> TpFunction::dom_type_;
own<TypeWithFills> TpLambda::dom_type_;
own<TypeWithFills> TpColdLambda::dom_type_;
own<TypeWithFills> TpVoid::dom_type_;
own<TypeWithFills> TpOptional::dom_type_;
own<TypeWithFills> TpClass::dom_type_;
own<TypeWithFills> TpRef::dom_type_;
own<TypeWithFills> TpWeak::dom_type_;
own<TypeWithFills> Field::dom_type_;
own<TypeWithFills> Method::dom_type_;
own<TypeWithFills> Function::dom_type_;

namespace {
	template<typename CLS>
	void make_bin_op(const char* name, const pin<TypeInfo>& op_array_2) {
		CLS::dom_type_ = (new CppClassType<CLS>(cpp_dom, { "m0", name }))
			->field("p", pin<CppField<BinaryOp, own<Action>[2], &BinaryOp::p>>::make(op_array_2));
	}
}

void initialize() {
	if (cpp_dom)
		return;
	cpp_dom = new dom::Dom;
	auto weak_type = cpp_dom->mk_type(Kind::WEAK);
	auto own_type = cpp_dom->mk_type(Kind::OWN);
	auto size_t_type = cpp_dom->mk_type(Kind::UINT, sizeof(size_t));
	auto atom_type = cpp_dom->mk_type(Kind::ATOM);
	pin<dom::TypeInfo> own_vector_type = new dom::VectorType<own<dom::DomItem>>(own_type);
	auto op_array_2 = cpp_dom->mk_type(Kind::FIX_ARRAY, 2, own_type);
	Ast::dom_type_ = (new CppClassType<Ast>(cpp_dom, { "m0", "Ast" }))
		->field("entry", pin<CppField<Ast, own<Function>, &Ast::entry_point>>::make(own_type))
		->field("functions", pin<CppField<Ast, vector<own<Function>>, &Ast::functions>>::make(own_vector_type))
		->field("classes", pin<CppField<Ast, vector<own<TpClass>>, &Ast::classes>>::make(own_vector_type));
	Var::dom_type_ = (new CppClassType<Var>(cpp_dom, { "m0", "Var" }))
		->field("name", pin<CppField<Var, own<dom::Name>, &Var::name>>::make(atom_type))
		->field("initializer", pin<CppField<Var, own<Action>, &Var::initializer>>::make(own_type));
	ConstInt64::dom_type_ = (new CppClassType<ConstInt64>(cpp_dom, {"m0", "Int"}))
		->field("value", pin<CppField<ConstInt64, int64_t, &ConstInt64::value>>::make(
			cpp_dom->mk_type(Kind::INT, sizeof(int64_t))));
	ConstDouble::dom_type_ = (new CppClassType<ConstDouble>(cpp_dom, { "m0", "Double" }))
		->field("value", pin<CppField<ConstDouble, double, &ConstDouble::value>>::make(
			cpp_dom->mk_type(Kind::FLOAT, sizeof(double))));
	ConstVoid::dom_type_ = (new CppClassType<ConstVoid>(cpp_dom, { "m0", "VoidVal" }));
	ConstBool::dom_type_ = (new CppClassType<ConstBool>(cpp_dom, { "m0", "BoolVal" }))
		->field("val", pin<CppField<ConstBool, bool, &ConstBool::value>>::make(cpp_dom->mk_type(Kind::BOOL)));
	Get::dom_type_ = (new CppClassType<Get>(cpp_dom, { "m0", "Get" }))
		->field("var", pin<CppField<DataRef, weak<Var>, &Get::var>>::make(weak_type));
	Set::dom_type_ = (new CppClassType<Set>(cpp_dom, { "m0", "Set" }))
		->field("var", pin<CppField<DataRef, weak<Var>, &Get::var>>::make(weak_type))
		->field("val", pin<CppField<Set, own<Action>, &Set::val>>::make(own_type));
	MkInstance::dom_type_ = (new CppClassType<MkInstance>(cpp_dom, { "m0", "MkInstance" }))
		->field("class", pin<CppField<MkInstance, own<TpClass>, &MkInstance::cls>>::make(weak_type));
	GetField::dom_type_ = (new CppClassType<GetField>(cpp_dom, { "m0", "GetField" }))
		->field("field", pin<CppField<FieldRef, weak<Field>, &FieldRef::field>>::make(weak_type))
		->field("base", pin<CppField<FieldRef, own<Action>, &FieldRef::base>>::make(own_type));
	SetField::dom_type_ = (new CppClassType<SetField>(cpp_dom, { "m0", "SetField" }))
		->field("field", pin<CppField<FieldRef, weak<Field>, &FieldRef::field>>::make(weak_type))
		->field("base", pin<CppField<FieldRef, own<Action>, &FieldRef::base>>::make(own_type))
		->field("val", pin<CppField<SetField, own<Action>, &SetField::val>>::make(own_type));
	ToIntOp::dom_type_ = (new CppClassType<ToIntOp>(cpp_dom, { "m0", "ToInt" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	ToFloatOp::dom_type_ = (new CppClassType<ToFloatOp>(cpp_dom, { "m0", "ToFloat" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	NotOp::dom_type_ = (new CppClassType<NotOp>(cpp_dom, { "m0", "Not" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	NegOp::dom_type_ = (new CppClassType<NegOp>(cpp_dom, { "m0", "Neg" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	RefOp::dom_type_ = (new CppClassType<RefOp>(cpp_dom, { "m0", "Ref" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	Loop::dom_type_ = (new CppClassType<Loop>(cpp_dom, { "m0", "Loop" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	CopyOp::dom_type_ = (new CppClassType<CopyOp>(cpp_dom, { "m0", "Copy" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	MkWeakOp::dom_type_ = (new CppClassType<MkWeakOp>(cpp_dom, { "m0", "MkWeak" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	DerefWeakOp::dom_type_ = (new CppClassType<DerefWeakOp>(cpp_dom, { "m0", "DerefWeak" }))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	Block::dom_type_ = (new CppClassType<Block>(cpp_dom, { "m0", "Block" }))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type))
		->field("locals", pin<CppField<Block, vector<own<Var>>, &Block::names>>::make(own_vector_type));
	MkLambda::dom_type_ = (new CppClassType<MkLambda>(cpp_dom, { "m0", "MkLambda" }))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type))
		->field("params", pin<CppField<Block, vector<own<Var>>, &Block::names>>::make(own_vector_type));
	Function::dom_type_ = (new CppClassType<Function>(cpp_dom, { "m0", "Function" }))
		->field("name", pin<CppField<Function, own<dom::Name>, &Function::name>>::make(atom_type))
		->field("is_external", pin<CppField<Function, bool, &Function::is_platform>>::make(cpp_dom->mk_type(Kind::BOOL)))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type))
		->field("params", pin<CppField<Block, vector<own<Var>>, &Block::names>>::make(own_vector_type));
	Call::dom_type_ = (new CppClassType<Call>(cpp_dom, { "m0", "Call" }))
		->field("callee", pin<CppField<Call, own<Action>, &Call::callee>>::make(own_type))
		->field("params", pin<CppField<Call, vector<own<Action>>, &Call::params>>::make(own_vector_type));
	GetAtIndex::dom_type_ = (new CppClassType<GetAtIndex>(cpp_dom, { "m0", "GetAtIndex" }))
		->field("indexed", pin<CppField<GetAtIndex, own<Action>, &GetAtIndex::indexed>>::make(own_type))
		->field("indexes", pin<CppField<GetAtIndex, vector<own<Action>>, &GetAtIndex::indexes>>::make(own_vector_type));
	SetAtIndex::dom_type_ = (new CppClassType<SetAtIndex>(cpp_dom, { "m0", "SetAtIndex" }))
		->field("indexed", pin<CppField<GetAtIndex, own<Action>, &GetAtIndex::indexed>>::make(own_type))
		->field("indexes", pin<CppField<GetAtIndex, vector<own<Action>>, &GetAtIndex::indexes>>::make(own_vector_type))
		->field("value", pin<CppField<SetAtIndex, own<Action>, &SetAtIndex::value>>::make(own_type));
	MakeDelegate::dom_type_ = (new CppClassType<MakeDelegate>(cpp_dom, { "m0", "MkDelegate" }))
		->field("method", pin<CppField<MakeDelegate, weak<Method>, &MakeDelegate::method>>::make(weak_type))
		->field("base", pin<CppField<MakeDelegate, own<Action>, &MakeDelegate::base>>::make(own_type));
	MakeFnPtr::dom_type_ = (new CppClassType<MakeFnPtr>(cpp_dom, { "m0", "MkFnPtr" }))
		->field("fn", pin<CppField<MakeFnPtr, weak<Function>, &MakeFnPtr::fn>>::make(weak_type));
	make_bin_op<CastOp>("Cast", op_array_2);
	make_bin_op<AddOp>("Add", op_array_2);
	make_bin_op<SubOp>("Sub", op_array_2);
	make_bin_op<MulOp>("Mul", op_array_2);
	make_bin_op<DivOp>("Div", op_array_2);
	make_bin_op<ModOp>("Mod", op_array_2);
	make_bin_op<AndOp>("And", op_array_2);
	make_bin_op<OrOp>("Or", op_array_2);
	make_bin_op<XorOp>("Xor", op_array_2);
	make_bin_op<ShlOp>("Shl", op_array_2);
	make_bin_op<ShrOp>("Shr", op_array_2);
	make_bin_op<EqOp>("Eq", op_array_2);
	make_bin_op<LtOp>("Lt", op_array_2);
	make_bin_op<If>("If", op_array_2);
	make_bin_op<LAnd>("LAnd", op_array_2);
	make_bin_op<Else>("Else", op_array_2);
	make_bin_op<LOr>("LOr", op_array_2);
	TpInt64::dom_type_ = new CppClassType<TpInt64>(cpp_dom, {"m0", "Type", "Int64"});
	TpDouble::dom_type_ = new CppClassType<TpDouble>(cpp_dom, { "m0", "Type", "Double" });
	TpFunction::dom_type_ = (new CppClassType<TpFunction>(cpp_dom, { "m0", "Type", "Function" }))
		->field("params", pin<CppField<TpFunction, vector<own<Type>>, &TpFunction::params>>::make(own_vector_type));
	TpLambda::dom_type_ = (new CppClassType<TpLambda>(cpp_dom, { "m0", "Type", "Lambda" }))
		->field("params", pin<CppField<TpFunction, vector<own<Type>>, &TpFunction::params>>::make(own_vector_type));
	TpColdLambda::dom_type_ = (new CppClassType<TpColdLambda>(cpp_dom, { "m0", "Type", "ColdLambda" }))
		->field("resolved", pin<CppField<TpColdLambda, own<Type>, &TpColdLambda::resolved>>::make(own_type));
	TpVoid::dom_type_ = (new CppClassType<TpVoid>(cpp_dom, { "m0", "Type", "Void" }));
	TpOptional::dom_type_ = (new CppClassType<TpOptional>(cpp_dom, { "m0", "Type", "Optional" }))
		->field("wrapped", pin<CppField<TpOptional, own<Type>, &TpOptional::wrapped>>::make(own_type));
	Field::dom_type_ = (new CppClassType<Field>(cpp_dom, { "m0", "Field" }))
		->field("name", pin<CppField<Field, own<dom::Name>, &Field::name>>::make(atom_type))
		->field("type", pin<CppField<Field, own<Action>, &Field::initializer>>::make(own_type));
	Method::dom_type_ = (new CppClassType<Method>(cpp_dom, { "m0", "Method" }))
		->field("name", pin<CppField<Function, own<dom::Name>, &Function::name>>::make(atom_type))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type))
		->field("result_type", pin<CppField<Function, own<Action>, &Function::type_expression>>::make(own_type))
		->field("params", pin<CppField<Block, vector<own<Var>>, &Block::names>>::make(own_vector_type));
	TpClass::dom_type_ = (new CppClassType<TpClass>(cpp_dom, { "m0", "Type", "Class" }))
		->field("name", pin<CppField<TpClass, own<dom::Name>, &TpClass::name>>::make(atom_type))
		->field("base", pin<CppField<TpClass, weak<TpClass>, &TpClass::base_class>>::make(weak_type))
		->field("fields", pin<CppField<TpClass, vector<own<Field>>, &TpClass::fields>>::make(own_vector_type))
		->field("methods", pin<CppField<TpClass, vector<own<Method>>, &TpClass::new_methods>>::make(own_vector_type));
	TpRef::dom_type_ = (new CppClassType<TpRef>(cpp_dom, { "m0", "Type", "Ref" }))
		->field("target", pin<CppField<TpRef, own<TpClass>, &TpRef::target>>::make(own_type));
	TpWeak::dom_type_ = (new CppClassType<TpWeak>(cpp_dom, { "m0", "Type", "Weak" }))
		->field("target", pin<CppField<TpRef, own<TpClass>, &TpRef::target>>::make(own_type));
}

own<Type>& Action::type() {
	for (;;) {
		auto as_cold = dom::strict_cast<TpColdLambda>(type_);
		if (!as_cold || !as_cold->resolved)
			return type_;
		type_ = as_cold->resolved;
	}
}

void Action::match(ActionMatcher& matcher) { matcher.on_unmatched(*this); };
void ConstInt64::match(ActionMatcher& matcher) { matcher.on_const_i64(*this); }
void ConstDouble::match(ActionMatcher& matcher) { matcher.on_const_double(*this); }
void ConstVoid::match(ActionMatcher& matcher) { matcher.on_const_void(*this); }
void ConstBool::match(ActionMatcher& matcher) { matcher.on_const_bool(*this); }
void Get::match(ActionMatcher& matcher) { matcher.on_get(*this); }
void Set::match(ActionMatcher& matcher) { matcher.on_set(*this); }
void GetField::match(ActionMatcher& matcher) { matcher.on_get_field(*this); }
void SetField::match(ActionMatcher& matcher) { matcher.on_set_field(*this); }
void MkInstance::match(ActionMatcher& matcher) { matcher.on_mk_instance(*this); }
void MkLambda::match(ActionMatcher& matcher) { matcher.on_mk_lambda(*this); }
void Call::match(ActionMatcher& matcher) { matcher.on_call(*this); }
void GetAtIndex::match(ActionMatcher& matcher) { matcher.on_get_at_index(*this); }
void SetAtIndex::match(ActionMatcher& matcher) { matcher.on_set_at_index(*this); }
void MakeDelegate::match(ActionMatcher& matcher) { matcher.on_make_delegate(*this); }
void MakeFnPtr::match(ActionMatcher& matcher) { matcher.on_make_fn_ptr(*this); }
void ToIntOp::match(ActionMatcher& matcher) { matcher.on_to_int(*this); }
void ToFloatOp::match(ActionMatcher& matcher) { matcher.on_to_float(*this); }
void NotOp::match(ActionMatcher& matcher) { matcher.on_not(*this); }
void NegOp::match(ActionMatcher& matcher) { matcher.on_neg(*this); }
void RefOp::match(ActionMatcher& matcher) { matcher.on_ref(*this); }
void Loop::match(ActionMatcher& matcher) { matcher.on_loop(*this); }
void CopyOp::match(ActionMatcher& matcher) { matcher.on_copy(*this); }
void MkWeakOp::match(ActionMatcher& matcher) { matcher.on_mk_weak(*this); }
void DerefWeakOp::match(ActionMatcher& matcher) { matcher.on_deref_weak(*this); }
void Block::match(ActionMatcher& matcher) { matcher.on_block(*this); }
void CastOp::match(ActionMatcher& matcher) { matcher.on_cast(*this); }
void AddOp::match(ActionMatcher& matcher) { matcher.on_add(*this); }
void SubOp::match(ActionMatcher& matcher) { matcher.on_sub(*this); }
void MulOp::match(ActionMatcher& matcher) { matcher.on_mul(*this); }
void DivOp::match(ActionMatcher& matcher) { matcher.on_div(*this); }
void ModOp::match(ActionMatcher& matcher) { matcher.on_mod(*this); }
void AndOp::match(ActionMatcher& matcher) { matcher.on_and(*this); }
void OrOp::match(ActionMatcher& matcher) { matcher.on_or(*this); }
void XorOp::match(ActionMatcher& matcher) { matcher.on_xor(*this); }
void ShlOp::match(ActionMatcher& matcher) { matcher.on_shl(*this); }
void ShrOp::match(ActionMatcher& matcher) { matcher.on_shr(*this); }
void EqOp::match(ActionMatcher& matcher) { matcher.on_eq(*this); }
void LtOp::match(ActionMatcher& matcher) { matcher.on_lt(*this); }
void If::match(ActionMatcher& matcher) { matcher.on_if(*this); }
void LAnd::match(ActionMatcher& matcher) { matcher.on_land(*this); }
void Else::match(ActionMatcher& matcher) { matcher.on_else(*this); }
void LOr::match(ActionMatcher& matcher) { matcher.on_lor(*this); }

void ActionMatcher::on_unmatched(Action& node) {}
void ActionMatcher::on_un_op(UnaryOp& node) { on_unmatched(node); }
void ActionMatcher::on_bin_op(BinaryOp& node) { on_unmatched(node); }
void ActionMatcher::on_const_i64(ConstInt64& node) { on_unmatched(node); }
void ActionMatcher::on_const_double(ConstDouble& node) { on_unmatched(node); }
void ActionMatcher::on_const_void(ConstVoid& node) { on_unmatched(node); }
void ActionMatcher::on_const_bool(ConstBool& node) { on_unmatched(node); }
void ActionMatcher::on_get(Get& node) { on_unmatched(node); }
void ActionMatcher::on_set(Set& node) { on_unmatched(node); }
void ActionMatcher::on_get_field(GetField& node) { on_unmatched(node); }
void ActionMatcher::on_set_field(SetField& node) { on_unmatched(node); }
void ActionMatcher::on_mk_lambda(MkLambda& node) { on_unmatched(node); }
void ActionMatcher::on_mk_instance(MkInstance& node) { on_unmatched(node); }
void ActionMatcher::on_call(Call& node) { on_unmatched(node); }
void ActionMatcher::on_get_at_index(GetAtIndex& node) { on_unmatched(node); }
void ActionMatcher::on_set_at_index(SetAtIndex& node) { on_unmatched(node); }
void ActionMatcher::on_make_delegate(MakeDelegate& node) { on_unmatched(node); }
void ActionMatcher::on_make_fn_ptr(MakeFnPtr& node) { on_unmatched(node); }
void ActionMatcher::on_to_int(ToIntOp& node) { on_un_op(node); }
void ActionMatcher::on_to_float(ToFloatOp& node) { on_un_op(node); }
void ActionMatcher::on_not(NotOp& node) { on_un_op(node); }
void ActionMatcher::on_neg(NegOp& node) { on_un_op(node); }
void ActionMatcher::on_ref(RefOp& node) { on_un_op(node); }
void ActionMatcher::on_loop(Loop& node) { on_un_op(node); }
void ActionMatcher::on_copy(CopyOp& node) { on_un_op(node); }
void ActionMatcher::on_mk_weak(MkWeakOp& node) { on_un_op(node); }
void ActionMatcher::on_deref_weak(DerefWeakOp& node) { on_un_op(node); }
void ActionMatcher::on_block(Block& node) { on_unmatched(node); }
void ActionMatcher::on_cast(CastOp& node) { on_bin_op(node); }
void ActionMatcher::on_add(AddOp& node) { on_bin_op(node); }
void ActionMatcher::on_sub(SubOp& node) { on_bin_op(node); }
void ActionMatcher::on_mul(MulOp& node) { on_bin_op(node); }
void ActionMatcher::on_div(DivOp& node) { on_bin_op(node); }
void ActionMatcher::on_mod(ModOp& node) { on_bin_op(node); }
void ActionMatcher::on_and(AndOp& node) { on_bin_op(node); }
void ActionMatcher::on_or(OrOp& node) { on_bin_op(node); }
void ActionMatcher::on_xor(XorOp& node) { on_bin_op(node); }
void ActionMatcher::on_shl(ShlOp& node) { on_bin_op(node); }
void ActionMatcher::on_shr(ShrOp& node) { on_bin_op(node); }
void ActionMatcher::on_eq(EqOp& node) { on_bin_op(node); }
void ActionMatcher::on_lt(LtOp& node) { on_bin_op(node); }
void ActionMatcher::on_if(If& node) { on_bin_op(node); }
void ActionMatcher::on_land(LAnd& node) { on_bin_op(node); }
void ActionMatcher::on_else(Else& node) { on_bin_op(node); }
void ActionMatcher::on_lor(LOr& node) { on_bin_op(node); }

void ActionMatcher::fix(own<Action>& ptr) {
	auto saved = fix_result;
	fix_result = &ptr;
	ptr.pinned()->match(*this);
	fix_result = saved;
}

void ActionScanner::on_un_op(UnaryOp& node) { fix(node.p); }
void ActionScanner::on_bin_op(BinaryOp& node) {
	fix(node.p[0]);
	fix(node.p[1]);
}
void ActionScanner::on_set(Set& node) { fix(node.val); }
void ActionScanner::on_mk_lambda(MkLambda& node) { on_block(node); }
void ActionScanner::on_call(Call& node) {
	for (auto& p : node.params)
		fix(p);
	fix(node.callee);
}
void ActionScanner::on_get_at_index(GetAtIndex& node) {
	for (auto& p : node.indexes)
		fix(p);
	fix(node.indexed);
}
void ActionScanner::on_set_at_index(SetAtIndex& node) {
	on_get_at_index(node);
	fix(node.value);
}
void ActionScanner::on_make_delegate(MakeDelegate& node) { fix(node.base); }
void ActionScanner::on_block(Block& node) {
	for (auto& l : node.names)
		fix(l->initializer);
	for (auto& p : node.body)
		fix(p);
}
void ActionScanner::on_get_field(GetField& node) { fix(node.base); }
void ActionScanner::on_set_field(SetField& node) {
	fix(node.base);
	fix(node.val);
}

void TpInt64::match(TypeMatcher& matcher) { matcher.on_int64(*this); }
void TpDouble::match(TypeMatcher& matcher) { matcher.on_double(*this); }
void TpFunction::match(TypeMatcher& matcher) { matcher.on_function(*this); }
void TpLambda::match(TypeMatcher& matcher) { matcher.on_lambda(*this); }
void TpColdLambda::match(TypeMatcher& matcher) { matcher.on_cold_lambda(*this); }
void TpVoid::match(TypeMatcher& matcher) { matcher.on_void(*this); }
void TpOptional::match(TypeMatcher& matcher) { matcher.on_optional(*this); }
void TpClass::match(TypeMatcher& matcher) { matcher.on_class(*this); }
void TpRef::match(TypeMatcher& matcher) { matcher.on_ref(*this); }
void TpWeak::match(TypeMatcher& matcher) { matcher.on_weak(*this); }

size_t typelist_hasher::operator() (const vector<own<Type>>* v) const {
	size_t r = 0;
	for (const auto& p : *v)
		r += std::hash<void*>()(p);
	return r;
}

bool typelist_comparer::operator() (const vector<own<Type>>* a, const vector<own<Type>>* b) const {
	return *a == *b;
}

Ast::Ast()
	: dom(new dom::Dom(cpp_dom)) {
	auto sys = dom->names()->get("sys");
	auto obj = new TpClass;
	classes.push_back(obj);
	obj->name = sys->get("Object");
	classes_by_names[obj->name] = obj;
	object = obj;
	auto blob = new TpClass;
	classes.push_back(blob);
	blob->name = sys->get("Blob");
	classes_by_names[blob->name] = blob;
	this->blob = blob;
	auto mk_field = [&](const char* name, pin<Action> initializer) {
		auto f = pin<Field>::make();
		f->name = sys->get(name);
		f->initializer = initializer;
		return f;
	};
	blob->fields.push_back(mk_field("size", new ConstInt64));
	blob->fields.push_back(mk_field("_data", new ConstInt64));
	auto mk_fn = [&](pin<dom::Name> name, pin<Action> result_type, std::initializer_list<pin<Type>> params) {
		auto fn = pin<ast::Function>::make();
		functions.push_back(fn);
		fn->name = name;
		fn->is_platform = true;
		functions_by_names[fn->name] = fn;
		fn->type_expression = result_type;
		for (auto& p : params) {
			fn->names.push_back(new Var);
			fn->names.back()->type = p;
		}
	};
	mk_fn(sys->get("Blob")->get("resize"), new ConstVoid, { get_ref(blob), tp_int64() });
	mk_fn(sys->get("Blob")->get("getAt"), new ConstInt64, { get_ref(blob), tp_int64() });
	mk_fn(sys->get("Blob")->get("setAt"), new ConstVoid, { get_ref(blob), tp_int64(), tp_int64() });
	mk_fn(sys->get("Blob")->get("getByteAt"), new ConstInt64, { get_ref(blob), tp_int64() });
	mk_fn(sys->get("Blob")->get("setByteAt"), new ConstVoid, { get_ref(blob), tp_int64(), tp_int64() });
	mk_fn(sys->get("Blob")->get("copy"), new ConstBool, { get_ref(blob), tp_int64(), get_ref(blob), tp_int64(), tp_int64() });
}

pin<TpInt64> Ast::tp_int64() {
	static auto r = own<TpInt64>::make();
	return r;
}
pin<TpDouble> Ast::tp_double() {
	static auto r = own<TpDouble>::make();
	return r;
}
pin<TpVoid> Ast::tp_void() {
	static auto r = own<TpVoid>::make();
	return r;
}
pin<TpFunction> Ast::tp_function(vector<own<Type>>&& params) {
	auto at = function_types_.find(&params);
	if (at != function_types_.end())
		return at->second;
	auto r = pin<TpFunction>::make();
	r->params = move(params);
	function_types_.insert({ &r->params, r });
	return r;
}
pin<TpLambda> Ast::tp_lambda(vector<own<Type>>&& params) {
	auto at = lambda_types_.find(&params);
	if (at != lambda_types_.end())
		return at->second;
	auto r = pin<TpLambda>::make();
	r->params = move(params);
	lambda_types_.insert({&r->params, r});
	return r;
}
pin<TpOptional> Ast::tp_optional(pin<Type> wrapped) {
	int depth = 0;
	if (auto as_optional = dom::strict_cast<ast::TpOptional>(wrapped)) {
		depth = as_optional->depth + 1;
		wrapped = as_optional->wrapped;
	}
	auto& depths = optional_types_[wrapped];
	assert(depth <= depths.size());
	if (depth == depths.size()) {
		depths.push_back(pin<TpOptional>::make());
		depths.back()->wrapped = wrapped;
		depths.back()->depth = depth;
	}
	return depths[depth];
}

pin<Type> Ast::get_wrapped(pin<TpOptional> opt) {
	return opt->depth == 0
		? opt->wrapped
		: optional_types_[opt->wrapped][size_t(opt->depth) - 1];
}

pin<TpRef> Ast::get_ref(pin<TpClass> target) {
	auto& r = refs[target];
	if (!r) {
		r = new TpRef;
		r->target = target;
	}
	return r;
}

pin<TpWeak> Ast::get_weak(pin<TpClass> target) {
	auto& w = weaks[target];
	if (!w) {
		w = new TpWeak;
		w->target = target;
	}
	return w;
}

pin<TpClass> Ast::get_class(pin<dom::Name> name) {
	if (auto r = peek_class(name))
		return r;
	auto r = pin<TpClass>::make();
	r->name = name;
	classes.push_back(r);
	classes_by_names.insert({ name, r });
	return r;
}

pin<TpClass> Ast::peek_class(pin<dom::Name> name) {
	auto it = classes_by_names.find(name);
	return it == classes_by_names.end() ? nullptr : it->second;
}

pin<TpClass> Ast::extract_class(pin<Type> pointer) {
	if (auto as_ref = dom::strict_cast<ast::TpRef>(pointer))
		return as_ref->target;
	if (auto as_class = dom::strict_cast<ast::TpClass>(pointer))
		return as_class;
	// if (auto as_weak = dom::strict_cast<ast::TpWeak>(pointer)) // weak is not directly accessible without a null check
	//	return as_weak->target;
	return nullptr;
}

void Node::err_out(const std::string& message) {
	std::cerr << message;
	throw 1;
}

string Node::get_annotation() {
	return (std::stringstream() << *this).str();
}

string Action::get_annotation() {
	std::stringstream r;
	r << Node::get_annotation();
	if (type_)
		r << " tp=" << type_.pinned();
	return r.str();
}

string Var::get_annotation() {
	std::stringstream r;
	r << Node::get_annotation();
	if (type)
		r << " tp=" << type.pinned();
	if (lexical_depth)
		r << " depth=" << lexical_depth;
	return r.str();
}

string DataRef::get_annotation() {
	return var
		? Node::get_annotation()
		: format_str(Node::get_annotation(), " var_name=", var_name);
}

string FieldRef::get_annotation() {
	return field
		? Node::get_annotation()
		: format_str(Node::get_annotation(), " field_name=", field_name);
}

} // namespace ast

namespace std {

std::ostream& operator<< (std::ostream& dst, const ast::Node& n) {
	return dst << '(' << n.line << ':' << n.pos << ')';
}

std::ostream& operator<< (std::ostream& dst, const ltm::pin<ast::Type>& t) {
	struct type_matcher : ast::TypeMatcher {
		std::ostream& dst;
		type_matcher(std::ostream& dst) : dst(dst) {}
		void on_int64(ast::TpInt64& type) override { dst << "int"; }
		void on_double(ast::TpDouble& type) override { dst << "double"; }
		void on_void(ast::TpVoid& type) override { dst << "void"; }
		void out_proto(ast::TpFunction& type) {
			size_t i = 0, last = type.params.size() - 1;
			for (auto& p : type.params) {
				dst << (i == 0
					? i == last
						? "(){"
						: "("
					: i == last
						? "){"
						: ",")
					<< p.pinned();
				i++;
			}
			dst << "}";
		}
		void on_function(ast::TpFunction& type) override {
			dst << "fn";
			out_proto(type);
		}
		void on_lambda(ast::TpLambda& type) override {
			out_proto(type);
		}
		void on_cold_lambda(ast::TpColdLambda& type) override {
			dst << "[" << type.callees.size() << "]";
			if (type.resolved)
				dst << type.resolved.pinned();
			else
				dst << "(~)";
		}
		void on_optional(ast::TpOptional& type) override {
			if (dom::strict_cast<ast::TpVoid>(type.wrapped)) {
				for (int i = type.depth; --i >= 0;)
					dst << "?";
				dst << "bool";
			} else {
				for (int i = type.depth + 1; --i >= 0;)
					dst << "?";
				dst << type.wrapped.pinned();
			}
		}
		void on_class(ast::TpClass& type) override {
			dst << "@" << type.name.pinned();
		}
		void on_ref(ast::TpRef& type) override {
			dst << type.target->name.pinned();
		}
		void on_weak(ast::TpWeak& type) override {
			dst << "&" << type.target->name.pinned();
		}
	};
	t->match(type_matcher(dst));
	return dst;
}

}  // namespace std
