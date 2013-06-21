#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "../lib/sstring.h"
#include "../core/hooks.h"
#include "../core/nsmalloc.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "trusts.h"

trustgroup *tglist;

void th_dbupdatecounts(trusthost *);
void tg_dbupdatecounts(trustgroup *);

static trusthost *th_getnextchildbyhost(trusthost *, trusthost *);

void trusts_freeall(void) {
  trustgroup *tg, *ntg;
  trusthost *th, *nth;

  for(tg=tglist;tg;tg=ntg) {
    ntg = tg->next;
    for(th=tg->hosts;th;th=nth) {
      nth = th->next;

      th_free(th);
    }

    tg_free(tg, 1);
  }

  tglist = NULL;
}

trustgroup *tg_getbyid(unsigned int id) {
  trustgroup *tg;

  for(tg=tglist;tg;tg=tg->next)
    if(tg->id == id)
      return tg;

  return NULL;
}

void th_free(trusthost *th) {
  triggerhook(HOOK_TRUSTS_LOSTHOST, th);

  nsfree(POOL_TRUSTS, th);
}

static void th_updatechildren(trusthost *th) {
  trusthost *nth = NULL;

  th->children = NULL;

  for(;;) {
    nth = th_getnextchildbyhost(th, nth);
    if(!nth)
      break;

    nth->nextbychild = th->children;
    th->children = nth;
  }
}

void th_linktree(void) {
  trustgroup *tg;
  trusthost *th;

  /* ugh */
  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      th->parent = th_getsmallestsupersetbyhost(&th->ip, th->bits);

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if(th->parent)
        th_updatechildren(th->parent);
}

trusthost *th_add(trusthost *ith) {
  trusthost *th;

  th = nsmalloc(POOL_TRUSTS, sizeof(trusthost));
  if(!th)
    return NULL;

  memcpy(th, ith, sizeof(trusthost));

  th->users = NULL;
  th->count = 0;

  th->parent = NULL;
  th->children = NULL;

  th->marker = 0;

  th->next = th->group->hosts;
  th->group->hosts = th;

  return th;
}

void tg_free(trustgroup *tg, int created) {
  if(created)
    triggerhook(HOOK_TRUSTS_LOSTGROUP, tg);

  freesstring(tg->name);
  freesstring(tg->createdby);
  freesstring(tg->contact);
  freesstring(tg->comment);
  nsfree(POOL_TRUSTS, tg);
}

trustgroup *tg_add(trustgroup *itg) {
  trustgroup *tg = nsmalloc(POOL_TRUSTS, sizeof(trustgroup));
  if(!tg)
    return NULL;

  memcpy(tg, itg, sizeof(trustgroup));

  tg->name = getsstring(tg->name->content, TRUSTNAMELEN);
  tg->createdby = getsstring(tg->createdby->content, CREATEDBYLEN);
  tg->contact = getsstring(tg->contact->content, CONTACTLEN);
  tg->comment = getsstring(tg->comment->content, COMMENTLEN);
  if(!tg->name || !tg->createdby || !tg->contact || !tg->comment) {
    tg_free(tg, 0);
    return NULL;
  }

  tg->hosts = NULL;
  tg->marker = 0;
  tg->count = 0;

  memset(tg->exts, 0, sizeof(tg->exts));

  tg->next = tglist;
  tglist = tg;

  triggerhook(HOOK_TRUSTS_NEWGROUP, tg);

  return tg;
}

trusthost *th_getbyhost(struct irc_in_addr *ip) {
  trustgroup *tg;
  trusthost *th, *result = NULL;
  uint32_t bits;

  for(tg=tglist;tg;tg=tg->next) {
    for(th=tg->hosts;th;th=th->next) {
      if(ipmask_check(ip, &th->ip, th->bits)) {
        if(!result || (th->bits > bits)) {
          bits = th->bits;
          result = th;
        }
      }
    }
  }

  return result;
}

trusthost *th_getbyhostandmask(struct irc_in_addr *ip, uint32_t bits) {
  trustgroup *tg;
  trusthost *th;

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if(ipmask_check(ip, &th->ip, 128) && th->bits == bits)
        return th;

  return NULL;
}

