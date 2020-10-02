#include "type-checker.h"

// #include <unordered_set>
// #include <unordered_map>
// #include <list>
// #include <utility>

namespace {

// using std::unordered_set;
// using std::unordered_map;
// using std::list;
// using std::pair;
// using std::function;
// using std::vector;
// using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::strict_cast;
// using dom::Name;
// using ast::Node;

auto type_in_progress = own<ast::Type>::make();
auto no_return = own<ast::Type>::make();

struct Typer : ast::ActionMatcher {
	Typer(ltm::pin<ast::Ast> ast) : ast(ast) {}
	void on_block(ast::Block& node) override {
		for (auto& n : node.body)
			find_type(n);
		unite(node, node.body.empty() ? nullptr : node.body.back());
	}
	void on_loop(ast::Loop& node) override {
		for (auto& n : node.body)
			find_type(n);
		unite(node, no_return);
	}
	void on_local(ast::Local& node) override {
		find_type(node.var->initializer);
		on_block(node);
	}
	void on_break(ast::Break& node) override {
		if (node.result)
			find_type(node.result);
		unite(*node.to_break.pinned(), node.result);
		unite(node, no_return);
	}
	void on_const_i64(ast::ConstInt64& node) override { unite(node, ast->tp_int64()); }
	void on_const_atom(ast::ConstAtom& node) override { unite(node, ast->tp_atom()); }
	void on_const_double(ast::ConstDouble& node) override { unite(node, ast->tp_double()); }
	void on_const_bool(ast::ConstBool& node) override { unite(node, ast->tp_bool()); }
	void on_if(ast::If& node) override {
		expect_type(find_type(node.p[0]), ast->tp_bool());
		find_type(node.p[1]);
		unite(node, ast->tp_void());
	}
	void on_else(ast::Else& node) override {
		find_type(node.p[0]);
		find_type(node.p[1]);
		if (auto p0 = dom::strict_cast<ast::If>(node.p[0])) {
			unite(node, p0->p[1]);
			unite(node, node.p[1]);
		} else
			node.error("optional type is not suppored yet");
	}
	void on_int_or_double_op(ast::BinaryOp& node) {
		auto& t0 = find_type(node.p[0])->min_type;
		if (dom::strict_cast<ast::TpInt64>(t0)) {}
		else if (dom::strict_cast<ast::TpDouble>(t0)) {}
		else
			node.p[0]->error("expected int or double");
		expect_type(find_type(node.p[1]), t0);
		unite(node, node.p[1]);
	}
	void on_int_op(ast::BinaryOp& node) {
		unite(node, ast->tp_int64());
		expect_type(find_type(node.p[0]), node.min_type);
		expect_type(find_type(node.p[1]), node.min_type);
	}
	void on_add(ast::AddOp& node) override { on_int_or_double_op(node); }
	void on_sub(ast::SubOp& node) override { on_int_or_double_op(node); }
	void on_mul(ast::MulOp& node) override { on_int_or_double_op(node); }
	void on_div(ast::DivOp& node) override { on_int_or_double_op(node); }
	void on_mod(ast::ModOp& node) override { on_int_op(node); }
	void on_and(ast::AndOp& node) override { on_int_op(node); }
	void on_or(ast::OrOp& node) override { on_int_op(node); }
	void on_xor(ast::XorOp& node) override { on_int_op(node); }
	void on_shl(ast::ShlOp& node) override { on_int_op(node); }
	void on_shr(ast::ShrOp& node) override { on_int_op(node); }
	void on_eq(ast::EqOp& node) override {
		unite(node, ast->tp_bool());
		expect_type(find_type(node.p[0]), find_type(node.p[0])->wild_type);
	}
	void on_lt(ast::LtOp& node) override { on_int_or_double_op(node); }
	void on_log_and(ast::LogAndOp& node) override {
		unite(node, ast->tp_bool());
		expect_type(find_type(node.p[0]), node.min_type);
		expect_type(find_type(node.p[1]), node.min_type);
	}
	void on_log_or(ast::LogOrOp& node) override {
		unite(node, ast->tp_bool());
		expect_type(find_type(node.p[0]), node.min_type);
		expect_type(find_type(node.p[1]), node.min_type);
	}
	void on_not(ast::NotOp& node) override {
		unite(node, ast->tp_bool());
		expect_type(find_type(node.p), node.min_type);
	}
	void on_to_int(ast::ToIntOp& node) override {
		unite(node, ast->tp_int64());
		expect_type(find_type(node.p), ast->tp_double());
	}
	void on_to_float(ast::ToFloatOp& node) override {
		unite(node, ast->tp_double());
		expect_type(find_type(node.p), ast->tp_int64());
	}
	void on_own(ast::Own& node) override {
		if (auto a = dom::strict_cast<ast::TpPin>(find_type(node.p)->min_type))
			unite(node, ast->tp_own(a->cls), ast->tp_own(node.p->wild_type.cast<ast::TpPin>()->cls));
		else
			node.p->error("expected pin pointer");
	}
	void on_weak(ast::Weak& node) override {
		if (auto a = dom::strict_cast<ast::TpPin>(find_type(node.p)))
			unite(node, ast->tp_weak(a->cls), ast->tp_weak(node.p->wild_type.cast<ast::TpPin>()->cls));
		else
			node.p->error("expected pin pointer");
	}
	pin<ast::MakeInstance> get_class(pin<ast::Type> t) {
		if (auto as_pin = strict_cast<ast::TpWeak>(t))
			return as_pin->cls;
		if (auto as_own = strict_cast<ast::TpOwn>(t))
			return as_own->cls;
		if (auto as_weak = strict_cast<ast::TpWeak>(t))
			return as_weak->cls;
		return nullptr;
	}

