/* 
 * chanservnetevents.c:
 *  Handles the bot reaction to network events (joins, parts, mode changes
 *  etc.)
 */

#include "chanserv.h"

#include "../channel/channel.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"

#include <stdio.h>

/* cs_handlenick:
 *  This is for HOOK_NICK_NEWNICK and HOOK_NICK_ACCOUNT.
 *  Associate the nick with it's correct account.
 */

void cs_handlenick(int hooknum, void *arg) {
  nick *np=(nick *)arg;

  cs_checknick(np);
}

void cs_handlesethost(int hooknum, void *arg) {
  nick *np=arg;

  cs_checknickbans(np);
}

void cs_handlelostnick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  nicklist **nlh;
  nicklist *nlp;
  reguser *rup;
  
  if ((rup=getreguserfromnick(np))) {
    for (nlh=&(rup->nicks);*nlh;nlh=&((*nlh)->next)) {
      if ((*nlh)->np==np) {
        nlp=*nlh;
        *nlh=nlp->next;
        freenicklist(nlp);
        break;
      }
    }
    if ((rup->status & QUSTAT_DEAD) && !rup->nicks) {
      freereguser(rup);
    }	
  }
  
  if (getactiveuserfromnick(np))
    freeactiveuser(getactiveuserfromnick(np));
}

/*
 * cs_handlenewchannel:
 *  This is for the HOOK_CHANNEL_NEWCHANNEL message.
 *  A new channel has just been created on the network, associate
 *  with the registered channel (if one exists) unless it's suspended.
 */