/* returns the ip with the smallest prefix that is still a superset of the given host */
trusthost *th_getsmallestsupersetbyhost(struct irc_in_addr *ip, uint32_t bits) {
  trustgroup *tg;
  trusthost *th, *result = NULL;
  uint32_t sbits;

  for(tg=tglist;tg;tg=tg->next) {
    for(th=tg->hosts;th;th=th->next) {
      if(ipmask_check(ip, &th->ip, th->bits)) {
        if((th->bits < bits) && (!result || (th->bits > sbits))) {
          sbits = th->bits;
          result = th;
        }
      }
    }
  }

  return result;
}

/* returns the first ip that is a subset it comes across */
trusthost *th_getsubsetbyhost(struct irc_in_addr *ip, uint32_t bits) {
  trustgroup *tg;
  trusthost *th;

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if(ipmask_check(ip, &th->ip, th->bits))
        if(th->bits > bits)
          return th;

  return NULL;
}

/* NOT reentrant obviously */
static trusthost *th_getnextchildbyhost(trusthost *orig, trusthost *th) {
  if(!th) {
    trustgroup *tg;

    tg = tglist;
    for(tg=tglist;tg;tg=tg->next) {
      th = tg->hosts;
      if(th)
        break;
    }

    /* INVARIANT: tg => th */
    if(!tg)
      return NULL;

    if(th->parent == orig)
      return th;
  }

  for(;;) {
    if(th->next) {
      th = th->next;
    } else {
      trustgroup *tg = th->group;

      do {
        tg = tg->next;
      } while (tg && !tg->hosts);

      if(!tg)
        return NULL;

      th = tg->hosts;
    }

    if(th->parent == orig)
      return th;
  }
}

void th_getsuperandsubsets(struct irc_in_addr *ip, uint32_t bits, trusthost **superset, trusthost **subset) {
  *superset = th_getsmallestsupersetbyhost(ip, bits);
  *subset = th_getsubsetbyhost(ip, bits);
}

void trusts_flush(void (*thflush)(trusthost *), void (*tgflush)(trustgroup *)) {
  trustgroup *tg;
  trusthost *th;
  time_t t = getnettime();

  for(tg=tglist;tg;tg=tg->next) {
    if(tg->count > 0)
      tg->lastseen = t;

    tgflush(tg);

    for(th=tg->hosts;th;th=th->next) {
      if(th->count > 0)
        th->lastseen = t;

      thflush(th);
    }
  }
}

trustgroup *tg_strtotg(char *name) {
  unsigned long id;
  trustgroup *tg;

  /* legacy format */
  if(name[0] == '#') {
    char *endp;
    id = strtoul(&name[1], &endp, 10);
    if(!id || *endp)
      return NULL;

    return tg_getbyid(id);
  }

  for(tg=tglist;tg;tg=tg->next)
    if(!strcasecmp(name, tg->name->content))
      return tg;

  return NULL;
}

