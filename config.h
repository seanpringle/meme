// Meme configuration

// a folder
#define MEMEDIR "/home/sean/.meme/"

// any URI
#define HOMEPAGE "http://aerosuidae.net/meme.html"

// a script file that can be executed with CTL+j
// useful for implementing form password autofill - see example script.js
// NULL to ignore
//#define SCRIPTFILE MEMEDIR "script.js"
#define SCRIPTFILE NULL

// a script file executed when a new page is loaded
// note: page may not have finished loading/rendering! running script.js here won't always work...
// NULL to ignore
//#define ONLOADFILE MEMEDIR "onload.js"
#define ONLOADFILE NULL

// also see cookies setting in DOWNLOAD, below
// NULL to ignore
#define COOKIEFILE MEMEDIR "cookies"

// list of urls
// NULL to ignore
#define BOOKMARKFILE MEMEDIR "bookmarks"

// %s is replace with the search term, url encoded
// check and change .au
#define SEARCHURL "http://google.com.au/search?q=%s"

// default cookie life
#define SESSIONTIME 86400

// check and change 'en-AU'
#define USERAGENT "Meme/1.0 (X11; U; Unix; en-AU) AppleWebKit/531.2+ Compatible (Safari)"
//#define USERAGENT "Mozilla/5.0 (X11; U; Linux x86_64; en-US) AppleWebKit/534.13 (KHTML, like Gecko) Chrome/9.0.597.0 Safari/534.13"
//#define USERAGENT "Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 Mobile/7B334b Safari/531.21.10"

// initial window dimensions
#define WIDTH 1024
#define HEIGHT 768

// runs wget in an xterm.
#define DOWNLOAD(uri, file, ref, cwd) \
	(const char *[]){ "/bin/sh", "-c", \
	"xterm -e \"wget --load-cookies " COOKIEFILE " --user-agent='" USERAGENT "' --referer='$3' -O '$1/$2' '$0';\"", \
	uri, cwd, file, ref, NULL }

// starts a new window as a separate process
#define NEWWINDOW(uri) \
	(const char *[]){ "/bin/sh", "-c", \
	"meme \"$0\"", uri, NULL }

// key combinations
#define ALT GDK_MOD1_MASK
#define CTL GDK_CONTROL_MASK
struct keycontrol keys[] = {
	// modifier, key, action
	{ ALT, GDK_Home,  "go-home"        },
	{ ALT, GDK_Left,  "go-back"        },
	{ ALT, GDK_Right, "go-forward"     },
	{ CTL, GDK_equal, "zoom-in"        },
	{ CTL, GDK_minus, "zoom-out"       },
	{ CTL, GDK_0,     "zoom-reset"     },
	{ CTL, GDK_s,     "toggle-source"  },
	{ CTL, GDK_r,     "reload-nocache" },
	{ CTL, GDK_j,     "run-scriptfile" },
	{ CTL, GDK_l,     "focus-navbar"   },
	{ CTL, GDK_t,     "new-window"     },
	{ CTL, GDK_p,     "print-page"     },
	{ CTL, GDK_f,     "find-text"      },
	{ CTL, GDK_b,     "bookmark-page"  },
	{ 0, 0, NULL }
};

// true to enable webkit plugins (flash, etc) by default
// when false, can still be enabled on the fly with "!plugins on"
gboolean flag_plugins = FALSE;
