/*
	hcc.c
	HCode compiler based on qcc, modifed by Eric Hobbs to work with DCC

	$Header: /cvsroot/uhexen2/utils/dcc/hcc.c,v 1.32 2009-06-15 09:12:20 sezero Exp $
*/

#include "q_stdinc.h"
#include "compiler.h"
#include "arch_def.h"
#include "cmdlib.h"
#include "util_io.h"
#include "qcc.h"
#include "crc.h"
#include "q_endian.h"
#include "byteordr.h"

static char	sourcedir[1024];
static char	destfile[1024];

byte		_pr_globals[MAX_REGS * sizeof(float)];
float		*pr_globals = (float *)_pr_globals;
int			numpr_globals;

char		strings[MAX_STRINGS];
int			strofs;

dstatement_t	statements[MAX_STATEMENTS];
int			numstatements;
int			statement_linenums[MAX_STATEMENTS];

dfunction_t	functions[MAX_FUNCTIONS];
int			numfunctions;

ddef_t		globals[MAX_GLOBALS];
int			numglobaldefs;

ddef_t		fields[MAX_FIELDS];
int			numfielddefs;

char		precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
int			precache_sounds_block[MAX_SOUNDS];
int			numsounds;

char		precache_models[MAX_MODELS][MAX_DATA_PATH];
int			precache_models_block[MAX_SOUNDS];
int			nummodels;

char		precache_files[MAX_FILES][MAX_DATA_PATH];
int			precache_files_block[MAX_SOUNDS];
int			numfiles;

//ADDED BY EMAN
extern FILE	*PR_FILE;
extern char	*temp_val[MAX_REGS];
extern char	*func_headers[MAX_FUNCTIONS];
extern void Dcc_Functions (void);
extern void FindBuiltinParameters(int func);
extern void DccFunctionOP(unsigned short op);
extern void DEC_ReadData (const char *srcfile);
extern void PR_PrintFunction (const char *name);

extern int	FILE_NUM_FOR_NAME;


/*
============
WriteFiles

  Generates files.dat, which contains all of the
  data files actually used by the game, to be
  processed by qfiles.exe
============
*/
static void WriteFiles (void)
{
	FILE	*f;
	int		i;
	char	filename[1024];

	sprintf (filename, "%sfiles.dat", sourcedir);
	f = fopen (filename, "w");
	if (!f)
		Error ("Couldn't open %s", filename);

	fprintf (f, "%i\n", numsounds);
	for (i = 0 ; i < numsounds ; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i], precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i = 0 ; i < nummodels ; i++)
		fprintf (f, "%i %s\n", precache_models_block[i], precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i = 0 ; i < numfiles ; i++)
		fprintf (f, "%i %s\n", precache_files_block[i], precache_files[i]);

	fclose (f);
}


// CopyString returns an offset from the string heap
int	CopyString (const char *str)
{
	int		old;
	int		i, l;

	for (i = 0 ; i < strofs ; i += l)
	{
		l = strlen(strings+i) + 1;
		if (!strcmp(strings+i, str))
			return i;
	}

	old = strofs;
	strcpy (strings+strofs, str);
	strofs += strlen(str)+1;

//	printf("\tsaving %s to string heap\n", str);
//	if (!strcmp("modelindex", str))
//		getche();
	return old;
}


static void PrintStrings (void)
{
	int		i, l, j;

	for (i = 0 ; i < strofs ; i += l)
	{
		l = strlen(strings+i) + 1;
		printf ("%5i : ", i);
		for (j = 0 ; j < l ; j++)
		{
			if (strings[i+j] == '\n')
			{
				putchar ('\\');
				putchar ('n');
			}
			else
				putchar (strings[i+j]);
		}
		printf ("\n");
	}
}

static void PrintFunctions (void)
{
	int		i, j;
	dfunction_t	*d;

	for (i = 0 ; i < numfunctions ; i++)
	{
		d = &functions[i];
		printf ("%s : %s : %i %i (", strings + d->s_file, strings + d->s_name, d->first_statement, d->parm_start);
		for (j = 0 ; j < d->numparms ; j++)
			printf ("%i ", d->parm_size[j]);
		printf (")\n");
	}
}

static void PrintFields (void)
{
	int		i;
	ddef_t	*d;

	for (i = 0 ; i < numfielddefs ; i++)
	{
		d = &fields[i];
		printf ("%5i %5i : (%i) %s\n", i, d->ofs, d->type, strings + d->s_name);
	}
}

