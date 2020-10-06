#include "type-checker.h"

// #include <unordered_set>
// #include <unordered_map>
// #include <list>
// #include <utility>

namespace {

using std::unordered_set;
// using std::unordered_map;
// using std::list;
// using std::pair;
// using std::function;
using std::vector;
// using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::strict_cast;
// using dom::Name;
using ast::Type;

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
	pin<ast::Type> remove_own_and_weak(pin<ast::Type> t) {
		if (auto as_own = strict_cast<ast::TpOwn>(t))
			return ast->tp_pin(as_own->cls);
		if (auto as_weak = strict_cast<ast::TpWeak>(t))
			return ast->tp_pin(as_weak->cls);
		return t;
	}
	void on_get_var(ast::GetVar& node) override {
		node.type = remove_own_and_weak(find_type(node.var->initializer)->type);
	}
	void on_set_var(ast::SetVar& node) override {
		auto var_type = remove_own_and_weak(find_type(node.var->initializer)->type);
		check_assignable(var_type, find_type(node.value)->type);
	}
	void on_get_field(ast::GetField& node) override {
	
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
	struct TypeContextStripper : ast::TypeMatcher {
		pin<Type> r;
		pin<ast::Ast> ast;
		pin<ast::MakeInstance> context;

		void on_unmatched(Type& type) override { r = &type; }
		void on_own(ast::TpOwn& type) override { r = ast->tp_own(strip(type.cls)); }
		void on_weak(ast::TpWeak& type) override { r = ast->tp_weak(strip(type.cls)); }
		void on_pin(ast::TpPin& type) override { r = ast->tp_pin(strip(type.cls)); }
		void on_array(ast::TpArray& type) override {
			type.element->match(*this);
			r = ast->tp_array(r);
		}
		pin<ast::MakeInstance> strip(const own<ast::MakeInstance>& src) {
			if (src->params.empty()) {
				if (auto as_real = dom::strict_cast<ast::ClassDef>(src->cls))
					return src;
				if (auto as_param = dom::strict_cast<ast::ClassParamDef>(src->cls)) {
					if (context->params.size() < as_param->index)
						src->error("internal error - type parameter index is out of context");
					return context->params[as_param->index];
				} else
					src->error("internal error - unexpected ClassDef type");
			}
			auto r = ast::make_at_location<ast::MakeInstance>(*src);
			if (auto as_real = dom::strict_cast<ast::ClassDef>(src->cls))
				r->cls = as_real;
			else
				src->error("type parameter cannot have parameters");
			for (auto& p : src->params)
				r->params.push_back(strip(p));
			return ast->intern(r);
		}
		pin<Type> process(pin<ast::Ast> ast, pin<ast::MakeInstance> context, pin<Type> src) {
			ast = move(ast);
			context = move(context);
			src->match(*this);
			return r;
		}
	};
	void fill_member_types(
		const own<ast::ClassDef>& c,
		unordered_set<pin<ast::ClassDef>>& visited)
	{
		if (visited.find(c) != visited.end())
			return;
		visited.insert(c);
		for (auto& b : c->bases)
			fill_member_types(b->cls.cast<ast::ClassDef>(), visited);
		for (auto f : c->fields)
			c->internals_types[f].push_back(f->initializer->type);
		for (auto& m : c->methods) {
			auto& types = c->internals_types[m];
			types.push_back(m->body->type);
			for (auto& p : m->params)
				types.push_back(p->initializer->type);
		}
		for (auto& b : c->bases) {
			for (auto& m : b->cls.cast<ast::ClassDef>()->internals_types) {
				auto& types = c->internals_types[m.first];
				if (!types.empty())
					c->error("internal error, member redefinition, see ", m.first.pinned());
				for (auto& t : m.second)
					types.push_back(TypeContextStripper().process(ast, b, t));
			}
		}
	}
	void process() {
		unordered_set<pin<ast::ClassDef>> visited;
		for (auto& m : ast->modules) {
			for (auto& c : m->classes) {
				fill_member_types(c, visited);
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

}  // namespace

void check_types(ltm::pin<ast::Ast> ast) {
	Typer(ast).process();
}
