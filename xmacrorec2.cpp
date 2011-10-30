/***************************************************************************** 
 *
 * xmacrorec2 - a utility for recording X mouse and key events.
 * Portions Copyright (C) 2000 Gabor Keresztfalvi <keresztg@mail.com>
 *
 * The recorded events are emitted to the standard output and can be played
 * with the xmacroplay utility. Using the Record extension on the local
 * display. No XTest playing on remote display during recording in this
 * version!
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
 *	
 ****************************************************************************/
//#define DEBUG
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
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>

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

#define PROG "xmacrorec2"

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

/***************************************************************************** 
 * Key used for quitting the application.
 ****************************************************************************/
unsigned int QuitKey;
bool HasQuitKey = false;

/***************************************************************************** 
 * Private data used in eventCallback.
 ****************************************************************************/
typedef struct
{
	int Status1, Status2, x, y, mmoved, doit;
	unsigned int QuitKey;
	Display *LocalDpy, *RecDpy;
	XRecordContext rc;
} Priv;

/****************************************************************************/
/*! Prints the usage, i.e. how the program is used. Exits the application with
    the passed exit-code.

	\arg const int ExitCode - the exitcode to use for exiting.
*/
/****************************************************************************/
void usage (const int exitCode) {

  // print the usage
  cerr << PROG << " " << VERSION << endl;
  cerr << "Usage: " << PROG << " [options] " << endl;
  cerr << "Options: " << endl;
  cerr << "  -s  FACTOR  scalefactor for coordinates. Default: 1.0." << endl
	   << "  -k  KEYCODE the keycode for the key used for quitting." << endl
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

    // is this '-k'?
	else if ( strcmp (argv[Index], "-k" ) == 0 && Index + 1 < argc ) {
	  // yep, and there seems to be a parameter too, interpret it as a
	  // number
	  if ( sscanf ( argv[Index + 1], "%d", &QuitKey ) != 1 ) {
		// oops, not a valid integer
		cerr << "Invalid parameter for '-k'." << QuitKey << endl;
		usage ( EXIT_FAILURE );
	  }

	  // now we have a key for quitting
	  HasQuitKey = true;
	  Index++;
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
/*! Connects to the local display. Returns the \c Display or \c 0 if
    no display could be obtained.
*/
/****************************************************************************/
Display * localDisplay () {

  // open the display
  Display * D = XOpenDisplay ( 0 );

  // did we get it?
  if ( ! D ) {
	// nope, so show error and abort
	cerr << PROG << ": could not open display \"" << XDisplayName ( 0 )
		 << "\", aborting." << endl;
	exit ( EXIT_FAILURE );
  }

  // return the display
  return D;
}

/****************************************************************************/
/*! Function that finds out the key the user wishes to use for quitting the
    application. This must be configurable, as a suitable key is not always
	possible to determine in advance. By letting the user pick a key one that
	does not interfere with the needed applications can be chosen.

	The function grabs the keyboard and waits for a key to be pressed. Returns
	the keycode of the pressed key.

    \arg Display * Dpy - used display.
	\arg int Screen - the used screen.
*/
/****************************************************************************/
int findQuitKey (Display * Dpy, int Screen) { 

  XEvent    Event;
  XKeyEvent EKey;
  Window    Target, Root;
  bool      Loop = true;
  int       Error;
  
  // get the root window and set default target
  Root   = RootWindow ( Dpy, Screen );
  Target = None;

  // grab the keyboard
  Error = XGrabKeyboard ( Dpy, Root, False, GrabModeSync, GrabModeAsync, CurrentTime );

  // did we succeed in grabbing the keyboard?
  if ( Error != GrabSuccess) {
	// nope, abort
	cerr << "Could not grab the keyboard, aborting." << endl;
	exit ( EXIT_FAILURE );
  }

  // print a message to the user informing about what's going on
  cerr << endl
	   << "Press the key you want to use to end the application. "
	   << "This key can be any key, " << endl
	   << "as long as you don't need it while working with the remote display." << endl
	   << "A good choice is Escape." << endl << endl;
 
  // let the user select a window...
  while ( Loop ) {
    // allow one more event
	XAllowEvents ( Dpy, SyncPointer, CurrentTime);	
    XWindowEvent ( Dpy, Root, KeyPressMask, &Event);
	
	// what did we get?
    if ( Event.type == KeyPress ) {
	  // a key was pressed, don't loop more
	  EKey = Event.xkey;
	  Loop = false;
	}
  } 

  // we're done with pointer and keyboard
  XUngrabPointer  ( Dpy, CurrentTime );      
  XUngrabKeyboard ( Dpy, CurrentTime );      

  // show the user what was chosen
  cerr << "The chosen quit-key has the keycode: " << EKey.keycode << endl;
  
  // return the found key
  return EKey.keycode;
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


#ifdef DEBUG
#define DBG cerr << "type: " << type << " serial: " << seq << endl; \
		cerr << "send_event: " << (unsigned int)(ud1[0]>>8) << endl; \
		cerr << "window:  " << hex << wevent << " root: " << wroot << endl; \
		cerr << "subwindow:  " << wchild << " time: " << dec << tstamp << endl; \
		cerr << "x:  " << eventx << " y: " << eventy << endl; \
		cerr << "x_root:  " << rootx << " y_root: " << rooty << endl; \
		cerr << "state:  " << kstate << " detail: " << detail << endl; \
		cerr << "same_screen:  " << samescreen << endl << "------" << endl
#else
#define DBG
#endif

void eventCallback(XPointer priv, XRecordInterceptData *d)
{
  Priv *p=(Priv *) priv;
  unsigned int *ud4, tstamp, wroot, wevent, wchild, type, detail;
  unsigned char *ud1, type1, detail1, samescreen;
  unsigned short *ud2, seq;
  short *d2, rootx, rooty, eventx, eventy, kstate;

  if (d->category==XRecordStartOfData) cerr << "Got Start Of Data" << endl;
  if (d->category==XRecordEndOfData) cerr << "Got End Of Data" << endl;
  if (d->category!=XRecordFromServer || p->doit==0)
  {
	cerr << "Skipping..." << endl;
  	goto returning;
  }
  if (d->client_swapped==True) cerr << "Client is swapped!!!" << endl;
  ud1=(unsigned char *)d->data;
  ud2=(unsigned short *)d->data;
   d2=(short *)d->data;
  ud4=(unsigned int *)d->data;

  type1=ud1[0]&0x7F; type=type1;
  detail1=ud1[1]; detail=detail1;
  seq=ud2[1];
  tstamp=ud4[1];
  wroot=ud4[2];
  wevent=ud4[3];
  wchild=ud4[4];
  rootx=d2[10];
  rooty=d2[11];
  eventx=d2[12];
  eventy=d2[13];
  kstate=d2[14];
  samescreen=ud1[30];

  if (p->Status1)
  {
	  p->Status1--;
	  if (type==KeyRelease)
	  {
		cerr << "- Skipping stale KeyRelease event. " << p->Status1 << endl;
		goto returning;
	  } else p->Status1=0;
  }
  if (p->x==-1 && p->y==-1 && p->mmoved==0 && type!=MotionNotify)
  {
  	cerr << "- Please move the mouse before any other event to synchronize pointer" << endl;
  	cerr << "  coordinates! This event is now ignored!" << endl;
  	goto returning;
  }
  // what did we get?
  switch (type) {
    case ButtonPress:
	  // button pressed, create event
		DBG;
	  if (p->mmoved)
	  {
		cout << "MotionNotify " << p->x << " " << p->y << endl;
		p->mmoved=0;
	  }
	  if (p->Status2<0) p->Status2=0;
	  p->Status2++;
	  cout << "ButtonPress " << detail << endl;
      break;

    case ButtonRelease:
	  // button released, create event
		DBG;
	  if (p->mmoved)
	  {
		cout << "MotionNotify " << p->x << " " << p->y << endl;
		p->mmoved=0;
	  }
	  p->Status2--;
	  if (p->Status2<0) p->Status2=0;
	  cout << "ButtonRelease " << detail << endl;
	  break;

	case MotionNotify:
	  // motion-event, create event
		DBG;
	  if (p->Status2>0)
	  {
	  	cout << "MotionNotify " << rootx << " " << rooty << endl;
	  	p->mmoved=0;
	  }
	  else p->mmoved=1;
	  p->x=rootx;
	  p->y=rooty;
	  break;

	case KeyPress:
	  // a key was pressed
		DBG;
	  // should we stop looping, i.e. did the user press the quitkey?
	  if ( detail == p->QuitKey ) {
		// yep, no more loops
		cerr << "Got QuitKey, so exiting..." << endl;
		p->doit=0;
	  }
	  else {
		// send the keycode to the remote server
		if (p->mmoved)
		{
			cout << "MotionNotify " << p->x << " " << p->y << endl;
			p->mmoved=0;
		}
		cout << "KeyStrPress " << XKeysymToString(XKeycodeToKeysym(p->LocalDpy,detail,0)) << endl;
	  }
	  break;

	case KeyRelease:
	  // a key was released
		DBG;
	  if (p->mmoved)
	  {
		cout << "MotionNotify " << p->x << " " << p->y << endl;
		p->mmoved=0;
	  }
	  cout << "KeyStrRelease " << XKeysymToString(XKeycodeToKeysym(p->LocalDpy,detail,0)) << endl;
	  break;
  }
returning:
  XRecordFreeData(d);
}

/****************************************************************************/
/*! Main event-loop of the application. Loops until a key with the keycode
    \a QuitKey is pressed. Sends all mouse- and key-events to the remote
	display.

    \arg Display * LocalDpy - used display.
	\arg int LocalScreen - the used screen.
	\arg Display * RecDpy - used display.
	\arg unsigned int QuitKey - the key when pressed that quits the eventloop.
*/
/****************************************************************************/
void eventLoop (Display * LocalDpy, int LocalScreen,
				Display * RecDpy, unsigned int QuitKey) {

  Window       Root, rRoot, rChild;
  XRecordContext rc;
  XRecordRange *rr;
  XRecordClientSpec rcs;
  Priv         priv;
  int rootx, rooty, winx, winy;
  unsigned int mmask;
  Bool ret;
  Status sret;
  
  // get the root window and set default target
  Root = RootWindow ( LocalDpy, LocalScreen );

  ret=XQueryPointer(LocalDpy, Root, &rRoot, &rChild, &rootx, &rooty, &winx, &winy, &mmask);
  cerr << "XQueryPointer returned: " << ret << endl;
  rr=XRecordAllocRange();
  if (!rr)
  {
  	cerr << "Could not alloc record range, aborting." << endl;
  	exit(EXIT_FAILURE);
  }
  rr->device_events.first=KeyPress;
  rr->device_events.last=MotionNotify;
  rcs=XRecordAllClients;
  rc=XRecordCreateContext(RecDpy, 0, &rcs, 1, &rr, 1);
  if (!rc)
  {
  	cerr << "Could not create a record context, aborting." << endl;
  	exit(EXIT_FAILURE);
  }
  priv.x=rootx;
  priv.y=rooty;
  priv.mmoved=1;
  priv.Status2=0;
  priv.Status1=2;
  priv.doit=1;
  priv.QuitKey=QuitKey;
  priv.LocalDpy=LocalDpy;
  priv.RecDpy=RecDpy;
  priv.rc=rc;

  if (!XRecordEnableContextAsync(RecDpy, rc, eventCallback, (XPointer) &priv))
  {
  	cerr << "Could not enable the record context, aborting." << endl;
  	exit(EXIT_FAILURE);
  }

  while (priv.doit) XRecordProcessReplies(RecDpy);

  sret=XRecordDisableContext(LocalDpy, rc);
  if (!sret) cerr << "XRecordDisableContext failed!" << endl;
  sret=XRecordFreeContext(LocalDpy, rc);
  if (!sret) cerr << "XRecordFreeContext failed!" << endl;
  XFree(rr);
}


/****************************************************************************/
/*! Main function of the application. It expects no commandline arguments.

    \arg int argc - number of commandline arguments.
	\arg char * argv[] - vector of the commandline argument strings.
*/
/****************************************************************************/
int main (int argc, char * argv[]) {

  int Major, Minor;

  // parse commandline arguments
  parseCommandLine ( argc, argv );
  
  // open the local display twice
  Display * LocalDpy = localDisplay ();
  Display * RecDpy = localDisplay ();

  // get the screens too
  int LocalScreen  = DefaultScreen ( LocalDpy );

  cerr << "Server VendorRelease: " << VendorRelease(RecDpy) << endl;
  // does the remote display have the Xrecord-extension?
  if ( ! XRecordQueryVersion (RecDpy, &Major, &Minor ) ) {
	// nope, extension not supported
	cerr << PROG << ": XRecord extension not supported on server \""
		 << DisplayString(RecDpy) << "\"" << endl;

	// close the display and go away
	XCloseDisplay ( RecDpy );
	exit ( EXIT_FAILURE );
  }

  // print some information
  cerr << "XRecord for server \"" << DisplayString(RecDpy) << "\" is version "
	   << Major << "." << Minor << "." << endl << endl;;

  // do we already have a quit key? If one was supplied as a commandline
  // argument we use that key
  if ( ! HasQuitKey ) {
	// nope, so find the key that quits the application
	QuitKey = findQuitKey ( LocalDpy, LocalScreen );
  }

  else {
	// show the user which key will be used
	cerr << "The used quit-key has the keycode: " << QuitKey << endl;
  }
  
  // start the main event loop
  eventLoop ( LocalDpy, LocalScreen, RecDpy, QuitKey );

  // we're done with the display
//  XCloseDisplay ( RecDpy );
  XCloseDisplay ( LocalDpy );

  cerr << PROG << ": Exiting. " << endl;
  
  // go away
  exit ( EXIT_SUCCESS );
}
