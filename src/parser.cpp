#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include "parser.h"

namespace {

using std::string;
using std::unordered_map;
using std::vector;
using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::Name;
using ast::Module;
using ast::Ast;
using ast::Node;
using ast::Action;
using module_text_provider_t = const std::function<string (pin<Name> name)>&;

template<typename FN>
struct Guard{
	FN fn;
	Guard(FN fn) : fn(std::move(fn)) {}
	~Guard() { fn(); }
};
template<typename FN>
Guard<FN> mk_guard(FN fn) { return Guard<FN>(std::move(fn)); }

struct Parser {
	pin<dom::Dom> dom;
	pin<Ast> ast;
	string text;
	pin<Name> module_name;
	int line = 0, pos = 0;
	const char* cur = nullptr;
	pin<Module> module;
	pin<ast::Block> block_for_return_statement;
	pin<ast::Block> block_for_break_statement;
	pin<ast::Block> block_for_continue_statement;

	Parser(pin<Ast> ast, pin<Name> module_name)
		: dom(ast::static_dom)
		, ast(ast)
		, module_name(module_name)
	{}

	pin<Module> parse(
		vector<pin<Name>>& active_modules,
		module_text_provider_t module_text_provider)
	{
		module = new Module();
		dom->set_name(module, module_name);
		module->name = module_name;
		active_modules.push_back(module_name);
		auto guard = mk_guard([&]{ active_modules.pop_back(); });
		text = module_text_provider(module_name);
		cur = text.c_str();
		match_ws();
		expect("module");
		if (expect_domain_name(dom->names(), "module name") != module_name) {
			error("module name mismatch file path");
		}
		expect(".");
		module->version = expect_number("module version");
		if (match("{")) {
			do {
				auto import_name = expect_domain_name(dom->names(), "import name");
				expect(".");
				auto import_version = expect_number("import version");
				for (auto i = active_modules.begin(); i != active_modules.end(); ++i) {
					if (*i == import_name) {
						std::stringstream message;
						message << "curcular dependencies at module " << import_name << std::endl << "path:";
						for (; i != active_modules.end(); ++i)
							message << *i << std::endl;
						error(message.str());
					}
				}
				auto module = dom::strict_cast<Module>(dom->get_named(import_name));
				if (!module) {
					module = Parser(ast, import_name).parse(active_modules, module_text_provider);
				}
				if (module->version < import_version)
					error("outdated module version:" + std::to_string(module->version));
			} while (match(","));
			expect("}");
		}
		while (!is_eof()) {
			expect("class");
			auto class_name = module_name->get(expect_id("class"));
			auto cls = make<ast::ClassDef>();
			dom->set_name(cls, class_name);
			module->classes.push_back(cls);
			if (match("(")) {
				do {
					auto param = make<ast::ClassParamDef>();
					cls->type_params.push_back(param);
					param->name = module_name->get(expect_id("type parameter"));
					if (match(":"))
						param->bound_name = expect_domain_name(module_name, "class parameter base");
				} while(match(","));
				expect(")");
			}
			if (match(":")) {
				do
					cls->bases.push_back(parse_class_ref("expected base class name"));
				while(match(","));
			}
			expect("{");
			while (!match("}")) {
				if (match("+")) {
					auto method = make<ast::MethodDef>();
					cls->methods.push_back(method);
					method->name = module_name->get(expect_id("method"));
					if (match("(") && !match(")")) {
						do {
							auto param = make<ast::DataDef>();
							method->params.push_back(param);
							auto name = expect_id("parameter");
							param->name = dom->names()->get(name);
							expect(":");
							param->initializer = parse_expression();
						} while (match(","));
						expect(")");
					}
					method->body = parse_statement(&block_for_return_statement);
				} else {
					auto id = expect_id("field or override name");
					if (match("=")) {
						auto field = make<ast::DataDef>();
						cls->fields.push_back(field);
						field->name = module_name->get(id);
						field->initializer = parse_expression();
						expect(";");
					} else {
						auto ovr = make<ast::OverrideDef>();
						ovr->method_name = match_domain_name_tail(id, "override name");
						if (!ovr->method_name)
							ovr->method_name = dom->names()->get(id);
						if (match("."))
							ovr->atom = expect_domain_name(dom->names(), "selector");
						ovr->body = parse_statement(&block_for_return_statement);
					}
				}
			}
		}
		ast->modules.push_back(module);
		return module;
	}

