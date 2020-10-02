#include "ast.h"

namespace ast {

using dom::TypeInfo;
using dom::CppClassType;
using dom::TypeWithFills;
using dom::CppField;
using dom::Kind;

own<dom::Dom> static_dom;
own<TypeWithFills> Ast::dom_type_;
own<TypeWithFills> Module::dom_type_;
own<TypeWithFills> ClassParamDef::dom_type_;
own<TypeWithFills> AbstractClassDef::dom_type_;
own<TypeWithFills> ClassDef::dom_type_;
own<TypeWithFills> DataDef::dom_type_;
own<TypeWithFills> MethodDef::dom_type_;
own<TypeWithFills> OverrideDef::dom_type_;

own<TypeWithFills> Block::dom_type_;
own<TypeWithFills> Loop::dom_type_;
own<TypeWithFills> Local::dom_type_;
own<TypeWithFills> Break::dom_type_;
own<TypeWithFills> ConstInt64::dom_type_;
own<TypeWithFills> ConstAtom::dom_type_;
own<TypeWithFills> ConstBool::dom_type_;
own<TypeWithFills> ConstDouble::dom_type_;
own<TypeWithFills> If::dom_type_;
own<TypeWithFills> Else::dom_type_;
own<TypeWithFills> GetVar::dom_type_;
own<TypeWithFills> SetVar::dom_type_;
own<TypeWithFills> GetField::dom_type_;
own<TypeWithFills> SetField::dom_type_;
own<TypeWithFills> MakeInstance::dom_type_;
own<TypeWithFills> Own::dom_type_;
own<TypeWithFills> Weak::dom_type_;
own<TypeWithFills> Array::dom_type_;
own<TypeWithFills> Call::dom_type_;

own<TypeWithFills> AddOp::dom_type_;
own<TypeWithFills> SubOp::dom_type_;
own<TypeWithFills> MulOp::dom_type_;
own<TypeWithFills> DivOp::dom_type_;
own<TypeWithFills> ModOp::dom_type_;
own<TypeWithFills> ShlOp::dom_type_;
own<TypeWithFills> ShrOp::dom_type_;
own<TypeWithFills> AndOp::dom_type_;
own<TypeWithFills> OrOp::dom_type_;
own<TypeWithFills> XorOp::dom_type_;
own<TypeWithFills> EqOp::dom_type_;
own<TypeWithFills> LtOp::dom_type_;
own<TypeWithFills> LogAndOp::dom_type_;
own<TypeWithFills> LogOrOp::dom_type_;
own<TypeWithFills> NotOp::dom_type_;
own<TypeWithFills> ToIntOp::dom_type_;
own<TypeWithFills> ToFloatOp::dom_type_;

own<TypeWithFills> TpInt64::dom_type_;
own<TypeWithFills> TpAtom::dom_type_;
own<TypeWithFills> TpBool::dom_type_;
own<TypeWithFills> TpVoid::dom_type_;
own<TypeWithFills> TpDouble::dom_type_;
own<TypeWithFills> TpWeak::dom_type_;
own<TypeWithFills> TpOwn::dom_type_;
own<TypeWithFills> TpPin::dom_type_;
own<TypeWithFills> TpArray::dom_type_;

namespace {

template<typename CLS>
void make_bin_op(const char* name, const pin<TypeInfo>& op_array_2) {
	CLS::dom_type_ = (new CppClassType<CLS>(static_dom, {"m0", name}))
		->field("p", pin<CppField<BinaryOp, own<Action>[2], &BinaryOp::p>>::make(op_array_2));
}

}  // namespace

void initialize() {
	if (static_dom)
		return;
	static_dom = new dom::Dom;
	auto weak_type = static_dom->mk_type(Kind::WEAK);
	auto own_type = static_dom->mk_type(Kind::OWN);
	auto size_t_type = static_dom->mk_type(Kind::UINT, sizeof(size_t));
	auto atom_type = static_dom->mk_type(Kind::ATOM);
	pin<dom::TypeInfo> weak_vector_type = new dom::VectorType<weak<dom::DomItem>>(weak_type);
	pin<dom::TypeInfo> own_vector_type = new dom::VectorType<own<dom::DomItem>>(own_type);
	Ast::dom_type_ = (new CppClassType<Ast>(static_dom, { "m0", "Ast" }))
		->field("modules", pin<CppField<Ast, vector<own<Module>>, &Ast::modules>>::make(own_vector_type));
	Module::dom_type_ = (new CppClassType<Module>(static_dom, { "m0", "Module" }))
		->field("version", pin<CppField<Module, size_t, &Module::version>>::make(size_t_type))
		->field("classes", pin<CppField<Module, vector<own<ClassDef>>, &Module::classes>>::make(own_vector_type));
	ClassParamDef::dom_type_ = (new CppClassType<ClassParamDef>(static_dom, {"m0", "ClassParam"}))
		->field("bound", pin<CppField<ClassParamDef, weak<AbstractClassDef>, &ClassParamDef::bound>>::make(weak_type));
	AbstractClassDef::dom_type_ = (new CppClassType<AbstractClassDef>(static_dom, {"m0", "AnyClass"}));
	ClassDef::dom_type_ = (new CppClassType<ClassDef>(static_dom, {"m0", "Class"}))
		->field("is_interface", pin<CppField<ClassDef, bool, &ClassDef::is_interface>>::make(static_dom->mk_type(Kind::BOOL)))
		->field("params", pin<CppField<ClassDef, vector<own<ClassParamDef>>, &ClassDef::type_params>>::make(own_vector_type))
		->field("bases", pin<CppField<ClassDef, vector<own<MakeInstance>>, &ClassDef::bases>>::make(own_vector_type))
		->field("fields", pin<CppField<ClassDef, vector<own<DataDef>>, &ClassDef::fields>>::make(own_vector_type))
		->field("methods", pin<CppField<ClassDef, vector<own<MethodDef>>, &ClassDef::methods>>::make(own_vector_type))
		->field("overrides", pin<CppField<ClassDef, vector<own<OverrideDef>>, &ClassDef::overrides>>::make(own_vector_type));
	DataDef::dom_type_ = (new CppClassType<DataDef>(static_dom, {"m0", "Data"}))
		->field("name", pin<CppField<DataDef, own<Name>, &DataDef::name>>::make(atom_type))
		->field("init", pin<CppField<DataDef, own<Action>, &DataDef::initializer>>::make(own_type));
	MethodDef::dom_type_ = (new CppClassType<MethodDef>(static_dom, {"m0", "Method"}))
		->field("name", pin<CppField<MethodDef, own<Name>, &MethodDef::name>>::make(atom_type))
		->field("params", pin<CppField<MethodDef, vector<own<DataDef>>, &MethodDef::params>>::make(own_vector_type))
		->field("body", pin<CppField<MethodDef, own<Action>, &MethodDef::body>>::make(own_type));
	OverrideDef::dom_type_ = (new CppClassType<OverrideDef>(static_dom, {"m0", "Override"}))
		->field("atom", pin<CppField<OverrideDef, own<Name>, &OverrideDef::atom>>::make(atom_type))
		->field("method", pin<CppField<OverrideDef, weak<MethodDef>, &OverrideDef::method>>::make(weak_type))
		->field("body", pin<CppField<OverrideDef, own<Action>, &OverrideDef::body>>::make(own_type));
	Block::dom_type_ = (new CppClassType<Block>(static_dom, {"m0", "Block"}))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type));
	Loop::dom_type_ = (new CppClassType<Loop>(static_dom, {"m0", "Loop"}))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type));
	Local::dom_type_ = (new CppClassType<Local>(static_dom, {"m0", "Local"}))
		->field("body", pin<CppField<Block, vector<own<Action>>, &Block::body>>::make(own_vector_type))
		->field("var", pin<CppField<Local, own<DataDef>, &Local::var>>::make(own_type));
	Break::dom_type_ = (new CppClassType<Break>(static_dom, {"m0", "Break"}))
		->field("to_break", pin<CppField<Break, weak<Block>, &Break::to_break>>::make(weak_type))
		->field("result", pin<CppField<Break, own<Action>, &Break::result>>::make(own_type));
	ConstInt64::dom_type_ = (new CppClassType<ConstInt64>(static_dom, {"m0", "Int"}))
		->field("value", pin<CppField<ConstInt64, int64_t, &ConstInt64::value>>::make(
			static_dom->mk_type(Kind::INT, sizeof(int64_t))));
	ConstAtom::dom_type_ = (new CppClassType<ConstAtom>(static_dom, {"m0", "Atom"}))
		->field("value", pin<CppField<ConstAtom, own<Name>, &ConstAtom::value>>::make(atom_type));
	ConstBool::dom_type_ = (new CppClassType<ConstBool>(static_dom, {"m0", "Bool"}))
		->field("value", pin<CppField<ConstBool, bool, &ConstBool::value>>::make(static_dom->mk_type(Kind::BOOL)));
	ConstDouble::dom_type_ = (new CppClassType<ConstDouble>(static_dom, {"m0", "Double"}))
		->field("value", pin<CppField<ConstDouble, double, &ConstDouble::value>>::make(
			static_dom->mk_type(Kind::FLOAT, sizeof(double))));
	GetVar::dom_type_ = (new CppClassType<GetVar>(static_dom, {"m0", "GetVar"}))
		->field("var", pin<CppField<DataRef, weak<DataDef>, &DataRef::var>>::make(weak_type));
	SetVar::dom_type_ = (new CppClassType<SetVar>(static_dom, {"m0", "SetVar"}))
		->field("var", pin<CppField<DataRef, weak<DataDef>, &DataRef::var>>::make(weak_type))
		->field("val", pin<CppField<SetVar, own<Action>, &SetVar::value>>::make(own_type));
	GetField::dom_type_ = (new CppClassType<GetField>(static_dom, {"m0", "GetField"}))
		->field("var", pin<CppField<DataRef, weak<DataDef>, &DataRef::var>>::make(weak_type))
		->field("base", pin<CppField<GetField, own<Action>, &GetField::base>>::make(own_type));
	SetField::dom_type_ = (new CppClassType<SetField>(static_dom, {"m0", "SetField"}))
		->field("var", pin<CppField<DataRef, weak<DataDef>, &DataRef::var>>::make(weak_type))
		->field("base", pin<CppField<SetField, own<Action>, &SetField::base>>::make(own_type))
		->field("val", pin<CppField<SetField, own<Action>, &SetField::value>>::make(own_type));
	MakeInstance::dom_type_ = (new CppClassType<MakeInstance>(static_dom, {"m0", "Make"}))
		->field("class", pin<CppField<MakeInstance, weak<AbstractClassDef>, &MakeInstance::cls>>::make(weak_type))
		->field("params", pin<CppField<MakeInstance, vector<own<MakeInstance>>, &MakeInstance::params>>::make(own_vector_type));
	Array::dom_type_ = (new CppClassType<Array>(static_dom, {"m0", "Array"}))
		->field("size", pin<CppField<Array, own<Action>, &Array::size>>::make(own_type))
		->field("initializers", pin<CppField<Array, vector<own<Action>>, &Array::initializers>>::make(own_vector_type));
	Call::dom_type_ = (new CppClassType<Call>(static_dom, {"m0", "Call"}))
		->field("receiver", pin<CppField<Call, own<Action>, &Call::receiver>>::make(own_type))
		->field("method", pin<CppField<Call, weak<MethodDef>, &Call::method>>::make(weak_type))
		->field("params", pin<CppField<Call, vector<own<Action>>, &Call::params>>::make(own_vector_type));
	
	auto op_array_2 = static_dom->mk_type(Kind::FIX_ARRAY, 2, own_type);
	make_bin_op<AddOp>("If", op_array_2);
	make_bin_op<AddOp>("Else", op_array_2);
	make_bin_op<AddOp>("Add", op_array_2);
	make_bin_op<SubOp>("Sub", op_array_2);
	make_bin_op<MulOp>("Mul", op_array_2);
	make_bin_op<DivOp>("Div", op_array_2);
	make_bin_op<ModOp>("Mod", op_array_2);
	make_bin_op<ShlOp>("Shl", op_array_2);
	make_bin_op<ShrOp>("Shr", op_array_2);
	make_bin_op<AndOp>("And", op_array_2);
	make_bin_op<OrOp>("Or", op_array_2);
	make_bin_op<XorOp>("Xor", op_array_2);
	make_bin_op<EqOp>("Eq", op_array_2);
	make_bin_op<LtOp>("Lt", op_array_2);
	make_bin_op<LogAndOp>("LogAnd", op_array_2);
	make_bin_op<LogOrOp>("LogOr", op_array_2);
	NotOp::dom_type_ = (new CppClassType<NotOp>(static_dom, {"m0", "Not"}))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	ToIntOp::dom_type_ = (new CppClassType<ToIntOp>(static_dom, {"m0", "ToInt"}))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	ToFloatOp::dom_type_ = (new CppClassType<ToFloatOp>(static_dom, {"m0", "ToFloat"}))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	Own::dom_type_ = (new CppClassType<Own>(static_dom, {"m0", "Own"}))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	Weak::dom_type_ = (new CppClassType<Weak>(static_dom, {"m0", "Weak"}))
		->field("p", pin<CppField<UnaryOp, own<Action>, &UnaryOp::p>>::make(own_type));
	TpInt64::dom_type_ = new CppClassType<TpInt64>(static_dom, {"m0", "Type", "Int64"});
	TpAtom::dom_type_ = new CppClassType<TpAtom>(static_dom, {"m0", "Type", "Atom"});
	TpVoid::dom_type_ = new CppClassType<TpVoid>(static_dom, {"m0", "Type", "Void"});
	TpBool::dom_type_ = new CppClassType<TpBool>(static_dom, {"m0", "Type", "Bool"});
	TpDouble::dom_type_ = new CppClassType<TpDouble>(static_dom, {"m0", "Type", "Double"});
	TpWeak::dom_type_ = (new CppClassType<TpWeak>(static_dom, {"m0", "Type", "Weak"}))
		->field("c", pin<CppField<TpWeak, own<MakeInstance>, &TpWeak::cls>>::make(own_type));
	TpOwn::dom_type_ = (new CppClassType<TpOwn>(static_dom, {"m0", "Type", "Own"}))
		->field("c", pin<CppField<TpOwn, own<MakeInstance>, &TpOwn::cls>>::make(own_type));
	TpPin::dom_type_ = (new CppClassType<TpPin>(static_dom, {"m0", "Type", "Pin"}))
		->field("c", pin<CppField<TpPin, own<MakeInstance>, &TpPin::cls>>::make(own_type));
	TpArray::dom_type_ = (new CppClassType<TpArray>(static_dom, {"m0", "Type", "Array"}))
		->field("e", pin<CppField<TpArray, own<Type>, &TpArray::element>>::make(own_type));
}

