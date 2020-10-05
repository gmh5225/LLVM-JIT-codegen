#include "name-resolver.h"

#include <unordered_set>
#include <unordered_map>
#include <list>
#include <utility>

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
using ast::ClassDef;

namespace {

pin<Name> make_global(const pin<Name>& name, const pin<ast::Module>& module) {
	return name->domain && name->domain->domain
		? name
		: module->name->peek(name->name);
}

struct NameResolver : ast::ActionScanner {
	unordered_map<pin<Name>, pin<Node>> locals;
	pin<ast::ClassDef> cls;
	pin<dom::Dom> dom;
	pin<ast::Module> cls_module;

	NameResolver(pin<dom::Dom> dom)
		: dom(dom)
	{}

	pin<dom::DomItem> resolve(Node* location, pin<Name> name) {
		auto it = locals.find(name);
		if (it != locals.end())
			return it->second;
		name = make_global(name, cls_module);
		if (cls) {
			if (auto r = resolve_class_member(cls, name, location))
				return r;
		}
		if (auto r = dom->get_named(name))
			return r;
		location->error("unresolved name ", name);
		return nullptr;
	}

	template<typename T>
	void resolve(weak<T>& dst, pin<Name> name, pin<Node> location) {
		if (dst || !name)
			return;
		auto result = resolve(location.get(), name);
		if (auto val = dom::strict_cast<T>(result))
			dst = val;
		else
			location->error(
				"type mismatch, expected ", T::dom_type_->get_name(),
				" actual:", dom::Dom::get_type(result)->get_name());
	}

	void fix_with_params(const vector<own<ast::DataDef>>& params, own<ast::Action>& body) {
		vector<pair<pin<Name>, pin<Node>>> prev;
		prev.reserve(params.size());
		for (auto& p : params) {
			auto& dst = locals[p->name];
			prev.push_back({ p->name, dst });
			dst = p;
		}
		fix(body);
		for (auto& p : prev) {
			if (p.second)
				locals[p.first] = p.second;
			else
				locals.erase(p.first);
		}
	}

	void process_class(pin<ast::ClassDef> c) {
		cls_module = c->module;
		for (auto& p : c->type_params)
			resolve(p->bound, p->bound_name, p);
		for (auto& p : c->type_params)
			locals.insert({ p->name, p });
		for (auto& b : c->bases)
			process_cls_ref(b);
		cls = c;
		for (auto& f : c->fields)
			fix(f->initializer);
		for (auto& m : c->methods) {
			for (auto& p : m->params)
				fix(p->initializer);
			fix_with_params(m->params, m->body);
		}
		for (auto& ovr : c->overrides) {
			resolve(ovr->method, ovr->method_name, ovr);
			fix_with_params(ovr->method->params, ovr->body);
		}
	}
	void process_cls_ref(const pin<ast::MakeInstance>& cls_def) {
		resolve(cls_def->cls, cls_def->cls_name, cls_def);
		for (auto& p : cls_def->params)
			process_cls_ref(p);
	}
	void on_local(ast::Local& node) {
		auto& dst = locals[node.var->name];
		auto prev = dst;
		dst = node.var;
		ActionScanner::on_block(node);
		if (prev)
			locals[node.var->name] = prev;
		else
			locals.erase(node.var->name);
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
		} else if (auto as_data = dom::strict_cast<ast::DataDef>(val)) {
			if (as_data->name->domain && as_data->name->domain->domain) {
				auto r = make_at_location<ast::GetField>(node);
				r->var_name = node.var_name;
				r->var = as_data;
				*fix_result = r;
			} else {
				node.var = as_data;
			}
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
		else if (auto as_data = dom::strict_cast<ast::DataDef>(val)) {
			if (as_data->name->domain && as_data->name->domain->domain) {
				auto r = make_at_location<ast::SetField>(node);
				r->var_name = node.var_name;
				r->var = as_data;
				fix(node.value);
				r->value = move(node.value);
				*fix_result = r;
			} else {
				node.var = as_data;
			}
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
};

struct ClassMembersResolver {
	pin<ast::Ast> ast;
	unordered_set<weak<ClassDef>> processed_classes;
	unordered_set<weak<ClassDef>> classes_under_processing;

	void set_unique(weak<Node>& dst, const own<Node>& src) {
		if (dst)
			src->error("duplicated member in class, see ", dst);
		dst = src;
	}
	void handle_classes() {
		for (auto& m : ast->modules) {
			for (auto& c : m->classes) {
				for (auto& f : c->fields)
					set_unique(c->internals[f->name], f);
				for (auto& m : c->methods)
					set_unique(c->internals[m->name], m);
			}
		}
		for (auto& m : ast->modules) {
			for (auto& c : m->classes)
				handle_bases(c);
		}
	}
	void handle_bases(const pin<ClassDef>& c) {
		if (processed_classes.find(c) != processed_classes.end())
			return;
		if (classes_under_processing.find(c) != classes_under_processing.end())
			c->error("class inherites itself");
		classes_under_processing.insert(c);
		pin<ClassDef> base_class;
		for (auto& b : c->bases) {
			auto name = make_global(b->cls_name, c->module);
			if (auto base_class = ast::static_dom->get_named(name)) {
				if (auto bc = dom::strict_cast<ClassDef>(base_class)) {
					if (!bc->is_interface) {
						if (base_class)
							bc->error("class has more than one base class ", base_class);
						base_class = bc;
					}
					handle_bases(bc);
					for (auto& i : bc->internals) {
						auto dst = c->internals[i.first];
						if (!dst)
							dst = i.second;
					}
				} else
					b->error("base class name ", name, " refers to ", ast::static_dom->get_type(bc)->get_name());
			} else
				b->error("unresolved name ", name);
		}
		classes_under_processing.erase(c);
		processed_classes.insert(c);
	};
};

}  // namespace

void resolve_names(pin<ast::Ast> ast) {
	ClassMembersResolver{ ast }.handle_classes();
	for (auto& m : ast->modules) {
		for (auto& c : m->classes)
			NameResolver{ ast::static_dom }.process_class(c);
	}
}

pin<Node> resolve_class_member(pin<ClassDef> cls, pin<Name> name, pin<Node> location) {
	auto it = cls->internals.find(make_global(name, cls->module));
	if (it == cls->internals.end())
		location->error("name ", name, "not found in class", ast::static_dom->get_name(cls));
	return it->second;
}
