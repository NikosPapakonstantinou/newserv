/*
 * qlasthost functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

#include <stdlib.h>

void *qlasthost_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void qlasthost_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *qlasthost_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->exe = qlasthost_exe;
  thenode->free = qlasthost_free;

  return thenode;
}

void *qlasthost_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  authname *ap = (authname *)theinput;
  reguser *rup = ap->exts[chanservaext];
  if(!rup || !rup->lastuserhost)
    return "";

  return rup->lastuserhost->content;
}

void qlasthost_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