void Action::match(ActionMatcher& matcher) { matcher.on_unmatched(*this); };
void Block::match(ActionMatcher& matcher) { matcher.on_block(*this); }
void Loop::match(ActionMatcher& matcher) { matcher.on_loop(*this); }
void Local::match(ActionMatcher& matcher) { matcher.on_local(*this); }
void Break::match(ActionMatcher& matcher) { matcher.on_break(*this); }
void ConstInt64::match(ActionMatcher& matcher) { matcher.on_const_i64(*this); }
void ConstAtom::match(ActionMatcher& matcher) { matcher.on_const_atom(*this); }
void ConstDouble::match(ActionMatcher& matcher) { matcher.on_const_double(*this); }
void ConstBool::match(ActionMatcher& matcher) { matcher.on_const_bool(*this); }
void If::match(ActionMatcher& matcher) { matcher.on_if(*this); }
void Else::match(ActionMatcher& matcher) { matcher.on_else(*this); }
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
void LogAndOp::match(ActionMatcher& matcher) { matcher.on_log_and(*this); }
void LogOrOp::match(ActionMatcher& matcher) { matcher.on_log_or(*this); }
void NotOp::match(ActionMatcher& matcher) { matcher.on_not(*this); }
void ToIntOp::match(ActionMatcher& matcher) { matcher.on_to_int(*this); }
void ToFloatOp::match(ActionMatcher& matcher) { matcher.on_to_float(*this); }
void Own::match(ActionMatcher& matcher) { matcher.on_own(*this); }
void Weak::match(ActionMatcher& matcher) { matcher.on_weak(*this); }
void GetVar::match(ActionMatcher& matcher) { matcher.on_get_var(*this); }
void SetVar::match(ActionMatcher& matcher) { matcher.on_set_var(*this); }
void GetField::match(ActionMatcher& matcher) { matcher.on_get_field(*this); }
void SetField::match(ActionMatcher& matcher) { matcher.on_set_field(*this); }
void MakeInstance::match(ActionMatcher& matcher) { matcher.on_make_instance(*this); }
void Array::match(ActionMatcher& matcher) { matcher.on_array(*this); }
void Call::match(ActionMatcher& matcher) { matcher.on_call(*this); }

