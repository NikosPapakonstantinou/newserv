/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: accounthistory
 * CMDLEVEL: QCMD_OPER
 * CMDARGS: 1
 * CMDDESC: View password/email history for an account.
 * CMDFUNC: csa_doaccounthistory
 * CMDPROTO: int csa_doaccounthistory(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: accounthistory <account>
 * CMDHELP: Shows password/email history for the specified account.
 */

#include "../chanserv.h"
#include "../../lib/irc_string.h"
#include "../../dbapi/dbapi.h"

#include <stdio.h>
#include <string.h>

void csdb_doaccounthistory_real(DBConn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  reguser *rup;
  unsigned int userID;
  char *oldpass, *newpass, *oldemail, *newemail;
  time_t changetime, authtime;
  DBResult *pgres;
  int count=0;
  char tbuf[TIMELEN];

  if(!dbconn)
    return;

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv", ERR_ERROR, "Error loading account history data.");
    return;
  }

  if (dbnumfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Account history data format error.");
    dbclear(pgres);
    return;
  }

  if (!np) {
    dbclear(pgres);
    return;
  }

  if (!(rup=getreguserfromnick(np)) || !UHasOperPriv(rup)) {
    Error("chanserv", ERR_ERROR, "No reguser pointer or oper privs in account history.");
    dbclear(pgres);
    return;
  }

  /* @TIMELEN */
  chanservsendmessage(np, "Number: Time:               Old password:  New password:  Old email:                     New email:");
  while(dbfetchrow(pgres)) {
    userID=strtoul(dbgetvalue(pgres, 0), NULL, 10);
    changetime=strtoul(dbgetvalue(pgres, 1), NULL, 10);
    authtime=strtoul(dbgetvalue(pgres, 2), NULL, 10);
    oldpass=dbgetvalue(pgres, 3);
    newpass=dbgetvalue(pgres, 4);
    oldemail=dbgetvalue(pgres, 5);
    newemail=dbgetvalue(pgres, 6);
    q9strftime(tbuf, sizeof(tbuf), changetime);
    chanservsendmessage(np, "#%-6d %-19s %-14s %-14s %-30s %s", ++count, tbuf, oldpass, newpass, oldemail, newemail); /* @TIMELEN */
  }
  chanservstdmessage(np, QM_ENDOFLIST);

  dbclear(pgres);
}

void csdb_retreiveaccounthistory(nick *np, reguser *rup, int limit) {
  q9u_asyncquery(csdb_doaccounthistory_real, (void *)np->numeric,
    "SELECT userID, changetime, authtime, oldpassword, newpassword, oldemail, newemail from chanserv.accounthistory where "
    "userID=%u order by changetime desc limit %d", rup->ID, limit);
}

int csa_doaccounthistory(void *source, int cargc, char **cargv) {
  reguser *rup, *trup;
  nick *sender=source;
  
  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "accounthistory");
    return CMD_ERROR;
  }
  
  if (!(trup=findreguser(sender, cargv[0])))
    return CMD_ERROR;
  
  if ((rup != trup) && UHasOperPriv(trup) && !UHasAdminPriv(rup)) {
    chanservstdmessage(sender, QM_NOACCESS, "accounthistory", cargv[0]);
    return CMD_ERROR;
  }
  
  csdb_retreiveaccounthistory(sender, trup, 10);
  
  return CMD_OK;
}
