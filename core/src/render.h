/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_RENDER_H
#define OZNI_RENDER_H

#include "ast.h"
#include "buf.h"

bool ozni_render_check(ozni_ctx *c, ozni_buf *b, bool *has_errors);
bool ozni_render_result_head(ozni_ctx *c, ozni_buf *b, bool *has_errors);
bool ozni_render_ast(const ozni_ctx *c, const ozni_ast *ast, unsigned flags, ozni_buf *b);

#endif
