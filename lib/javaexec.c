/* Execute a Java program.
   Copyright (C) 2001 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "liballoca.h"

/* Specification.  */
#include "javaexec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execute.h"
#include "xsetenv.h"
#include "sh-quote.h"
#include "xmalloc.h"
#include "error.h"
#include "gettext.h"

#define _(str) gettext (str)


/* CLASSPATH handling subroutines.  */
#include "classpath.c"

/* Survey of Java virtual machines.

   A = does it work without CLASSPATH being set
   B = does it work with CLASSPATH being set to empty
   C = option to set CLASSPATH, other than setting it in the environment
   T = test for presence

   Program    from         A B  C              T

   $JAVA      unknown      N Y  n/a            true
   gij        GCC 3.0      Y Y  n/a            gij --version >/dev/null
   java       JDK 1.1.8    Y Y  -classpath P   java -version 2>/dev/null
   jre        JDK 1.1.8    N Y  -classpath P   jre 2>/dev/null; test $? = 1
   java       JDK 1.3.0    Y Y  -classpath P   java -version 2>/dev/null
   jview      MS IE        Y Y  -cp P          jview -? >nul; %errorlevel% = 1

   The CLASSPATH is a colon separated list of pathnames. (On Windows: a
   semicolon separated list of pathnames.)

   We try the Java virtual machines in the following order:
     1. getenv ("JAVA"), because the user must be able to override our
	preferences,
     2. "gij", because it is a completely free JVM,
     3. "java", because it is a standard JVM,
     4. "jre", comes last because it requires a CLASSPATH environment variable,
     5. "jview", on Windows only, because it is frequently installed.

   We unset the JAVA_HOME environment variable, because a wrong setting of
   this variable can confuse the JDK's javac.
 */

