/*-----------------------------------------------------------------
 *	$Id: x2sys.c,v 1.10 2004-06-09 17:45:21 pwessel Exp $
 *
 *      Copyright (c) 1999-2001 by P. Wessel
 *      See COPYING file for copying and redistribution conditions.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; version 2 of the License.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      Contact info: www.soest.hawaii.edu/wessel
 *--------------------------------------------------------------------*/
/* x2sys.c contains the source code for the X2SYS crossover library
 * libx2sys.a.  The code is copylefted under the GNU Public Library
 * License.
 *
 * The following functions are external and user-callable form other
 * programs:
 *
 * x2sys_initialize	: Reads the definition info file for current data files
 * x2sys_read_file	: Reads and returns the entire data matrix
 * x2sys_read_gmtfile	: Specifically reads an old .gmt file
 * x2sys_read_mgd77file : Specifically reads an MGD77 file
 * x2sys_read_list	: Read an ascii list of track names
 * x2sys_distances	: Calculate cumulative distances along track
 * x2sys_dummytimes	: Make dummy times for tracks missing times
 * x2sys_n_data_cols	: Gives number of data columns in this data set
 * x2sys_fopen		: Opening files with error message and exit
 * x2sys_fclose		: Closes files and gives error messages
 * x2sys_skip_header	: Skips the header record(s) in the open file
 * x2sys_read_record	: Reads and returns one record from the open file
 * x2sys_output_record	: Writes one data record to stdout
 * x2sys_pick_fields	: Decodes the -F<fields> flag of desired columns
 * x2sys_free_info	: Frees the information structure
 * x2sys_free_data	: Frees the data matrix
 *------------------------------------------------------------------
 * Core crossover functions are part of GMT:
 * GMT_init_track	: Prepares a track for crossover analysis
 * GMT_crossover	: Calculates crossovers for two data sets
 * GMT_x_alloc		: Allocate space for crossovers
 * GMT_x_free		: Free crossover structure
 * GMT_ysort		: Sorting routine used in x2sys_init_track [Hidden]
 *------------------------------------------------------------------
 * These routines are local to x2sys and used by the above routines:
 *
 * x2sys_set_home	: Initializes X2SYS paths
 * x2sys_record_length	: Returns the record length of current file
 *
 *------------------------------------------------------------------
 * Author:	Paul Wessel
 * Date:	18-MAY-2004
 * Version:	1.1, based on the spirit of the old xsystem code
 *
 */

#include "x2sys.h"

/* Global variables used by X2SYS functions */

char *X2SYS_HOME;

char *x2sys_xover_format = "%9.5lf %9.5lf %10.1lf %10.1lf %9.2lf %9.2lf %9.2lf %8.1lf %8.1lf %8.1lf %5.1lf %5.1lf\n";
char *x2sys_xover_header = "%s %d %s %d\n";
char *x2sys_header = "> %s %d %s %d\n";

void x2sys_set_home (void);
int x2sys_record_length (struct X2SYS_INFO *s);

FILE *x2sys_fopen (char *fname, char *mode)
{
	FILE *fp;

	if ((fp = fopen (fname, mode)) == NULL) {
		fprintf (stderr, "x2sys: Error from fopen on %s using mode %s\n", fname, mode);
		exit (EXIT_FAILURE);
	}
	return (fp);
}

void x2sys_fclose (char *fname, FILE *fp)
{

	if (fclose (fp)) {
		fprintf (stderr, "x2sys: Error from fclose on %s\n", fname);
		exit (EXIT_FAILURE);
	}
}

void x2sys_skip_header (FILE *fp, struct X2SYS_INFO *s)
{
	int i;
	char line[BUFSIZ];

	if (s->ascii_in) {	/* ASCII, skip records */
		for (i = 0; i < s->skip; i++) fgets (line, BUFSIZ, fp);
	}
	else {			/* Binary, skip bytes */
		fseek (fp, (long)s->skip, SEEK_CUR);
	}
}

/*
 * x2sys_data_read:  Read subroutine for x2_sys data input.
 * This function will read one logical record of ascii or
 * binary data from the open file, and return with a double
 * array called data[] with each data value in it.
 */

