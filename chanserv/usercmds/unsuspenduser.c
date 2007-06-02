/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: unsuspenduser
 * CMDLEVEL: QCMD_OPER
 * CMDARGS: 1
 * CMDDESC: Unsuspend a user.
 * CMDFUNC: csu_dounsuspenduser
 * CMDPROTO: int csu_dounsuspenduser(void *source, int cargc, char **cargv);
 */

#include "../chanserv.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>

int csu_dounsuspenduser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *vrup;
  char action[100];
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unsuspenduser");
    return CMD_ERROR;
  }
  
  if (cargv[0][0] == '#') {
    if (!(vrup=findreguserbynick(&cargv[0][1]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, &cargv[0][1]);
      return CMD_ERROR;
    }
  }
  else {
    nick *np;
    
    if (!(np=getnickbynick(cargv[0]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, cargv[0]);
      return CMD_ERROR;
    }
    
    if (!(vrup=getreguserfromnick(np)) && sender) {
      chanservstdmessage(sender, QM_USERNOTAUTHED, cargv[0]);
      return CMD_ERROR;
    }
  }
  
  if (!UHasSuspension(vrup)) {
    chanservstdmessage(sender, QM_USERNOTSUSPENDED, cargv[0]);
    return CMD_ERROR;
  }
  
  if (UHasOperPriv(vrup) && !UHasAdminPriv(rup)) {
    snprintf(action, 99, "unsuspenduser on %s", vrup->username);
    chanservstdmessage(sender, QM_NOACCESS, action);
    chanservwallmessage("%s (%s) FAILED to unsuspend %s", sender->nick, rup->username, vrup->username);
    return CMD_ERROR;
  }
  
  if (UIsDelayedGline(vrup)) {
    strcpy(action, "removed delayed gline on");
  }
  else if (UIsGline(vrup)) {
    strcpy(action, "removed instant gline on");
  }
  else if (UIsSuspended(vrup)) {
    strcpy(action, "unsuspended");
  }
  else if (UIsNeedAuth(vrup)) {
    strcpy(action, "enabled");
  }
  else {
    chanservsendmessage(sender, "Unknown suspend type encountered.");
    return CMD_ERROR;
  }
  
  vrup->flags&=(~(QUFLAG_GLINE|QUFLAG_DELAYEDGLINE|QUFLAG_SUSPENDED|QUFLAG_NEEDAUTH));
  vrup->suspendby=0;
  vrup->suspendexp=0;
  freesstring(vrup->suspendreason);
  vrup->suspendreason=0;
  csdb_updateuser(vrup);
  
  chanservwallmessage("%s (%s) %s %s", sender->nick, rup->username, action, vrup->username);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}