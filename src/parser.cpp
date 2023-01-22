#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <variant>
#include <optional>
#include <cfenv>
#include "parser.h"

namespace {

using std::string;
using std::unordered_map;
using std::vector;
using std::variant;
using std::optional;
using std::nullopt;
using std::get_if; 
using ltm::weak;
using ltm::own;
using ltm::pin;
using dom::Name;
using ast::Ast;
using ast::Node;
using ast::Action;
using ast::make_at_location;
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
	int line = 1, pos = 1;
	const char* cur = nullptr;

	Parser(pin<Ast> ast, pin<Name> module_name)
		: dom(ast->dom)
		, ast(ast)
		, module_name(module_name)
	{}

	void parse_fn_def(pin<ast::Function> fn) {
		if (match("(")) {
			while (!match(")")) {
				auto param = make<ast::Var>();
				fn->names.push_back(param);
				param->initializer = parse_type();
				param->name = ast->dom->names()->get(expect_id("parameter name"));
				if (match(")"))
					break;
				expect(",");
			}
		}
		if (match(";")) {
			fn->type_expression = make<ast::ConstVoid>();
			fn->is_platform = true;
			return;
		}
		if (match("{")) {
			fn->type_expression = make<ast::ConstVoid>();
		} else {
			fn->type_expression = parse_type();
			if (match(";")) {
				fn->is_platform = true;
				return;
			}
			expect("{");
		}
		parse_statement_sequence(fn->body);
		expect("}");
	}

	pin<ast::Method> make_method(pin<dom::Name> name, pin<ast::TpClass> cls, bool is_interface) {
		auto method = make<ast::Method>();
		method->name = name;
		auto this_param = make<ast::Var>();
		method->names.push_back(this_param);
		this_param->name = ast->dom->names()->get("this");
		auto this_init = make<ast::MkInstance>();
		this_init->cls = cls;
		this_param->initializer = this_init;
		parse_fn_def(method);
		if (is_interface != method->body.empty()) {
			error(is_interface ? "empty" : "not empty", " body expected");
		}
		return method;
	}

	void parse(module_text_provider_t module_text_provider)
	{
		text = module_text_provider(module_name);
		cur = text.c_str();
		match_ws();
		for (;;) {
			bool is_interface = match("interface");
			if (is_interface || match("class")) {
				auto cls = ast->get_class(expect_domain_name("class or interface name"));
				cls->is_interface = is_interface;
				expect("{");
				while (!match("}")) {
					if (match("+")) {
						auto& base_content = cls->overloads[ast->get_class(expect_domain_name("base class or interface"))];
						if (match("{")) {
							if (is_interface)
								error("interface can't have overrides");
							while (!match("}"))
								base_content.push_back(make_method(expect_domain_name("override method name"), cls, is_interface));
						} else {
							expect(";");
						}
					} else {
						auto member_name = expect_domain_name("method or field name");
						if (match("=")) {
							cls->fields.push_back(make<ast::Field>());
							cls->fields.back()->name = member_name;
							cls->fields.back()->initializer = parse_expression();
							expect(";");
						} else {
							cls->new_methods.push_back(make_method(member_name, cls, is_interface));
						}
					}
				}
			} else if (match("fn")) {
				auto fn = make<ast::Function>();
				fn->name = expect_domain_name("function name");
				auto& fn_ref = ast->functions_by_names[fn->name];
				if (fn_ref)
					error("duplicated function name, see", fn_ref.pinned());
				fn_ref = fn;
				ast->functions.push_back(fn);
				parse_fn_def(fn);
			} else {
				break;
			}
		}
		ast->entry_point = make<ast::Function>();
		parse_statement_sequence(ast->entry_point->body);
		if (*cur)
			error("unexpected statements");
	}

	void parse_statement_sequence(vector<own<Action>>& body) {
		do {
			if (*cur == '}') {
				body.push_back(make<ast::ConstVoid>());
				break;
			}
			body.push_back(parse_statement());
		} while (match(";"));
	}