int x2sys_read_record (FILE *fp, double *data, struct X2SYS_INFO *s, struct GMT_IO *G)
{
	int j, k, i, n_read = 0;
	BOOLEAN error = FALSE;
	char line[BUFSIZ], buffer[64], *p, c;
	unsigned char u;
	short int h;
	float f;
	long L;
	double NaN;
	
	GMT_make_dnan(NaN);

	for (j = 0; !error && j < s->n_fields; j++) {

		switch (s->info[j].intype) {

			case 'A':	/* ASCII Card Record, must extract columns */
				if (j == 0) {
					s->ms_next = FALSE;
					fgets (line, BUFSIZ, fp);	/* Get new record */
					while (line[0] == '#' || line[0] == s->ms_flag) {
						fgets (line, BUFSIZ, fp);
						if (s->multi_segment) s->ms_next = TRUE;
					}
				}
				strncpy (buffer, &line[s->info[j].start_col], s->info[j].n_cols);
				buffer[s->info[j].n_cols] = 0;
				GMT_scanf (buffer, G->in_col_type[j], &data[j]);
				break;

			case 'a':	/* ASCII Record, get all columns directly */
				k = 0;
				s->ms_next = FALSE;
				fgets (line, BUFSIZ, fp);
				while (line[0] == '#' || line[0] == s->ms_flag) {
					fgets (line, BUFSIZ, fp);
					if (s->multi_segment) s->ms_next = TRUE;
				}
				p = strtok (line, " ,\t\n");
				while (p) {
					GMT_scanf (p, G->in_col_type[k], &data[k]);
					k++;;
					p = strtok (NULL, " ,\t\n");
				}
				return ((k != s->n_fields) ? -1 : 0);
				break;

			case 'c':	/* Binary signed 1-byte character */
				n_read += fread ((void *)&c, sizeof (char), (size_t)1, fp);
				data[j] = (double)c;
				break;

			case 'u':	/* Binary unsigned 1-byte character */
				n_read += fread ((void *)&u, sizeof (unsigned char), (size_t)1, fp);
				data[j] = (double)u;
				break;

			case 'h':	/* Binary signed 2-byte integer */
				n_read += fread ((void *)&h, sizeof (short int), (size_t)1, fp);
				data[j] = (double)h;
				break;

			case 'i':	/* Binary signed 4-byte integer */
				n_read += fread ((void *)&i, sizeof (int), (size_t)1, fp);
				data[j] = (double)i;
				break;

			case 'l':	/* Binary signed 4/8-byte integer (long) */
				n_read += fread ((void *)&L, sizeof (long), (size_t)1, fp);
				data[j] = (double)L;
				break;

			case 'f':	/* Binary signed 4-byte float */
				n_read += fread ((void *)&f, sizeof (float), (size_t)1, fp);
				data[j] = (double)i;
				break;

			case 'd':	/* Binary signed 8-byte float */
				n_read += fread ((void *)&data[j], sizeof (double), (size_t)1, fp);
				break;

			default:
				error = TRUE;
				break;
		}
	}

	/* Change nan-proxies to NaNs and apply any data scales and offsets */

	for (i = 0; i < s->n_fields; i++) {
		if (s->info[i].has_nan_proxy && data[i] == s->info[i].nan_proxy)
			data[i] = NaN;
		else if (s->info[i].do_scale)
			data[i] = data[i] * s->info[i].scale + s->info[i].offset;
		if (GMT_is_dnan (data[i])) s->info[i].has_nans = TRUE;
		if (i == s->x_col && s->geographic) GMT_lon_range_adjust (s->geodetic, &data[i]);
	}

	return ((error || n_read != s->n_fields) ? -1 : 0);
}
 
