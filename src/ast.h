#ifndef AST_H
#define AST_H

#include "dom.h"
#include "dom-to-string.h"

#include <sstream>
#include <unordered_set>

namespace ast {

using std::move;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::Name;

struct ClassDef;
struct Module;
struct DataDef;
struct MethodDef;
struct OverrideDef;
struct Node;  // having file position
struct Action;  // having result, able to build code
struct MakeInstance;
struct ActionMatcher;

struct Node: dom::DomItem {
	int line = 0;
	int pos = 0;
	weak<Module> module;
	void err_out(const std::string& message);
	template<typename... T>
	void error(const T&... t) { err_out(((std::stringstream() << "Error at " << this << ": ") << ... << t).str()); }
};

struct TypeMatcher;

struct Type : dom::DomItem {
	Type() { make_shared(); }
	virtual void match(TypeMatcher& matcher);
};
struct TpInt64 : Type {
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpInt64);
};
struct TpAtom : Type {
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpAtom);
};
struct TpVoid : Type {
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpVoid);
};
struct TpBool : Type {  // TODO: make it optional(void)
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpBool);
};
struct TpDouble : Type {
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpDouble);
};
struct TpWeak : Type {
	own<MakeInstance> cls;
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpWeak);
};
struct TpOwn : Type {
	own<MakeInstance> cls;
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpOwn);
};
struct TpPin : Type {
	own<MakeInstance> cls;
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpPin);
};
//TODO: TpOptional
struct TpArray : Type {
	own<Type> element;
	void match(TypeMatcher& matcher) override;
	DECLARE_DOM_CLASS(TpArray);
};

struct TypeMatcher {
	virtual void on_unmatched(Type& type) {}
	virtual void on_int64(TpInt64& type) {}
	virtual void on_void(TpVoid& type) {}
	virtual void on_bool(TpBool& type) {}
	virtual void on_double(TpDouble& type) {}
	virtual void on_atom(TpAtom& type) {}
	virtual void on_own(TpOwn& type) {}
	virtual void on_weak(TpWeak& type) {}
	virtual void on_pin(TpPin& type) {}
	virtual void on_array(TpArray& type) {}
};

struct Action: Node {
	weak<Type> type;
	virtual void match(ActionMatcher& matcher);
};

struct class_key{
	own<MakeInstance> holder;
	MakeInstance& data;
	explicit class_key(const pin<MakeInstance>& p) : data(*p) {}
	explicit class_key(const class_key& src) : holder(&src.data), data(*holder) {}
	void operator=(const class_key& src) = delete;
};

struct class_key_hasher {
	size_t operator() (const class_key&) const;
};
struct class_key_comparer {
	bool operator() (const class_key&, const class_key&) const;
};

extern own<dom::Dom> static_dom;

struct Ast: dom::DomItem {
	vector<own<Module>> modules;

	// We use MakeInstance nodes produced by intern method as peremeterized class definitions, they are shared.
	unordered_map<own<MakeInstance>, own<TpWeak>> weak_types_;
	unordered_map<own<MakeInstance>, own<TpOwn>> own_types_;
	unordered_map<own<MakeInstance>, own<TpPin>> pin_types_;
	unordered_map<own<Type>, own<TpArray>> array_types_;
	unordered_set<class_key, class_key_hasher, class_key_comparer> interned_;

	pin<TpInt64> tp_int64();
	pin<TpBool> tp_bool();
	pin<TpAtom> tp_atom();
	pin<TpVoid> tp_void();
	pin<TpDouble> tp_double();
	pin<TpOwn> tp_own(const pin<MakeInstance>& target);
	pin<TpWeak> tp_weak(const pin<MakeInstance>& target);
	pin<TpPin> tp_pin(const pin<MakeInstance>& target);
	pin<TpArray> tp_array(const pin<Type>& element);
	pin<MakeInstance> intern(const pin<MakeInstance>& cls);
	DECLARE_DOM_CLASS(Ast);
};

struct Module: dom::DomItem {
	size_t version;
	own<Name> name;
	vector<own<ClassDef>> classes;
	DECLARE_DOM_CLASS(Module);
};

struct AbstractClassDef: Node {
	DECLARE_DOM_CLASS(AbstractClassDef); // Not abstract to support strict_cast
};

struct ClassParamDef: AbstractClassDef {
	weak<AbstractClassDef> bound;
	own<Name> bound_name;
	own<Name> name;
	int index;  // 0-based index in class params list
	DECLARE_DOM_CLASS(ClassParamDef);
};

struct ClassTypeContext : ltm::Object {
	ClassTypeContext() { make_shared(); }
	weak<MakeInstance> context;
	own<ClassTypeContext> next;
	ClassTypeContext() {}
	ClassTypeContext(weak<MakeInstance> context, own<ClassTypeContext> next)
		: context(move(context))
		, next(move(next))
	{}
	LTM_COPYABLE(ClassTypeContext);
};

struct ClassDef: AbstractClassDef {
	bool is_interface; // TODO: replace with role(interface, mixin, class)
	vector<own<ClassParamDef>> type_params;
	vector<own<MakeInstance>> bases;
	vector<own<DataDef>> fields;
	vector<own<MethodDef>> methods;
	vector<own<OverrideDef>> overrides;
	unordered_map<own<Name>, weak<Node>> internals;  // all field and methods including inherited
	unordered_map<weak<Node>, own<ClassTypeContext>> internal_contexts;
	DECLARE_DOM_CLASS(ClassDef);
};

struct DataDef: Node {
	own<Name> name;
	own<Action> initializer; // for params it's type expression (see Action::type)
	DECLARE_DOM_CLASS(DataDef);
};