bool
execute_java_class (class_name,
		    classpaths, classpaths_count, use_minimal_classpath,
		    args,
		    verbose, quiet,
		    executer, private_data)
     const char *class_name;
     const char * const *classpaths;
     unsigned int classpaths_count;
     bool use_minimal_classpath;
     const char * const *args;
     bool verbose;
     bool quiet;
     execute_fn *executer;
     void *private_data;
{
  bool err = false;
  unsigned int nargs;
  char *old_JAVA_HOME;

  {
    const char *java = getenv ("JAVA");
    if (java != NULL && java[0] != '\0')
      {
	/* Because $JAVA may consist of a command and options, we use the
	   shell.  Because $JAVA has been set by the user, we leave all
	   all environment variables in place, including JAVA_HOME, and
	   we don't erase the user's CLASSPATH.  */
	char *old_classpath;
	unsigned int command_length;
	char *command;
	char *argv[4];
	const char * const *arg;
	char *p;

	/* Set CLASSPATH.  */
	old_classpath =
	  set_classpath (classpaths, classpaths_count, false,
			 verbose);

	command_length = strlen (java);
	command_length += 1 + shell_quote_length (class_name);
	for (arg = args; *arg != NULL; arg++)
	  command_length += 1 + shell_quote_length (*arg);
	command_length += 1;

	command = (char *) alloca (command_length);
	p = command;
	/* Don't shell_quote $JAVA, because it may consist of a command
	   and options.  */
	memcpy (p, java, strlen (java));
	p += strlen (java);
	*p++ = ' ';
	p = shell_quote_copy (p, class_name);
	for (arg = args; *arg != NULL; arg++)
	  {
	    *p++ = ' ';
	    p = shell_quote_copy (p, *arg);
	  }
	*p++ = '\0';
	/* Ensure command_length was correctly calculated.  */
	if (p - command > command_length)
	  abort ();

	if (verbose)
	  printf ("%s\n", command);

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = NULL;
	err = executer (java, "/bin/sh", argv, private_data);

	/* Reset CLASSPATH.  */
	reset_classpath (old_classpath);

	goto done1;
      }
  }

  /* Count args.  */
  {
    const char * const *arg;

    for (nargs = 0, arg = args; *arg != NULL; nargs++, arg++)
     ;
  }

  /* Unset the JAVA_HOME environment variable.  */
  old_JAVA_HOME = getenv ("JAVA_HOME");
  if (old_JAVA_HOME != NULL)
    {
      old_JAVA_HOME = xstrdup (old_JAVA_HOME);
      unsetenv ("JAVA_HOME");
    }

  {
    static bool gij_tested;
    static bool gij_present;

    if (!gij_tested)
      {
	/* Test for presence of gij: "gij --version > /dev/null"  */
	char *argv[3];
	int exitstatus;

	argv[0] = "gij";
	argv[1] = "--version";
	argv[2] = NULL;
	exitstatus = execute ("gij", "gij", argv, false, true, true, false);
	gij_present = (exitstatus == 0);
	gij_tested = true;
      }

    if (gij_present)
      {
	char *old_classpath;
	char **argv = (char **) alloca ((2 + nargs + 1) * sizeof (char *));
	unsigned int i;

	/* Set CLASSPATH.  */
	old_classpath =
	  set_classpath (classpaths, classpaths_count, use_minimal_classpath,
			 verbose);

	argv[0] = "gij";
	argv[1] = (char *) class_name;
	for (i = 0; i <= nargs; i++)
	  argv[2 + i] = (char *) args[i];

	if (verbose)
	  {
	    char *command = shell_quote_argv (argv);
	    printf ("%s\n", command);
	    free (command);
	  }

	err = executer ("gij", "gij", argv, private_data);

	/* Reset CLASSPATH.  */
	reset_classpath (old_classpath);

	goto done2;
      }
  }

  {
    static bool java_tested;
    static bool java_present;

    if (!java_tested)
      {
	/* Test for presence of java: "java -version 2> /dev/null"  */
	char *argv[3];
	int exitstatus;

	argv[0] = "java";
	argv[1] = "-version";
	argv[2] = NULL;
	exitstatus = execute ("java", "java", argv, false, true, true, false);
	java_present = (exitstatus == 0);
	java_tested = true;
      }

    if (java_present)
      {
	char *old_classpath;
	char **argv = (char **) alloca ((2 + nargs + 1) * sizeof (char *));
	unsigned int i;

	/* Set CLASSPATH.  We don't use the "-classpath ..." option because
	   in JDK 1.1.x its argument should also contain the JDK's classes.zip,
	   but we don't know its location.  (In JDK 1.3.0 it would work.)  */
	old_classpath =
	  set_classpath (classpaths, classpaths_count, use_minimal_classpath,
			 verbose);

	argv[0] = "java";
	argv[1] = (char *) class_name;
	for (i = 0; i <= nargs; i++)
	  argv[2 + i] = (char *) args[i];

	if (verbose)
	  {
	    char *command = shell_quote_argv (argv);
	    printf ("%s\n", command);
	    free (command);
	  }

	err = executer ("java", "java", argv, private_data);

	/* Reset CLASSPATH.  */
	reset_classpath (old_classpath);

	goto done2;
      }
  }

  {
    static bool jre_tested;
    static bool jre_present;

    if (!jre_tested)
      {
	/* Test for presence of jre: "jre 2> /dev/null ; test $? = 1"  */
	char *argv[2];
	int exitstatus;

	argv[0] = "jre";
	argv[1] = NULL;
	exitstatus = execute ("jre", "jre", argv, false, true, true, false);
	jre_present = (exitstatus == 0 || exitstatus == 1);
	jre_tested = true;
      }

    if (jre_present)
      {
	char *old_classpath;
	char **argv = (char **) alloca ((2 + nargs + 1) * sizeof (char *));
	unsigned int i;

	/* Set CLASSPATH.  We don't use the "-classpath ..." option because
	   in JDK 1.1.x its argument should also contain the JDK's classes.zip,
	   but we don't know its location.  */
	old_classpath =
	  set_classpath (classpaths, classpaths_count, use_minimal_classpath,
			 verbose);

	argv[0] = "jre";
	argv[1] = (char *) class_name;
	for (i = 0; i <= nargs; i++)
	  argv[2 + i] = (char *) args[i];

	if (verbose)
	  {
	    char *command = shell_quote_argv (argv);
	    printf ("%s\n", command);
	    free (command);
	  }

	err = executer ("jre", "jre", argv, private_data);

	/* Reset CLASSPATH.  */
	reset_classpath (old_classpath);

	goto done2;
      }
  }

#if defined _WIN32 || defined __WIN32__
  /* Win32 */
  {
    static bool jview_tested;
    static bool jview_present;

    if (!jview_tested)
      {
	/* Test for presence of jview: "jview -? >nul ; test $? = 1"  */
	char *argv[3];
	int exitstatus;

	argv[0] = "jview";
	argv[1] = "-?";
	argv[2] = NULL;
	exitstatus = execute ("jview", "jview", argv, false, true, true,
			      false);
	jview_present = (exitstatus == 0 || exitstatus == 1);
	jview_tested = true;
      }

    if (jview_present)
      {
	char *old_classpath;
	char **argv = (char **) alloca ((2 + nargs + 1) * sizeof (char *));
	unsigned int i;

	/* Set CLASSPATH.  */
	old_classpath =
	  set_classpath (classpaths, classpaths_count, use_minimal_classpath,
			 verbose);

	argv[0] = "jview";
	argv[1] = (char *) class_name;
	for (i = 0; i <= nargs; i++)
	  argv[2 + i] = (char *) args[i];

	if (verbose)
	  {
	    char *command = shell_quote_argv (argv);
	    printf ("%s\n", command);
	    free (command);
	  }

	err = executer ("jview", "jview", argv, private_data);

	/* Reset CLASSPATH.  */
	reset_classpath (old_classpath);

	goto done2;
      }
  }
#endif

  if (!quiet)
    error (0, 0, _("Java virtual machine not found, try installing gij or set $JAVA"));
  err = true;

 done2:
  if (old_JAVA_HOME != NULL)
    {
      xsetenv ("JAVA_HOME", old_JAVA_HOME, 1);
      free (old_JAVA_HOME);
    }

 done1:
  return err;
}