static void PrintGlobals (void)
{
	int		i;
	ddef_t	*d;

	for (i = 0 ; i < numglobaldefs ; i++)
	{
		d = &globals[i];
		printf ("%5i : %5i : (%i) %s\n", i, d->ofs, d->type, strings + d->s_name);
	}
}

static void PrintPRGlobals (void)
{
	int		i;

	for (i = 0 ; i < numpr_globals ; i++)
	{
		printf ("%5i %5.5f %5d\n", i, pr_globals[i], ((int *)pr_globals)[i]);
	}
}

static void Printstatements (void)
{
	int		i;
	dstatement_t	*ds;

	for (i = 0 ; i < numstatements ; i++)
	{
		ds = statements + i;
		printf ("%5d op: %2d a: %5d b: %5d c: %5d\n", i, ds->op, ds->a, ds->b, ds->c);
	}
}


static void InitData (void)
{
	int		i;

	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;

	def_ret.ofs = OFS_RETURN;
	for (i = 0 ; i < MAX_PARMS ; i++)
		def_parms[i].ofs = OFS_PARM0 + 3*i;
}


static void WriteData (int crc)
{
	def_t		*def;
	ddef_t		*dd;
	dprograms_t	progs;
	FILE	*h;
	int			i;

	for (def = pr.def_head.next ; def ; def = def->next)
	{
		if (def->type->type == ev_function)
		{
//			df = &functions[numfunctions];
//			numfunctions++;
		}
		else if (def->type->type == ev_field)
		{
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = CopyString (def->name);
			dd->ofs = G_INT(def->ofs);
		}
		dd = &globals[numglobaldefs];
		numglobaldefs++;
		dd->type = def->type->type;
		if (!def->initialized
			&& def->type->type != ev_function
			&& def->type->type != ev_field
			&& def->scope == NULL)
		{
			dd->type |= DEF_SAVEGLOBAL;
		}
		dd->s_name = CopyString (def->name);
		dd->ofs = def->ofs;
	}

//	PrintStrings ();
//	PrintFunctions ();
//	PrintFields ();
//	PrintGlobals ();

	strofs = (strofs + 3) & ~3;

	printf ("%10i strofs\n", strofs);
	printf ("%10i numstatements\n", numstatements);
	printf ("%10i numfunctions\n", numfunctions);
	printf ("%10i numglobaldefs\n", numglobaldefs);
	printf ("%10i numfielddefs\n", numfielddefs);
	printf ("%10i numpr_globals\n", numpr_globals);

	h = SafeOpenWrite (destfile);
	SafeWrite (h, &progs, sizeof(progs));

	progs.ofs_strings = ftell (h);
	progs.numstrings = strofs;
	SafeWrite (h, strings, strofs);

	progs.ofs_statements = ftell (h);
	progs.numstatements = numstatements;
	for (i = 0 ; i < numstatements ; i++)
	{
		statements[i].op = LittleShort(statements[i].op);
		statements[i].a = LittleShort(statements[i].a);
		statements[i].b = LittleShort(statements[i].b);
		statements[i].c = LittleShort(statements[i].c);
	}
	SafeWrite (h, statements, numstatements*sizeof(dstatement_t));

	progs.ofs_functions = ftell (h);
	progs.numfunctions = numfunctions;
	for (i = 0 ; i < numfunctions ; i++)
	{
		functions[i].first_statement = LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong (functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}
	SafeWrite (h, functions, numfunctions*sizeof(dfunction_t));

	progs.ofs_globaldefs = ftell (h);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0 ; i < numglobaldefs ; i++)
	{
		globals[i].type = LittleShort (globals[i].type);
		globals[i].ofs = LittleShort (globals[i].ofs);
		globals[i].s_name = LittleLong (globals[i].s_name);
	}
	SafeWrite (h, globals, numglobaldefs*sizeof(ddef_t));

	progs.ofs_fielddefs = ftell (h);
	progs.numfielddefs = numfielddefs;
	for (i = 0 ; i < numfielddefs ; i++)
	{
		fields[i].type = LittleShort (fields[i].type);
		fields[i].ofs = LittleShort (fields[i].ofs);
		fields[i].s_name = LittleLong (fields[i].s_name);
	}
	SafeWrite (h, fields, numfielddefs*sizeof(ddef_t));

	progs.ofs_globals = ftell (h);
	progs.numglobals = numpr_globals;
	for (i = 0 ; i < numpr_globals ; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);
	SafeWrite (h, pr_globals, numpr_globals*4);

	printf ("%10i TOTAL SIZE\n", (int)ftell (h));

	progs.entityfields = pr.size_fields;

	progs.version = PROG_VERSION;
	progs.crc = crc;

// byte swap the header and write it out
	for (i = 0 ; i < (int)sizeof(progs)/4 ; i++)
		((int *)&progs)[i] = LittleLong ( ((int *)&progs)[i] );
	fseek (h, 0, SEEK_SET);
	SafeWrite (h, &progs, sizeof(progs));
	fclose (h);
}


