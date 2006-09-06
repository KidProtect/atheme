/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains protocol support for plexus-based ircd.
 *
 * $Id: plexus.c 6297 2006-09-06 14:41:39Z pippijn $
 */

/* option: set the netadmin umode +N */
/*#define USE_NETADMIN*/

#include "atheme.h"
#include "uplink.h"
#include "pmodule.h"
#include "protocol/plexus.h"

DECLARE_MODULE_V1("protocol/plexus", TRUE, _modinit, NULL, "$Id: plexus.c 6297 2006-09-06 14:41:39Z pippijn $", "Atheme Development Group <http://www.atheme.org>");

/* *INDENT-OFF* */

ircd_t PleXusIRCd = {
        "hybrid-7.2.1+plexus-3.x family",	/* IRCd name */
        "$$",                           /* TLD Prefix, used by Global. */
        FALSE,                          /* Whether or not we use IRCNet/TS6 UID */
        FALSE,                          /* Whether or not we use RCOMMAND */
        TRUE,                           /* Whether or not we support channel owners. */
        TRUE,                           /* Whether or not we support channel protection. */
        TRUE,                           /* Whether or not we support halfops. */
	FALSE,				/* Whether or not we use P10 */
	TRUE,				/* Whether or not we use vHosts. */
	CMODE_OPERONLY,			/* Oper-only cmodes */
        CMODE_OWNER,	                /* Integer flag for owner channel flag. */
        CMODE_PROTECT,                  /* Integer flag for protect channel flag. */
        CMODE_HALFOP,                   /* Integer flag for halfops. */
        "+q",                           /* Mode we set for owner. */
        "+a",                           /* Mode we set for protect. */
        "+h",                           /* Mode we set for halfops. */
	PROTOCOL_PLEXUS,		/* Protocol type */
	0,                              /* Permanent cmodes */
	"beI",                          /* Ban-like cmodes */
	'e',                            /* Except mchar */
	'I'                             /* Invex mchar */
};

struct cmode_ plexus_mode_list[] = {
  { 'i', CMODE_INVITE },
  { 'm', CMODE_MOD    },
  { 'n', CMODE_NOEXT  },
  { 'p', CMODE_PRIV   },
  { 's', CMODE_SEC    },
  { 't', CMODE_TOPIC  },
  { 'c', CMODE_NOCOLOR },
  { 'R', CMODE_REGONLY },
  { 'O', CMODE_OPERONLY },
  { 'S', CMODE_STRIP },
  { 'K', CMODE_NOKNOCK },
  { 'N', CMODE_STICKY },
  { 'B', CMODE_BWSAVE },
  { '\0', 0 }
};

struct extmode plexus_ignore_mode_list[] = {
  { '\0', 0 }
};

struct cmode_ plexus_status_mode_list[] = {
  { 'q', CMODE_OWNER   },
  { 'a', CMODE_PROTECT },
  { 'o', CMODE_OP      },
  { 'h', CMODE_HALFOP  },
  { 'v', CMODE_VOICE   },
  { '\0', 0 }
};

struct cmode_ plexus_prefix_mode_list[] = {
  { '~', CMODE_OWNER   },
  { '&', CMODE_PROTECT },
  { '@', CMODE_OP      },
  { '%', CMODE_HALFOP  },
  { '+', CMODE_VOICE   },
  { '\0', 0 }
};

/* *INDENT-ON* */

/* login to our uplink */
static uint8_t plexus_server_login(void)
{
	int8_t ret;

	ret = sts("PASS %s :TS", curr_uplink->pass);
	if (ret == 1)
		return 1;

	me.bursting = TRUE;

	sts("CAPAB :QS EX IE KLN UNKLN ENCAP SERVICES");
	sts("SERVER %s 1 :%s", me.name, me.desc);
	sts("SVINFO 5 3 0 :%ld", CURRTIME);

	return 0;
}

/* introduce a client */
static void plexus_introduce_nick(char *nick, char *user, char *host, char *real, char *uid)
{
	sts("NICK %s 1 %ld +%s %s %s %s 0 0 :%s", nick, CURRTIME, "io", user, host, me.name, real);
}

/* invite a user to a channel */
static void plexus_invite_sts(user_t *sender, user_t *target, channel_t *channel)
{
	sts(":%s INVITE %s %s", sender->nick, target->nick, channel->name);
}

static void plexus_quit_sts(user_t *u, char *reason)
{
	if (!me.connected)
		return;

	sts(":%s QUIT :%s", u->nick, reason);
}