int x2sys_read_file (char *fname, double ***data, struct X2SYS_INFO *s, struct X2SYS_FILE_INFO *p, struct GMT_IO *G)
{
	/* Reads the entire contents of the file given and returns the
	 * number of data records.  The data matrix is return in the
	 * pointer data.
	 */

	int i, j;
	size_t n_alloc;
	FILE *fp;
	double **z, *rec;

	if ((fp = fopen (fname, "rb")) == NULL) {
		fprintf (stderr, "x2sys_read_file: Could not open %s\n", fname);
		return (-1);
	}

	n_alloc = GMT_CHUNK;

	rec = (double *) GMT_memory (VNULL, s->n_fields, sizeof (double), "x2sys_read_file");
	z = (double **) GMT_memory (VNULL, s->n_fields, sizeof (double *), "x2sys_read_file");
	for (i = 0; i < s->n_fields; i++) z[i] = (double *) GMT_memory (VNULL, n_alloc, sizeof (double), "x2sys_read_file");
	p->ms_rec = (int *) GMT_memory (VNULL, n_alloc, sizeof (int), "x2sys_read_file");
	x2sys_skip_header (fp, s);
	if (s->multi_segment) p->n_segments = -1;	/* So that first increment sets it to 0 */

	j = 0;
	while (!x2sys_read_record (fp, rec, s, G)) {	/* Gets the next data record */
		for (i = 0; i < s->n_fields; i++) z[i][j] = rec[i];
		if (s->multi_segment && s->ms_next) p->n_segments++;
		p->ms_rec[j] = p->n_segments;
		j++;
		if (j == (int)n_alloc) {	/* Get more */
			n_alloc += GMT_CHUNK;
			for (i = 0; i < s->n_fields; i++) z[i] = (double *) GMT_memory ((void *)z[i], n_alloc, sizeof (double), "x2sys_read_file");
			p->ms_rec = (int *) GMT_memory ((void *)p->ms_rec, n_alloc, sizeof (int), "x2sys_read_file");
		}
	}

	fclose (fp);
	for (i = 0; i < s->n_fields; i++) z[i] = (double *) GMT_memory ((void *)z[i], (size_t)j, sizeof (double), "x2sys_read_file");
	p->ms_rec = (int *) GMT_memory ((void *)p->ms_rec, (size_t)j, sizeof (int), "x2sys_read_file");

	*data = z;

	p->n_rows = j;
	p->year = 0;
	strncpy (p->name, fname, 32);

	return (j);
}

struct X2SYS_INFO *x2sys_initialize (char *fname, struct GMT_IO *G)
{
	/* Reads the format definition file and sets all information variables */

	int i = 0, c;
	size_t n_alloc = 10;
	FILE *fp;
	struct X2SYS_INFO *X;
	char line[BUFSIZ], cardcol[80], yes_no;

	x2sys_set_home ();

	X = (struct X2SYS_INFO *) GMT_memory (VNULL, n_alloc, sizeof (struct X2SYS_INFO), "x2sys_initialize");
	X->info = (struct X2SYS_DATA_INFO *) GMT_memory (VNULL, n_alloc, sizeof (struct X2SYS_DATA_INFO), "x2sys_initialize");
	X->ascii_in = TRUE;
	X->x_col = X->y_col = X->t_col = -1;
	X->ms_flag = '>';	/* Default multisegment header flag */
	sprintf (line, "%s%c%s.def\0", X2SYS_HOME, DIR_DELIM, fname);

	fp = x2sys_fopen (line, "r");

	if (!strcmp (fname, "gmt")) {
		X->read_file = (PFI) x2sys_read_gmtfile;
		X->geographic = TRUE;
		X->geodetic = 0;
	}
	else if (!strcmp (fname, "mgd77")) {
		X->read_file = (PFI) x2sys_read_mgd77file;
		X->geographic = TRUE;
		X->geodetic = 1;
	}
	else
		X->read_file = (PFI) x2sys_read_file;