void ActionMatcher::on_unmatched(Action& node) {}
void ActionMatcher::on_bin_op(BinaryOp& node) { on_unmatched(node); }
void ActionMatcher::on_un_op(UnaryOp& node) { on_unmatched(node); }
void ActionMatcher::on_block(Block& node) { on_unmatched(node); }
void ActionMatcher::on_local(Local& node) { on_unmatched(node); }
void ActionMatcher::on_loop(Loop& node) { on_unmatched(node); }
void ActionMatcher::on_break(Break& node) { on_unmatched(node); }
void ActionMatcher::on_const_i64(ConstInt64& node) { on_unmatched(node); }
void ActionMatcher::on_const_atom(ConstAtom& node) { on_unmatched(node); }
void ActionMatcher::on_const_double(ConstDouble& node) { on_unmatched(node); }
void ActionMatcher::on_const_bool(ConstBool& node) { on_unmatched(node); }
void ActionMatcher::on_if(If& node) { on_bin_op(node); }
void ActionMatcher::on_else(Else& node) { on_bin_op(node); }
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
void ActionMatcher::on_log_and(LogAndOp& node) { on_bin_op(node); }
void ActionMatcher::on_log_or(LogOrOp& node) { on_bin_op(node); }
void ActionMatcher::on_not(NotOp& node) { on_un_op(node); }
void ActionMatcher::on_to_int(ToIntOp& node) { on_un_op(node); }
void ActionMatcher::on_to_float(ToFloatOp& node) { on_un_op(node); }
void ActionMatcher::on_own(Own& node) { on_un_op(node); }
void ActionMatcher::on_weak(Weak& node) { on_un_op(node); }
void ActionMatcher::on_get_var(GetVar& node) { on_unmatched(node); }
void ActionMatcher::on_set_var(SetVar& node) { on_unmatched(node); }
void ActionMatcher::on_get_field(GetField& node) { on_unmatched(node); }
void ActionMatcher::on_set_field(SetField& node) { on_unmatched(node); }
void ActionMatcher::on_make_instance(MakeInstance& node) { on_unmatched(node); }
void ActionMatcher::on_array(Array& node) { on_unmatched(node); }
void ActionMatcher::on_call(Call& node) { on_unmatched(node); }