void cs_handlenewchannel(int hooknum, void *arg) {
  channel *cp=(channel *)arg;
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup;
  nick *np;
  int i;

  /* Get the registered channel */
  if ((rcp=(regchan *)cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  /*
   * If we're supposed to be joined, join ourselves..
   */

  if (CIsJoined(rcp) && (chanservnick!=NULL)) {
    chanservjoinchan(cp);
  }

  /* This code interacts badly with bursts.. need to fix */
  
#if 0
  /* We need to watch out for people sneaking into +k/i channels..
   * This code relies on the fact that that user will already be in 
   * the channel at this point in time.. */
  if (rcp->forcemodes & (CHANMODE_KEY | CHANMODE_INVITEONLY)) {
    /* OK, this channel is keyed.. let's find out if the user is allowed to be here */
    for (i=0;i<cp->users->hashsize;i++) {
      if (cp->users->content[i]!=nouser && (np=getnickbynumeric(cp->users->content[i]))) {
	if (IsService(np))
	  /* service (might even be us..) */
	  continue;

	if ((rup=getreguserfromnick(np)) && (rcup=findreguseronchannel(rcp, rup)) &&
	    CUKnown(rcup) && !CUIsBanned(rcup))
	  /* legit user */
	  continue;

	localkickuser(chanservnick, cp, np, "Private channel.");
      }
    }
  }
#endif

  /* Make sure the right modes are set/cleared */
  cs_checkchanmodes(cp);
  
  /* Start the timer rolling */
  cs_timerfunc(cp->index);

  /* If topicsave or forcetopic is set, update the topic */
  if (CIsForceTopic(rcp) || CIsTopicSave(rcp)) {
    localsettopic(chanservnick, cp, (rcp->topic) ? rcp->topic->content : "");
  }
}

void cs_handlechanlostuser(int hooknum, void *arg) {
  void **args=(void **)arg;
  channel *cp=args[0];
  regchan *rcp;

  if (!(rcp=cp->index->exts[chanservext]) || CIsSuspended(rcp) || !chanservnick ||
      (chanservnick==args[1]))
    return;

  if (!CIsJoined(rcp))
    return;

  if (cp->users->totalusers==2 &&
      getnumerichandlefromchanhash(cp->users, chanservnick->numeric)) {
    /* OK, Q is on the channel and not the one leaving.. */
    rcp->lastpart=time(NULL);
    cs_schedupdate(cp->index, 1, 5);
  }
}
      

/*
 * cs_handlelostchannel:
 *  This is for the HOOK_CHANNEL_LOSTCHANNEL message.
 *  A channel has just disappeared, clear our association with it.
 */

void cs_handlelostchannel(int hooknum, void *arg) {
  /*  channel *cp=(channel *)arg; */
  
  /* Do we actually need to do anything here? */
}

/* 
 * cs_handlejoin:
 *  A user joined a channel.  See if we need to op/etc. them.
 *  Use this for JOIN or CREATE. 
 */

void cs_handlejoin(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  channel *cp=(channel *)arglist[0];
  nick *np=(nick *)arglist[1];
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup=NULL;
  chanindex *cip;
  int iscreate;
  int dowelcome=0;

  short modes=0;
 
  /* If not registered or suspended, ignore */
  if (!(rcp=cp->index->exts[chanservext]) || CIsSuspended(rcp))
    return;

  cip=cp->index;

  rcp->tripjoins++;
  rcp->totaljoins++;
  if (cp->users->totalusers > rcp->maxusers)
    rcp->maxusers=cp->users->totalusers;
  if (cp->users->totalusers > rcp->tripusers)
    rcp->tripusers=cp->users->totalusers;

  /* If their auth is deleted, pretend they don't exist */
  rup=getreguserfromnick(np);
  if (rup && (rup->status & QUSTAT_DEAD))
    rup=NULL;
  
  if (rup && (rcup=findreguseronchannel(rcp,rup)) && CUKnown(rcup) && cp->users->totalusers >= 3)
    rcp->lastactive=time(NULL);
  
  if (rcp->lastcountersync < (time(NULL) - COUNTERSYNCINTERVAL)) {
    csdb_updatechannelcounters(rcp);
    rcp->lastcountersync=time(NULL);
  }

  if (hooknum==HOOK_CHANNEL_CREATE) {
    iscreate=1;
  } else {
    iscreate=0;
  }  

  /* Check for "Q ban" */
  if (!IsService(np) && cs_bancheck(np,cp)) {
    /* They got kicked.. */
    return;
  }

  /* Check for other ban lurking on channel which we are enforcing */
  if (!IsService(np) && CIsEnforce(rcp) && nickbanned(np,cp)) {
    localkickuser(chanservnick,cp,np,"Banned.");
    return;
  }

  /* Check for +b chanlev flag */
  if (!IsService(np) && rcup && CUIsBanned(rcup)) {
    cs_banuser(NULL, cip, np, NULL);
    cs_timerfunc(cip);
    return;
  } 

  /* Check for +k chan flag */
  if (!IsService(np) && CIsKnownOnly(rcp) && !(rcup && CUKnown(rcup))) {
    if (IsInviteOnly(cp) || (IsRegOnly(cp) && !IsAccount(np))) {
      localkickuser(chanservnick,cp,np,"Authorised users only.");
    } else {      
      cs_banuser(NULL, cip, np, "Authorised users only.");
      cs_timerfunc(cip);
    }
    return;
  }

  if (!rup || !rcup) {
    /* They're not a registered user, so deop if it is a create */
    if (iscreate && !IsService(np)) {
      modes |= MC_DEOP;
    }
    if (CIsVoiceAll(rcp)) {
      modes |= MC_VOICE;
    }

    if (CIsWelcome(rcp)) {
      dowelcome=1;  /* Send welcome message */
    } else if (!CIsJoined(rcp) && iscreate) {
      dowelcome=2;  /* Send a generic warning */
    }
  } else {
    /* Update last use time */
    rcup->usetime=getnettime();

    /* DB update removed for efficiency..
     * csdb_updatelastjoin(rcup); */
    
    /* They are registered, let's see what to do with them */
    if (CUIsOp(rcup) && (CIsAutoOp(rcp) || CUIsAutoOp(rcup) || CUIsProtect(rcup) || CIsProtect(rcp)) && 
    	!CUIsDeny(rcup)) {
      /* Auto op */
      if (!iscreate) {
        modes |= MC_OP;
      }
    } else {
      /* Not auto op */
      if (iscreate && !CUIsOp(rcup) && !IsService(np)) {
        modes |= MC_DEOP;
      }

      if (!CUIsQuiet(rcup) &&                                                /* Not +q */
	  ((CUIsVoice(rcup) && (CIsAutoVoice(rcp) || CUIsAutoVoice(rcup) || 
	                        CIsProtect(rcp) || CUIsProtect(rcup))) ||   /* +[gp]v */
           (CIsVoiceAll(rcp)))) {                                 /* Or voice-all chan */
        modes |= MC_VOICE;
      }
    }

    if (CIsWelcome(rcp) && !CUIsHideWelcome(rcup)) {
      dowelcome=1;
    }
  }

  /* Actually do the mode change, if any */
  if (modes) {
    localsetmodes(chanservnick, cp, np, modes);
  }
  
  switch(dowelcome) {

  case 1: if (rcp->welcome)
            chanservsendmessage(np,"[%s] %s",cip->name->content, rcp->welcome->content);
          break;
    
  case 2: if (chanservnick) {
            /* Channel x is protected by y */
            chanservstdmessage(np,QM_PROTECTED,cip->name->content,chanservnick->nick);
          }
          break;
  }        

  if (rup && rcup && CIsInfo(rcp) && UIsInfo(rcup->user) && !CUIsHideInfo(rcup) && chanservnick) {
    if (rcup->info) {
      /* Chan-specific info */
      sendmessagetochannel(chanservnick, cp, "[%s] %s",np->nick, rcup->info->content);
    } else if (rup->info) {
      /* Default info */
      sendmessagetochannel(chanservnick, cp, "[%s] %s",np->nick, rup->info->content);
    }
  }
}

/* cs_handlemodechange:
 *  Handle mode change on channel 
 */

void cs_handlemodechange(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  channel *cp=(channel *)arglist[0];
  int changeflags=(int)arglist[2];
  regchan *rcp;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  if (changeflags & MODECHANGE_MODES)
    rcp->status |= QCSTAT_MODECHECK;

  if (changeflags & MODECHANGE_USERS)
    rcp->status |= QCSTAT_OPCHECK;

  if (changeflags & MODECHANGE_BANS) 
    rcp->status |= QCSTAT_BANCHECK;

  cs_timerfunc(rcp->index);
}

void cs_handleburst(int hooknum, void *arg) {
  channel *cp=(channel *)arg;
  regchan *rcp;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;
  
  /* Check everything at some future time */
  /* If autolimit is on, make sure the limit gets reset at that point too */
  if (CIsAutoLimit(rcp)) {
    rcp->limit=0;
  }
  cs_schedupdate(cp->index, 1, 5);
  rcp->status |= (QCSTAT_OPCHECK | QCSTAT_MODECHECK | QCSTAT_BANCHECK);
  rcp->lastbancheck=0; /* Force re-check of all bans on channel */
}

/* cs_handleopchange:
 *  Handle [+-][ov] event 
 */

void cs_handleopchange(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  channel *cp=(channel *)arglist[0];
  regchan *rcp;
  regchanuser *rcup;
  reguser *rup;
  nick *target=(nick *)arglist[2];
  short modes=0;
  
  /* Check that the channel is registered and active */
  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  rup=getreguserfromnick(target);

  switch(hooknum) {
    case HOOK_CHANNEL_OPPED:
      if (CIsBitch(rcp) && !IsService(target)) {
        if (!rup || (rcup=findreguseronchannel(rcp,rup))==NULL || !CUIsOp(rcup) || CUIsDeny(rcup))
          modes |= MC_DEOP;
      }
      break;
 
    /* Open question:
     *  Should +b prevent extra voices?
     *  If not, should we have a seperate mode that does? 
     */
      
    case HOOK_CHANNEL_DEOPPED:
      if (CIsProtect(rcp)) {
        if (rup && (rcup=findreguseronchannel(rcp,rup))!=NULL && CUIsOp(rcup) && !CUIsDeny(rcup))
          modes |= MC_OP;
      }
      break;
      
    case HOOK_CHANNEL_DEVOICED:
      if (CIsProtect(rcp)) {
        if (rcp && (rcup=findreguseronchannel(rcp,rup))!=NULL && CUIsVoice(rcup) && !CUIsQuiet(rcup)) 
          modes |= MC_VOICE;
      }
      break;
  }
  
  if (modes)
    localsetmodes(chanservnick, cp, target, modes);
}      

/* cs_handlenewban:
 *  Handle ban being added to channel
 */

void cs_handlenewban(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  regchan *rcp;
  channel *cp=(channel *)arglist[0];    
  chanban *cbp;
  int i;
  nick *np;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  if ((cbp=cp->bans)==NULL) {
    Error("chanserv",ERR_WARNING,"Told ban added but no bans on channel?");
    return;
  }

  if (CIsEnforce(rcp)) {
    for (i=0;i<cp->users->hashsize;i++) {
      if (cp->users->content[i]!=nouser) {
	if ((np=getnickbynumeric(cp->users->content[i]))==NULL) {
	  Error("chanserv",ERR_WARNING,"Found user on channel %s who doesn't exist!",cp->index->name->content);
	  continue;
	}
	if (!IsService(np) && nickmatchban(np,cbp)) {
	  localkickuser(chanservnick,cp,np,"Banned.");
	}
      }
    }
  }
}

/* cs_handletopicchange:
 *  Handle topic change on channel
 */

void cs_handletopicchange(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  channel *cp=(channel *)arglist[0];
  regchan *rcp;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;
 
  if (CIsForceTopic(rcp)) {
    if (rcp->topic) {
      /* Forced topic: change it back */
      localsettopic(chanservnick, cp, rcp->topic->content);
    }
  } else if (CIsTopicSave(rcp)) {
    if (rcp->topic) {
      freesstring(rcp->topic);
    }
    if (cp->topic) {
      rcp->topic=getsstring(cp->topic->content,TOPICLEN);
    } else {
      rcp->topic=NULL;
    }
    csdb_updatetopic(rcp);
  }
}