	while (fgets (line, BUFSIZ, fp)) {
		if (line[0] == '\0') continue;
		if (line[0] == '#') {
			if (!strncmp (line, "#SKIP ", 6)) X->skip = atoi (&line[6]);
			if (!strncmp (line, "#BINARY ", 7)) X->ascii_in = FALSE;
			if (!strncmp (line, "#GEO ", 3)) X->geographic = TRUE;
			if (!strncmp (line, "#MULTISEG ", 9)) {
				X->multi_segment = TRUE;
				sscanf (line, "%*s %c", &X->ms_flag);
			}
			continue;
		}

		sscanf (line, "%s %c %c %lf %lf %lf %s %s", X->info[i].name, &X->info[i].intype, &yes_no, &X->info[i].nan_proxy, &X->info[i].scale, &X->info[i].offset, X->info[i].format, cardcol);
		if (X->info[i].intype == 'A') {	/* ASCII Card format */
			sscanf (cardcol, "%d-%d", &X->info[i].start_col, &X->info[i].stop_col);
			X->info[i].n_cols = X->info[i].stop_col - X->info[i].start_col + 1;
		}
		c = (int)X->info[i].intype;
		if (tolower (c) != 'a') X->ascii_in = FALSE;
		c = (int)yes_no;
		if (tolower (c) != 'Y') X->info[i].has_nan_proxy = TRUE;
		if (!(X->info[i].scale == 1.0 && X->info[i].offset == 0.0)) X->info[i].do_scale = TRUE;
		if (!strcmp (X->info[i].name, "x") || !strcmp (X->info[i].name, "lon"))  X->x_col = i;
		if (!strcmp (X->info[i].name, "y") || !strcmp (X->info[i].name, "lat"))  X->y_col = i;
		if (!strcmp (X->info[i].name, "t") || !strcmp (X->info[i].name, "time")) X->t_col = i;
		i++;
		if (i == (int)n_alloc) {
			n_alloc += 10;
			X->info = (struct X2SYS_DATA_INFO *) GMT_memory ((void *)X->info, n_alloc, sizeof (struct X2SYS_DATA_INFO), "x2sys_initialize");
		}
			
	}
	fclose (fp);

	if (i < (int)n_alloc) X->info = (struct X2SYS_DATA_INFO *) GMT_memory ((void *)X->info, i, sizeof (struct X2SYS_DATA_INFO), "x2sys_initialize");
	X->n_fields = X->n_out_columns = i;

	X->out_order  = (int *) GMT_memory (VNULL, sizeof (int), (size_t)X->n_fields, "x2sys_initialize");
	X->use_column = (int *) GMT_memory (VNULL, sizeof (int), (size_t)X->n_fields, "x2sys_initialize");
	for (i = 0; i < X->n_fields; i++) {	/* Default is same order and use all columns */
		X->out_order[i] = i;
		X->use_column[i] = 1;
		G->in_col_type[i] = G->out_col_type[i] = (X->x_col == i) ? GMT_IS_LON : ((X->y_col == i) ? GMT_IS_LAT : GMT_IS_UNKNOWN);
	}
	X->n_data_cols = x2sys_n_data_cols (X);
	X->rec_size = (8 + X->n_data_cols) * sizeof (double);

	return (X);
}

int x2sys_record_length (struct X2SYS_INFO *s)
{
	int i, rec_length = 0;

	for (i = 0; i < s->n_fields; i++) {
		switch (s->info[i].intype) {
			case 'c':
			case 'u':
				rec_length += 1;
				break;
			case 'h':
				rec_length += 2;
				break;
			case 'i':
			case 'f':
				rec_length += 4;
				break;
			case 'l':
				rec_length += sizeof (long);
				break;
			case 'd':
				rec_length += 8;
				break;
		}
	}
	return (rec_length);
}

int x2sys_n_data_cols (struct X2SYS_INFO *s)
{
	int i, n = 0;

	for (i = 0; i < s->n_fields; i++) {
		if (i == s->x_col) continue;
		if (i == s->y_col) continue;
		if (i == s->t_col) continue;
		if (!s->use_column[i]) continue;
		n++;
	}

	return (n);
}