void ActionScanner::fix(own<Action>& ptr) {
	auto saved = fix_result;
	fix_result = &ptr;
	ptr.pinned()->match(*this);
	fix_result = saved;
}

void ActionScanner::on_bin_op(BinaryOp& node) {
	fix(node.p[0]);
	fix(node.p[1]);
}
void ActionScanner::on_un_op(UnaryOp& node) { fix(node.p); }
void ActionScanner::on_block(Block& node) {
	for (auto& i : node.body)
		fix(i);
}
void ActionScanner::on_loop(Loop& node) {
	for (auto& i : node.body)
		fix(i);
}
void ActionScanner::on_local(Local& node) {
	fix(node.var->initializer);
	for (auto& i : node.body)
		fix(i);
}
void ActionScanner::on_break(Break& node) {
	if (node.result)
		fix(node.result);
}
void ActionScanner::on_set_var(SetVar& node) { fix(node.value); }
void ActionScanner::on_get_field(GetField& node) { fix(node.base); }
void ActionScanner::on_set_field(SetField& node) {
	fix(node.base);
	fix(node.value);
}
void ActionScanner::on_make_instance(MakeInstance& node) {
	for (auto& i : node.params)
		fix(i);
}
void ActionScanner::on_array(Array& node) {
	if (node.size)
		fix(node.size);
	for (auto& i : node.initializers)
		fix(i);
}
void ActionScanner::on_call(Call& node) {
	fix(node.receiver);
	for (auto& i : node.params)
		fix(i);
}

