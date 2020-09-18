#ifndef _AK_NAME_RESOLVER_H_
#define _AK_NAME_RESOLVER_H_

#include "ast.h"

void resolve_names(ltm::pin<ast::Ast> ast);
ltm::pin<ast::Node> resolve_class_member(ltm::pin<ast::ClassDef> cls, ltm::pin<dom::Name> name);

#endif  // _AK_NAME_RESOLVER_H_
