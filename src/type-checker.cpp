#include <algorithm>
#include <vector>
#include <cassert>

#include "type-checker.h"

namespace {

	using std::vector;
using ltm::own;
using ltm::pin;
using ltm::weak;
using dom::strict_cast;
using ast::Type;

auto type_in_progress = own<ast::TpInt64>::make();

struct Typer : ast::ActionMatcher {
	Typer(ltm::pin<ast::Ast> ast)
		: ast(ast)
	{
		tp_bool = ast->tp_optional(ast->tp_void());
	}

	void on_int_op(ast::BinaryOp& node) {
		node.type_ = ast->tp_int64();
		expect_type(find_type(node.p[0]), node.type());
		expect_type(find_type(node.p[1]), node.type());
	}

	void on_int_or_double_op(ast::BinaryOp& node) {
		auto& t0 = find_type(node.p[0])->type();
		if (!dom::strict_cast<ast::TpInt64>(t0) && !dom::strict_cast<ast::TpDouble>(t0))
			node.p[0]->error("expected int or double");
		node.type_ = t0;
		expect_type(find_type(node.p[1]), t0);
	}

	void on_const_i64(ast::ConstInt64& node) override { node.type_ = ast->tp_int64(); }
	void on_const_double(ast::ConstDouble& node) override { node.type_ = ast->tp_double(); }
	void on_const_void(ast::ConstVoid& node) override { node.type_ = ast->tp_void(); }
	void on_const_bool (ast::ConstBool& node) override { node.type_ = tp_bool; }
	void on_mk_lambda(ast::MkLambda& node) override {
		auto r = pin<ast::TpColdLambda>::make();
		r->callees.push_back(weak<ast::MkLambda>(&node));
		node.type_ = r;

	}
	void on_block(ast::Block& node) override {
		if (node.body.empty()) {
			node.type_ = ast->tp_void();
			return;
		}
		for (auto& l : node.names) {
			if (l->initializer)
				l->type = find_type(l->initializer)->type();
		}
		for (auto& a : node.body)
			find_type(a);
		if (auto ret_as_get = dom::strict_cast<ast::Get>(node.body.back())) {
			if (std::find(node.names.begin(), node.names.end(), ret_as_get->var) != node.names.end()) {
				node.type_ = ret_as_get->var->type;
				return;
			}
		}
		node.type_ = node.body.back()->type();
	}
	void type_call(ast::Action& node, pin<ast::Action> callee, const vector<pin<ast::Action>>& actual_params) {
		pin<Type> callee_type = callee->type();
		if (auto as_fn = dom::strict_cast<ast::TpFunction>(callee_type)) {
			if (as_fn->params.size() - 1 != actual_params.size())
				node.error("Mismatched params count: expected ", as_fn->params.size() - 1, " provided ", actual_params.size(), " see function definition:", *callee);
			for (size_t i = 0; i < actual_params.size(); i++)
				expect_type(actual_params[i], as_fn->params[i]);
			node.type_ = as_fn->params.back();
		} else if (auto as_lambda = dom::strict_cast<ast::TpLambda>(callee_type)) {
			if (as_lambda->params.size() - 1 != actual_params.size())
				node.error("Mismatched params count: expected ", as_lambda->params.size() - 1, " provided ", actual_params.size(), " see function definition:", *callee);
			for (size_t i = 0; i < actual_params.size(); i++)
				expect_type(actual_params[i], as_lambda->params[i]);
			node.type_ = as_lambda->params.back();
		} else if (auto as_cold = dom::strict_cast<ast::TpColdLambda>(callee_type)) {
			own<ast::TpLambda> lambda_type;
			for (auto& weak_fn : as_cold->callees) {
				auto fn = weak_fn.pinned();
				if (fn->names.size() != actual_params.size())
					node.error("Mismatched params count: expected ", fn->names.size(), " provided ", actual_params.size(), " see function definition:", *fn);
				if (!lambda_type) {
					vector<own<ast::Type>> param_types;
					auto fn_param = fn->names.begin();
					for (auto& p : actual_params) {
						param_types.push_back(p->type());
						(*fn_param++)->type = param_types.back();
					}
					// TODO: if lambda marked with @: special mode signaling that get tpCls, should spawn tpCls, notRef
					for (auto& p : fn->body)
						find_type(p);
					auto& fn_result = find_type(fn->body.back())->type();
					param_types.push_back(fn_result);
					if (dom::strict_cast<ast::TpColdLambda>(fn_result) || dom::strict_cast<ast::TpLambda>(fn_result))
						fn->error("So far functions cannot return lambdas");
					lambda_type = ast->tp_lambda(move(param_types));
					as_cold->resolved = lambda_type;
					callee->type_ = lambda_type;
				} else {
					for (size_t i = 0; i < fn->names.size(); i++)
						fn->names[i]->type = lambda_type->params[i];
					// TODO: if lambda marked with @: special mode signaling that get tpCls, should spawn tpCls, notRef
					for (auto& p : fn->body)
						find_type(p);
					expect_type(fn->body.back(), lambda_type->params.back());
				}
			}
			node.type_ = lambda_type->params.back();
		} else {
			node.error(callee_type, " is not callable");
		}
	}
	void on_call(ast::Call& node) override {
		vector<pin<ast::Action>> params;
		for (auto& p : node.params)
			params.push_back(find_type(p));
		type_call(node, find_type(node.callee), params);
	}
	void handle_index_op(ast::GetAtIndex& node, own<ast::Action> opt_value, const char* name) {
		auto indexed = ast->extract_class(find_type(node.indexed)->type());
		if (!indexed)
			node.error("Only objects can be indexed, not ", node.indexed->type());
		auto fn = ast->functions_by_names[indexed->name->get(name)].pinned();
		if (!fn)
			node.error("function ", indexed->name->get(name), " not found");
		auto fnref = ast::make_at_location<ast::MakeFnPtr>(node);
		fnref->fn = fn;
		auto r = ast::make_at_location<ast::Call>(node).owned();
		r->callee = fnref;
		r->params = move(node.indexes);
		r->params.insert(r->params.begin(), move(node.indexed));
		if (opt_value)
			r->params.push_back(move(opt_value));
		fix(r);
		*fix_result = move(r);
	}
	void on_get_at_index(ast::GetAtIndex& node) override { handle_index_op(node, nullptr, "getAt"); }
	void on_set_at_index(ast::SetAtIndex& node) override { handle_index_op(node, move(node.value), "setAt"); }
	pin<ast::Function> type_fn(pin<ast::Function> fn) {
		if (!fn->type_) {
			vector<own<ast::Type>> params;
			for (size_t i = dom::strict_cast<ast::Method>(fn) ? 1 : 0; i < fn->names.size(); i++) {
				auto& p = fn->names[i];
				if (!p->type)
					p->type = find_type(p->initializer)->type();
				params.push_back(p->type);
			}
			params.push_back(find_type(fn->type_expression)->type());
			fn->type_ = dom::strict_cast<ast::Method>(fn)
				? ast->tp_lambda(move(params))  // TODO: introduce TpDelegate
				: ast->tp_function(move(params));
		}
		return fn;
	}
	void on_make_delegate(ast::MakeDelegate& node) override {
		node.type_ = type_fn(node.method)->type_;
	}
	void on_to_int(ast::ToIntOp& node) override {
		node.type_ = ast->tp_int64();
		expect_type(find_type(node.p), ast->tp_double());
	}
	void on_copy(ast::CopyOp& node) override {
		auto param_type = find_type(node.p)->type();
		if (auto param_as_ref = dom::strict_cast<ast::TpRef>(param_type))
			node.type_ = param_as_ref->target;
		else
			node.error("copy parameter should be a reference, not ", param_type);
	}
	void on_to_float(ast::ToFloatOp& node) override {
		node.type_ = ast->tp_double();
		expect_type(find_type(node.p), ast->tp_int64());
	}
	void on_not(ast::NotOp& node) override {
		node.type_ = tp_bool;
		expect_type(find_type(node.p), tp_bool);
	}
	void on_neg(ast::NegOp& node) override {
		auto& t = find_type(node.p)->type();
		if (!dom::strict_cast<ast::TpInt64>(t) && !dom::strict_cast<ast::TpDouble>(t))
			node.p->error("expected int or double");
		node.type_ = t;
	}
	void on_ref(ast::RefOp& node) override {
		auto& as_class = dom::strict_cast<ast::TpClass>(find_type(node.p)->type());
		if (!as_class)
			node.p->error("expected own class or interface, not ", node.p->type());
		node.type_ = ast->get_ref(as_class);
	}
	void on_loop(ast::Loop& node) override {
		find_type(node.p);
		if (auto as_opt = dom::strict_cast<ast::TpOptional>(node.p->type())) {
			node.type_ = ast->get_wrapped(as_opt);
		} else {
			node.error("loop body returned ", node.p->type().pinned(), ", that is not bool or optional");
		}
	}
	void on_get(ast::Get& node) override {
		if (auto as_class = dom::strict_cast<ast::TpClass>(node.var->type)) {
			// TODO in ret of fn result it still returns tpCls
			node.type_ = ast->get_ref(as_class);
		} else {
			node.type_ = node.var->type;
		}
	}
	void on_set(ast::Set& node) override {
		auto value_type = find_type(node.val)->type();
		auto& variable_type = node.var->type;
		if (auto value_as_class = dom::strict_cast<ast::TpClass>(value_type)) {
			node.type_ = ast->get_ref(value_as_class);
			auto variable_as_class = dom::strict_cast<ast::TpClass>(variable_type);
			if (!variable_as_class)
				value_type = ast->get_ref(value_as_class);
		} else {
			node.type_ = value_type;
			if (dom::strict_cast<ast::TpRef>(value_type)) {
				if (auto variable_as_class = dom::strict_cast<ast::TpClass>(variable_type))
					variable_type = ast->get_ref(variable_as_class);
			}
		}
		expect_type(node, value_type, variable_type);
	}
	void on_mk_instance(ast::MkInstance& node) override {
		node.type_ = node.cls;
	}
	void on_make_fn_ptr(ast::MakeFnPtr& node) override {
		node.type_ = node.fn->type();
	}
	void on_mk_weak(ast::MkWeakOp& node) override {
		if (auto as_ref = dom::strict_cast<ast::TpRef>(find_type(node.p)->type())) {
			node.type_ = ast->get_weak(as_ref->target);
			return;
		}
		if (auto as_mk_instance = dom::strict_cast<ast::MkInstance>(node.p)) {
			node.type_ = ast->get_weak(as_mk_instance->cls);
			return;
		}
		node.p->error("Expected &ClassName or expression returning reference, not expr of type ", node.p->type().pinned());
	}
	pin<ast::TpClass> class_from_action(own<ast::Action>& node) {
		if (auto cls = ast->extract_class(find_type(node)->type()))
			return cls;
		node->error("Expected pointer to class, not ", node->type().pinned());
	}
	void on_get_field(ast::GetField& node) override {
		if (!node.field) {
			auto cls = class_from_action(node.base);
			if (!cls->handle_member(node, node.field_name,
				[&](auto field) { node.field = field; },
				[&](auto method) {
					auto r = ast::make_at_location<ast::MakeDelegate>(node);
					r->base = move(node.base);
					r->method = method;
					*fix_result = r;
					find_type(*fix_result);
				}, [&] { node.error("field name is ambigiuous"); }))
				node.error("class ", cls->name.pinned(), " doesn't have field ", node.field_name);
		}
		if (&node == fix_result->pinned()) {
			node.type_ = node.field->initializer->type();
			if (auto as_class = dom::strict_cast<ast::TpClass>(node.type())) {
				node.type_ = ast->get_ref(as_class);
			}
		}
	}
	void on_set_field(ast::SetField& node) override {
		auto cls = class_from_action(node.base);
		if (!node.field) {
			if (!cls->handle_member(node, node.field_name,
				[&](auto field) { node.field = field; },
				[&](auto method) { node.error("Cannot assign to method"); },
				[&] { node.error("field name is ambiguous"); }))
				node.error("class ", cls->name.pinned(), " doesn't have field ", node.field_name);
		}
		node.type_ = node.field->initializer->type();
		expect_type(find_type(node.val), node.field->initializer->type());
	}
	void on_cast(ast::CastOp& node) override {
		auto src_cls = class_from_action(node.p[0]);
		auto dst_cls = class_from_action(node.p[1]);
		node.type_ = dom::strict_cast<ast::TpClass>(node.p[0]->type())
			? (pin<ast::Type>) dst_cls
			: ast->get_ref(dst_cls);
		if (src_cls->overloads.count(dst_cls))  // no-op conversion
			node.p[1] = nullptr;
		else
			node.type_ = ast->tp_optional(node.type_);
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
	void on_lt(ast::LtOp& node) override {
		on_int_or_double_op(node);
		node.type_ = tp_bool;
	}
	void on_eq(ast::EqOp& node) override {
		auto& t0 = find_type(node.p[0])->type();
		node.type_ = tp_bool;
		expect_type(find_type(node.p[1]), t0);;
	}
	pin<ast::TpOptional> handle_condition(own<ast::Action>& action) {
		auto cond = dom::strict_cast<ast::TpOptional>(find_type(action)->type());
		if (cond)
			return cond;
		if (auto as_weak = dom::strict_cast<ast::TpWeak>(action->type())) {
			auto deref = ast::make_at_location<ast::DerefWeakOp>(*action);
			deref->p = move(action);
			deref->type_ = cond = ast->tp_optional(ast->get_ref(as_weak->target));
			action = deref;
			return cond;
		}
		action->error("expected optional, weak or bool type");
	}
	void on_if(ast::If& node) override {
		auto cond = handle_condition(node.p[0]);
		if (auto as_block = dom::strict_cast<ast::Block>(node.p[1])) {
			if (!as_block->names.empty() && !as_block->names.front()->initializer) {
				as_block->names.front()->type = ast->get_wrapped(cond);
			}
		}
		node.type_ = ast->tp_optional(find_type(node.p[1])->type());
	}
	void on_land(ast::LAnd& node) override {
		auto cond = handle_condition(node.p[0]);
		if (auto as_block = dom::strict_cast<ast::Block>(node.p[1])) {
			if (!as_block->names.empty() && !as_block->names.front()->initializer) {
				as_block->names.front()->type = ast->get_wrapped(cond);
			}
		}
		node.type_ = find_type(node.p[1])->type();
	}

	void on_else(ast::Else& node) override {
		node.type_ = ast->get_wrapped(handle_condition(node.p[0]));
		expect_type(find_type(node.p[1]), node.type_);
	}

	void on_lor(ast::LOr& node) override {
		node.type_ = handle_condition(node.p[0]);
		expect_type(find_type(node.p[1]), node.type_);
	}

	own<ast::Action>& find_type(own<ast::Action>& node) {
		if (auto type = node->type()) {
			if (type == type_in_progress)
				node->error("cannot deduce type due circular references"); // TODO: switch to Hindley-Milner++
			return node;
		}
		node->type_ = type_in_progress;
		fix(node);
		return node;
	}

	void unify(pin<ast::TpColdLambda> cold, pin<ast::TpLambda> lambda, ast::Action& node) {
		cold->resolved = lambda;
		for (auto& w_fn : cold->callees) {
			auto fn = w_fn.pinned();
			if (fn->names.size() != lambda->params.size() - 1)
				node.error("Mismatched params count: expected ", fn->names.size(), " provided ", lambda->params.size() - 1, " see function definition:", *fn);
			for (size_t i = 0; i < fn->names.size(); i++)
				fn->names[i]->type = lambda->params[i];
			for (auto& a : fn->body)
				find_type(a);
			expect_type(fn->body.back(), lambda->params.back());
		}
	}
	void expect_type(pin<ast::Action> node, pin<ast::Type> expected_type) {
		expect_type(*node, node->type(), expected_type);
	}
	void expect_type(ast::Action& node, pin<ast::Type> actual_type, pin<ast::Type> expected_type) {
		if (expected_type == ast->tp_void())
			return;
		if (actual_type == expected_type)
			return;
		if (auto exp_as_ref = dom::strict_cast<ast::TpRef>(expected_type)) {
			if (auto actual_class = ast->extract_class(actual_type)) {
				if (actual_class->overloads.count(exp_as_ref->target))
					return;
			}
		} else if (auto exp_as_class = dom::strict_cast<ast::TpClass>(expected_type)) {
			if (auto actual_class = dom::strict_cast<ast::TpClass>(actual_type)) {
				if (actual_class->overloads.count(exp_as_class))
					return;
			}
		} else if (auto exp_as_weak = dom::strict_cast<ast::TpWeak>(expected_type)) {
			if (auto actual_as_weak = dom::strict_cast<ast::TpWeak>(actual_type)) {
				if (actual_as_weak->target->overloads.count(exp_as_weak->target))
					return;
			}
		} else if (auto act_as_cold = dom::strict_cast<ast::TpColdLambda>(actual_type)) {
			if (auto exp_as_cold = dom::strict_cast<ast::TpColdLambda>(expected_type)) {
				assert(!act_as_cold->resolved);  // node->type() skips all resolved cold levels
				act_as_cold->resolved = exp_as_cold;
				for (auto& fn : act_as_cold->callees)
					exp_as_cold->callees.push_back(move(fn));
				act_as_cold->callees.clear();
				return;
			} else if (auto& exp_as_lambda = dom::strict_cast<ast::TpLambda>(expected_type)) {
				unify(act_as_cold, exp_as_lambda, node);
				return;
			}
		} else if (auto act_as_lambda = dom::strict_cast<ast::TpLambda>(actual_type)) {
			if (auto exp_as_cold = dom::strict_cast<ast::TpColdLambda>(expected_type)) {
				unify(exp_as_cold, act_as_lambda, node);
				return;
			}
		}
		node.error("Expected type: ", expected_type, " not ", actual_type);
	}

	void process_method(own<ast::Method>& m) {
		m->names[0]->type = find_type(m->names[0]->initializer)->type();
		for (auto& a : m->body)
			find_type(a);
		if (!m->body.empty())  // empty == interface method
			expect_type(m->body.back(), m->type_expression->type());
		if (m->base)
			expect_type(m, m->base->type());
	}

	void process() {
		for (auto& c : ast->classes) {
			this_class = c;
			for (auto& m : c->new_methods)
				type_fn(m);
			for (auto& b : c->overloads)
				for (auto& m : b.second)
					type_fn(m);
		}

		for (auto& fn : ast->functions)
			type_fn(fn);
		for (auto& c : ast->classes) {
			this_class = c;
			for (auto& f : c->fields) {
				find_type(f->initializer);
				if (dom::strict_cast<ast::TpRef>(f->initializer->type())) {
					f->error("Fields cannot be temp-references. Make it @own or &weak");
				}
			}
			for (auto& m : c->new_methods)
				process_method(m);
			for (auto& b : c->overloads)
				for (auto& m : b.second)
					process_method(m);
		}
		for (auto& fn : ast->functions) {
			for (auto& a : fn->body)
				find_type(a);
			if (!fn->is_platform)
				expect_type(fn->body.back(), fn->type_expression->type());
		}
		expect_type(find_type(ast->entry_point), ast->tp_lambda({ ast->tp_int64() }));
	}

	pin<ast::Ast> ast;
	pin<Type> tp_bool;
	pin<ast::TpClass> this_class;
};

}  // namespace

void check_types(ltm::pin<ast::Ast> ast) {
	Typer(ast).process();
}