void Type::match(TypeMatcher& matcher) { matcher.on_unmatched(*this); }
void TpInt64::match(TypeMatcher& matcher) { matcher.on_int64(*this); }
void TpAtom::match(TypeMatcher& matcher) { matcher.on_atom(*this); }
void TpVoid::match(TypeMatcher& matcher) { matcher.on_void(*this); }
void TpBool::match(TypeMatcher& matcher) { matcher.on_bool(*this); }
void TpDouble::match(TypeMatcher& matcher) { matcher.on_double(*this); }
void TpWeak::match(TypeMatcher& matcher) { matcher.on_weak(*this); }
void TpOwn::match(TypeMatcher& matcher) { matcher.on_own(*this); }
void TpPin::match(TypeMatcher& matcher) { matcher.on_pin(*this); }
void TpArray::match(TypeMatcher& matcher) { matcher.on_array(*this); }

size_t class_key_hasher::operator() (const class_key& key) const {
	size_t r = std::hash<void*>()(key.data.cls);
	for (const auto& p : key.data.params)
		r += std::hash<void*>()(p);
	return r;
}

bool class_key_comparer::operator() (const class_key& a, const class_key& b) const {
	if (a.data.cls != b.data.cls)
		return false;
	// assert(a.data.params.size() == b.data.params.size());
	for (size_t i = 0, n = a.data.params.size(); i < n; i++) {
		if (a.data.params[i] != b.data.params[i])
			return false;	
	}
	return true;
}

