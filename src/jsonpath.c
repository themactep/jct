#include "jsonpath.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Minimal dynamic array helpers
typedef struct { JsonValue *val; char *path; } NodeRef;
typedef struct { NodeRef *items; int count; int cap; } NodeVec;
static void nv_init(NodeVec *v){ v->items=NULL; v->count=0; v->cap=0; }
static int nv_push(NodeVec *v, JsonValue *val, const char *path){
    if (v->count==v->cap){ int nc = v->cap? v->cap*2:16; NodeRef*ni=(NodeRef*)realloc(v->items, nc*sizeof(NodeRef)); if(!ni) return 0; v->items=ni; v->cap=nc; }
    v->items[v->count].val = val;
    v->items[v->count].path = path? strdup(path): NULL;
    if (path && !v->items[v->count].path) return 0;
    v->count++;
    return 1;
}
static void nv_free(NodeVec *v){ if(!v) return; for(int i=0;i<v->count;i++) free(v->items[i].path); free(v->items); }

// String builder
typedef struct { char *s; int len; int cap; } Str;
static void sb_init(Str *b){ b->s=NULL; b->len=0; b->cap=0; }
static int sb_putc(Str *b, char c){ if(b->len+1>=b->cap){ int nc=b->cap? b->cap*2:64; char *ns=(char*)realloc(b->s,nc); if(!ns) return 0; b->s=ns;b->cap=nc;} b->s[b->len++]=c; b->s[b->len]='\0'; return 1; }
static int sb_puts(Str *b, const char *s){ while(*s){ if(!sb_putc(b,*s++)) return 0; } return 1; }
static char* sb_steal(Str *b){ return b->s; }

// Lexer helpers over the expression string
typedef struct { const char *s; int pos; int len; const JsonPathOptions *opt; } Scan;
static int at_end(Scan *sc){ return sc->pos>=sc->len; }
static char peek(Scan *sc){ return at_end(sc)? '\0': sc->s[sc->pos]; }
static char getc_(Scan *sc){ return at_end(sc)? '\0': sc->s[sc->pos++]; }
static void skip_ws(Scan *sc){ while(!at_end(sc) && isspace((unsigned char)sc->s[sc->pos])) sc->pos++; }
static int match(Scan *sc, const char *lit){ int n=(int)strlen(lit); if(sc->pos+n>sc->len) return 0; if(strncmp(sc->s+sc->pos,lit,n)==0){ sc->pos+=n; return 1;} return 0; }
static void set_err(const JsonPathOptions *opt, const char *msg, int pos){ if(opt && opt->strict){ fprintf(stderr,"jsonpath error at %d: %s\n", pos, msg); } }

// Forward decls
static int eval_steps(JsonValue *doc, Scan *sc, NodeVec *out, int *emitted);

// Utilities to iterate object/array with names and indices
static void foreach_object(JsonValue *obj, void (*cb)(const char*, JsonValue*, void*), void *ud){
    if(!obj || obj->type!=JSON_OBJECT) return; for(JsonKeyValue *kv=obj->value.object_head; kv; kv=kv->next){ cb(kv->key, kv->value, ud); }
}
static void foreach_array(JsonValue *arr, void (*cb)(int, JsonValue*, void*), void *ud){
    if(!arr || arr->type!=JSON_ARRAY) return; int idx=0; for(JsonArrayItem *it=arr->value.array_head; it; it=it->next, idx++){ cb(idx, it->value, ud); }
}

// Build child path strings
static char* path_append_prop(const char *base, const char *name){ Str b; sb_init(&b); if(!sb_puts(&b, base? base: "$")) return NULL; if(name && *name && (isalpha((unsigned char)name[0])||name[0]=='_' )){
    if(!sb_putc(&b, '.')) return NULL; if(!sb_puts(&b, name)) return NULL;
} else {
    if(!sb_puts(&b, "['")) return NULL; if(!sb_puts(&b, name? name: "")) return NULL; if(!sb_puts(&b, "']")) return NULL;
}
    return sb_steal(&b);
}
static char* path_append_index(const char *base, int idx){ char buf[64]; snprintf(buf,sizeof(buf),"[%d]", idx); Str b; sb_init(&b); if(!sb_puts(&b, base? base: "$")) return NULL; if(!sb_puts(&b, buf)) return NULL; return sb_steal(&b); }

