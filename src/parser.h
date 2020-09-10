#ifndef _AK_PARSER_H_
#define _AK_PARSER_H_

#include "ast.h"

void parse(
	ltm::pin<ast::Ast> ast,
	ltm::pin<dom::Name> module_name,
	const std::function<std::string (ltm::pin<dom::Name> name)>& module_text_provider);

#endif  // _AK_PARSER_H_