	pin<ast::MakeInstance> parse_class_ref(string message) {
		auto r = make<ast::MakeInstance>();
		r->cls_name = expect_domain_name(module_name, message.c_str());
		if (match("(")) {
			while(!match(")"))
				r->params.push_back(parse_class_ref(message + " parameter"));
		}
		return r;
	}

	pin<ast::Action> parse_statement(pin<ast::Block>* special_block = nullptr, bool in_block = false) {
		if (match("{")) {
			auto r = make<ast::Block>();
			auto prev = *special_block;
			if (special_block)
				*special_block = r;
			while (!match("}"))
				r->body.push_back(parse_statement(nullptr, true));
			*special_block = prev;
			return r;
		}
		if (match("if")) {
			auto condition = parse_expression();
			auto r = fill(make<ast::If>(), condition, parse_statement());
			if (match("else"))
				return fill(make<ast::Else>(), r, parse_statement());
			return r;
		}
		if (match("while")) {
			auto cond = parse_expression();
			auto r = make<ast::Loop>();
			auto brk = make<ast::Break>();
			auto prev = block_for_break_statement;
			block_for_break_statement = r;
			r->body.push_back(
				fill(make<ast::Else>(),
					fill(make<ast::If>(), cond, parse_statement(&block_for_continue_statement)),
					brk));
			brk->to_break = r;
			block_for_break_statement = prev;
			return r;
		}
		if (match("return")) {
			auto r = make<ast::Break>();
			r->to_break = block_for_return_statement;
			if (!match(";")) {
				r->result = parse_expression();
				expect(";");
			}
			return r;
		}
		if (match("break")) {
			if (!block_for_break_statement)
				error("break outlide of loop");
			auto r = make<ast::Break>();
			r->to_break = block_for_break_statement;
			expect(";");
			return r;
		}
		if (match("continue")) {
			if (!block_for_break_statement)
				error("continue outlide of loop with {body}");
			auto r = make<ast::Break>();
			r->to_break = block_for_continue_statement;
			expect(";");
			return r;
		}
		// todo: if a=expr action
		// todo: :label for... break:label expression
		auto lead = parse_expression();
		auto lead_as_id = dom::strict_cast<ast::GetVar>(lead);
		if (lead_as_id && match_a_and_not_b('=', '=')) {
			if (in_block)
				error("local cannot be declared outside {block}");
			auto local = make<ast::Local>();
			auto var = make<ast::DataDef>();
			local->var = var;
			var->name = lead_as_id->var_name;
			var->initializer = parse_expression();
			expect(";");
			while (!peek('}'))
				local->body.push_back(parse_statement());
			return local;
		}
		expect(";");
		return lead;
	}

	pin<Action> parse_expression() {
		return parse_elses();
	}

	pin<Action> parse_elses() {
		auto r = parse_ifs();
		return match("\\") ? fill(make<ast::Else>(), r, parse_elses()) : r;
	}

	pin<Action> parse_ifs() {
		auto r = parse_ors();
		return match("?") ? fill(make<ast::If>(), r, parse_elses()) : r;
	}

	pin<Action> parse_ors() {
		auto r = parse_ands();
		while (match("||")) {
			r = fill(make<ast::LogOrOp>(), r, parse_ands());
		}
		return r;
	}

	pin<Action> parse_ands() {
		auto r = parse_comparisons();
		while (match("&&")) {
			r = fill(make<ast::LogAndOp>(), r, parse_comparisons());
		}
		return r;
	}

	pin<Action> parse_comparisons() {
		auto r = parse_adds();
		if (match("==")) {
			r = fill(make<ast::EqOp>(), r, parse_adds());
		} else if (match("!=")) {
			r = fill(make<ast::NotOp>(), fill(make<ast::EqOp>(), r, parse_adds()));
		} else if (match("<=")) {
			r = fill(make<ast::NotOp>(), fill(make<ast::LtOp>(), parse_adds(), r));
		} else if (match(">=")) {
			r = fill(make<ast::NotOp>(), fill(make<ast::LtOp>(), r, parse_adds()));
		} else if (match("<")) {
			r = fill(make<ast::LtOp>(), r, parse_adds());
		} else if (match(">")) {
			r = fill(make<ast::LtOp>(), parse_adds(), r);
		}
		return r;
	}