	pin<Action> parse_type() {
		if (match("~"))
			return parse_type();
		if (match("int"))
			return mk_const<ast::ConstInt64>(0);
		if (match("double"))
			return mk_const<ast::ConstDouble>(0.0);
		if (match("bool"))
			return make<ast::ConstBool>();
		if (match("void"))
			return make<ast::ConstVoid>();
		if (match("?")) {
			auto r = make<ast::If>();
			r->p[0] = make<ast::ConstBool>();
			r->p[1] = parse_type();
			return r;
		}
		if (match("&")) {
			auto r = make<ast::MkWeakOp>();
			auto get = make<ast::Get>();
			get->var_name = expect_domain_name("class or interface name");
			r->p = get;
			return r;
		}
		if (match("@")) {
			auto get = make<ast::Get>();
			get->var_name = expect_domain_name("class or interface name");
			return get;
		}
		auto parse_params = [&](pin<ast::MkLambda> fn) {
			if (!match(")")) {
				for (;;) {
					fn->names.push_back(make<ast::Var>());
					fn->names.back()->initializer = parse_type();
					if (match(")"))
						break;
					expect(",");
				}
			}
			fn->body.push_back(parse_type());
			return fn;
		};
		if (match("fn")) {
			expect("(");
			return parse_params(make<ast::Function>());
		}
		if (match("("))
			return parse_params(make<ast::MkLambda>());
		if (auto name = match_domain_name("class or interface name")) {
			auto r = make<ast::RefOp>();
			auto get = make<ast::Get>();
			get->var_name = *name;
			r->p = get;
			return r;
		}
		// TODO &(T,T)T - delegate
		error("Expected type name");
	}

	pin<Action> parse_statement() {
		auto r = parse_expression();
		if (auto as_get = dom::strict_cast<ast::Get>(r)) {
			if (!as_get->var && match("=")) {
				if (as_get->var_name->domain != ast->dom->names())
					error("local var names should not contain '_'");
				auto block = make<ast::Block>();
				auto var = make_at_location<ast::Var>(*r);
				var->name = as_get->var_name;
				var->initializer = parse_expression();
				block->names.push_back(var);
				expect(";");
				parse_statement_sequence(block->body);
				return block;
			}
		}
		return r;
	}

	pin<Action> parse_expression() {
		return parse_elses();
	}

	pin<Action> parse_elses() {
		auto r = parse_ifs();
		for (;;) {
			if (match(":"))
				r = fill(make<ast::Else>(), r, parse_ifs());
			else if (match("||"))
				r = fill(make<ast::LOr>(), r, parse_ifs());
			else
				break;
		}
		return r;
	}

	pin<Action> parse_ifs() {
		auto r = parse_cond();
		bool is_and = match_ns("&&");
		if (is_and || match_ns("?")) {
			pin<ast::BinaryOp> iff;
			if (is_and)
				iff = make<ast::LAnd>();
			else
				iff = make<ast::If>();
			auto rhs = make<ast::Block>();
			rhs->names.push_back(make<ast::Var>());
			if (auto maybe_id = match_id()) {
				rhs->names.back()->name = ast->dom->names()->get(*maybe_id);
			} else {
				rhs->names.back()->name = ast->dom->names()->get("_");
				match_ws();
			}
			rhs->body.push_back(parse_ifs());
			r = fill(iff, r, rhs);
		}
		return r;
	}

	pin<Action> parse_cond() {
		auto r = parse_adds();
		if (match("==")) return fill(make<ast::EqOp>(), r, parse_adds());
		if (match("<")) return fill(make<ast::LtOp>(), r, parse_adds());
		if (match(">")) return fill(make<ast::LtOp>(), parse_adds(), r);
		if (match("!=")) return fill(make<ast::NotOp>(), fill(make<ast::EqOp>(), r, parse_adds()));
		if (match(">=")) return fill(make<ast::NotOp>(), fill(make<ast::LtOp>(), r, parse_adds()));
		if (match("<=")) return fill(make<ast::NotOp>(), fill(make<ast::LtOp>(), parse_adds(), r));
		return r;
	}

	pin<Action> parse_adds() {
		auto r = parse_muls();
		for (;;) {
			if (match("+")) r = fill(make<ast::AddOp>(), r, parse_muls());
			else if (match("-")) r = fill(make<ast::SubOp>(), r, parse_muls());
			else break;
		}
		return r;
	}

	pin<Action> parse_muls() {
		auto r = parse_unar();
		for (;;) {
			if (match("*")) r = fill(make<ast::MulOp>(), r, parse_unar());
			else if (match("/")) r = fill(make<ast::DivOp>(), r, parse_unar());
			else if (match("%")) r = fill(make<ast::ModOp>(), r, parse_unar());
			else if (match("<<")) r = fill(make<ast::ShlOp>(), r, parse_unar());
			else if (match(">>")) r = fill(make<ast::ShrOp>(), r, parse_unar());
			else if (match_and_not("&", '&')) r = fill(make<ast::AndOp>(), r, parse_unar());
			else if (match_and_not("|", '|')) r = fill(make<ast::OrOp>(), r, parse_unar());
			else if (match("^")) r = fill(make<ast::XorOp>(), r, parse_unar());
			else break;
		}
		return r;
	}

