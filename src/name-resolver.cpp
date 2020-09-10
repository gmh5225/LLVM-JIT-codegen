#include "name-resolver.h"

#include <unordered_set>
#include <unordered_map>
#include <list>
#include <utility>

namespace {

using std::unordered_set;
using std::unordered_map;
using std::list;
using std::pair;
using std::function;
using std::vector;
using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::Name;
using ast::Node;

struct Solver : ltm::Object {
	virtual pin<Node> resolve(Node* node, pin<Name> name) =0;
	virtual ~Solver() {}
};

struct ClassParamsSolver : Solver {
	pin<ast::ClassDef> cls;
	ClassParamsSolver(pin<ast::ClassDef> cls) : cls(cls) {}

	pin<Node> resolve(Node* node, pin<Name> name_to_lookup) override {
		pin<Node> r;
		auto set_result = [&] (pin<Node> target) {
			if (!r) {
				r = target;
				return;
			}
			node->error("Name redefinition ", name_to_lookup, " occurence1: ", r, " occurence2:", target);
		};
		if (auto name = name_to_lookup->domain && name_to_lookup->domain->domain
			? name_to_lookup
			: cls->module->name->peek(name_to_lookup->name)) {
			for (auto& p : cls->type_params) {
				if (p->name == name)
					set_result(p);
			}
		}
		return r;
	}
	LTM_COPYABLE(ClassParamsSolver);
};

struct ClassSolver : Solver {
	pin<ast::ClassDef> cls;
	ClassSolver(pin<ast::ClassDef> cls) : cls(cls) {}

	pin<Node> resolve(Node* node, pin<Name> name_to_lookup) override {
		pin<Node> r;
		auto set_result = [&] (pin<Node> target) {
			if (!r) {
				r = target;
				return;
			}
			node->error("Name redefinition ", name_to_lookup, " occurence1: ", r, " occurence2:", target);
		};
		function<void(pin<ast::ClassDef> cls)> scan = [&] (pin<ast::ClassDef> cls) {
			if (auto name = name_to_lookup->domain && name_to_lookup->domain->domain
				? name_to_lookup
				: cls->module->name->peek(name_to_lookup->name))
			{
				for (auto& f : cls->fields) {
					if (f->name == name)
						set_result(f);
				}
				for (auto& m : cls->methods) {
					if (m->name == name)
						set_result(m);
				}
				if (r)
					return;
			}
			for (auto& base : cls->bases) {
				if (auto b = dom::strict_cast<ast::ClassDef>(base->cls)) {
					scan(b);
				}
			}
		};
		scan(cls);
		return r;
	}
	LTM_COPYABLE(ClassSolver);
};

struct NameResolver : ast::ActionScanner {
	unordered_set<pin<ast::ClassDef>> handled_classes;
	list<own<Solver>> solvers;
	pin<ast::Module> current_module;
	pin<dom::Dom> dom;

	pin<dom::DomItem> resolve(Node* location, pin<Name> name) {
		for (auto i = solvers.rbegin(); i != solvers.rend(); ++i) {
			if (auto r = (*i)->resolve(location, name))
				return r;
		}
		if (auto n = name->domain && name->domain->domain
			? name
			: current_module->name->peek(name->name)) {
			if (auto r = dom->get_named(n))
				return r;
		}
		location->error("unresolved name ", name);
		return nullptr;
	}

	template<typename T>
	void resolve(weak<T>& dst, pin<Name> name, pin<Node> location) {
		if (dst)
			return;
		auto result = resolve(location.get(), name);
		if (auto val = dom::strict_cast<T>(result))
			dst = val;
		else
			location->error(
				"type mismatch, expected ", T::dom_type_->get_name(),
				" actual:", dom::Dom::get_type(result)->get_name());
	}