/* WALLOPS wrapper */
static void plexus_wallops(char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	if (config_options.silent)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	va_end(ap);

	sts(":%s WALLOPS :%s", me.name, buf);
}

/* join a channel */
static void plexus_join_sts(channel_t *c, user_t *u, boolean_t isnew, char *modes)
{
	if (isnew)
		sts(":%s SJOIN %ld %s %s :@%s", me.name, c->ts, c->name,
				modes, u->nick);
	else
		sts(":%s SJOIN %ld %s + :@%s", me.name, c->ts, c->name,
				u->nick);
}

/* kicks a user from a channel */
static void plexus_kick(char *from, char *channel, char *to, char *reason)
{
	channel_t *chan = channel_find(channel);
	user_t *user = user_find(to);

	if (!chan || !user)
		return;

	sts(":%s KICK %s %s :%s", from, channel, to, reason);

	chanuser_delete(chan, user);
}

/* PRIVMSG wrapper */
static void plexus_msg(char *from, char *target, char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	va_end(ap);

	sts(":%s PRIVMSG %s :%s", from, target, buf);
}

/* NOTICE wrapper */
static void plexus_notice(char *from, char *target, char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	va_end(ap);

	sts(":%s NOTICE %s :%s", from, target, buf);
}

static void plexus_wallchops(user_t *sender, channel_t *channel, char *message)
{
	/* not sure if we need to be on channel, oh well */
	if (chanuser_find(channel, sender))
		sts(":%s NOTICE @%s :%s", CLIENT_NAME(sender), channel->name,
				message);
	else /* do not join for this, everyone would see -- jilles */
		generic_wallchops(sender, channel, message);
}

/* numeric wrapper */
static void plexus_numeric_sts(char *from, int numeric, char *target, char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	va_end(ap);

	sts(":%s %d %s %s", from, numeric, target, buf);
}

/* KILL wrapper */
static void plexus_skill(char *from, char *nick, char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZE, fmt, ap);
	va_end(ap);

	sts(":%s KILL %s :%s!%s!%s (%s)", from, nick, from, from, from, buf);
}

/* PART wrapper */
static void plexus_part(char *chan, char *nick)
{
	user_t *u = user_find(nick);
	channel_t *c = channel_find(chan);
	chanuser_t *cu;

	if (!u || !c)
		return;

	if (!(cu = chanuser_find(c, u)))
		return;

	sts(":%s PART %s", u->nick, c->name);

	chanuser_delete(c, u);
}

/* server-to-server KLINE wrapper */
static void plexus_kline_sts(char *server, char *user, char *host, long duration, char *reason)
{
	if (!me.connected)
		return;

	sts(":%s KLINE %s %ld %s %s :%s", me.name, server, duration, user, host, reason);
}

/* server-to-server UNKLINE wrapper */
static void plexus_unkline_sts(char *server, char *user, char *host)
{
	if (!me.connected)
		return;

	sts(":%s UNKLINE %s %s %s", opersvs.nick, server, user, host);
}

/* topic wrapper */
static void plexus_topic_sts(char *channel, char *setter, time_t ts, char *topic)
{
	if (!me.connected)
		return;

	sts(":%s TOPIC %s :%s", chansvs.nick, channel, topic);
}

/* mode wrapper */
static void plexus_mode_sts(char *sender, char *target, char *modes)
{
	if (!me.connected)
		return;

	sts(":%s MODE %s %s", sender, target, modes);
}

/* ping wrapper */
static void plexus_ping_sts(void)
{
	if (!me.connected)
		return;

	sts("PING :%s", me.name);
}

/* protocol-specific stuff to do on login */
static void plexus_on_login(char *origin, char *user, char *wantedhost)
{
	user_t *u;

	if (!me.connected)
		return;

	/* Can only do this for nickserv, and can only record identified
	 * state if logged in to correct nick, sorry -- jilles
	 */
	if (nicksvs.me == NULL || irccasecmp(origin, user))
		return;

	u = user_find(origin);

	if (u == NULL)
		return;		/* realistically, this should never happen --nenolod */

#ifdef USE_NETADMIN
	if (has_priv(u, PRIV_ADMIN))
		sts(":%s ENCAP * SVSMODE %s %ld +rdN %ld", nicksvs.nick, origin, (unsigned long) u->ts, CURRTIME);
	else
#endif
		sts(":%s ENCAP * SVSMODE %s %ld +rd %ld", nicksvs.nick, origin, (unsigned long) u->ts, CURRTIME);
}

