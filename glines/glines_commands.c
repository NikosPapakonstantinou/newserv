#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/version.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "glines.h"
#include "../trusts/trusts.h"

MODULE_VERSION("");

static void registercommands(int, void *);
static void deregistercommands(int, void *);

static int glines_cmdblock(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nick *target;
  int hits, duration;
  char *reason;
  char creator[32];

  if (cargc < 3)
    return CMD_USAGE;

  target = getnickbynick(cargv[0]);

  if (!target) {
    controlreply(sender, "Sorry, couldn't find that user.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[1]);

  if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);
  hits = glinebynick(target, duration, reason, 0, creator);

  controlwall(NO_OPER, NL_GLINES, "%s BLOCK'ed user '%s!%s@%s' for %s with reason '%s' (%d hits)", controlid(sender), target->nick, target->ident, target->host->name->content, longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Total hits: %d", hits);

  return CMD_OK;
}

static int glines_cmdrawgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int duration, hits;
  char *mask, *reason;
  char creator[32];
  gline *gl;

  if (cargc < 3)
    return CMD_USAGE;

  mask = cargv[0];

  duration = durationtolong(cargv[1]);

  if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

#ifdef SNIRCD_13
  gl = findgline(mask);

  if (gl) {
     /* snircd 1.3:  warn opers that they can't modify this gline */
    if (gl->flags & GLINE_ACTIVE) {
      controlreply(sender, "Active G-Line already exists on %s - unable to modify", cargv[0]);
      return CMD_ERROR;
    }

    controlreply(sender, "Reactivating existing gline on %s", mask);
  }
#endif

  gl = makegline(mask);
  hits = gline_count_hits(gl);
  freegline(gl);

  if (gl->flags & GLINE_BADCHAN && hits > MAXGLINECHANNELS) {
    controlreply(sender, "This G-Line would hit %d channels. Limit is %d. NOT SET.", hits, MAXGLINECHANNELS);
    return CMD_ERROR;
  } else if (hits > MAXGLINEUSERS) {
    controlreply(sender, "This G-Line would hit %d users. Limit is %d. NOT SET.", hits, MAXGLINEUSERS);
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);
  glinesetmask(mask, duration, reason, creator);

  controlwall(NO_OPER, NL_GLINES, "%s RAWGLINE'd mask '%s' for %s with reason '%s' (%d hits)", controlid(sender), mask, longtoduration(duration, 0), reason, hits);


  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdrawglinesimulate(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *mask, *type;
  gline *gl;
  int hits;

  if (cargc < 1)
    return CMD_USAGE;

  mask = cargv[0];

  gl = makegline(mask);

  if (!gl) {
    controlreply(sender, "Invalid G-Line mask.");
    return CMD_ERROR;
  }

  hits = gline_count_hits(gl);
  freegline(gl);

  if (gl->flags & GLINE_BADCHAN)
    type = "channel";
  else
    type = "client";

  controlreply(sender, "G-Line on '%s' would hit %d %s%s.", mask, hits, type, (hits == 1) ? "" : "s");

  return CMD_OK;
}

static int glines_cmdgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *origmask;
  char mask[512];
  char *p, *user, *host;
  struct irc_in_addr ip;
  unsigned char bits;
  int count, duration;
  char *reason;
  char creator[32];

  if (cargc < 3)
    return CMD_USAGE;

  origmask = cargv[0];

  if (origmask[0] == '#' || origmask[0] == '&') {
    controlreply(sender, "Please use rawgline for badchans and regex glines.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[1]);

  if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  strncpy(mask, origmask, sizeof(mask));

  if (strchr(mask, '!')) {
    controlreply(sender, "Use rawgline to place nick glines.");
    return CMD_ERROR;
  }

  p = strchr(mask, '@');

  if (!p) {
    controlreply(sender, "Mask must contain a username (e.g. user@ip).");
    return CMD_ERROR;
  }

  user = mask;
  host = p + 1;
  *p = '\0';

  if (strchr(user, '*') || strchr(user, '?')) {
    controlreply(sender, "Usernames may not contain wildcards.");
    return CMD_ERROR;
  }

  if (!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid CIDR mask.");
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);
  count = glinebyip(user, &ip, 128, duration, reason, 0, creator);

  controlreply(sender, "Total hits: %d", count);

  return CMD_OK;
}

static int glines_cmdungline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline *gl;

  if (cargc < 1)
    return CMD_USAGE;

  gl = findgline(cargv[0]);

  if (!gl) {
    controlreply(sender, "No such G-Line.");
    return CMD_ERROR;
  }

  if (!(gl->flags & GLINE_ACTIVE)) {
    controlreply(sender, "G-Line was already deactivated.");
    return CMD_ERROR;
  }

  gline_deactivate(gl, 0, 1);

  controlreply(sender, "G-Line deactivated.");

  return CMD_OK;
}

static int glines_cmdclearchan(void *source, int cargc, char **cargv) {
  nick *sender = source;
  channel *cp;
  nick *np;
  char *reason = "Clearing channel.";
  int mode, duration, i, slot;
  array victims;
  char creator[32];

  if (cargc < 2)
    return CMD_USAGE;

  cp = findchannel(cargv[0]);

  if (!cp) {
    controlreply(sender, "Couldn't find that channel.");
    return CMD_ERROR;
  }

  if (strcmp(cargv[1], "kick") == 0)
    mode = 0;
  else if (strcmp(cargv[1], "kill") == 0)
    mode = 1;
  else if (strcmp(cargv[1], "gline") == 0)
    mode = 2;
  else if (strcmp(cargv[1], "glineall") == 0)
    mode = 3;
  else
    return CMD_USAGE;

  if (mode == 0 || mode == 1) {
    if (cargc >= 3) {
      rejoinline(cargv[2], cargc - 2);
      reason = cargv[2];
    }
  } else {
    if (cargc < 3)
      return CMD_USAGE;

    duration = durationtolong(cargv[2]);

    if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
      controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
      return CMD_ERROR;
    }

    if (cargc >= 4)
      reason = cargv[3];
  }

  array_init(&victims, sizeof(nick *));

  /* we need to make a list of the channel users here because
   * kicking/killing them will affect their position in the channel's
   * user list. */
  for (i = 0; i < cp->users->hashsize; i++) {
    if (cp->users->content[i] == nouser)
      continue;

    np = getnickbynumeric(cp->users->content[i]);

    if (!np)
      continue;

    if (IsService(np) || IsOper(np))
      continue;

    slot = array_getfreeslot(&victims);
    (((nick **)victims.content)[slot]) = np;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  for (i = 0; i < victims.cursi; i++) {
    np = ((nick **)victims.content)[i];

    switch (mode) {
      case 0:
        localkickuser(NULL, cp, np, reason);
        break;
      case 1:
        killuser(NULL, np, "%s", reason);
        break;
      case 2:
        if (IsAccount(np))
          break;
        /* fall through */
      case 3:
        glinebynick(np, duration, reason, 0, creator);
        break;
      default:
        assert(0);
    }
  }

  array_free(&victims);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdtrustgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  int duration, count;
  char *reason;
  char mask[512];
  char creator[32];

  if (cargc < 4)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);

  if (!(tg->flags & TRUST_RELIABLE_USERNAME)) {
    controlreply(sender, "Sorry, that trust group does not have the \"reliable username\" flag.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[2]);

  if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[3];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  count = 0;

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  for(th = tg->hosts; th; th = th->next) {
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], trusts_cidr2str(&th->ip, th->bits));
    count += glinesetmask(mask, duration, reason, creator);
  }

  controlreply(sender, "Done. %d G-Lines set.", count);

  return CMD_OK;
}

static int glines_cmdtrustungline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  char mask[512];
  int count;

  if (cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);

  if (!(tg->flags & TRUST_RELIABLE_USERNAME)) {
    controlreply(sender, "Sorry, that trust group does not have the \"reliable username\" flag.");
    return CMD_ERROR;
  }

  count = 0;

  for (th = tg->hosts; th; th = th->next) {
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], trusts_cidr2str(&th->ip, th->bits));
    count += glineunsetmask(mask);
  }

  controlreply(sender, "Done. %d G-Lines deactivated.", count);

  return CMD_OK;
}