pin<TpInt64> Ast::tp_int64() {
	static auto r = own<TpInt64>::make();
	return r;
}
pin<TpBool> Ast::tp_bool() {
	static auto r = own<TpBool>::make();
	return r;
}
pin<TpAtom> Ast::tp_atom() {
	static auto r = own<TpAtom>::make();
	return r;
}
pin<TpVoid> Ast::tp_void() {
	static auto r = own<TpVoid>::make();
	return r;
}
pin<TpDouble> Ast::tp_double() {
	static auto r = own<TpDouble>::make();
	return r;
}
pin<TpOwn> Ast::tp_own(const pin<MakeInstance>& target) {
	auto& r = own_types_[target];
	if (!r) {
		r = own<TpOwn>::make();	
		r->cls = target;
	}
	return r;
}
pin<TpWeak> Ast::tp_weak(const pin<MakeInstance>& target) {
	auto& r = weak_types_[target];
	if (!r) {
		r = own<TpWeak>::make();	
		r->cls = target;
	}
	return r;
}
pin<TpPin> Ast::tp_pin(const pin<MakeInstance>& target) {
	auto& r = pin_types_[target];
	if (!r) {
		r = own<TpPin>::make();	
		r->cls = target;
	}
	return r;
}
pin<TpArray> Ast::tp_array(const pin<Type>& element) {
	auto& r = array_types_[element];
	if (!r) {
		r = own<TpArray>::make();
		r->element = element;
	}
	return r;
}
pin<MakeInstance> Ast::intern(const pin<MakeInstance>& cls) {
	auto r = interned_.insert(class_key(cls));
	return r.first->holder;
}

void Node::err_out(const std::string& message) const {
	std::cerr << message;
	throw 1;
}

} // namespace ast

namespace std {

std::ostream& operator<< (std::ostream& dst, ast::Node* n) {
	if (n->module)
		dst << n->module->name.pinned();
	else
		dst << "???";
	return dst << '(' << n->line << ':' << n->pos << ')';
}

}  // namespace std
