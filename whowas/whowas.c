#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

whowas *whowas_head = NULL, *whowas_tail = NULL;
int whowas_count = 0;

whowas *whowas_fromnick(nick *np) {
  whowas *ww;

  /* Create a new record. */
  ww = malloc(sizeof(whowas));
  memset(ww, 0, sizeof(whowas));
  strncpy(ww->nick, np->nick, NICKLEN);
  strncpy(ww->ident, np->ident, USERLEN);
  strncpy(ww->host, np->host->name->content, HOSTLEN);
  memcpy(&ww->ip, &np->p_ipaddr, sizeof(struct irc_in_addr));
  strncpy(ww->realname, np->realname->name->content, REALLEN);
  ww->seen = getnettime();

  return ww;
}

void whowas_linkrecord(whowas *ww) {
  if (whowas_head)
    whowas_head->prev = ww;

  ww->next = whowas_head;
  whowas_head = ww;

  ww->prev = NULL;

  if (!whowas_tail)
    whowas_tail = ww;

  whowas_count++;
}

void whowas_unlinkrecord(whowas *ww) {
  if (!ww->next)
    whowas_tail = ww->prev;

  if (ww->prev)
    ww->prev->next = NULL;
  else
    whowas_head = ww->prev;

  whowas_count--;
}

void whowas_free(whowas *ww) {
  if (!ww)
    return;

  freesstring(ww->reason);
  free(ww);
}

static void whowas_cleanup(void) {
  time_t now;
  whowas *ww;

  time(&now);

  /* Clean up old records. */
  while (whowas_tail && (whowas_tail->seen < now - WW_MAXAGE || whowas_count >= WW_MAXENTRIES)) {
    ww = whowas_tail;
    whowas_unlinkrecord(ww);
    whowas_free(ww);
  }
}

static void whowas_handlequitorkill(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *reason = args[1];
  char *rreason;
  char resbuf[512];
  whowas *ww;

  whowas_cleanup();

  /* Create a new record. */
  ww = whowas_fromnick(np);

  if (hooknum == HOOK_NICK_KILL) {
    if ((rreason = strchr(reason, ' '))) {
      sprintf(resbuf, "Killed%s", rreason);
      reason = resbuf;
    }

    ww->type = WHOWAS_KILL;
  } else
    ww->type = WHOWAS_QUIT;

  ww->reason = getsstring(reason, WW_REASONLEN);

  whowas_linkrecord(ww);
}

static void whowas_handlerename(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *oldnick = args[1];
  whowas *ww;

  whowas_cleanup();

  ww = whowas_fromnick(np);
  ww->type = WHOWAS_RENAME;
  ww->newnick = getsstring(ww->nick, NICKLEN);
  strncpy(ww->nick, oldnick, NICKLEN);
  ww->nick[NICKLEN] = '\0';

  whowas_linkrecord(ww);
}

whowas *whowas_chase(const char *nick, int maxage) {
  whowas *ww;
  time_t now;

  now = getnettime();

  for (ww = whowas_head; ww; ww = ww->next) {
    if (ww->seen < now - maxage)
      break; /* records are in timestamp order, we're done */

    if (ircd_strcmp(ww->nick, nick) == 0)
      return ww;
  }

  return NULL;
}

void whowas_spew(whowas *ww, nick *np) {
  char timebuf[30];
  char hostmask[WW_MASKLEN + 1];

  snprintf(hostmask, sizeof(hostmask), "%s!%s@%s [%s]", ww->nick, ww->ident, ww->host, IPtostr(ww->ip));
  strftime(timebuf, 30, "%d/%m/%y %H:%M:%S", localtime(&(ww->seen)));

  if (ww->type == WHOWAS_RENAME)
    controlreply(np, "[%s] NICK %s (%s) -> %s", timebuf, hostmask, ww->realname, ww->newnick->content);
  else
    controlreply(np, "[%s] %s %s (%s): %s", timebuf, (ww->type == WHOWAS_QUIT) ? "QUIT" : "KILL", hostmask, ww->realname, ww->reason->content);
}

void _init(void) {
  registerhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  registerhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  registerhook(HOOK_NICK_RENAME, whowas_handlerename);
}

void _fini(void) {
  deregisterhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_RENAME, whowas_handlerename);

  while (whowas_head)
    whowas_unlinkrecord(whowas_head);
}