int x2sys_output_record (FILE *fp, double data[], struct X2SYS_INFO *s)
{
	int i, k;

	if (s->ascii_out) {
		for (i = 0; i < s->n_out_columns-1; i++) {
			k = s->out_order[i];
			(GMT_is_dnan (data[k])) ? fprintf (fp, "NaN") : fprintf (fp, s->info[k].format, data[k]);
			fputc ('\t', fp);
		}
		k = s->out_order[i];
		(GMT_is_dnan (data[k])) ? fprintf (fp, "NaN") : fprintf (fp, s->info[k].format, data[k]);
		fputc ('\n', fp);
	}
	else {	/* Binary */
		for (i = 0; i < s->n_out_columns; i++) {
			k = s->out_order[i];
			fwrite ((void *)&data[k], sizeof (double), (size_t)1, fp);
		}
	}
	return (s->n_out_columns);
}

void x2sys_pick_fields (char *string, struct X2SYS_INFO *s)
{
	/* Scan the -Fstring and select which columns to use and which order
	 * they should appear on output.  Default is all columns and the same
	 * order as on input
	 */

	char line[BUFSIZ], *p;
	int i = 0, j;

	strncpy (line, string, BUFSIZ);
	memset ((void *)s->use_column, 0, (size_t)(s->n_fields * sizeof (int)));

	p = strtok (line, ",");

	while (p) {
		j = 0;
		while (j < s->n_fields && strcmp (p, s->info[j].name)) j++;
		if (j < s->n_fields) {
			s->out_order[i] = j;
			s->use_column[j] = 1;
		}
		else {
			fprintf (stderr, "X2SYS: ERROR: Unknown column name %s\n", p);
			exit (EXIT_FAILURE);
		}
		p = strtok (NULL, ",");
		i++;
	}

	s->n_out_columns = i;
}

void x2sys_set_home (void)
{
	char *this;

	if (X2SYS_HOME) return;	/* Already set elsewhere */

	if ((this = getenv ("X2SYS_HOME")) != CNULL) {	/* Set user's default path */
		X2SYS_HOME = (char *) GMT_memory (VNULL, (size_t)(strlen (this) + 1), 1, "x2sys_set_home");
		strcpy (X2SYS_HOME, this);
	}
	else if ((this = getenv ("GMTHOME")) != CNULL) {	/* Use GMT path */
		X2SYS_HOME = (char *) GMT_memory (VNULL, (size_t)(strlen (this) + 11), 1, "x2sys_set_home");
		sprintf (X2SYS_HOME, "%s%cshare%cx2sys\0", this, DIR_DELIM, DIR_DELIM);
	}
	else {	/* Set default path */
#ifdef _WIN32
		X2SYS_HOME = (char *) GMT_memory (VNULL, (size_t)23, 1, "x2sys_set_home");
		strcpy (X2SYS_HOME, "C:\\usr\\local\\gmt\\x2sys");
#else
		X2SYS_HOME = (char *) GMT_memory (VNULL, (size_t)21, 1, "x2sys_set_home");
		strcpy (X2SYS_HOME, "/usr/local/gmt/x2sys");
#endif
	}
}

void x2sys_free_info (struct X2SYS_INFO *s)
{
	GMT_free ((void *)s->info);
	GMT_free ((void *)s);
}

void x2sys_free_data (double **data, int n)
{
	int i;

	for (i = 0; i < n; i++) GMT_free ((void *)data[i]);
	GMT_free ((void *)data);
}

double *x2sys_distances (double x[], double y[], int n, int dist_flag)
{
	int this, prev;
	double *d, km_pr_deg;
	
	km_pr_deg = 0.001 * 2.0 * M_PI * 6371008.7714 / 360.0;

	d = (double *) GMT_memory (VNULL, (size_t)n, sizeof (double), "x2sys_distances");

	switch (dist_flag) {

		case 0:	/* Cartesian distances */

			for (this = 1, prev = 0; this < n; this++, prev++)
				d[this] = d[prev] + hypot (x[this] - x[prev], y[this] - y[prev]);
			break;

		case 1:	/* Flat earth distances */

			for (this = 1, prev = 0; this < n; this++, prev++)
				d[this] = d[prev] + hypot ((x[this] - x[prev]) * cosd (0.5 * (y[this] + y[prev])), y[this] - y[prev]) * km_pr_deg;
			break;

		case 2:	/* Great circle distances */

			for (this = 1, prev = 0; this < n; this++, prev++)
				d[this] = d[prev] + GMT_great_circle_dist (x[this], y[this], x[prev], y[prev]) * km_pr_deg;
			break;
		default:
			fprintf (stderr, "x2sys: Error: Wrong flag passed to x2sys_distances (%d)\n", dist_flag);
			exit (EXIT_FAILURE);
			break;
	}

	return (d);
}