	pin<Action> parse_expression_in_parethesis() {
		expect("(");
		auto r = parse_expression();
		expect(")");
		return r;
	}
	pin<Action> parse_unar() {
		auto r = parse_unar_head();
		for (;;) {
			if (match("(")) {
				auto call = make<ast::Call>();
				call->callee = r;
				r = call;
				while (!match(")")) {
					call->params.push_back(parse_expression());
					if (match(")"))
						break;
					expect(",");
				}
			} else if (match("[")) {
				auto gi = make<ast::GetAtIndex>();
				do
					gi->indexes.push_back(parse_expression());
				while (match(","));
				expect("]");
				if (match(":=")) {
					auto si = make_at_location<ast::SetAtIndex>(*gi);
					si->indexes = move(gi->indexes);
					si->value = parse_expression();
					gi = si;
				}
				gi->indexed = r;
				r = gi;
			} else if (match(".")) {
				pin<ast::FieldRef> gf = make<ast::GetField>();
				gf->field_name = expect_domain_name("field name");
				if (match(":=")) {
					auto sf = make_at_location<ast::SetField>(*gf);
					sf->field_name = gf->field_name;
					sf->val = parse_expression();
					gf = sf;
				}
				gf->base = r;
				r = gf;
			} else if (match(":=")) {
				if (auto as_get = dom::strict_cast<ast::Get>(r)) {
					auto set = make<ast::Set>();
					set->var = as_get->var;
					set->var_name = as_get->var_name;
					set->val = parse_expression();
					r = set;
				} else {
					error("expected variable name in front of := operator");
				}
			} else if (match("~")) {
				r = fill(make<ast::CastOp>(), r, parse_unar_head());
			} else
				return r;
		}
	}
	pin<Action> parse_unar_head() {
		if (match("(")) {
			pin<Action> start_expr;
			auto lambda = make<ast::MkLambda>();
			if (!match(")")) {
				start_expr = parse_expression();
				while (!match(")")) {
					expect(",");
					lambda->names.push_back(make<ast::Var>());
					lambda->names.back()->name = ast->dom->names()->get(expect_id("parameter"));
				}
			}
			if (match("{")) {
				if (start_expr) {
					if (auto as_ref = dom::strict_cast<ast::Get>(start_expr)) {
						lambda->names.insert(lambda->names.begin(), make<ast::Var>());
						lambda->names.front()->name = as_ref->var_name;
					} else {
						start_expr->error("lambda definition requires parameter name");
					}
				}
				parse_statement_sequence(lambda->body);
				expect("}");
				return lambda;
			} else if (lambda->names.empty() && start_expr){
				return start_expr;
			}
			lambda->error("expected single expression in parentesis or lambda {body}");
		}
		if (match("@"))
			return fill(make<ast::CopyOp>(), parse_unar());
		if (match("&"))
			return fill(make<ast::MkWeakOp>(), parse_unar());
		if (match("!"))
			return fill(make<ast::NotOp>(), parse_unar());
		if (match("-"))
			return fill(make<ast::NegOp>(), parse_unar());
		if (match("~"))
			return fill(make<ast::MulOp>(),
				parse_unar(),
				mk_const<ast::ConstInt64>(-1));
		if (auto n = match_num()) {
			if (auto v = get_if<uint64_t>(&*n))
				return mk_const<ast::ConstInt64>(*v);
			if (auto v = get_if<double>(&*n))
				return mk_const<ast::ConstDouble>(*v);
		}
		if (match("{")) {
			auto r = make<ast::Block>();
			parse_statement_sequence(r->body);
			expect("}");
			return r;
		}
		bool matched_true = match("+");
		if (matched_true || match("?")) {
			auto r = make<ast::If>();
			auto cond = make<ast::ConstBool>();
			cond->value = matched_true;
			r->p[0] = cond;
			r->p[1] = parse_unar();
			return r;
		}
		matched_true = match("true");
		if (matched_true || match("false")) {
			auto r = make<ast::ConstBool>();
			r->value = matched_true;
			return r;
		}
		if (match("void"))
			return make<ast::ConstVoid>();
		if (match("int"))
			return fill(make<ast::ToIntOp>(), parse_expression_in_parethesis());
		if (match("double"))
			return fill(make<ast::ToFloatOp>(), parse_expression_in_parethesis());
		if (match("loop")) 
			return fill(make<ast::Loop>(), parse_unar());
		if (auto name = match("_")) {
			auto r = make<ast::Get>();
			r->var_name = ast->dom->names()->get("_");
			return r;
		}
		if (auto name = match_domain_name("domain name")) {
			auto r = make<ast::Get>();
			r->var_name = *name;
			return r;
		}
		error("syntax error");
	}