/* protocol-specific stuff to do on login */
static boolean_t plexus_on_logout(char *origin, char *user, char *wantedhost)
{
	user_t *u;

	if (!me.connected)
		return FALSE;

	if (nicksvs.me == NULL || irccasecmp(origin, user))
		return FALSE;

	u = user_find(origin);

	if (u == NULL)
		return FALSE;

#ifdef USE_NETADMIN
	sts(":%s ENCAP * SVSMODE %s %ld -rN", nicksvs.nick, origin, (unsigned long) u->ts, origin);
#else
	sts(":%s ENCAP * SVSMODE %s %ld -r", nicksvs.nick, origin, (unsigned long) u->ts, origin);
#endif
	return FALSE;
}

static void plexus_jupe(char *server, char *reason)
{
	if (!me.connected)
		return;

	sts(":%s SQUIT %s :%s", opersvs.nick, server, reason);
	sts(":%s SERVER %s 2 :%s", me.name, server, reason);
}

static void plexus_sethost_sts(char *source, char *target, char *host)
{
	if (!me.connected)
		return;

	sts(":%s ENCAP * CHGHOST %s :%s", ME, target, host);
}

static void m_topic(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	channel_t *c = channel_find(parv[0]);

	if (!c)
		return;

	handle_topic_from(si, c, si->su->nick, CURRTIME, parv[1]);
}

static void m_ping(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	/* reply to PING's */
	sts(":%s PONG %s %s", me.name, me.name, parv[0]);
}

static void m_pong(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	server_t *s;

	/* someone replied to our PING */
	if (!parv[0])
		return;
	s = server_find(parv[0]);
	if (s == NULL)
		return;
	handle_eob(s);

	if (irccasecmp(me.actual, parv[0]))
		return;

	me.uplinkpong = CURRTIME;

	/* -> :test.projectxero.net PONG test.projectxero.net :shrike.malkier.net */
	if (me.bursting)
	{
#ifdef HAVE_GETTIMEOFDAY
		e_time(burstime, &burstime);

		slog(LG_INFO, "m_pong(): finished synching with uplink (%d %s)", (tv2ms(&burstime) > 1000) ? (tv2ms(&burstime) / 1000) : tv2ms(&burstime), (tv2ms(&burstime) > 1000) ? "s" : "ms");

		wallops("Finished synching to network in %d %s.", (tv2ms(&burstime) > 1000) ? (tv2ms(&burstime) / 1000) : tv2ms(&burstime), (tv2ms(&burstime) > 1000) ? "s" : "ms");
#else
		slog(LG_INFO, "m_pong(): finished synching with uplink");
		wallops("Finished synching to network.");
#endif

		me.bursting = FALSE;
	}
}

static void m_privmsg(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	if (parc != 2)
		return;

	handle_message(si, parv[0], FALSE, parv[1]);
}

static void m_notice(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	if (parc != 2)
		return;

	handle_message(si, parv[0], TRUE, parv[1]);
}

static void m_sjoin(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	/* -> :proteus.malkier.net SJOIN 1073516550 #shrike +tn :@sycobuny @+rakaur */

	channel_t *c;
	uint8_t userc;
	char *userv[256];
	uint8_t i;
	time_t ts;

	/* :origin SJOIN ts chan modestr [key or limits] :users */
	c = channel_find(parv[1]);
	ts = atol(parv[0]);

	if (!c)
	{
		slog(LG_DEBUG, "m_sjoin(): new channel: %s", parv[1]);
		c = channel_add(parv[1], ts);
	}

	if (ts < c->ts)
	{
		chanuser_t *cu;
		node_t *n;

		/* the TS changed.  a TS change requires the following things
		 * to be done to the channel:  reset all modes to nothing, remove
		 * all status modes on known users on the channel (including ours),
		 * and set the new TS.
		 */

		clear_simple_modes(c);

		LIST_FOREACH(n, c->members.head)
		{
			cu = (chanuser_t *)n->data;
			cu->modes = 0;
		}

		slog(LG_INFO, "m_sjoin(): TS changed for %s (%ld -> %ld)", c->name, c->ts, ts);

		c->ts = ts;
		hook_call_event("channel_tschange", c);
	}

	channel_mode(NULL, c, parc - 3, parv + 2);

	userc = sjtoken(parv[parc - 1], ' ', userv);

	for (i = 0; i < userc; i++)
		chanuser_add(c, userv[i]);

	if (c->nummembers == 0 && !(c->modes & ircd->perm_mode))
		channel_delete(c->name);
}