// Collect descendants depth-first with their paths
static int collect_descendants(JsonValue *root, const char *path, NodeVec *vec){
    // include current? For recursive descent, selection applies to next step, so we return only descendants as candidates
    if(!root) return 1;
    if(root->type==JSON_OBJECT){
        for(JsonKeyValue *kv=root->value.object_head; kv; kv=kv->next){ char *p=path_append_prop(path, kv->key); if(!p) return 0; if(!nv_push(vec, kv->value, p)){ free(p); return 0;} free(p); // push immediate child as candidate
            // and recurse further
            if(!collect_descendants(kv->value, path_append_prop(path, kv->key), vec)) return 0; }
    } else if(root->type==JSON_ARRAY){
        int i=0; for(JsonArrayItem *it=root->value.array_head; it; it=it->next, i++){ char *p=path_append_index(path, i); if(!p) return 0; if(!nv_push(vec, it->value, p)){ free(p); return 0;} free(p); if(!collect_descendants(it->value, path_append_index(path, i), vec)) return 0; }
    }
    return 1;
}

// Parse an identifier (dot-child name)
static char* parse_identifier(Scan *sc){ int start=sc->pos; if(! (isalpha((unsigned char)peek(sc)) || peek(sc)=='_' )) return NULL; while(!at_end(sc)){
        char c=peek(sc); if(isalnum((unsigned char)c)||c=='_' ){ sc->pos++; } else break; }
    int n=sc->pos-start; char *s=(char*)malloc(n+1); if(!s) return NULL; memcpy(s, sc->s+start, n); s[n]='\0'; return s; }

static char* parse_quoted(Scan *sc){ char quote = getc_(sc); if(quote!='\'' && quote!='\"') return NULL; Str b; sb_init(&b); while(!at_end(sc)){
        char c=getc_(sc); if(c==quote) break; if(c=='\\'){ char n=getc_(sc); if(n=='\0') break; c=n; } if(!sb_putc(&b,c)) return NULL; }
    return sb_steal(&b);
}

// Parse integer (non-negative)
static int parse_int(Scan *sc, int *ok){ int sign=1; if(peek(sc)=='-'){ sign=-1; sc->pos++; }
    if(!isdigit((unsigned char)peek(sc))){ *ok=0; return 0; } long v=0; while(!at_end(sc) && isdigit((unsigned char)peek(sc))){ v = v*10 + (getc_(sc)-'0'); if(v>2147483647) v=2147483647; }
    *ok=1; return (int)(sign*v);
}

// Evaluate path fragment starting from a working set of nodes
static int apply_child_name(NodeVec *cur, const char *name, NodeVec *next, int *emitted){
    for(int i=0;i<cur->count;i++){
        JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$";
        if(v && v->type==JSON_OBJECT){ JsonValue *c=get_object_item(v,name); if(c){ char *np=path_append_prop(p,name); if(!np) return 0; if(!nv_push(next,c,np)){ free(np); return 0; } free(np);} }
    }
    return 1;
}
static int apply_wildcard(NodeVec *cur, NodeVec *next){
    for(int i=0;i<cur->count;i++){
        JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$";
        if(!v) continue; if(v->type==JSON_OBJECT){ for(JsonKeyValue *kv=v->value.object_head; kv; kv=kv->next){ char *np=path_append_prop(p, kv->key); if(!np) return 0; if(!nv_push(next, kv->value, np)){ free(np); return 0; } free(np);} }
        else if(v->type==JSON_ARRAY){ int idx=0; for(JsonArrayItem *it=v->value.array_head; it; it=it->next, idx++){ char *np=path_append_index(p, idx); if(!np) return 0; if(!nv_push(next, it->value, np)){ free(np); return 0; } free(np);} }
    }
    return 1;
}

// Filter expression evaluation (very small expression parser)
// Forward decls
static int eval_filter_expr(Scan *sc, JsonValue *ctx);
static int eval_or(Scan *sc, JsonValue *ctx);
static int eval_and(Scan *sc, JsonValue *ctx);
static int eval_unary(Scan *sc, JsonValue *ctx);
static int eval_cmp(Scan *sc, JsonValue *ctx);
static JsonValue* eval_path_from_at(Scan *sc, JsonValue *ctx);
static int cmp_values(JsonValue *a, JsonValue *b, const char *op);