	void process_class(pin<ast::ClassDef> c) {
		if (handled_classes.find(c) != handled_classes.end())
			return;
		handled_classes.insert(c);
		for (auto& p : c->type_params)
			resolve(p->bound, p->bound_name, p);
		solvers.push_back(new ClassParamsSolver(c));
		for (auto& b : c->bases)
			process_cls_ref(b);
		solvers.push_back(new ClassSolver(c));
		for (auto& f : c->fields)
			f->initializer->match(*this);
		for (auto& m : c->methods) {
			for (auto& p : m->params)
				p->initializer->match(*this);
			m->body->match(*this);
		}
		for (auto& ovr : c->overrides) {
			resolve(ovr->method, ovr->method_name, ovr);
			ovr->body->match(*this);
		}
		solvers.pop_back();
		solvers.pop_back();
	}
	void process_cls_ref(const pin<ast::MakeInstance>& cls_def) {
		resolve(cls_def->cls, cls_def->cls_name, cls_def);
		for (auto& p : cls_def->params)
			process_cls_ref(p);
	}
	void on_get_var(ast::GetVar& node) override {
		if (node.var)
			return; // already resolved to local
		if (!node.var_name)
			return; // special `this` case
		auto val = resolve(&node, node.var_name);
		if (auto as_class = dom::strict_cast<ast::ClassDef>(val)) {
			if (!as_class->type_params.empty())
				node.error("type instantiation requires parameters");
			auto r = make_at_location<ast::MakeInstance>(node);
			r->cls_name = node.var_name;
			r->cls = as_class;
			*fix_result = r;
		} else if (auto as_field = dom::strict_cast<ast::DataDef>(val)) {
			auto r = make_at_location<ast::GetField>(node);
			r->var_name = node.var_name;
			r->var = as_field;
			*fix_result = r;
		} else {
			node.error("unexpected reference to ", dom::Dom::get_type(val)->get_name(), " while expected this field or class name");
		}
	}
	void on_set_var(ast::SetVar& node) override {
		if (node.var)
			return; // already resolved to local
		if (!node.var_name)
			node.error("`this` cannot be set");
		auto val = resolve(&node, node.var_name);
		if (auto as_class = dom::strict_cast<ast::ClassDef>(val))
			node.error("class name cannot be assigned");
		else if (auto as_field = dom::strict_cast<ast::DataDef>(val)) {
			auto r = make_at_location<ast::SetField>(node);
			r->var_name = node.var_name;
			r->var = as_field;
			fix(node.value);
			r->value = move(node.value);
			*fix_result = r;
		} else {
			node.error("unexpected reference to ", dom::Dom::get_type(val)->get_name(), " while expected this field or class name");
		}
	}
	// We can fix get_field/set_field only at the type checker phase.
	void on_call(ast::Call& node) override {
		ast::ActionScanner::on_call(node);
		if (!node.receiver)
			return;
		auto val = resolve(&node, node.method_name);
		if (auto as_class = dom::strict_cast<ast::ClassDef>(val)) {
			if (as_class->type_params.size() != node.params.size())
				node.error("type should have ", as_class->type_params.size(), " parameters");
			auto r = make_at_location<ast::MakeInstance>(node);
			r->cls_name = node.method_name;
			r->cls = as_class;
			for (auto& p : node.params) {
				if (!dom::strict_cast<ast::MakeInstance>(p))
					node.error("expected type name");
				r->params.push_back(move(p.cast<ast::MakeInstance>()));
			}
			*fix_result = r;
		}
	}
	template<typename T>
	pin<T> make_at_location(Node& src) {
		auto r = pin<T>::make();
		r->line = src.line;
		r->pos = src.pos;
		r->module = src.module;
		return r;
	}
	void process(ltm::pin<ast::Ast> ast) {
		for (auto& m : ast->modules) {
			current_module = m;
			for (auto& c : m->classes) {
				process_class(c);
			}
		}
	}
};

}  // namespace

void resolve_names(ltm::pin<ast::Ast> ast) {
	(NameResolver()).process(ast);
}