static void m_part(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	uint8_t chanc;
	char *chanv[256];
	int i;

	if (parc < 1)
		return;
	chanc = sjtoken(parv[0], ',', chanv);
	for (i = 0; i < chanc; i++)
	{
		slog(LG_DEBUG, "m_part(): user left channel: %s -> %s", si->su->nick, chanv[i]);

		chanuser_delete(channel_find(chanv[i]), si->su);
	}
}

static void m_nick(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	user_t *u;
	server_t *s;

	/* got the right number of args for an introduction? */
	if (parc == 10)
	{
		s = server_find(parv[6]);
		if (!s)
		{
			slog(LG_DEBUG, "m_nick(): new user on nonexistant server: %s", parv[6]);
			return;
		}

		slog(LG_DEBUG, "m_nick(): new user on `%s': %s", s->name, parv[0]);

		u = user_add(parv[0], parv[4], parv[5], NULL, NULL, NULL, parv[9], s, atoi(parv[2]));

		user_mode(u, parv[3]);

		handle_nickchange(u);
	}

	/* if it's only 2 then it's a nickname change */
	else if (parc == 2)
	{
		node_t *n;

                if (!si->su)
                {       
                        slog(LG_DEBUG, "m_nick(): server trying to change nick: %s", si->s != NULL ? si->s->name : "<none>");
                        return;
                }

		slog(LG_DEBUG, "m_nick(): nickname change from `%s': %s", si->su->nick, parv[0]);

		/* remove the current one from the list */
		n = node_find(si->su, &userlist[si->su->hash]);
		node_del(n, &userlist[si->su->hash]);
		node_free(n);

		/* change the nick */
		strlcpy(si->su->nick, parv[0], NICKLEN);
		si->su->ts = atoi(parv[1]);

		/* readd with new nick (so the hash works) */
		n = node_create();
		si->su->hash = UHASH((unsigned char *)si->su->nick);
		node_add(si->su, n, &userlist[si->su->hash]);

		handle_nickchange(si->su);
	}
	else
	{
		int i;
		slog(LG_DEBUG, "m_nick(): got NICK with wrong number of params");

		for (i = 0; i < parc; i++)
			slog(LG_DEBUG, "m_nick():   parv[%d] = %s", i, parv[i]);
	}
}

static void m_quit(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	slog(LG_DEBUG, "m_quit(): user leaving: %s", si->su->nick);

	/* user_delete() takes care of removing channels and so forth */
	user_delete(si->su);
}

static void m_mode(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	if (parc < 2)
	{
		slog(LG_DEBUG, "m_mode(): missing parameters in MODE");
		return;
	}

	if (*parv[0] == '#')
		channel_mode(NULL, channel_find(parv[0]), parc - 1, &parv[1]);
	else
		user_mode(user_find(parv[0]), parv[1]);
}

static void m_kick(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	user_t *u = user_find(parv[1]);
	channel_t *c = channel_find(parv[0]);

	/* -> :rakaur KICK #shrike rintaun :test */
	slog(LG_DEBUG, "m_kick(): user was kicked: %s -> %s", parv[1], parv[0]);

	if (!u)
	{
		slog(LG_DEBUG, "m_kick(): got kick for nonexistant user %s", parv[1]);
		return;
	}

	if (!c)
	{
		slog(LG_DEBUG, "m_kick(): got kick in nonexistant channel: %s", parv[0]);
		return;
	}

	if (!chanuser_find(c, u))
	{
		slog(LG_DEBUG, "m_kick(): got kick for %s not in %s", u->nick, c->name);
		return;
	}

	chanuser_delete(c, u);

	/* if they kicked us, let's rejoin */
	if (is_internal_client(u))
	{
		slog(LG_DEBUG, "m_kick(): %s got kicked from %s; rejoining", si->su->nick, parv[0]);
		join(parv[0], u->nick);
	}
}

static void m_kill(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	if (parc < 1)
		return;
	handle_kill(si, parv[0], parc > 1 ? parv[1] : "<No reason given>");
}

static void m_squit(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	slog(LG_DEBUG, "m_squit(): server leaving: %s from %s", parv[0], parv[1]);
	server_delete(parv[0]);
}