static void skip_ws_opt(Scan *sc){ skip_ws(sc); }

static int eval_filter_expr(Scan *sc, JsonValue *ctx){ return eval_or(sc, ctx); }
static int eval_or(Scan *sc, JsonValue *ctx){ int v = eval_and(sc, ctx); skip_ws_opt(sc); while(match(sc, "||")){ int r = eval_and(sc, ctx); v = (v||r); skip_ws_opt(sc);} return v; }
static int eval_and(Scan *sc, JsonValue *ctx){ int v = eval_unary(sc, ctx); skip_ws_opt(sc); while(match(sc, "&&")){ int r = eval_unary(sc, ctx); v = (v&&r); skip_ws_opt(sc);} return v; }
static int eval_unary(Scan *sc, JsonValue *ctx){ skip_ws_opt(sc); if(match(sc, "!")){ return !eval_unary(sc, ctx);} return eval_cmp(sc, ctx); }

static JsonValue* make_bool(int b){ JsonValue *v=create_json_value(JSON_BOOL); if(v) v->value.boolean=b; return v; }
static JsonValue* make_number(double d){ JsonValue *v=create_json_value(JSON_NUMBER); if(v) v->value.number=d; return v; }
static JsonValue* make_string_lit(const char *s){ JsonValue *v=create_json_value(JSON_STRING); if(v) v->value.string=strdup(s?s:""); return v; }
static JsonValue* make_null(){ return create_json_value(JSON_NULL); }

static JsonValue* parse_literal(Scan *sc){ skip_ws_opt(sc); if(match(sc,"true")) return make_bool(1); if(match(sc,"false")) return make_bool(0); if(match(sc,"null")) return make_null();
    if(peek(sc)=="'"[0] || peek(sc)=='"'){ char *s=parse_quoted(sc); if(!s) return NULL; JsonValue *v=make_string_lit(s); free(s); return v; }
    // number
    int pos0=sc->pos; int sign=1; if(peek(sc)=='-'){ sign=-1; sc->pos++; }
    if(isdigit((unsigned char)peek(sc))){ double val=0; while(!at_end(sc)&&isdigit((unsigned char)peek(sc))){ val = val*10 + (getc_(sc)-'0'); }
        if(peek(sc)=='.'){ sc->pos++; double frac=0,base=1; while(!at_end(sc)&&isdigit((unsigned char)peek(sc))){ frac = frac*10 + (getc_(sc)-'0'); base*=10; } val += frac/base; }
        JsonValue *v=make_number(sign*val); return v; }
    sc->pos = pos0; return NULL;
}

static int eval_cmp(Scan *sc, JsonValue *ctx){ skip_ws_opt(sc);
    JsonValue *lhs=NULL, *rhs=NULL; int lhs_is_path=0, rhs_is_path=0;
    if(peek(sc)=='@'){ getc_(sc); lhs = eval_path_from_at(sc, ctx); lhs_is_path=1; }
    else { lhs = parse_literal(sc); }
    skip_ws_opt(sc);
    const char *op=NULL;
    if(match(sc, "==")) op="=="; else if(match(sc, "!=")) op="!="; else if(match(sc, ">=")) op=">="; else if(match(sc, "<=")) op="<="; else if(match(sc, ">")) op=">"; else if(match(sc, "<")) op="<";
    if(!op){ // treat truthiness of lhs
        int res=0; if(lhs){ if(lhs->type==JSON_BOOL) res=lhs->value.boolean; else if(lhs->type==JSON_NULL) res=0; else res=1; free_json_value(lhs); } return res; }
    skip_ws_opt(sc);
    if(peek(sc)=='@'){ getc_(sc); rhs = eval_path_from_at(sc, ctx); rhs_is_path=1; }
    else { rhs = parse_literal(sc); }
    int res = 0; if(lhs && rhs) res = cmp_values(lhs, rhs, op);
    if(lhs && !lhs_is_path) free_json_value(lhs); if(rhs && !rhs_is_path) free_json_value(rhs);
    return res;
}