double *x2sys_dummytimes (int n)
{
	int i;
	double *t;

	/* Make monotonically increasing dummy time sequence */

	t = (double *) GMT_memory (VNULL, (size_t)n, sizeof (double), "x2sys_dummytimes");

	for (i = 0; i < n; i++) t[i] = (double)i;

	return (t);
}

int x2sys_read_gmtfile (char *fname, double ***data, struct X2SYS_INFO *s, struct X2SYS_FILE_INFO *p, struct GMT_IO *G)
{
	/* Reads the entire contents of the file given and returns the
	 * number of data records.  The data matrix is return in the
	 * pointer data.  The input file format is the venerable GMT
	 * MGG format from old Lamont by Wessel and Smith.
	 */

	int i, j;
	char gmtfile[BUFSIZ], name[80];
	FILE *fp;
	double **z;
	double NaN;
	struct GMTMGG_REC record;

	GMT_make_dnan(NaN);

	if (!(s->flags & 1)) {	/* Must init gmt file paths */
		gmtmggpath_init ();
		s->flags |= 1;
	}

	strncpy (name, fname, 80);
	if (strstr (fname, ".gmt"))	/* Name includes .gmt suffix */
		name[strlen(fname)-4] = 0;

  	if (gmtmggpath_func (gmtfile, name)) {
   		fprintf (stderr, "x2sys_read_gmtfile : Cannot find leg %s\n", name);
     		return (-1);
  	}

	if ((fp = fopen (gmtfile, "rb")) == NULL) {
		fprintf (stderr, "x2sys_read_gmtfile: Could not open %s\n", gmtfile);
		return (-1);
	}

	if (fread ((void *)&(p->year), sizeof (int), (size_t)1, fp) != 1) {
		fprintf (stderr, "x2sys_read_gmtfile: Could not read leg year from %s\n", gmtfile);
		return (-1);
	}
	if (fread ((void *)&(p->n_rows), sizeof (int), (size_t)1, fp) != 1) {
		fprintf (stderr, "x2sys_read_gmtfile: Could not read n_records from %s\n", gmtfile);
		return (-1);
	}
	memset ((void *)p->name, 0, (size_t)32);

	if (fread ((void *)p->name, (size_t)10, sizeof (char), fp) != 1) {
		fprintf (stderr, "x2sys_read_gmtfile: Could not read agency from %s\n", gmtfile);
		return (-1);
	}

	z = (double **) GMT_memory (VNULL, (size_t)6, sizeof (double *), "x2sys_read_gmtfile");
	for (i = 0; i < 6; i++) z[i] = (double *) GMT_memory (VNULL, (size_t)p->n_rows, sizeof (double), "x2sys_read_gmtfile");

	for (j = 0; j < p->n_rows; j++) {

		if (fread ((void *)&record, (size_t)18, (size_t)1, fp) != 1) {
			fprintf (stderr, "x2sys_read_gmtfile: Could not read record %d from %s\n", j, gmtfile);
			exit (EXIT_FAILURE);
		}

		z[0][j] = record.time;
		z[1][j] = record.lat * MDEG2DEG;
		z[2][j] = record.lon * MDEG2DEG;
		z[3][j] = (record.gmt[0] == GMTMGG_NODATA) ? NaN : 0.1 * record.gmt[0];
		z[4][j] = (record.gmt[1] == GMTMGG_NODATA) ? NaN : record.gmt[1];
		z[5][j] = (record.gmt[2] == GMTMGG_NODATA) ? NaN : record.gmt[2];

	}

	fclose (fp);

	*data = z;

	return (p->n_rows);
}