static int glines_cmdglstats(void *source, int cargc, char **cargv) {
  nick* sender = (nick*)source;
  gline* g;
  gline* sg;
  time_t curtime = getnettime();
  int glinecount = 0, hostglinecount = 0, ipglinecount = 0, badchancount = 0, rnglinecount = 0;
  int deactivecount = 0, activecount = 0;

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expire <= curtime) {
      g->flags &= ~GLINE_ACTIVE;
    }

    if (g->flags & GLINE_ACTIVE) {
      activecount++;
    } else {
      deactivecount++;
    }

    if (g->flags & GLINE_IPMASK)
      ipglinecount++;
    else if (g->flags & GLINE_HOSTMASK)
      hostglinecount++;
    else if (g->flags & GLINE_REALNAME)
      rnglinecount++;
    else if (g->flags & GLINE_BADCHAN)
      badchancount++;
    glinecount++;
  }

  controlreply(sender, "Total G-Lines set: %d", glinecount);
  controlreply(sender, "Hostmask G-Lines:  %d", hostglinecount);
  controlreply(sender, "IPMask G-Lines:    %d", ipglinecount);
  controlreply(sender, "Channel G-Lines:   %d", badchancount);
  controlreply(sender, "Realname G-Lines:  %d", rnglinecount);

  controlreply(sender, "Active G-Lines:    %d", activecount);
  controlreply(sender, "Inactive G-Lines:  %d", deactivecount);

  /* TODO show top 10 creators here */
  /* TODO show unique creators count */
  /* TODO show glines per create %8.1f", ccount?((float)gcount/(float)ccount):0 */
  return CMD_OK;
}