	pin<Action> parse_adds() {
		auto r = parse_muls();
		for (;;) {
			if (match("+")) {
				r = fill(make<ast::AddOp>(), r, parse_muls());
			} else if (match("-")) {
				r = fill(make<ast::SubOp>(), r, parse_muls());
			} else {
				break;
			}
		}
		return r;
	}

	pin<Action> parse_muls() {
		auto r = parse_un();
		for (;;) {
			if (match("*")) {
				r = fill(make<ast::MulOp>(), r, parse_un());
			} else if (match("/")) {
				r = fill(make<ast::DivOp>(), r, parse_un());
			} else if (match("%")) {
				r = fill(make<ast::ModOp>(), r, parse_un());
			} else if (match("<<")) {
				r = fill(make<ast::ShlOp>(), r, parse_un());
			} else if (match("^")) {
				r = fill(make<ast::XorOp>(), r, parse_un());
			} else if (match(">>")) {
				r = fill(make<ast::ShrOp>(), r, parse_un());
			} else if (match_a_and_not_b('&', '&')) {
				r = fill(make<ast::AndOp>(), r, parse_un());
			} else if (match_a_and_not_b('|', '|')) {
				r = fill(make<ast::OrOp>(), r, parse_un());
			} else {
				break;
			}
		}
		return r;
	}

	pin<Action> parse_un() {
		auto r = parse_un_head();
		for (;;) {
			auto n = parse_un_tail(r);
			if (n)
				r = n;
			else
				return r;
		}
	}

	// b ; own and pin to pin, weak and opt own and opt pin to optional pin
	// &b ; all to optional weak (weak is always optional)
	// *b ; pin and own - copy to temp own, temp own - move, optional(C) - src?*_
	// copy(C) as *C but temp own gets copied
	// only temp_own can be assigned to own. so use (a := *b) or (a := copy(b))
	pin<Action> parse_un_head() {
		uint64_t num;
		string name;
		if (match("(")) {
			auto r = parse_expression();
			expect(")");
			return r;
		} else if (match("-")) {
			return fill(make<ast::MulOp>(), parse_un(), mk_const<ast::ConstInt64>(-1));
		} else if (match("~")) {
			return fill(make<ast::XorOp>(), parse_un(), mk_const<ast::ConstInt64>(-1));
		} else if (match("!")) {
			return fill(make<ast::NotOp>(), parse_un());
		} else if (match_num(num)) {
			return mk_const<ast::ConstInt64>(num);
		} else if (match_ns(".")) {
			return mk_const<ast::ConstAtom>(expect_domain_name(dom->names(), "atom"));
		} else if (match_id(name)) {
			if (auto domain_name = match_domain_name_tail(name, "name")) {
				auto r = make<ast::GetVar>();
				r->var_name = domain_name;
				return r;
			}
			if (name == "true")
				return mk_const<ast::ConstBool>(true);
			if (name == "false")
				return mk_const<ast::ConstBool>(false);
			auto r = make<ast::GetVar>();
			r->var_name = dom->names()->get(name);
			return r;
		} else if (match("&")) {
			return fill(make<ast::Weak>(), parse_un());
		} else if (match("*")) {
			return fill(make<ast::Own>(), parse_un());
		} else if (match("{")) {
			auto r = make<ast::Block>();
			while (!match("}"))
				r->body.push_back(parse_statement());
			return r;
		} else if (match("[")) {
			auto r = make<ast::Array>();
			do 
				r->initializers.push_back(parse_expression());
			while (match(","));
			expect("]");
			if (match("*"))
				r->size = parse_un();
			return r;
		}
		error("expected expression");
		return nullptr;
	}