int x2sys_read_mgd77file (char *fname, double ***data, struct X2SYS_INFO *s, struct X2SYS_FILE_INFO *p, struct GMT_IO *G)
{
	int n_read, i, j, len, n_alloc = GMT_CHUNK;
	char line[BUFSIZ];
	double **z;
	FILE *fp;
	struct GMTMGG_REC record;
	struct GMTMGG_TIME *gmt = NULL;
	double NaN;
	
	GMT_make_dnan(NaN);

  	if ((fp = fopen (fname, "r")) == NULL) {
   		fprintf (stderr, "x2sys_read_mgd77file : Cannot find file %s\n", fname);
     		return (-1);
  	}

	z = (double **) GMT_memory (VNULL, (size_t)6, sizeof (double *), "x2sys_read_mgd77file");
	for (i = 0; i < 6; i++) z[i] = (double *) GMT_memory (VNULL, (size_t)n_alloc, sizeof (double), "x2sys_read_mgd77file");

	j = n_read = 0;
	while (fgets (line, BUFSIZ, fp)) {
		n_read++;
		if (!(line[0] == '3' || line[0] == '5')) continue;	/* Only process data records */
		if ((len = (int)strlen(line)) != 121) {
			fprintf (stderr, "x2sys_read_mgd77file: Record # %d has incorrect length (%d), skipped\n", n_read, len);
			continue;
		}
		if (!gmtmgg_decode_MGD77 (line, FALSE, &record, &gmt)) {
			z[0][j] = record.time;
			z[1][j] = record.lon * MDEG2DEG;
			z[2][j] = record.lat * MDEG2DEG;
			z[3][j] = (record.gmt[0] == GMTMGG_NODATA) ? NaN : 0.1 * record.gmt[0];
			z[4][j] = (record.gmt[1] == GMTMGG_NODATA) ? NaN : record.gmt[1];
			z[5][j] = (record.gmt[2] == GMTMGG_NODATA) ? NaN : record.gmt[2];
			j++;
		}
		else
			fprintf (stderr, "x2sys_read_mgd77file: Trouble decoding record # %d (skipped)\n", n_read);

		if (j == n_alloc) {
			n_alloc += GMT_CHUNK;
			for (i = 0; i < 6; i++) z[i] = (double *) GMT_memory ((void *)z[i], (size_t)n_alloc, sizeof (double), "x2sys_read_mgd77file");
		}
	}
	fclose (fp);

	strncpy (p->name, fname, 32);
	p->year = gmt->first_year;
	p->n_rows = j;
	for (i = 0; i < 6; i++) z[i] = (double *) GMT_memory ((void *)z[i], (size_t)p->n_rows, sizeof (double), "x2sys_read_mgd77file");

	*data = z;

	return (p->n_rows);
}

int x2sys_xover_output (FILE *fp, int n, double out[])
{
	/* Write old xover formatted output.  This assumes data files are .gmt files */

	/* y x t1 t2 X1 X2 X3 M1 M2 M3 h1 h2 */

	fprintf (fp, x2sys_xover_format, out[1], out[0], out[2], out[3], out[9], out[11], out[13], out[8], out[10], out[12], out[6], out[7]);
	return (12);
}

int x2sys_read_list (char *file, char ***list)
{
	int n_alloc = GMT_CHUNK, n = 0;
	char **p, line[BUFSIZ], name[64];
	FILE *fp;

	fp = x2sys_fopen (file, "r");

	p = (char **) GMT_memory (VNULL, n_alloc, sizeof (char *), "x2sys_read_list");

	while (fgets (line, BUFSIZ, fp)) {
		sscanf (line, "%s", name);
		p[n] = (char *) GMT_memory (VNULL, (size_t)(strlen(name)+1), sizeof (char), "x2sys_read_list");
		strcpy (p[n], name);
		n++;
		if (n == n_alloc) {
			n_alloc += GMT_CHUNK;
			p = (char **) GMT_memory ((void *)p, n_alloc, sizeof (char *), "x2sys_read_list");
		}
	}
	fclose (fp);

	p = (char **) GMT_memory ((void *)p, n, sizeof (char *), "x2sys_read_list");

	*list = p;

	return (n);
}
