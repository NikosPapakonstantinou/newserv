/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: deluser
 * CMDLEVEL: QCMD_DEV
 * CMDARGS: 2
 * CMDDESC: Removes a user from the bot.
 * CMDFUNC: csu_dodeluser
 * CMDPROTO: int csu_dodeluser(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: deluser <username> <reason>
 * CMDHELP: Removes the specified username from the bot.
 * CMDHELP: A reason is required and will be broadcasted.
 */

#include "../chanserv.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>

int csu_dodeluser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender), *target;

  if (!rup)
    return CMD_ERROR;
  
  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "deluser");
    return CMD_ERROR;
  }

  if (!(target=findreguser(sender, cargv[0])))
    return CMD_ERROR;
  
  if(UHasStaffPriv(target)) {
    cs_log(sender,"DELUSER FAILED username %s (%s)",target->username,cargc>1?cargv[1]:"");
    chanservwallmessage("%s (%s) just FAILED using DELUSER on %s (%s)", sender->nick, rup->username, target->username, cargv[1]);
    chanservsendmessage(sender, "Sorry, that user is privileged.");
    return CMD_ERROR;
  }

  cs_log(sender,"DELUSER OK username %s (%s)",target->username,cargv[1]);
  chanservwallmessage("%s (%s) just used DELUSER on %s (%s)", sender->nick, rup->username, target->username, cargv[1]);

  cs_removeuser(target);

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}
