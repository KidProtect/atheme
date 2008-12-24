/*
 * Copyright (C) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Config file parser.
 *
 * $Id: confparse.h 8375 2007-06-03 20:03:26Z pippijn $
 */

#ifndef CONFPARSE_H
#define CONFPARSE_H

struct _configfile
{
	char *cf_filename;
	config_entry_t *cf_entries;
	config_file_t *cf_next;
};

struct _configentry
{
	config_file_t *ce_fileptr;

	int ce_varlinenum;
	char *ce_varname;
	char *ce_vardata;
	int ce_vardatanum;

	int ce_sectlinenum;
	config_entry_t *ce_entries;

	config_entry_t *ce_prevlevel;

	config_entry_t *ce_next;
};

/* confp.c */
E void config_free(config_file_t *cfptr);
E config_file_t *config_load(const char *filename);

#endif

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs ts=8 sw=8 noexpandtab
 */