static int glines_cmdglist(void *source, int cargc, char **cargv) {
  nick *sender = (nick *)source;
  gline *g;
  gline *sg;
  time_t curtime = time(NULL);
  int flags = 0;
  char *mask;
  int count = 0;
  int limit = 500;
  char tmp[250];

  if (cargc < 1 || (cargc == 1 && cargv[0][0] == '-')) {
    controlreply(sender, "Syntax: glist [-flags] <mask>");
    controlreply(sender, "Valid flags are:");
    controlreply(sender, "-c: Count G-Lines.");
    controlreply(sender, "-f: Find G-Lines active on <mask>.");
    controlreply(sender, "-x: Find G-Lines matching <mask> exactly.");
    controlreply(sender, "-R: Find G-lines on realnames.");
    controlreply(sender, "-o: Search for glines by owner.");
    controlreply(sender, "-r: Search for glines by reason.");
    controlreply(sender, "-i: Include inactive glines.");
    return CMD_ERROR;
  }

  mask = cargv[0];

  if (cargc > 1) {
    char* ch = cargv[0];

    for (; *ch; ch++)
      switch (*ch) {
      case '-':
        break;

      case 'c':
        flags |= GLIST_COUNT;
        break;

      case 'f':
        flags |= GLIST_FIND;
        break;

      case 'x':
        flags |= GLIST_EXACT;
        break;

      case 'r':
        flags |= GLIST_REASON;
        break;
      case 'o':
        flags |= GLIST_OWNER;
        break;

      case 'R':
        flags |= GLIST_REALNAME;
        break;

      case 'i':
        flags |= GLIST_INACTIVE;
        break;

      default:
        controlreply(sender, "Invalid flag '%c'.", *ch);
        return CMD_ERROR;
      }

      mask = cargv[1];
  } else {
    mask = cargv[0];
  }

  if ((flags & (GLIST_EXACT|GLIST_FIND)) == (GLIST_EXACT|GLIST_FIND)) {
    controlreply(sender, "You cannot use -x and -f flags together.");
    return CMD_ERROR;
  }

  if (!(flags & GLIST_COUNT))
    controlreply(sender, "%-50s %-19s %-25s %s", "Mask:", "Expires in:", "Creator:", "Reason:");

  gline *searchgl = makegline(mask);

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expire <= curtime) {
      g->flags &= ~GLINE_ACTIVE;
    }

    if (!(g->flags & GLINE_ACTIVE)) {
      if (!(flags & GLIST_INACTIVE)) {
        continue;
      }
    }

    if (flags & GLIST_REALNAME) {
      if (!(g->flags & GLINE_REALNAME))
        continue;
      if (flags & GLIST_EXACT) {
        if (!glineequal(searchgl, g)) {
          continue;
        }
      } else if (flags & GLIST_FIND) {
        if (!gline_match_mask(searchgl, g)) {
          continue;
        }
      }
    } else {
      if (g->flags & GLINE_REALNAME)
        continue;

      if (flags & GLIST_REASON) {
        if (flags & GLIST_EXACT) {
          if (g->reason && ircd_strcmp(mask, g->reason->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (g->reason && match(g->reason->content, mask))
            continue;
        } else if (g->reason && match(mask, g->reason->content))
            continue;
      } else if (flags & GLIST_OWNER) {
        if (flags & GLIST_EXACT) {
          if (g->creator && ircd_strcmp(mask, g->creator->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (g->creator && match(g->creator->content, mask))
            continue;
        } else if (g->creator && match(mask, g->creator->content))
          continue;
      } else {
        if (flags & GLIST_EXACT) {
          if (!glineequal(searchgl, g)) {
            continue;
          }
        } else if (flags & GLIST_FIND) {
          if (!gline_match_mask(searchgl, g)) {
            continue;
          }
        }
      }
    }

    if (count == limit && !(flags & GLIST_COUNT))
      controlreply(sender, "More than %d matches, list truncated.", limit);

    count++;

    if (!(flags & GLIST_COUNT) && (count < limit)) {
      snprintf(tmp, 249, "%s", glinetostring(g));
      controlreply(sender, "%s%-49s %-19s %-25s %s", g->flags & GLINE_ACTIVE ? "+" : "-", tmp, g->flags & GLINE_ACTIVE ? (char*)longtoduration(g->expire - curtime, 0) : "<inactive>",
            g->creator ? g->creator->content : "", g->reason ? g->reason->content : "");
    }
  }

  controlreply(sender, "%s%d G-Line%s found.", (flags & GLIST_COUNT) ? "" : "End of list - ", count, count == 1 ? "" : "s");

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if (commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("block", NO_OPER, 3, glines_cmdblock, "Usage: block <nick> <duration> <reason>\nSets a gline using an appropriate mask given the user's nickname.");
  registercontrolhelpcmd("rawgline", NO_OPER, 3, glines_cmdrawgline, "Usage: rawgline <mask> <duration> <reason>\nSets a gline.");
  registercontrolhelpcmd("rawglinesimulate", NO_OPER, 1, glines_cmdrawglinesimulate, "Usage: rawglinesimulate <mask>\nSimulates what happens when a gline is set.");
  registercontrolhelpcmd("gline", NO_OPER, 3, glines_cmdgline, "Usage: gline <user@host> <duration> <reason>\nSets a gline. Automatically adjusts the mask so as not to hit unrelated trusted users.");
  registercontrolhelpcmd("ungline", NO_OPER, 1, glines_cmdungline, "Usage: ungline <mask>\nDeactivates a gline.");
  registercontrolhelpcmd("clearchan", NO_OPER, 4, glines_cmdclearchan, "Usage: clearchan <#channel> <how> <duration> ?reason?\nClears a channel.\nhow can be one of:\nkick - Kicks users.\nkill - Kills users.\ngline - Glines non-authed users (using an appropriate mask).\nglineall - Glines users.\nDuration is only valid when glining users. Reason defaults to \"Clearing channel.\".");
  registercontrolhelpcmd("trustgline", NO_OPER, 4, glines_cmdtrustgline, "Usage: trustgline <#id|name> <user> <duration> <reason>\nSets a gline on the specified username for each host in the specified trust group. The username may contain wildcards.");
  registercontrolhelpcmd("trustungline", NO_OPER, 2, glines_cmdtrustungline, "Usage: trustungline <#id|name> <user>\nRemoves a gline that was previously set with trustgline.");
  registercontrolhelpcmd("glstats", NO_OPER, 0, glines_cmdglstats, "Usage: glstat\nShows statistics about G-Lines.");
  registercontrolhelpcmd("glist", NO_OPER, 2, glines_cmdglist, "Usage: glist [-flags] <mask>\nLists matching G-Lines.\nValid flags are:\n-c: Count G-Lines.\n-f: Find G-Lines active on <mask>.\n-x: Find G-Lines matching <mask> exactly.\n-R: Find G-lines on realnames.\n-o: Search for glines by owner.\n-r: Search for glines by reason.\n-i: Include inactive glines.");
}

static void deregistercommands(int hooknum, void *arg) {
  if (!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("block", glines_cmdblock);
  deregistercontrolcmd("rawgline", glines_cmdrawgline);
  deregistercontrolcmd("rawglinesimulate", glines_cmdrawglinesimulate);
  deregistercontrolcmd("gline", glines_cmdgline);
  deregistercontrolcmd("ungline", glines_cmdungline);
  deregistercontrolcmd("clearchan", glines_cmdclearchan);
  deregistercontrolcmd("trustgline", glines_cmdtrustgline);
  deregistercontrolcmd("trustungline", glines_cmdtrustungline);
  deregistercontrolcmd("glstats", glines_cmdglstats);
  deregistercontrolcmd("glist", glines_cmdglist);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if (trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);
}
