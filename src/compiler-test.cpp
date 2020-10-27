#include <unordered_map>
#include "fake-gunit.h"
#include "ast.h"
#include "parser.h"
#include "dom-to-string.h"
#include "name-resolver.h"
#include "type-checker.h"

namespace {

using std::unordered_map;
using std::string;
using std::move;
using ltm::pin;
using ltm::own;
using dom::Name;
using ast::Ast;

auto source_text = R"-(
module ak_test.1

class MyClass {
    x_ = 0;
    y_ = 0;
    +init(x:0, y:0) {
        x_ := x;
        y_ := y;
        this;
    }
}
)-";

TEST(Parser, Simple) {
	ast::initialize();
	auto start_module_name = ast::static_dom->names()->get("ak")->get("test");
	unordered_map<own<Name>, string> texts{{start_module_name, source_text}};
	auto ast = own<Ast>::make();
	parse(ast, start_module_name, [&] (pin<Name> name) {
		auto it = texts.find(name);
		return it == texts.end() ? "" : move(it->second);
	});
	// auto s = std::to_string(ast, ast::static_dom);
	resolve_names(ast);
	std::cout << std::make_pair(ast.pinned(), ast::static_dom.pinned()) << "\n";
	check_types(ast);
}

}  // namespace