struct MethodDef: Node {
	vector<own<DataDef>> params;
	own<Action> body;
	own<Name> name;
	DECLARE_DOM_CLASS(MethodDef);
};

struct OverrideDef: Node {
	own<Name> atom;
	weak<MethodDef> method;
	own<Name> method_name;
	own<Action> body;
	DECLARE_DOM_CLASS(OverrideDef);
};

struct Block: Action {
	vector<own<Action>> body;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Block);
};

struct Loop: Block {  // Returns break result.
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Loop);
};

struct Local: Block {
	own<DataDef> var;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Local);
};

struct Break: Action {
	weak<Block> to_break;
	own<Action> result;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Break);
};

struct ConstInt64: Action {
	int64_t value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ConstInt64);
};

struct ConstBool: Action {
	bool value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ConstBool);
};

struct ConstDouble: Action {
	double value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ConstDouble);
};

struct ConstAtom: Action {
	own<Name> value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ConstAtom);
};

struct BinaryOp: Action {
	own<Action> p[2];
};
struct If: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(If);
};
struct Else: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Else);
};
struct AddOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(AddOp);
};
struct SubOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(SubOp);
};
struct MulOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(MulOp);
};
struct DivOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(DivOp);
};
struct ModOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ModOp);
};
struct ShlOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ShlOp);
};
struct ShrOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ShrOp);
};
struct AndOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(AndOp);
};
struct OrOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(OrOp);
};
struct XorOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(XorOp);
};
struct EqOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(EqOp);
};
struct LtOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(LtOp);
};
struct LogAndOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(LogAndOp);
};
struct LogOrOp: BinaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(LogOrOp);
};

struct UnaryOp: Action {
	own<Action> p;
};
struct NotOp: UnaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(NotOp);
};
struct ToIntOp: UnaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ToIntOp);
};
struct ToFloatOp: UnaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(ToFloatOp);
};
struct Own: UnaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Own);
};
struct Weak: UnaryOp {
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Weak);
};

struct DataRef: Action {
	weak<DataDef> var;
	own<Name> var_name;
};

struct GetVar: DataRef { // returns value type or pin(T)
	// if var==null refs to this
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(GetVar);
};

struct SetVar: DataRef {
	own<Action> value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(SetVar);
};

struct GetField: DataRef { // returns value type or pin(T).
	own<Action> base; // if base==null, refs to this.
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(GetField);
};

struct SetField: DataRef {
	own<Action> base;
	own<Action> value;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(SetField);
};

struct MakeInstance: Action {
	weak<AbstractClassDef> cls;
	own<Name> cls_name;
	vector<own<MakeInstance>> params;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(MakeInstance);
};

struct Array: Action {
	own<Action> size;
	vector<own<Action>> initializers;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Array);
};

struct Call: Action {
	own<Action> receiver;
	weak<MethodDef> method;
	own<Name> method_name;
	vector<own<Action>> params;
	void match(ActionMatcher& matcher) override;
	DECLARE_DOM_CLASS(Call);
};

struct ActionMatcher {
	virtual void on_unmatched(Action& node);
	virtual void on_bin_op(BinaryOp& node);
	virtual void on_un_op(UnaryOp& node);

	virtual void on_block(Block& node);
	virtual void on_loop(Loop& node);
	virtual void on_local(Local& node);
	virtual void on_break(Break& node);
	virtual void on_const_i64(ConstInt64& node);
	virtual void on_const_atom(ConstAtom& node);
	virtual void on_const_double(ConstDouble& node);
	virtual void on_const_bool(ConstBool& node);
	virtual void on_if(If& node);
	virtual void on_else(Else& node);
	virtual void on_add(AddOp& node);
	virtual void on_sub(SubOp& node);
	virtual void on_mul(MulOp& node);
	virtual void on_div(DivOp& node);
	virtual void on_mod(ModOp& node);
	virtual void on_and(AndOp& node);
	virtual void on_or(OrOp& node);
	virtual void on_xor(XorOp& node);
	virtual void on_shl(ShlOp& node);
	virtual void on_shr(ShrOp& node);
	virtual void on_eq(EqOp& node);
	virtual void on_lt(LtOp& node);
	virtual void on_log_and(LogAndOp& node);
	virtual void on_log_or(LogOrOp& node);
	virtual void on_not(NotOp& node);
	virtual void on_to_int(ToIntOp& node);
	virtual void on_to_float(ToFloatOp& node);
	virtual void on_own(Own& node);
	virtual void on_weak(Weak& node);
	virtual void on_get_var(GetVar& node);
	virtual void on_set_var(SetVar& node);
	virtual void on_get_field(GetField& node);
	virtual void on_set_field(SetField& node);
	virtual void on_make_instance(MakeInstance& node);
	virtual void on_array(Array& node);
	virtual void on_call(Call& node);
};

struct ActionScanner : ActionMatcher {
	own<Action>* fix_result = nullptr;
	void fix(own<Action>& ptr);
	void on_bin_op(BinaryOp& node) override;
	void on_un_op(UnaryOp& node) override;
	void on_block(Block& node) override;
	void on_loop(Loop& node) override;
	void on_local(Local& node) override;
	void on_break(Break& node) override;
	void on_set_var(SetVar& node) override;
	void on_get_field(GetField& node) override;
	void on_set_field(SetField& node) override;
	void on_make_instance(MakeInstance& node) override;
	void on_array(Array& node) override;
	void on_call(Call& node) override;
};

void initialize();

} // namespace ast

namespace std {

std::ostream& operator<< (std::ostream& dst, ast::Node* n);
inline std::ostream& operator<< (std::ostream& dst, const ltm::pin<ast::Node>& n) { return dst << n.get(); }

}

#endif // AST_H