/*
===============
PR_String

Returns a string suitable for printing (no newlines, max 60 chars length)
===============
*/
const char *PR_String (const char *string)
{
	static char	buf[80];
	char	*s;

	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}

def_t *PR_DefForFieldOfs (gofs_t ofs)
{
	def_t	*d;

	for (d = pr.def_head.next ; d ; d = d->next)
	{
		if (d->type->type != ev_field)
			continue;
		if (((int *)pr_globals)[d->ofs] == ofs)
			return d;
	}
	Error ("%s: couldn't find %i", __thisfunc__, ofs);
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
const char *PR_ValueString (etype_t type, void *val)
{
	static char	line[256];
	def_t		*def;
	dfunction_t	*f;

	switch (type)
	{
	case ev_string:
		sprintf (line, "%s", PR_String(strings + *(int *)val));
		break;
	case ev_entity:
		sprintf (line, "entity %i", *(int *)val);
		break;
	case ev_function:
		f = functions + *(int *)val;
		if (!f)
			sprintf (line, "undefined function");
		else
			sprintf (line, "%s()", strings + f->s_name);
		break;
	case ev_field:
		def = PR_DefForFieldOfs ( *(int *)val );
		sprintf (line, ".%s", def->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%5.1f", *(float *)val);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
static const char *PR_GlobalStringNoContents (gofs_t ofs)
{
	int		i;
	def_t		*def;
	static char	line[128];

	def = pr_global_defs[ofs];
	if (!def)
	//	Error ("%s: no def for %i", __thisfunc__, ofs);
		sprintf (line,"%i(?)", ofs);
	else
		sprintf (line,"%i(%s)", ofs, def->name);

	i = strlen(line);
	for ( ; i < 16 ; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

static const char *PR_GlobalString (gofs_t ofs)
{
	const char	*s;
	int		i;
	def_t		*def;
	static char	line[128];

	def = pr_global_defs[ofs];
	if (!def)
		return PR_GlobalStringNoContents(ofs);
	if (def->initialized && def->type->type != ev_function)
	{
		s = PR_ValueString (def->type->type, &pr_globals[ofs]);
		sprintf (line, "%i(%s)", ofs, s);
	}
	else
		sprintf (line, "%i(%s)", ofs, def->name);

	i = strlen(line);
	for ( ; i < 16 ; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

/*
============
PR_PrintOfs
============
*/
#if 0	/* all uses are commented out */
void PR_PrintOfs (gofs_t ofs)
{
	printf ("%s\n", PR_GlobalString(ofs));
}
#endif

/*
=================
PR_PrintStatement
=================
*/
static void PR_PrintStatement (dstatement_t *s)
{
	int		i;

	printf ("%4i : %4i : %s ", (int)(s - statements), statement_linenums[s-statements], pr_opcodes[s->op].opname);
	i = strlen(pr_opcodes[s->op].opname);
	for ( ; i < 10 ; i++)
		printf (" ");

	if (s->op == OP_IF || s->op == OP_IFNOT)
		printf ("%sbranch %i", PR_GlobalString(s->a), s->b);
	else if (s->op == OP_GOTO)
	{
		printf ("branch %i", s->a);
	}
	else if ( (unsigned int)(s->op - OP_STORE_F) < 6)
	{
		printf ("%s", PR_GlobalString(s->a));
		printf ("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			printf ("%s", PR_GlobalString(s->a));
		if (s->b)
			printf ("%s", PR_GlobalString(s->b));
		if (s->c)
			printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	printf ("\n");
}

/*
==============
PR_BeginCompilation

called before compiling a batch of files, clears the pr struct
==============
*/
static void PR_BeginCompilation (void)
{
	int		i;

	numpr_globals = RESERVED_OFS;
	pr.def_tail = &pr.def_head;
	for (i = 0 ; i < RESERVED_OFS ; i++)
	{
		pr_global_defs[i] = &def_void;
	}

	// Link the function type in so state forward declarations match
	// proper type.
	pr.types = &type_function;
	type_function.next = NULL;
	pr_error_count = 0;
}

/*
==============
PR_FinishCompilation

called after all files are compiled to check for errors
Returns false if errors were detected.
==============
*/
static qboolean PR_FinishCompilation (void)
{
	def_t		*d;
	qboolean	errors;

	errors = false;

	// check to make sure all functions prototyped have code
	for (d = pr.def_head.next ; d ; d = d->next)
	{
		if (d->type->type == ev_function && !d->scope)
		{ // function parms are ok
		//	f = G_FUNCTION(d->ofs);
		//	if (!f || (!f->code && !f->builtin) )
			if (!d->initialized)
			{
				printf("function '%s' was not defined\n", d->name);
				errors = true;
			}
		}
	}

	return !errors;
}

//=============================================================================

/*
============
PR_WriteProgdefs

Writes the global and entity structures out
Returns a crc of the header, to be stored in the progs file for comparison
at load time.
============
*/
static int PR_WriteProgdefs (const char *filename)
{
	def_t	*d;
	FILE	*f;
	unsigned short		crc;
	int		c;

	printf ("writing %s\n", filename);
	printf ("compacting string heap, this may take a while...\n");
	f = fopen (filename, "w");

	// print global vars until the first field is defined
	fprintf (f,"\n/* generated by hcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n", RESERVED_OFS);
	for (d = pr.def_head.next ; d ; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			break;

		switch (d->type->type)
		{
		case ev_float:
			fprintf (f, "\tfloat\t%s;\n", d->name);
			break;
		case ev_vector:
			fprintf (f, "\tvec3_t\t%s;\n", d->name);
			d = d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f, "\tstring_t\t%s;\n", d->name);
			break;
		case ev_function:
			fprintf (f, "\tfunc_t\t%s;\n", d->name);
			break;
		case ev_entity:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		default:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		}
	}
	fprintf (f, "} globalvars_t;\n\n");

	// print all fields
	fprintf (f, "typedef struct\n{\n");
	for (d = pr.def_head.next ; d ; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;

		switch (d->type->aux_type->type)
		{
		case ev_float:
			fprintf (f, "\tfloat\t%s;\n", d->name);
			break;
		case ev_vector:
			fprintf (f, "\tvec3_t\t%s;\n", d->name);
			d = d->next->next->next;	// skip the elements
			break;
		case ev_string:
			fprintf (f, "\tstring_t\t%s;\n", d->name);
			break;
		case ev_function:
			fprintf (f, "\tfunc_t\t%s;\n", d->name);
			break;
		case ev_entity:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		default:
			fprintf (f, "\tint\t%s;\n", d->name);
			break;
		}
	}
	fprintf (f, "} entvars_t;\n\n");

	fclose (f);

	// do a crc of the file
	CRC_Init (&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc(f)) != EOF)
		CRC_ProcessByte (&crc, (byte)c);

	fprintf (f, "#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}

static void PrintFunction (const char *name)
{
	int		i;
	dstatement_t	*ds;
	dfunction_t		*df;

	for (i = 0 ; i < numfunctions ; i++)
		if (!strcmp (name, strings + functions[i].s_name))
			break;
	if (i == numfunctions)
		Error ("No function names \"%s\"", name);
	df = functions + i;

	printf ("Statements for %s:\n", name);
	ds = statements + df->first_statement;
	while (1)
	{
		PR_PrintStatement (ds);
		if (!ds->op)
			break;
		ds++;
	}
}


//============================================================================

/*
============
main
============
*/
int main (int argc, char **argv)
{
	const char	*psrc;
	void		*src, *src2;
	char	filename[1024];
	int		p, c;
	unsigned short		crc;
	double	start, stop;
	FILE	*f;

	myargc = argc;
	myargv = argv;

	if (CheckParm("-?") || CheckParm("-h") || CheckParm("-help") || CheckParm("--help"))
	{
		printf(" Compiles progs.dat using progs.src in the current directory\n");
		printf(" -src <directory> : Specify source directory\n");
		printf(" -dcc : decompile the progs.dat in the current directory\n");
		printf(" -dcc -fix : fixes mangled names during decompilation\n");
		printf(" -dcc -asm <functionname> : decompile filename to the console\n");
		printf(" -dcc -dump -asm <functionname> : same as above but will show\n\t\tinstructions (opcodes and parms) as well\n");
		exit(0);
	}

	ValidateByteorder ();

	start = GetTime ();

	p = CheckParm("-src");
	if (p && p < argc-1)
	{
		strcpy(sourcedir, argv[p+1]);
		strcat(sourcedir, "/");
		printf("Source directory: %s\n", sourcedir);
	}
	else
	{
		sourcedir[0] = '\0';
	}

	InitData ();

	PR_FILE = stdout;

	p = CheckParm("-dump");
	if (p)
		pr_dumpasm = true;

	// do a crc of the file
	p = CheckParm("-crc");
	if (p)
	{
		CRC_Init (&crc);
		f = fopen ("progdefs.h", "r");
		while ((c = fgetc(f)) != EOF)
			CRC_ProcessByte (&crc, (byte)c);

		printf ("#define PROGHEADER_CRC %i %d\n", crc, (int)crc);
		fclose (f);
		exit (0);
	}

	p = CheckParm("-dcc");
	if (p)
	{
		DEC_ReadData ("progs.dat");
		//fix mangled names if asked
		p = CheckParm ("-fix");
		if (p)
			FILE_NUM_FOR_NAME = 1;

		memset(func_headers, 0, MAX_FUNCTIONS * sizeof(char *));
		memset(temp_val, 0, MAX_REGS * sizeof(char *));

		p = CheckParm("-bbb");
		if (p)
		{
		/*	i= -999;
			for (p = 0; p < numstatements; p++)
				if ((statements+p)->op > i)
					i = (statements+p)->op;
			printf("largest op %d\n", i); */
			FindBuiltinParameters(1);

			exit (0);
		}

		p = CheckParm("-ddd");
		if (p)
		{
			for (p++ ; p < argc ; p++)
			{
				if (argv[p][0] == '-')
					break;
				DccFunctionOP (atoi(argv[p]));
			}
			exit (0);
		}

		p = CheckParm("-info2");
		if (p)
		{
			printf("\n=======================\n");
			printf("fields\n");
			printf("=======================\n");
			PrintFields ();
			printf("\n=======================\n");
			printf("globals\n");
			printf("=======================\n");
			PrintGlobals ();
			exit (0);
		}

		p = CheckParm("-info");
		if (p)
		{
			printf("\n=======================\n");
			printf("strings\n");
			printf("=======================\n");
			PrintStrings ();
			printf("\n=======================\n");
			printf("functions");
			printf("\n=======================\n");
			PrintFunctions ();
			printf("\n=======================\n");
			printf("fields\n");
			printf("=======================\n");
			PrintFields ();
			printf("\n=======================\n");
			printf("globals\n");
			printf("=======================\n");
			PrintGlobals ();
			printf("\n=======================\n");
			printf("pr_globals\n");
			printf("=======================\n");
			PrintPRGlobals ();
			printf("\n=======================\n");
			printf("statements\n");
			printf("=======================\n");
			Printstatements();
			exit (0);
		}

		p = CheckParm("-asm");
		if (p)
		{
			for (p++; p < argc; p++)
			{
				if (argv[p][0] == '-')
					break;
				PR_PrintFunction(argv[p]);
			}
		}
		else
		{
			Dcc_Functions ();
			stop = GetTime ();
			printf("\n%d seconds elapsed.\n", (int)(stop-start));
		}

		exit (0);
	}

	sprintf(filename, "%sprogs.src", sourcedir);
	LoadFile(filename, &src);
	psrc = (char *) src;

	psrc = COM_Parse(psrc);
	if (!psrc)
	{
		Error("No destination filename. dhcc -help for info.\n");
	}

	strcpy(destfile, com_token);
	printf("outputfile: %s\n", destfile);

	pr_dumpasm = false;

	PR_BeginCompilation();

	// compile all the files
	do
	{
		psrc = COM_Parse(psrc);
		if (!psrc)
			break;

		sprintf (filename, "%s%s", sourcedir, com_token);
		printf ("compiling %s\n", filename);
		LoadFile (filename, &src2);

		if (!PR_CompileFile((char *)src2, filename))
			exit (1);

	} while (1);

	if (!PR_FinishCompilation())
	{
		Error ("compilation errors");
	}

	p = CheckParm("-asm");
	if (p)
	{
		for (p++; p < argc; p++)
		{
			if (argv[p][0] == '-')
			{
				break;
			}
			PrintFunction(argv[p]);
		}
	}

	// write progdefs.h
	crc = PR_WriteProgdefs("progdefs.h");
//	crc = 14046;	// FIXME: cheap hack for now!!!!!!!!!!!!!

	// write data file
	WriteData(crc);

	// write files.dat
	WriteFiles();

	stop = GetTime ();
	printf("\n%d seconds elapsed.\n", (int)(stop-start));

	exit (0);
}
