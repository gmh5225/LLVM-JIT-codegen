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
// using dom::Name;
// using ast::Node;

auto type_in_progress = own<ast::Type>::make();
auto no_return = own<ast::Type>::make();

struct Typer : ast::ActionMatcher {
	Typer(ltm::pin<ast::Ast> ast) : ast(ast) {}
	void on_block(ast::Block& node) override {
		for (auto& n : node.body)
			find_type(n);
		unite(node, node.body.empty()
			? ast->tp_void()
			: node.body.back()->type);
	}
	void on_loop(ast::Loop& node) override {
		for (auto& n : node.body)
			find_type(n);
		if (node.type == type_in_progress)
			node.type = no_return;
	}
	void on_local(ast::Local& node) override {
		find_type(node.var->initializer);
		for (auto& n : node.body)
			find_type(n);
		node.type = node.body.empty()
			? ast->tp_void()
			: node.body.back()->type;
	}
	void on_break(ast::Break& node) override {
		if (node.result)
			find_type(node.result);
		unite(*node.to_break.pinned(), node.result ? node.result->type : ast->tp_void());
		node.type = no_return;
	}
	void on_const_i64(ast::ConstInt64& node) override { node.type = ast->tp_int64(); }
	void on_const_atom(ast::ConstAtom& node) override { node.type = ast->tp_atom(); }
	void on_const_double(ast::ConstDouble& node) override { node.type = ast->tp_double(); }
	void on_const_bool(ast::ConstBool& node) override { node.type = ast->tp_bool(); }
	void on_if(ast::If& node) override {
		expect_type(find_type(node.p[0]), ast->tp_bool());
		find_type(node.p[1]);
		node.type = ast->tp_void();
	}
	void on_else(ast::Else& node) override {
		if (auto p0 = dom::strict_cast<ast::If>(node.p[0])) {
			expect_type(find_type(p0->p[0]), ast->tp_bool());
			node.type = p0->type = find_type(p0->p[1])->type;
			unite(node, find_type(node.p[1])->type);
		} else
			node.error("optional type is not suppored yet");
	}
	void on_int_or_double_op(ast::BinaryOp& node) {
		auto& t0 = find_type(node.p[0])->type;
		if (dom::strict_cast<ast::TpInt64>(t0)) {}
		else if (dom::strict_cast<ast::TpDouble>(t0)) {}
		else
			node.p[0]->error("expected int or double");
		expect_type(find_type(node.p[1]), t0);
		node.type = node.p[1]->type;
	}
	void on_int_op(ast::BinaryOp& node) {
		node.type = ast->tp_int64();
		expect_type(find_type(node.p[0]), node.type);
		expect_type(find_type(node.p[1]), node.type);
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
		expect_type(find_type(node.p[0]), find_type(node.p[0])->type);
		node.type = ast->tp_bool();
	}
	void on_lt(ast::LtOp& node) override { on_int_or_double_op(node); }
	void on_log_and(ast::LogAndOp& node) override {
		node.type = ast->tp_bool();
		expect_type(find_type(node.p[0]), node.type);
		expect_type(find_type(node.p[1]), node.type);
	}
	void on_log_or(ast::LogOrOp& node) override {
		node.type = ast->tp_bool();
		expect_type(find_type(node.p[0]), node.type);
		expect_type(find_type(node.p[1]), node.type);
	}
	void on_not(ast::NotOp& node) override {
		node.type = ast->tp_bool();
		expect_type(find_type(node.p), node.type);
	}
	void on_to_int(ast::ToIntOp& node) override {
		expect_type(find_type(node.p), ast->tp_double());
		node.type = ast->tp_int64();
	}
	void on_to_float(ast::ToFloatOp& node) override {
		expect_type(find_type(node.p), ast->tp_int64());
		node.type = ast->tp_double();
	}
	void on_own(ast::Own& node) override {
		if (auto a = dom::strict_cast<ast::TpPin>(find_type(node.p)))
			node.type = ast->tp_own(a->cls);
		else
			node.p->error("expected pin pointer");
	}
	void on_weak(ast::Weak& node) override {
		if (auto a = dom::strict_cast<ast::TpPin>(find_type(node.p)))
			node.type = ast->tp_weak(a->cls);
		else
			node.p->error("expected pin pointer");
	}
	void on_get_var(ast::GetVar& node) override {}
	void on_set_var(ast::SetVar& node) override {}
	void on_get_field(ast::GetField& node) override {}
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

	void unite(ast::Action& a, pin<ast::Type> b) {
		if (a.type == b)
			return;
		if (a.type == type_in_progress) {
			a.type = b;
			return;
		}
		// todo: find common base type if unque
		a.error("incompatible types");
	}

	void process() {
		for (auto& m : ast->modules) {
			for (auto& c : m->classes) {
				// TODO: build category
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

}  // namespace

void check_types(ltm::pin<ast::Ast> ast) {
	Typer(ast).process();
}