static int numcmp(double a, double b){ if(a<b) return -1; if(a>b) return 1; return 0; }
static int strcmp_null(const char *a, const char *b){ if(!a&&!b) return 0; if(!a) return -1; if(!b) return 1; return strcmp(a,b); }
static int cmp_values(JsonValue *a, JsonValue *b, const char *op){
    if(a->type==JSON_NUMBER && b->type==JSON_NUMBER){ int c=numcmp(a->value.number,b->value.number);
        if(strcmp(op,"==")==0) return c==0; if(strcmp(op,"!=")==0) return c!=0; if(strcmp(op,">" )==0) return c>0; if(strcmp(op,">=")==0) return c>=0; if(strcmp(op,"<")==0) return c<0; if(strcmp(op,"<=")==0) return c<=0; }
    // Compare strings lexicographically
    if(a->type==JSON_STRING && b->type==JSON_STRING){ int c=strcmp_null(a->value.string,b->value.string);
        if(strcmp(op,"==")==0) return c==0; if(strcmp(op,"!=")==0) return c!=0; if(strcmp(op,">" )==0) return c>0; if(strcmp(op,">=")==0) return c>=0; if(strcmp(op,"<")==0) return c<0; if(strcmp(op,"<=")==0) return c<=0; }
    // bool vs bool
    if(a->type==JSON_BOOL && b->type==JSON_BOOL){ int c= (a->value.boolean - b->value.boolean);
        if(strcmp(op,"==")==0) return c==0; if(strcmp(op,"!=")==0) return c!=0; if(strcmp(op,">" )==0) return c>0; if(strcmp(op,">=")==0) return c>=0; if(strcmp(op,"<")==0) return c<0; if(strcmp(op,"<=")==0) return c<=0; }
    // null comparisons: only == and !=
    if(a->type==JSON_NULL || b->type==JSON_NULL){ if(strcmp(op,"==")==0) return a->type==b->type; if(strcmp(op,"!=")==0) return a->type!=b->type; }
    return 0;
}

static JsonValue* eval_path_from_at(Scan *sc, JsonValue *ctx){ JsonValue *cur=ctx; skip_ws_opt(sc); int progressed=1; while(progressed){ progressed=0; if(match(sc, ".")){
            if(match(sc, ".")){ // recursive descent from current: return first found? We'll treat as no-op here for simplicity
                // Not supporting @.. in filters beyond simple @.a.b
                continue;
            }
            char *name=parse_identifier(sc); if(!name){ return cur; }
            if(cur && cur->type==JSON_OBJECT) cur=get_object_item(cur, name); else cur=NULL; free(name); progressed=1; continue; }
        if(match(sc, "[")){
            if(peek(sc)=='\''||peek(sc)=='"'){ char *q=parse_quoted(sc); if(!q){ return cur; } skip_ws_opt(sc); if(!match(sc,"]")) return cur; if(cur && cur->type==JSON_OBJECT) cur=get_object_item(cur,q); else cur=NULL; free(q); progressed=1; continue; }
            int ok=0; int idx=parse_int(sc,&ok); if(!ok){ return cur; } skip_ws_opt(sc); if(!match(sc,"]")) return cur; if(cur && cur->type==JSON_ARRAY) cur=get_array_item(cur, idx); else cur=NULL; progressed=1; continue; }
    }
    if(!cur) return create_json_value(JSON_NULL); // treat missing as null
    return cur; // note: caller should not free if returned is original
}