	template<typename T, typename VT>
	pin<Action> mk_const(VT&& v) {
		auto r = pin<T>::make();
		r->value = v;
		return r;
	}
	pin<Action> parse_un_tail(pin<Action> head) {
		if (match("(")) {
			if (auto as_get_var = dom::strict_cast<ast::GetVar>(head)) {
				// this can be class_name(type params) or this_method_name(call params)
				auto r = pin<ast::Call>();
				r->method_name = as_get_var->var_name;
				if (!match(")")) {
					do
						r->params.push_back(parse_expression());
					while (match(","));
					expect(")");
				}
				return r;
			} else
				error("expect method name or class to instantiate");
		} else if (match("[")) {
			auto r = make<ast::Call>();
			r->receiver = head;
			do
				r->params.push_back(parse_expression());
			while (match(","));
			expect("]");
			if (match(":=")) { // TODO: { _=base; _#=param#; b[i] _[_#]:= _b[_#] op x; }
				r->method_name = dom->names()->get("set_at");
				r->params.push_back(parse_expression());
			} else {
				r->method_name = dom->names()->get("get_at");
			}
			return r;
		} else if (match(".")) {
			auto field_name = expect_domain_name(dom->names(), "field");
			if (match("(")) {
				auto r = pin<ast::Call>();
				r->receiver = head;
				r->method_name = field_name;
				if (!match(")")) {
					do
						r->params.push_back(parse_expression());
					while (match(","));
					expect(")");
				}
				return r;
			} else if (match(":=")) {
				auto r = make<ast::SetField>();
				r->base = head;
				r->var_name = field_name;
				r->value = parse_expression();
				return r;
			} else if (auto op = match_bin_op()) {  // {_=base; _.f :=_.f op x;}
				auto local = make<ast::Local>();
				auto var = make<ast::DataDef>();
				local->var = var;
				var->name = dom->names()->get("_");
				var->initializer = head;
				auto setter = make<ast::SetField>();
				setter->var_name = field_name;
				setter->base = make<ast::GetVar>();
				setter->base.cast<ast::GetVar>()->var = var;
				setter->value = op;
				op = setter->value.cast<ast::BinaryOp>();
				auto getter = make<ast::GetField>();
				getter->base = make<ast::GetVar>();
				getter->base.cast<ast::GetVar>()->var = var;
				getter->var_name = field_name;
				op->p[0] = getter;
				op->p[1] = parse_expression();
				local->body.push_back(setter);
				return local;
			} else {
				auto r = make<ast::GetField>();
				r->base = head;
				r->var_name = field_name;
				return r;
			}
		} else if (match(":=")) {
			if (auto as_get_var = dom::strict_cast<ast::GetVar>(head)) {
				auto r = make<ast::SetVar>();
				r->var_name = as_get_var->var_name;
				r->var = as_get_var->var;
				r->value = parse_expression();
				return r;
			} else
				error("only variable can be assigned");
		} else if (auto op = match_bin_op()) {
			if (auto as_get_var = dom::strict_cast<ast::GetVar>(head)) {
				auto setter = make<ast::SetVar>();
				setter->var_name = as_get_var->var_name;
				setter->var = as_get_var->var;
				setter->value = op;
				fill(setter->value.cast<ast::BinaryOp>(), head, parse_expression());
				return setter;
			} else
				error("only variable can be modified");
		}
		return nullptr;
	}

	pin<ast::BinaryOp> match_bin_op() {
		if (match("+="))
			return make<ast::AddOp>();
		if (match("-="))
			return make<ast::SubOp>();
		if (match("*="))
			return make<ast::MulOp>();
		if (match("/="))
			return make<ast::DivOp>();
		if (match("%="))
			return make<ast::ModOp>();
		if (match("|="))
			return make<ast::OrOp>();
		if (match("&="))
			return make<ast::AndOp>();
		if (match("^="))
			return make<ast::XorOp>();
		if (match("<<="))
			return make<ast::ShlOp>();
		if (match(">>="))
			return make<ast::ShrOp>();
		return nullptr;
	}

	template<typename T>
	pin<T> make() {
		auto r = pin<T>::make();
		r->line = line;
		r->pos = pos;
		r->module = module;
		return r;
	}

	pin<ast::Action> fill(pin<ast::UnaryOp> op, pin<ast::Action> param) {
		op->p = param;
		return op;
	}