	void on_get_var(ast::GetVar& node) override {
		node.type = remove_own_and_weak(find_type(node.var->initializer)->type);
	}
	void on_set_var(ast::SetVar& node) override {
		auto var_type = remove_own_and_weak(find_type(node.var->initializer)->type);
		check_assignable(var_type, find_type(node.value)->type);
	}
	void on_get_field(ast::GetField& node) override {
		if (!node.var) {
			if (!node.base)
				node.error("internal error: unresolved field");
			if (auto as_pin = strict_cast<ast::TpPin>(remove_own_and_weak(node.base->type))) {
				if (auto as_type_param = strict_cast<ast::ClassParamDef>(as_pin->cls->cls)) {
					if (auto bound = strict_cast<ast::ClassDef>(as_type_param->bound)) {
						///
					}
				}
			}
		}
	}
	void on_set_field(ast::SetField& node) override {}
	void on_make_instance(ast::MakeInstance& node) override {}
	void on_array(ast::Array& node) override {}
	void on_call(ast::Call& node) override {}

	own<ast::Action>& find_type(own<ast::Action>& node) {
		if (node->type) {
			if (node->type == type_in_progress)
				node->error("cannot deduce type due circular references"); // TODO: switch to Hindley-Milner++
			return;
		}
		node->type = type_in_progress;
		node->match(*this);
		return node;
	}

	void expect_type(pin<ast::Action> node, pin<ast::Type> type) {
		if (node->type != type)
			node->error("expected type", type);
	}

	void unite(ast::Action& a, pin<ast::Type> min_type, pin<ast::Type> wild_type = nullptr) {
		if (!wild_type)
			wild_type = min_type;
		if (a.min_type == min_type && a.wild_type == wild_type)
			return;
		if (a.min_type == type_in_progress) {
			a.min_type = min_type;
			a.wild_type = wild_type;
			return;
		}
		// todo: find common base type if unque
		a.error("incompatible types");
	}

	void unite(ast::Action& a, pin<ast::Action> b) {
		if (b)
			unite(a, b->min_type, b->wild_type);
		else
			unite(a, ast->tp_void());
	}

	pin<ast::Type> resolve_type_in_context() {
	}
	pin<ast::MakeInstance> resolve_class_instance_in_context(pin<ast::MakeInstance> src, pin<ast::TypeContext> context) {
		while (auto as_param = strict_cast<ast::ClassParamDef>(src->cls)) {
			if (!src->params.empty())
				src->error("class parameter cannot be parameterized");
			if (!context)
				src->error("internal error, parameterized type not in context");
			if (as_param->index >= context->context->params.size())
				src->error("internal error, class parameters count dont match");
			src = context->context->params[as_param->index];
			context = context->next;
		}
		auto r = ast::make_at_location<ast::MakeInstance>(*src);
		r->cls = src->cls;
		for (auto& p : src->params) {
			r->params.push_back(resolve_class_instance_in_context(p, context));
		}
		return ast->intern(r);
	}
	void fill_context(pin<ast::ClassDef> dst, const own<ast::ClassDef>& c, pin<ast::TypeContext> context) {
		if (!c->internals_contexts.empty())
			return;
		for (auto f : c->fields) {
			if (dst->internals[f->name] == f)
				dst->internals_contexts[f] = context;
		}
		for (auto& m : c->methods) {
			if (dst->internals[m->name] == m) {
				dst->internals_contexts[m] = context;
			}
		}
		for (auto& b : c->bases)
			fill_context(
				b->cls.cast<ast::ClassDef>(),
				dst,
				pin<ast::TypeContext>::make(b, context));
	}
	void process() {
		for (auto& m : ast->modules) {
			for (auto& c : m->classes) {
				fill_context(c, c, nullptr);
			}
		}
		for (auto& m : ast->modules) {
			for (auto& c : m->classes) {
				for (auto& f: c->fields)
					find_type(f->initializer);
				for (auto& m : c->methods) {
					for (auto& p : m->params)
						find_type(p->initializer);
					find_type(m->body);
				}
				for (auto& m : c->overrides)
					find_type(m->body);
			}
		}
	}
	pin<ast::Ast> ast;
};

/*
	compiling class C having params CP# with binds CPB#
		we encounter MakeInstance D with factual params DF# referencing class X
			class X has params XP# with binds XPB#
			and member M with declared type T with factual param TF# referencing XP#
	- Since our type parameterization is generic-based, not template-based, we stataicaly compile all class X contents as if all XP# is really XPB# (and the same is true for CP# and CPB#).
	- There only place we take parameters in consideration is calculating the factual type TF based on declaration T of M in context X instantiated by D with parameters DF#.
	- In this context we assume that all CP# are statically bounded to CPB# and thus static.
	- This calculation requires: T(it stores TF# inside, that reference XP# if needed), and D as a container of DF#.
	- To calculate TF we map TF#: t
		t is a ref to XP#, it is replaced with DF# with the same index.

	Inheritance adds complexity:


	global MakeInstance
	source MakeInstance, that needs to be typed (S): class_or_param (classes_or_params)
	context
*/

}  // namespace

void check_types(ltm::pin<ast::Ast> ast) {
	Typer(ast).process();
}
