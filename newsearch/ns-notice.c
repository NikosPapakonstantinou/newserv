/*
 * NOTICE functionality
 * ---------
 * Use of markers allows us to use notice in both channel and nick searches so we can have newserv notice everyone on a channel and/or
 * everyone matching a certain nick_search pattern.
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../control/control.h" /* controlreply() */
#include "../lib/irc_string.h" /* IPtostr() */
#include "../lib/strlfunc.h"

extern nick *senderNSExtern;

void *notice_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void notice_free(searchCtx *ctx, struct searchNode *thenode);

struct notice_localdata {
  unsigned int marker;
  int count;
  char message[NSMAX_NOTICE_LEN];
};

struct searchNode *notice_parse(searchCtx *ctx, int argc, char **argv) {
  struct notice_localdata *localdata;
  struct searchNode *thenode, *message;
  char *p;
  
  if (!(localdata = (struct notice_localdata *) malloc(sizeof(struct notice_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  localdata->count = 0;
  if (ctx->searchcmd == reg_chansearch)
    localdata->marker = nextchanmarker();
  else if (ctx->searchcmd == reg_nicksearch)
    localdata->marker = nextnickmarker();
  else {
    free(localdata);
    parseError = "notice: invalid search type";
    return NULL;
  }
  if (argc!=1) {
    parseError = "notice: warning: you did not specify a message to notice out.";
    free(localdata);
    return NULL;
  }

  if (!(message=argtoconststr("notice", ctx, argv[0], &p))) {
    free(localdata);
    return NULL;
  }
  
  strlcpy(localdata->message, p, sizeof(localdata->message));
  (message->free)(ctx, message);
  
  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = notice_exe;
  thenode->free = notice_free;

  return thenode;
}

void *notice_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct notice_localdata *localdata;
  nick *np;
  chanindex *cip;

  localdata = thenode->localdata;

  if (ctx->searchcmd == reg_chansearch) {
    cip = (chanindex *)theinput;
    cip->marker = localdata->marker;
    localdata->count += cip->channel->users->totalusers;
  }
  else {
    np = (nick *)theinput;
    np->marker = localdata->marker;
    localdata->count++;
  }

  return (void *)1;
}

void notice_free(searchCtx *ctx, struct searchNode *thenode) {
  struct notice_localdata *localdata;
  nick *np, *nnp;
  chanindex *cip, *ncip;
  int i, j;
  unsigned int nickmarker;

  localdata = thenode->localdata;

  if (ctx->searchcmd == reg_chansearch) {
    nickmarker=nextnickmarker();
    for (i=0;i<CHANNELHASHSIZE;i++) {
      for (cip=chantable[i];cip;cip=ncip) {
        ncip = cip->next;
        if (cip != NULL && cip->channel != NULL && cip->marker == localdata->marker) {
          for (j=0;j<cip->channel->users->hashsize;j++) {
            if (cip->channel->users->content[j]==nouser)
              continue;
    
            if ((np=getnickbynumeric(cip->channel->users->content[j])))
              np->marker=nickmarker;
          }
        }
      }
    }
    for (i=0;i<NICKHASHSIZE;i++) {
      for(np=nicktable[i];np;np=nnp) {
        nnp = np->next;
        if (np->marker == nickmarker)
          controlnotice(np, "%s", localdata->message);
      }
    }
  }
  else {
    for (i=0;i<NICKHASHSIZE;i++) {
      for (np=nicktable[i];np;np=nnp) {
        nnp = np->next;
        if (np->marker == localdata->marker)
         controlnotice(np, "%s", localdata->message);
      }
    }
  }
  /* notify opers of the action */
  ctx->wall(NL_BROADCASTS, "%s/%s sent the following message to %d %s: %s", senderNSExtern->nick, senderNSExtern->authname, localdata->count, localdata->count != 1 ? "users" : "user", localdata->message);
  free(localdata);
  free(thenode);
}
