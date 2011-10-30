/***************************************************************************** 
 *
 * xmacroplay - a utility for playing X mouse and key events.
 * Portions Copyright (C) 2000 Gabor Keresztfalvi <keresztg@mail.com>
 *
 * The recorded events are read from the standard input.
 *
 * This program is heavily based on
 * xremote (http://infa.abo.fi/~chakie/xremote/) which is:
 * Copyright (C) 2000 Jan Ekholm <chakie@infa.abo.fi>
 *	
 * This program is free software; you can redistribute it and/or modify it  
 * under the terms of the GNU General Public License as published by the  
 * Free Software Foundation; either version 2 of the License, or (at your 
 * option) any later version.
 *	
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 * for more details.
 *	
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ****************************************************************************/

/***************************************************************************** 
 * Do we have config.h?
 ****************************************************************************/
#ifdef HAVE_CONFIG
#include "config.h"
#endif

/***************************************************************************** 
 * Includes
 ****************************************************************************/
#include <stdio.h>		
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "chartbl.h"
/***************************************************************************** 
 * What iostream do we have?
 ****************************************************************************/
#ifdef HAVE_IOSTREAM
#include <iostream>
#include <iomanip>
#else
#include <iostream.h>
#include <iomanip.h>
#endif

#define PROG "xmacroplay"

/***************************************************************************** 
 * The delay in milliseconds when sending events to the remote display
 ****************************************************************************/
const int DefaultDelay = 10;

/***************************************************************************** 
 * The multiplier used fot scaling coordinates before sending them to the
 * remote display. By default we don't scale at all
 ****************************************************************************/
const float DefaultScale = 1.0;

/***************************************************************************** 
 * Globals...
 ****************************************************************************/
int   Delay = DefaultDelay;
float Scale = DefaultScale;
char * Remote;

/****************************************************************************/
/*! Prints the usage, i.e. how the program is used. Exits the application with
    the passed exit-code.

	\arg const int ExitCode - the exitcode to use for exiting.
*/
/****************************************************************************/
void usage (const int exitCode) {

  // print the usage
  cerr << PROG << " " << VERSION << endl;
  cerr << "Usage: " << PROG << " [options] remote_display" << endl;
  cerr << "Options: " << endl;
  cerr << "  -d  DELAY   delay in milliseconds for events sent to remote display." << endl
	   << "              Default: 10ms."
	   << endl
	   << "  -s  FACTOR  scalefactor for coordinates. Default: 1.0." << endl
	   << "  -v          show version. " << endl
	   << "  -h          this help. " << endl << endl;

  // we're done
  exit ( EXIT_SUCCESS );
}


/****************************************************************************/
/*! Prints the version of the application and exits.
*/
/****************************************************************************/
void version () {

  // print the version
  cerr << PROG << " " << VERSION << endl;
 
  // we're done
  exit ( EXIT_SUCCESS );
}


/****************************************************************************/
/*! Parses the commandline and stores all data in globals (shudder). Exits
    the application with a failed exitcode if a parameter is illegal.

	\arg int argc - number of commandline arguments.
	\arg char * argv[] - vector of the commandline argument strings.
*/
/****************************************************************************/
void parseCommandLine (int argc, char * argv[]) {

  int Index = 1;
  
  // check the number of arguments
  if ( argc < 2 ) {
	// oops, too few arguments, go away
	usage ( EXIT_FAILURE );
  }

  // loop through all arguments except the last, which is assumed to be the
  // name of the display
  while ( Index < argc ) {
	
	// is this '-v'?
	if ( strcmp (argv[Index], "-v" ) == 0 ) {
	  // yep, show version and exit
	  version ();
	}

	// is this '-h'?
	if ( strcmp (argv[Index], "-h" ) == 0 ) {
	  // yep, show usage and exit
	  usage ( EXIT_SUCCESS );
	}

	// is this '-d'?
	else if ( strcmp (argv[Index], "-d" ) == 0 && Index + 1 < argc ) {
	  // yep, and there seems to be a parameter too, interpret it as a
	  // number
	  if ( sscanf ( argv[Index + 1], "%d", &Delay ) != 1 ) {
		// oops, not a valid intereger
		cerr << "Invalid parameter for '-d'." << endl;
		usage ( EXIT_FAILURE );
	  }
	  
	  Index++;
	}

	// is this '-s'?
	else if ( strcmp (argv[Index], "-s" ) == 0 && Index + 1 < argc ) {
	  // yep, and there seems to be a parameter too, interpret it as a
	  // floating point number
	  if ( sscanf ( argv[Index + 1], "%f", &Scale ) != 1 ) {
		// oops, not a valid intereger
		cerr << "Invalid parameter for '-s'." << endl;
		usage ( EXIT_FAILURE );
	  }
	  
	  Index++;
	}

	// is this the last parameter?
	else if ( Index == argc - 1 ) {
	  // yep, we assume it's the display, store it
	  Remote = argv [ Index ];
	}

	else {
	  // we got this far, the parameter is no good...
	  cerr << "Invalid parameter '" << argv[Index] << "'." << endl;
	  usage ( EXIT_FAILURE );
	}

	// next value
	Index++;
  }
}