	pin<ast::Action> fill(pin<ast::BinaryOp> op, pin<ast::Action> param1, pin<ast::Action> param2) {
		op->p[0] = param1;
		op->p[1] = param2;
		return op;
	}

	pin<Name> match_domain_name_tail(string id, const char* message) {
		if (!match("~"))
			return nullptr;
		pin<Name> r = dom->names()->get(id)->get(expect_id(message));
		while (match("~")){
			string name_val = expect_id(message);
			r = r->get(name_val);
		}
		return r;
	}

	pin<Name> expect_domain_name(pin<Name> default_domain, const char* message) {
		auto id = expect_id(message);
		auto name = match_domain_name_tail(id, message);
		return name ? name : default_domain->get(id);
	}

	uint64_t expect_number(const char* message) {
		uint64_t r = 0;
		if (!match_num(r))
			error("expected number");
		return r;
	}
	void expect(char* str) {
		if (!match(str))
			error(string("expected '") + str + "'");
	}

	bool match_ns(char* str) {
		int i = 0;
		for (; str[i]; i++) {
			if (str[i] != cur[i])
				return false;
		}
		if (is_alpha_num(str[i - 1]) && is_alpha_num(cur[i]))
			return false;
		cur += i;
		pos += i;
		return true;
	}

	bool match(char* str) {
		if (match_ns(str)) {
			match_ws();
			return true;
		}
		return false;
	}

	bool peek(char c) { return *cur == c; }

	bool match_a_and_not_b(char a, char b) {
		if (*cur == a && cur[1] != b) {
			cur++;
			pos++;
			match_ws();
			return true;
		}
		return false;
	}

	bool match_ws() {
		const char* c = cur;
		for (;; line++, pos = 0) {
			while (*cur == ' ') {
				++cur;
				++pos;
			}
			if (*cur == '\t') {
				error("tabs aren't allowed as white space");
			}
			if (*cur == '/' && cur[1] == '/') {
				while (*cur && *cur != '\n' && *cur != '\r') {
					++cur;
				}
			}
			if (*cur == '\n') {
				if (*++cur == '\r')
					++cur;
			}
			else if (*cur == '\r') {
				if (*++cur == '\n')
					++cur;
			}
			else if (!*cur || *cur > ' ')
				return c != cur;
		}
	}

	static bool is_alpha(char c) {
		return
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			c == '_';
	};

	static bool is_num(char c) {
		return c >= '0' && c <= '9';
	};

	static int get_digit(char c) {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return 255;
	}

	static bool is_alpha_num(char c) {
		return is_alpha(c) || is_num(c);
	};

	bool match_id(string& result) {
		if (!is_alpha(*cur))
			return false;
		while (is_alpha_num(*cur)) {
			result += *cur++;
			++pos;
		}
		match_ws();
		return true;
	}

	string expect_id(const char* message) {
		string r;
		if (!match_id(r)) {
			error(string("expected") + message);
		}
		return r;
	}

	bool match_num(uint64_t& result) {
		if (!is_num(*cur))
			return false;
		int radix = 10;
		if (*cur == '0') {
			cur++;
			switch (*cur) {
			case 'x': radix = 16; break;
			case 'o': radix = 8; break;
			case 'b': radix = 2; break;
			default: cur--; break;
			}
			cur++;
		}
		result = 0;
		for (;; cur++, pos++) {
			if (*cur == '_')
				continue;
			int digit = get_digit(*cur);
			if (digit == 255)
				break;
			if (digit >= radix)
				error("bad symbols in number");
			uint64_t next = result * radix + digit;
			if (next / radix != result)
				error("overflow");
			result = next;
		}
		match_ws();
		return true;
	}

	void error(const string &message) {
		auto name = dom->get_name(module);
		std::cerr
			<< "error:" << message
			<< " at " << (name ? std::to_string(name) : "<???>")
			<< ":" << line + 1
			<< ":" << pos + 1 << std::endl;
		throw 1;
	}

	bool is_eof() {
		return !*cur;
	}
};

}  // namespace

void parse(
	pin<Ast> ast,
	pin<Name> module_name,
	module_text_provider_t module_text_provider)
{
	vector<pin<Name>> active_modules;
	Parser(ast, module_name).parse(active_modules, module_text_provider);
}