	template<typename T, typename VT>
	pin<Action> mk_const(VT&& v) {
		auto r = pin<T>::make();
		r->value = v;
		return r;
	}

	template<typename T>
	pin<T> make() {
		auto r = pin<T>::make();
		r->line = line;
		r->pos = pos;
		// r->module = module;
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

	optional<string> match_id() {
		if (!is_id_head(*cur))
			return nullopt;
		string result;
		while (is_id_body(*cur)) {
			result += *cur++;
			++pos;
		}
		match_ws();
		return result;
	}

	string expect_id(const char* message) {
		if (auto r = match_id())
			return *r;
		error("expected ", message);
	}

	variant<uint64_t, double> expect_number() {
		if (auto n = match_num()) {
			if (auto v = get_if<uint64_t>(&*n))
				return *v;
			if (auto v = get_if<double>(&*n))
				return *v;
		}
		error("expected number");
	}

	int match_length(char* str) { // returns 0 if not matched, length to skip if matched
		int i = 0;
		for (; str[i]; i++) {
			if (str[i] != cur[i])
				return 0;
		}
		return i;
	}

	bool match_ns(char* str) {
		int i = match_length(str);
		if (i == 0 || (is_id_body(str[i - 1]) && is_id_body(cur[i])))
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

	bool match_and_not(char* str, char after) {
		if (int i = match_length(str)) {
			if (cur[i] != after) {
				cur += i;
				pos += i;
				match_ws();
				return true;
			}
		}
		return false;
	}

	void expect(char* str) {
		if (!match(str))
			error(string("expected '") + str + "'");
	}

	bool match_ws() {
		const char* c = cur;
		for (;; line++, pos = 1) {
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

	static bool is_id_head(char c) {
		return
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z');
	};

	static bool is_num(char c) {
		return c >= '0' && c <= '9';
	};

	static bool is_id_body(char c) {
		return is_id_head(c) || is_num(c);
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

	optional<variant<uint64_t, double>> match_num() {
		if (!is_num(*cur))
			return nullopt;
		int radix = 10;
		if (match_ns("0")) {
			switch (*cur) {
			case 'x': radix = 16; break;
			case 'o': radix = 8; break;
			case 'b': radix = 2; break;
			default:
				cur-=2;
				break;
			}
			cur++;
		}
		uint64_t result = 0;
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
		if (*cur != '.' && *cur != 'e' && *cur != 'E') {
			match_ws();
			return result;
		}
		std::feclearexcept(FE_ALL_EXCEPT);
		double d = double(result);
		if (match_ns(".")) {
			for (double weight = 0.1; is_num(*cur); weight *= 0.1)
				d += weight * (*cur++ - '0');
		}
		if (match_ns("E") || match_ns("e")) {
			int sign = match_ns("-") ? -1 : (match_ns("+"), 1);
			int exp = 0;
			for (; *cur >= '0' && *cur < '9'; cur++)
				exp = exp * 10 + *cur - '0';
			d *= pow(10, exp * sign);
		}
		if (std::fetestexcept(FE_OVERFLOW | FE_UNDERFLOW))
			error("numeric overflow");
		match_ws();
		return d;
	}

	pin<Name> expect_domain_name(const char* message) {
		auto id = expect_id(message);
		auto name = match_domain_name_tail(id, message);
		return name ? name : ast->dom->names()->get(id);
	}

	optional<pin<Name>> match_domain_name(const char* message) {
		auto id = match_id();
		if (!id)
			return nullopt;
		auto name = match_domain_name_tail(*id, message);
		return name ? name : ast->dom->names()->get(*id);
	}

	pin<Name> match_domain_name_tail(string id, const char* message) {
		if (!match("_"))
			return nullptr;
		pin<Name> r = dom->names()->get(id)->get(expect_id(message));
		while (match("_")) {
			string name_val = expect_id(message);
			r = r->get(name_val);
		}
		return r;
	}

	template<typename... T>
	[[noreturn]] void error(const T&... t) {
		std::cerr << "error:" << ast::format_str(t...) << ":" << line << ":" << pos << std::endl;
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
	Parser(ast, module_name).parse(module_text_provider);
}
