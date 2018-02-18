/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Protocol module stuff.
 * Modules usually do not need this.
 */

#ifndef PMODULE_H
#define PMODULE_H

struct proto_cmd
{
	char	*token;
	void	(*handler)(struct sourceinfo *si, int parc, char *parv[]);
	int	minparc;
	int	sourcetype;
};

/* values for sourcetype */
#define MSRC_UNREG	1 /* before SERVER is sent */
#define MSRC_USER	2 /* from users */
#define MSRC_SERVER	4 /* from servers */

#define MAXPARC		35 /* max # params to protocol command */

/* pmodule.c */
extern mowgli_heap_t *pcommand_heap;
extern mowgli_heap_t *messagetree_heap;
extern mowgli_patricia_t *pcommands;

extern bool pmodule_loaded;

extern void pcommand_init(void);
extern void pcommand_add(const char *token,
	void (*handler)(struct sourceinfo *si, int parc, char *parv[]),
	int minparc, int sourcetype);
extern void pcommand_delete(const char *token);
extern struct proto_cmd *pcommand_find(const char *token);

/* ptasks.c */
extern int get_version_string(char *, size_t);
extern void handle_version(user_t *);
extern void handle_admin(user_t *);
extern void handle_info(user_t *);
extern void handle_stats(user_t *, char);
extern void handle_whois(user_t *, const char *);
extern void handle_trace(user_t *, const char *, const char *);
extern void handle_motd(user_t *);
extern void handle_away(user_t *, const char *);
extern void handle_message(struct sourceinfo *, char *, bool, char *);
extern void handle_topic_from(struct sourceinfo *, struct channel *, const char *, time_t, const char *);
extern void handle_kill(struct sourceinfo *, const char *, const char *);
extern server_t *handle_server(struct sourceinfo *, const char *, const char *, int, const char *);
extern void handle_eob(server_t *);
extern bool should_reg_umode(user_t *);

/* services.c */
extern void services_init(void);
extern void reintroduce_user(user_t *u);
extern void handle_nickchange(user_t *u);
extern void handle_burstlogin(user_t *u, const char *login, time_t ts);
extern void handle_setlogin(struct sourceinfo *si, user_t *u, const char *login, time_t ts);
extern void handle_certfp(struct sourceinfo *si, user_t *u, const char *certfp);
extern void handle_clearlogin(struct sourceinfo *si, user_t *u);

#endif