// Apply array subscripts: indices, unions, slices
static int apply_array_subscript(NodeVec *cur, Scan *sc, NodeVec *next){ skip_ws(sc);
    // Wildcard [*]
    if(match(sc, "*")){
        skip_ws(sc); if(!match(sc, "]")) return 0;
        if(!apply_wildcard(cur, next)) return 0;
        return 1;
    }

    // Check for filter: [? ( expr ) ]
    if(match(sc, "?")){

        if(!match(sc, "(")) return 0; // parse error
        // Determine end position of the expression using a scratch scanner
        int expr_start = sc->pos;
        Scan sc_end = *sc; sc_end.pos = expr_start;
        (void)eval_filter_expr(&sc_end, NULL);
        int expr_end = sc_end.pos;
        for(int i=0;i<cur->count;i++){
            JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$";
            if(v && v->type==JSON_ARRAY){ int idx=0; for(JsonArrayItem *it=v->value.array_head; it; it=it->next, idx++){
                    Scan sc2 = *sc; sc2.pos = expr_start; int res = eval_filter_expr(&sc2, it->value);
                    if(res){ char *np=path_append_index(p, idx); if(!np) return 0; if(!nv_push(next, it->value, np)){ free(np); return 0;} free(np);} }
            } else if(v){ Scan sc2=*sc; sc2.pos=expr_start; int res=eval_filter_expr(&sc2, v); if(res){ if(!nv_push(next, v, p)) return 0; }}
        }
        sc->pos = expr_end; // advance past expression
        skip_ws(sc); if(!match(sc, ")")) return 0; skip_ws(sc); if(!match(sc,"]")) return 0; return 1;
    }

    // Union of names? ['a','b'] or ["a","b"]
    if(peek(sc)=='\'' || peek(sc)=='"'){
        // Collect names
        char **names=NULL; int n=0,cap=0; do{ char *q=parse_quoted(sc); if(!q) return 0; if(n==cap){ int nc=cap?cap*2:4; char **nn=(char**)realloc(names, nc*sizeof(char*)); if(!nn){ free(q); return 0;} names=nn; cap=nc;} names[n++]=q; skip_ws(sc); } while(match(sc, ",")); if(!match(sc,"]")){ for(int i=0;i<n;i++) free(names[i]); free(names); return 0; }
        for(int i=0;i<cur->count;i++){ JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$"; if(v && v->type==JSON_OBJECT){ for(int k=0;k<n;k++){ JsonValue *c=get_object_item(v, names[k]); if(c){ char *np=path_append_prop(p, names[k]); if(!np) return 0; if(!nv_push(next, c, np)){ free(np); return 0; } free(np);} } } }
        for(int i=0;i<n;i++) free(names[i]); free(names); return 1;
    }

    // Indices/union/slice
    int ok=0; int start=parse_int(sc,&ok); if(!ok){ return 0; }
    skip_ws(sc);
    if(match(sc, ":")){
        // slice start:end[:step]
        int has_end=0, has_step=0; int end=0, step=1; if(peek(sc)!=']'){ if(peek(sc)!=':'){ end=parse_int(sc,&has_end); if(!has_end){ end=0; } }
            if(match(sc, ":")){ has_step=1; step=parse_int(sc,&ok); if(!ok) step=1; }
        }
        skip_ws(sc); if(!match(sc, "]")) return 0;
        for(int i=0;i<cur->count;i++){ JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$"; if(v && v->type==JSON_ARRAY){ int n=get_array_size(v); int s=start; int e= has_end? end : n; if(s<0||e<0){ if(((JsonPathOptions*)sc->opt)->strict){ set_err(sc->opt,"negative indices not supported", sc->pos); return 0; } else { continue; } } if(s<0) s=0; if(e>n) e=n; if(step<=0) step=1; for(int idx=s; idx<e; idx+=step){ JsonValue *c=get_array_item(v, idx); if(c){ char *np=path_append_index(p, idx); if(!np) return 0; if(!nv_push(next, c, np)){ free(np); return 0;} free(np);} } } }
        return 1;
    }

    // union of indices: start[,i2,i3,...]
    int *idxs=NULL; int nidx=0, cap=0; if(nidx==cap){ cap=4; idxs=(int*)malloc(cap*sizeof(int)); if(!idxs) return 0; }
    idxs[nidx++]=start; skip_ws(sc); while(match(sc, ",")){ int v=parse_int(sc,&ok); if(!ok){ free(idxs); return 0;} if(nidx==cap){ int nc=cap*2; int *ni=(int*)realloc(idxs, nc*sizeof(int)); if(!ni){ free(idxs); return 0;} idxs=ni; cap=nc;} idxs[nidx++]=v; skip_ws(sc);} if(!match(sc,"]")){ free(idxs); return 0; }
    for(int i=0;i<cur->count;i++){ JsonValue *v=cur->items[i].val; const char *p=cur->items[i].path? cur->items[i].path: "$"; if(v && v->type==JSON_ARRAY){ int n=get_array_size(v); for(int k=0;k<nidx;k++){ int id=idxs[k]; if(id<0){ if(((JsonPathOptions*)sc->opt)->strict){ set_err(sc->opt,"negative indices not supported", sc->pos); free(idxs); return 0; } else continue; } if(id>=0 && id<n){ JsonValue *c=get_array_item(v, id); if(c){ char *np=path_append_index(p, id); if(!np){ free(idxs); return 0;} if(!nv_push(next,c,np)){ free(np); free(idxs); return 0;} free(np);} } } } }
    free(idxs); return 1;
}

static int eval_steps(JsonValue *doc, Scan *sc, NodeVec *out, int *emitted){
    // Start with root
    NodeVec cur; nv_init(&cur); if(!nv_push(&cur, doc, "$")){ nv_free(&cur); return 0; }
    skip_ws(sc); if(!match(sc, "$")){ set_err(sc->opt, "expected '$' at start", sc->pos); nv_free(&cur); return 0; }
    while(!at_end(sc)){
        skip_ws(sc);
        if(match(sc, ".")){
            if(match(sc, ".")){
                // recursive descent: collect all descendants of current set as candidates for next selector
                NodeVec desc; nv_init(&desc);
                for(int i=0;i<cur.count;i++){
                    if(!collect_descendants(cur.items[i].val, cur.items[i].path?cur.items[i].path:"$", &desc)){ nv_free(&desc); nv_free(&cur); return 0; }
                }
                // Now expect child selector (*) or identifier or bracket
                if(match(sc, "*")){
                    NodeVec tmp; nv_init(&tmp); if(!apply_wildcard(&desc, &tmp)){ nv_free(&desc); nv_free(&tmp); nv_free(&cur); return 0; } nv_free(&desc); nv_free(&cur); cur=tmp; continue;
                }
                if(peek(sc)=='['){ getc_(sc); NodeVec tmp; nv_init(&tmp); if(!apply_array_subscript(&desc, sc, &tmp)){ nv_free(&desc); nv_free(&tmp); nv_free(&cur); return 0;} nv_free(&desc); nv_free(&cur); cur=tmp; continue; }
                char *name=parse_identifier(sc); if(!name){ set_err(sc->opt, "expected name after '..'", sc->pos); nv_free(&desc); nv_free(&cur); return 0; }
                NodeVec tmp; nv_init(&tmp);
                for(int i=0;i<desc.count;i++){ JsonValue *v=desc.items[i].val; const char *p=desc.items[i].path? desc.items[i].path: "$"; if(v && v->type==JSON_OBJECT){ JsonValue *c=get_object_item(v, name); if(c){ char *np=path_append_prop(p, name); if(!np){ free(name); nv_free(&desc); nv_free(&tmp); nv_free(&cur); return 0;} if(!nv_push(&tmp,c,np)){ free(np); free(name); nv_free(&desc); nv_free(&tmp); nv_free(&cur); return 0;} free(np);} } }
                free(name); nv_free(&desc); nv_free(&cur); cur=tmp; continue;
            }
            // dot child
            if(match(sc, "*")){
                NodeVec next; nv_init(&next); if(!apply_wildcard(&cur, &next)){ nv_free(&cur); nv_free(&next); return 0; } nv_free(&cur); cur=next; continue;
            }
            char *name=parse_identifier(sc); if(!name){ set_err(sc->opt, "expected property name after '.'", sc->pos); nv_free(&cur); return 0; }
            NodeVec next; nv_init(&next); if(!apply_child_name(&cur, name, &next, emitted)){ free(name); nv_free(&cur); nv_free(&next); return 0; } free(name); nv_free(&cur); cur=next; continue;
        } else if(match(sc, "[")){
            NodeVec next; nv_init(&next); if(!apply_array_subscript(&cur, sc, &next)){ nv_free(&cur); nv_free(&next); return 0; } nv_free(&cur); cur=next; continue;
        } else {
            break;
        }
    }
    // Emit results in required mode
    for(int i=0;i<cur.count;i++){
        if(out->cap==0) out->items=NULL; // not used here
        // We'll reuse NodeVec 'cur' to transfer ownership to out
    }
    *out = cur; // transfer (paths already stored)
    return 1;
}

static JsonValue* build_values_array(const NodeVec *nodes){ JsonValue *arr=create_json_value(JSON_ARRAY); if(!arr) return NULL; for(int i=0;i<nodes->count;i++){ JsonValue *dup=clone_json_value(nodes->items[i].val); if(!dup) { free_json_value(arr); return NULL; } add_to_array(arr, dup); } return arr; }
static JsonValue* build_paths_array(const NodeVec *nodes){ JsonValue *arr=create_json_value(JSON_ARRAY); if(!arr) return NULL; for(int i=0;i<nodes->count;i++){ JsonValue *s=create_json_value(JSON_STRING); if(!s){ free_json_value(arr); return NULL; } s->value.string = nodes->items[i].path? strdup(nodes->items[i].path): strdup("$"); add_to_array(arr, s); } return arr; }
static JsonValue* build_pairs_array(const NodeVec *nodes){ JsonValue *arr=create_json_value(JSON_ARRAY); if(!arr) return NULL; for(int i=0;i<nodes->count;i++){ JsonValue *obj=create_json_value(JSON_OBJECT); if(!obj){ free_json_value(arr); return NULL; } JsonValue *sp=create_json_value(JSON_STRING); if(!sp){ free_json_value(obj); free_json_value(arr); return NULL; } sp->value.string = nodes->items[i].path? strdup(nodes->items[i].path): strdup("$"); add_to_object(obj, "path", sp); JsonValue *val=clone_json_value(nodes->items[i].val); if(!val){ free_json_value(obj); free_json_value(arr); return NULL; } add_to_object(obj, "value", val); add_to_array(arr, obj); } return arr; }

JsonPathResults *evaluate_jsonpath(JsonValue *doc, const char *expression, const JsonPathOptions *options){ if(!doc || !expression){ if(options && options->strict) fprintf(stderr,"jsonpath: null doc or expression\n"); return NULL; }
    Scan sc = { .s=expression, .pos=0, .len=(int)strlen(expression), .opt=options };
    NodeVec nodes; nv_init(&nodes); int emitted=0; if(!eval_steps(doc, &sc, &nodes, &emitted)){
        if(options && options->strict){ // parse/eval error
            nv_free(&nodes);
            return NULL;
        } else {
            // lenient -> empty results
            JsonPathResults *res=(JsonPathResults*)calloc(1,sizeof(JsonPathResults)); if(!res){ nv_free(&nodes); return NULL; } res->mode = options? options->mode: JSONPATH_MODE_VALUES; res->count=0; res->paths=NULL; res->values=NULL; nv_free(&nodes); return res;
        }
    }
    // Apply limit if requested
    int limit = (options && options->limit>0)? options->limit : nodes.count;
    if(limit < nodes.count) nodes.count = limit;

    JsonPathResults *res=(JsonPathResults*)calloc(1,sizeof(JsonPathResults)); if(!res){ nv_free(&nodes); return NULL; }
    JsonPathMode mode = options? options->mode: JSONPATH_MODE_VALUES; res->mode=mode; res->count=nodes.count;
    if(mode==JSONPATH_MODE_PATHS){ res->paths=(char**)calloc(nodes.count, sizeof(char*)); if(!res->paths){ nv_free(&nodes); free(res); return NULL;} for(int i=0;i<nodes.count;i++){ res->paths[i]= nodes.items[i].path? strdup(nodes.items[i].path): strdup("$"); } }
    else if(mode==JSONPATH_MODE_VALUES){ res->values=(JsonValue**)calloc(nodes.count, sizeof(JsonValue*)); if(!res->values){ nv_free(&nodes); free(res); return NULL;} for(int i=0;i<nodes.count;i++){ res->values[i]= clone_json_value(nodes.items[i].val); }
    }
    else { // pairs
        res->paths=(char**)calloc(nodes.count, sizeof(char*)); res->values=(JsonValue**)calloc(nodes.count, sizeof(JsonValue*)); if(!res->paths||!res->values){ nv_free(&nodes); free(res->paths); free(res->values); free(res); return NULL; }
        for(int i=0;i<nodes.count;i++){ res->paths[i]= nodes.items[i].path? strdup(nodes.items[i].path): strdup("$"); res->values[i]= clone_json_value(nodes.items[i].val); }
    }
    nv_free(&nodes); return res;
}

void free_jsonpath_results(JsonPathResults *res){ if(!res) return; if(res->paths){ for(int i=0;i<res->count;i++) free(res->paths[i]); free(res->paths);} if(res->values){ for(int i=0;i<res->count;i++) if(res->values[i]) free_json_value(res->values[i]); free(res->values);} free(res); }