/****************************************************************************/
/*! Connects to the desired display. Returns the \c Display or \c 0 if
    no display could be obtained.

	\arg const char * DisplayName - name of the remote display.
*/
/****************************************************************************/
Display * remoteDisplay (const char * DisplayName) {

  int Event, Error;
  int Major, Minor;  

  // open the display
  Display * D = XOpenDisplay ( DisplayName );

  // did we get it?
  if ( ! D ) {
	// nope, so show error and abort
	cerr << PROG << ": could not open display \"" << XDisplayName ( DisplayName )
		 << "\", aborting." << endl;
	exit ( EXIT_FAILURE );
  }

  // does the remote display have the Xtest-extension?
  if ( ! XTestQueryExtension (D, &Event, &Error, &Major, &Minor ) ) {
	// nope, extension not supported
	cerr << PROG << ": XTest extension not supported on server \""
		 << DisplayString(D) << "\"" << endl;

	// close the display and go away
	XCloseDisplay ( D );
	exit ( EXIT_FAILURE );
  }

  // print some information
  cerr << "XTest for server \"" << DisplayString(D) << "\" is version "
	   << Major << "." << Minor << "." << endl << endl;;

  // execute requests even if server is grabbed 
  XTestGrabControl ( D, True ); 

  // sync the server
  XSync ( D,True ); 

  // return the display
  return D;
}

/****************************************************************************/
/*! Scales the passed coordinate with the given saling factor. the factor is
    either given as a commandline argument or it is 1.0.
*/
/****************************************************************************/
int scale (const int Coordinate) {

  // perform the scaling, all in one ugly line
  return (int)( (float)Coordinate * Scale );
}

/****************************************************************************/
/*! Sends a \a character to the remote display \a RemoteDpy. The character is
    converted to a \c KeySym based on a character table and then reconverted to
	a \c KeyCode on the remote display. Seems to work quite ok, apart from
	something weird with the Alt key.

    \arg Display * RemoteDpy - used display.
	\arg char c - character to send.
*/
/****************************************************************************/
void sendChar(Display *RemoteDpy, char c)
{
	KeySym ks, sks, *kss, ksl, ksu;
	KeyCode kc, skc;
	int syms;
#ifdef DEBUG
	int i;
#endif

	sks=XK_Shift_L;

	ks=XStringToKeysym(chartbl[0][(unsigned char)c]);
	if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	{
  		cerr << "No keycode on remote display found for char: " << c << endl;
	  	return;
	}
	if ( ( skc = XKeysymToKeycode ( RemoteDpy, sks ) ) == 0 )
	{
  		cerr << "No keycode on remote display found for XK_Shift_L!" << endl;
	  	return;
	}

	kss=XGetKeyboardMapping(RemoteDpy, kc, 1, &syms);
	if (!kss)
	{
  		cerr << "XGetKeyboardMapping failed on the remote display (keycode: " << kc << ")" << endl;
	  	return;
	}
	for (; syms && (!kss[syms-1]); syms--);
	if (!syms)
	{
  		cerr << "XGetKeyboardMapping failed on the remote display (no syms) (keycode: " << kc << ")" << endl;
		XFree(kss);
	  	return;
	}
	XConvertCase(ks,&ksl,&ksu);
#ifdef DEBUG
	cout << "kss: ";
	for (i=0; i<syms; i++) cout << kss[i] << " ";
	cout << "(" << ks << " l: " << ksl << "  h: " << ksu << ")" << endl;
#endif
	if (ks==kss[0] && (ks==ksl && ks==ksu)) sks=NoSymbol;
	if (ks==ksl && ks!=ksu) sks=NoSymbol;
	if (sks!=NoSymbol) XTestFakeKeyEvent ( RemoteDpy, skc, True, Delay );
	XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	XFlush ( RemoteDpy );
	XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	if (sks!=NoSymbol) XTestFakeKeyEvent ( RemoteDpy, skc, False, Delay );
	XFlush ( RemoteDpy );
	XFree(kss);
}