void th_adjusthosts(trusthost *th, trusthost *superset, trusthost *subset) {
  /*
   * First and foremost, CIDR doesn't allow hosts to cross boundaries, i.e. everything with a smaller prefix
   * is entirely contained with the prefix that is one smaller.
   * e.g. 0.0.0.0/23, 0.0.0.128/23, you can't have a single prefix for 0.0.0.64-0.0.0.192, instead
   * you have two, 0.0.0.64/26 and 0.0.0.128/26.
   *
   * This makes the code MUCH easier as the entire thing is one huge set/tree.
   *
   * Four cases here:
   * 1: host isn't covered by any existing hosts.
   * 2: host is covered by a less specific one only, e.g. adding 0.0.0.1/32, while 0.0.0.0/24 already exists.
   * 3: host is covered by a more specific one only, e.g. adding 0.0.0.0/24 while 0.0.0.1/32 already exists
   *    (note there might be more than one more specific host, e.g. 0.0.0.1/32 and 0.0.0.2/32).
   * 4: covered by more and less specific cases, e.g. adding 0.0.0.0/24 to: { 0.0.0.1/32, 0.0.0.2/32, 0.0.0.0/16 }.
   *
   * CASE 1
   * ------
   *
   * !superset && !subset
   *
   * Scan through the host hash and add any clients which match our host, this is exactly the same as case 3
   * but without needing to check (though checking doesn't hurt), so we'll just use the code for that.
   *
   * CASE 2
   * ------
   *
   * superset && !subset
   *
   * We have the less specific host in 'superset', we know it is the only one so pull out clients in it's
   * ->users list matching our new host.
   * No need to look for extra hosts in the main nick hash as they're all covered already.
   *
   * CASE 3
   * ------
   *
   * !superset && subset
   *
   * We have one host in 'subset', but there might be more than one, we don't care though!
   * We can scan the entire host hash and pull out any hosts that match us and don't have
   * a trust group already, this ignores any with a more specific prefix.
   *
   * CASE 4
   * ------
   *
   * superset && subset
   *
   * Here we first fix up the ones less specific then us, so we just perform what we did for case 2,
   * then we perform what we did for case 3.
   *
   * So in summary:
   *   CASE 1: DO 3
   *   CASE 2: (work)
   *   CASE 3: (work)
   *   CASE 4: DO 2; DO 3
   * Or:
   *   if(2 || 4)     : DO 2
   *   if(1 || 3 || 4): DO 3
   */

  /* we let the compiler do the boolean minimisation for clarity reasons */

  if((superset && !subset) || (superset && subset)) { /* cases 2 and 4 */
    nick *np, *nnp;
    for(np=superset->users;np;np=nnp) {
      nnp = nextbytrust(np);
      if(ipmask_check(&np->p_nodeaddr, &th->ip, th->bits)) {
        trusts_lostnick(np, 1);
        trusts_newnick(np, 1);
      }
    }
  }

  if((!superset && !subset) || (!superset && subset) || (superset && subset)) { /* cases 1, 3 and 4 */
    nick *np;
    int i;

    for(i=0;i<NICKHASHSIZE;i++)
      for(np=nicktable[i];np;np=np->next)
        if(!gettrusthost(np) && ipmask_check(&np->p_nodeaddr, &th->ip, th->bits))
          trusts_newnick(np, 1);
  }
}

unsigned int nexttgmarker(void) {
  static unsigned int tgmarker = 0;
  trustgroup *tg;

  tgmarker++;
  if(!tgmarker) {
    /* If we wrapped to zero, zap the marker on all groups */
    for(tg=tglist;tg;tg=tg->next)
      tg->marker=0;

    tgmarker++;
  }

  return tgmarker;
}

unsigned int nextthmarker(void) {
  static unsigned int thmarker = 0;
  trustgroup *tg;
  trusthost *th;

  thmarker++;
  if(!thmarker) {
    /* If we wrapped to zero, zap the marker on all hosts */
    for(tg=tglist;tg;tg=tg->next)
      for(th=tg->hosts;th;th=th->next)
        th->marker=0;

    thmarker++;
  }

  return thmarker;
}

trusthost *th_getbyid(unsigned int id) {
  trustgroup *tg;
  trusthost *th;

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if(th->id == id)
        return th;

  return NULL;
}

int tg_modify(trustgroup *oldtg, trustgroup *newtg) {
  trustgroup vnewtg;

  memcpy(&vnewtg, oldtg, sizeof(trustgroup));

  /* unfortunately we can't just memcpy the new one over */

  vnewtg.name = getsstring(newtg->name->content, TRUSTNAMELEN);
  vnewtg.createdby = getsstring(newtg->createdby->content, CREATEDBYLEN);
  vnewtg.contact = getsstring(newtg->contact->content, CONTACTLEN);
  vnewtg.comment = getsstring(newtg->comment->content, COMMENTLEN);
  if(!vnewtg.name || !vnewtg.createdby || !vnewtg.contact || !vnewtg.comment) {
    freesstring(vnewtg.name);
    freesstring(vnewtg.createdby);
    freesstring(vnewtg.contact);
    freesstring(vnewtg.comment);
    return 0;
  }

  /* id remains the same, count/hosts/marker/next/exts are ignored */
  vnewtg.trustedfor = newtg->trustedfor;
  vnewtg.mode = newtg->mode;
  vnewtg.maxperident = newtg->maxperident;
  vnewtg.maxusage = newtg->maxusage;
  vnewtg.expires = newtg->expires;
  vnewtg.lastseen = newtg->lastseen;
  vnewtg.lastmaxusereset = newtg->lastmaxusereset;

  memcpy(oldtg, &vnewtg, sizeof(trustgroup));

  return 1;
}

int th_modify(trusthost *oldth, trusthost *newth) {
  oldth->maxpernode = newth->maxpernode;
  oldth->nodebits = newth->nodebits;

  return 1;
}