static void m_server(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	slog(LG_DEBUG, "m_server(): new server: %s", parv[0]);
	server_add(parv[0], atoi(parv[1]), si->origin ? si->origin : me.name, NULL, parv[2]);

	if (cnt.server == 2)
		me.actual = sstrdup(parv[0]);
	else
	{
		/* elicit PONG for EOB detection; pinging uplink is
		 * already done elsewhere -- jilles
		 */
		sts(":%s PING %s %s", me.name, me.name, parv[0]);
	}

	me.recvsvr = TRUE;
}

static void m_stats(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_stats(si->su, parv[0][0]);
}

static void m_admin(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_admin(si->su);
}

static void m_version(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_version(si->su);
}

static void m_info(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_info(si->su);
}

static void m_whois(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_whois(si->su, parc >= 2 ? parv[1] : "*");
}

static void m_trace(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_trace(si->su, parc >= 1 ? parv[0] : "*", parc >= 2 ? parv[1] : NULL);
}

static void m_join(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	chanuser_t *cu;
	node_t *n, *tn;

	/* JOIN 0 is really a part from all channels */
	if (parv[0][0] == '0')
	{
		LIST_FOREACH_SAFE(n, tn, si->su->channels.head)
		{
			cu = (chanuser_t *)n->data;
			chanuser_delete(cu->chan, si->su);
		}
	}
}

static void m_pass(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	if (strcmp(curr_uplink->pass, parv[0]))
	{
		slog(LG_INFO, "m_pass(): password mismatch from uplink; aborting");
		runflags |= RF_SHUTDOWN;
	}
}

static void m_error(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	slog(LG_INFO, "m_error(): error from server: %s", parv[0]);
}

static void m_capab(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	services_init();
}

static void m_encap(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	user_t *u;

	if (!irccmp("CHGHOST", parv[1]))
	{
		u = user_find(parv[2]);

		if (!u)
			return;

		strlcpy(u->vhost, parv[3], HOSTLEN);
	}
}

static void m_motd(sourceinfo_t *si, uint8_t parc, char *parv[])
{
	handle_motd(si->su);
}

void _modinit(module_t * m)
{
	/* Symbol relocation voodoo. */
	server_login = &plexus_server_login;
	introduce_nick = &plexus_introduce_nick;
	quit_sts = &plexus_quit_sts;
	wallops = &plexus_wallops;
	join_sts = &plexus_join_sts;
	kick = &plexus_kick;
	msg = &plexus_msg;
	notice_sts = &plexus_notice;
	wallchops = &plexus_wallchops;
	numeric_sts = &plexus_numeric_sts;
	skill = &plexus_skill;
	part = &plexus_part;
	kline_sts = &plexus_kline_sts;
	unkline_sts = &plexus_unkline_sts;
	topic_sts = &plexus_topic_sts;
	mode_sts = &plexus_mode_sts;
	ping_sts = &plexus_ping_sts;
	ircd_on_login = &plexus_on_login;
	ircd_on_logout = &plexus_on_logout;
	jupe = &plexus_jupe;
	sethost_sts = &plexus_sethost_sts;
	invite_sts = &plexus_invite_sts;

	mode_list = plexus_mode_list;
	ignore_mode_list = plexus_ignore_mode_list;
	status_mode_list = plexus_status_mode_list;
	prefix_mode_list = plexus_prefix_mode_list;

	ircd = &PleXusIRCd;

	pcommand_add("PING", m_ping);
	pcommand_add("PONG", m_pong);
	pcommand_add("PRIVMSG", m_privmsg);
	pcommand_add("NOTICE", m_notice);
	pcommand_add("SJOIN", m_sjoin);
	pcommand_add("PART", m_part);
	pcommand_add("NICK", m_nick);
	pcommand_add("QUIT", m_quit);
	pcommand_add("MODE", m_mode);
	pcommand_add("KICK", m_kick);
	pcommand_add("KILL", m_kill);
	pcommand_add("SQUIT", m_squit);
	pcommand_add("SERVER", m_server);
	pcommand_add("STATS", m_stats);
	pcommand_add("ADMIN", m_admin);
	pcommand_add("VERSION", m_version);
	pcommand_add("INFO", m_info);
	pcommand_add("WHOIS", m_whois);
	pcommand_add("TRACE", m_trace);
	pcommand_add("JOIN", m_join);
	pcommand_add("PASS", m_pass);
	pcommand_add("ERROR", m_error);
	pcommand_add("TOPIC", m_topic);
	pcommand_add("CAPAB", m_capab);
	pcommand_add("ENCAP", m_encap);
	pcommand_add("MOTD", m_motd);

	m->mflags = MODTYPE_CORE;

	pmodule_loaded = TRUE;
}