/****************************************************************************/
/*! Main event-loop of the application. Loops until a key with the keycode
    \a QuitKey is pressed. Sends all mouse- and key-events to the remote
	display.

    \arg Display * RemoteDpy - used display.
	\arg int RemoteScreen - the used screen.
*/
/****************************************************************************/
void eventLoop (Display * RemoteDpy, int RemoteScreen) {

  char ev[200], str[1024];
  int x, y;
  unsigned int b;
  KeySym ks;
  KeyCode kc;
  
  while ( !cin.eof() ) {
	cin >> ev;
	if (ev[0]=='#')
	{
	  cout << "Comment: " << ev << endl;
	  continue;
	}
	if (!strcasecmp("Delay",ev))
	{
	  cin >> b;
	  cout << "Delay: " << b << endl;
	  sleep ( b );
	}
	else if (!strcasecmp("ButtonPress",ev))
	{
	  cin >> b;
	  cout << "ButtonPress: " << b << endl;
	  XTestFakeButtonEvent ( RemoteDpy, b, True, Delay );
	}
	else if (!strcasecmp("ButtonRelease",ev))
	{
	  cin >> b;
	  cout << "ButtonRelease: " << b << endl;
	  XTestFakeButtonEvent ( RemoteDpy, b, False, Delay );
	}
	else if (!strcasecmp("MotionNotify",ev))
	{
	  cin >> x >> y;
	  cout << "MotionNotify: " << x << " " << y << endl;
	  XTestFakeMotionEvent ( RemoteDpy, RemoteScreen , scale ( x ), scale ( y ), Delay ); 
	}
	else if (!strcasecmp("KeyCodePress",ev))
	{
	  cin >> kc;
	  cout << "KeyPress: " << kc << endl;
	  XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	}
	else if (!strcasecmp("KeyCodeRelease",ev))
	{
	  cin >> kc;
	  cout << "KeyRelease: " << kc << endl;
  	  XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	}
	else if (!strcasecmp("KeySym",ev))
	{
	  cin >> ks;
	  cout << "KeySym: " << ks << endl;
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for keysym: " << ks << endl;
	  	continue;
	  }
	  XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	  XFlush ( RemoteDpy );
	  XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	}
	else if (!strcasecmp("KeySymPress",ev))
	{
	  cin >> ks;
	  cout << "KeySymPress: " << ks << endl;
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for keysym: " << ks << endl;
	  	continue;
	  }
	  XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	}
	else if (!strcasecmp("KeySymRelease",ev))
	{
	  cin >> ks;
	  cout << "KeySymRelease: " << ks << endl;
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for keysym: " << ks << endl;
	  	continue;
	  }
  	  XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	}
	else if (!strcasecmp("KeyStr",ev))
	{
	  cin >> ev;
	  cout << "KeyStr: " << ev << endl;
	  ks=XStringToKeysym(ev);
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for '" << ev << "': " << ks << endl;
	  	continue;
	  }
	  XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	  XFlush ( RemoteDpy );
	  XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	}
	else if (!strcasecmp("KeyStrPress",ev))
	{
	  cin >> ev;
	  cout << "KeyStrPress: " << ev << endl;
	  ks=XStringToKeysym(ev);
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for '" << ev << "': " << ks << endl;
	  	continue;
	  }
	  XTestFakeKeyEvent ( RemoteDpy, kc, True, Delay );
	}
	else if (!strcasecmp("KeyStrRelease",ev))
	{
	  cin >> ev;
	  cout << "KeyStrRelease: " << ev << endl;
	  ks=XStringToKeysym(ev);
	  if ( ( kc = XKeysymToKeycode ( RemoteDpy, ks ) ) == 0 )
	  {
	  	cerr << "No keycode on remote display found for '" << ev << "': " << ks << endl;
	  	continue;
	  }
  	  XTestFakeKeyEvent ( RemoteDpy, kc, False, Delay );
	}
	else if (!strcasecmp("String",ev))
	{
	  cin.ignore().get(str,1024);
	  cout << "String: " << str << endl;
	  b=0;
	  while(str[b]) sendChar(RemoteDpy, str[b++]);
	}
	else if (ev[0]!=0) cout << "Unknown tag: " << ev << endl;

	// sync the remote server
	XFlush ( RemoteDpy );
  } 
}


/****************************************************************************/
/*! Main function of the application. It expects no commandline arguments.

    \arg int argc - number of commandline arguments.
	\arg char * argv[] - vector of the commandline argument strings.
*/
/****************************************************************************/
int main (int argc, char * argv[]) {

  // parse commandline arguments
  parseCommandLine ( argc, argv );
  
  // open the remote display or abort
  Display * RemoteDpy = remoteDisplay ( Remote );

  // get the screens too
  int RemoteScreen = DefaultScreen ( RemoteDpy );
  
  XTestDiscard ( RemoteDpy );

  // start the main event loop
  eventLoop ( RemoteDpy, RemoteScreen );

  // discard and even flush all events on the remote display
  XTestDiscard ( RemoteDpy );
  XFlush ( RemoteDpy ); 

  // we're done with the display
  XCloseDisplay ( RemoteDpy );

  cerr << PROG << ": pointer and keyboard released. " << endl;
  
  // go away
  exit ( EXIT_SUCCESS );
}





