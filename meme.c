/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (c) 2011 Sean Pringle <sean.pringle@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <signal.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <webkit/webkit.h>
#include <glib/gstdio.h>
#include <JavaScriptCore/JavaScript.h>
#include <sys/file.h>

static GtkWidget *main_window;
static WebKitWebView *web_view;
static WebKitWebSettings *web_settings;
static WebKitWebInspector *web_inspector;
static GtkWidget* uri_entry;
static GtkEntryCompletion *uri_completion;
static GtkListStore *uri_model;
static GtkTreeIter uri_iter;

static gchar* main_title;
static gdouble load_progress;

#define BLOCK 1024

struct keycontrol {
	unsigned int mod;
	unsigned int key;
	char *action;
};

#include "config.h"

void
sigchld(int unused)
{
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
	{
		printf("Can't install SIGCHLD handler");
		exit(-1);
	}
	while(0 < waitpid(-1, NULL, WNOHANG));
}
void
spawn (const char **cmd)
{
	if(fork() == 0)
	{
		setsid();
		execvp((char*)cmd[0], (char**)cmd);
		fprintf(stderr, "meme: execvp %s", cmd[0]);
		perror(" failed");
		exit(0);
	}
}
void
js_frame (char *script, WebKitWebFrame *frame)
{
	JSValueRef exception = NULL;
	JSStringRef jsscript = JSStringCreateWithUTF8CString(script);
	JSContextRef ref = webkit_web_frame_get_global_context(frame);
	JSEvaluateScript(ref, jsscript, JSContextGetGlobalObject(ref), NULL, 0, &exception);
}
void
jsf_frame (char *src, WebKitWebFrame *frame)
{
	if (!src) return;
	GError *error = NULL; char *script;
	if (g_file_get_contents(src, &script, NULL, &error)) js_frame(script, frame);
	else fprintf(stderr, "failed to run: %s\n", src);
}
void
js (char *script)
{
	js_frame(script, webkit_web_view_get_main_frame(web_view));
}
void
jsf (char *src)
{
	if (!src) return;
	GError *error = NULL; char *script;
	if (g_file_get_contents(src, &script, NULL, &error)) js(script);
	else fprintf(stderr, "failed to run: %s\n", src);
}
void
toggle_source_mode()
{
	gboolean s = webkit_web_view_get_view_source_mode(web_view);
	webkit_web_view_set_view_source_mode(web_view, !s);
	webkit_web_view_reload(web_view);
}
void
apply_bookmarks()
{
	if (!BOOKMARKFILE) return;
	FILE *f = fopen(BOOKMARKFILE, "r");
	if (!f)
	{
		fprintf(stderr, "could not read: %s\n", BOOKMARKFILE);
		return;
	}

	uri_model = gtk_list_store_new(1, G_TYPE_STRING);
	char line[BLOCK], *p;
	while (fgets(line, BLOCK-2, f))
	{
		p = line; while (*p && *p != '\n') p++; *p = '\0';
		if (strlen(line))
		{
			gtk_list_store_append(uri_model, &uri_iter);
			gtk_list_store_set(uri_model, &uri_iter, 0, line, -1);
		}
	}
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(uri_model), 0, GTK_SORT_ASCENDING);
	gtk_entry_completion_set_model(uri_completion, GTK_TREE_MODEL(uri_model));

	fclose(f);
}
static void
select_uri_entry()
{
	gtk_editable_select_region(GTK_EDITABLE(uri_entry), 0, -1);
}
static void
focus_uri_entry()
{
	gtk_widget_grab_focus(GTK_WIDGET(uri_entry));
}
static void
focus_in_uri_entry_cb()
{
	apply_bookmarks();
}
static void
focus_uri_entry_search()
{
	focus_uri_entry();
	gtk_entry_set_text(GTK_ENTRY(uri_entry), "/");
	gtk_editable_set_position(GTK_EDITABLE(uri_entry), -1);
}
static void
focus_uri_entry_bookmark()
{
	gint pos = 0;
	focus_uri_entry();
	gtk_editable_insert_text(GTK_EDITABLE(uri_entry), "!bookmark ", 10, &pos);
	gtk_editable_select_region(GTK_EDITABLE(uri_entry), pos, -1);
}
void
default_uri_entry()
{
	gtk_entry_set_text(GTK_ENTRY(uri_entry), webkit_web_view_get_uri(web_view));
}
static void
activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
	char pad[BLOCK], tmp[BLOCK];
	const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!uri) return;
	// find text
	if (strstr(uri, "/") == uri)
	{
		webkit_web_view_search_text(web_view, uri+1, FALSE, TRUE, TRUE);
		return;
	}
	// command
	if (strstr(uri, "!") == uri)
	{
		gboolean flag;
		if (strstr(uri+1, "plugins") == uri+1)
		{
			flag = strstr(uri+9, "on") == uri+9 ? TRUE: FALSE;
			g_object_set(G_OBJECT(web_settings), "enable-plugins", flag, NULL);
			default_uri_entry();
			webkit_web_view_reload(web_view);
		} else
		if (strstr(uri+1, "bookmark") == uri+1 && isalnum(uri[10]))
		{
			sprintf(pad, "echo %s >> %s && sort -u -o %s %s",
				uri+10, BOOKMARKFILE, BOOKMARKFILE, BOOKMARKFILE);
			if (!system(pad)) apply_bookmarks();
			default_uri_entry();
		}
		return;
	}
	if (strcmp(uri, "about:bookmarks") == 0)
	{
		sprintf(pad, "file://%s", BOOKMARKFILE);
		webkit_web_view_load_uri(web_view, pad);
		return;
	}
	// convert a non-fqdn to a search term
	if (!strstr(uri, "localhost") && (strstr(uri, " ") || !strstr(uri, ".")))
	{
		int j = 0, i = 0;
		while (uri[j])
		{
			if (isalnum(uri[j])) tmp[i++] = uri[j];
			else i += sprintf(tmp+i, "%%%x", uri[j]);
			j++;
		} tmp[i] = '\0';
		sprintf(pad, SEARCHURL, tmp);
	} else
	{
		sprintf(pad, "%s%s", strstr(uri, "://") ? "": "http://", uri);
	}
	webkit_web_view_load_uri (web_view, pad);
}
/*
static void
go_back_cb (GtkWidget* widget, gpointer data)
{
	webkit_web_view_go_back (web_view);
}
static void
go_forward_cb (GtkWidget* widget, gpointer data)
{
	webkit_web_view_go_forward (web_view);
}
*/
static void
update_title (GtkWindow* window)
{
	GString* string = g_string_new(main_title && strlen(main_title) ? main_title : "untitled");
	if (load_progress < 100)
	{
		int d = load_progress;
		g_string_append_printf (string, " (%d%%)", d);
	}
	g_string_append(string, " - Meme");
	gchar* title = g_string_free (string, FALSE);
	gtk_window_set_title (window, title);
	g_free (title);
}
static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data)
{
	if (link) gtk_entry_set_text (GTK_ENTRY (uri_entry), link);
	else default_uri_entry();
}
static void
notify_title_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
	if (main_title) g_free (main_title);
	main_title = g_strdup(webkit_web_view_get_title(web_view));
	update_title (GTK_WINDOW (main_window));
}
static void
notify_load_status_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
	if (webkit_web_view_get_load_status (web_view) == WEBKIT_LOAD_COMMITTED)
	{
		notify_title_cb(web_view, pspec, data);
		default_uri_entry(); focus_uri_entry(); select_uri_entry();
	}
}
static void
notify_progress_cb (WebKitWebView* web_view, GParamSpec* pspec, gpointer data)
{
	load_progress = webkit_web_view_get_progress (web_view) * 100;
	update_title (GTK_WINDOW (main_window));
}
static void
destroy_cb (GtkWidget* widget, gpointer data)
{
	gtk_main_quit ();
}
static void
go_home_cb (GtkWidget* widget, gpointer data)
{
	webkit_web_view_load_uri (web_view, HOMEPAGE);
}
static void
go_back_cb (GtkWidget* widget, gpointer data)
{
	if (webkit_web_view_can_go_back(web_view))
		webkit_web_view_go_back_or_forward(web_view, -1);
	else exit(0);
}
static void
go_forward_cb (GtkWidget* widget, gpointer data)
{
	webkit_web_view_go_back_or_forward(web_view, 1);
}
static void
go_reload_cb (GtkWidget* widget, gpointer data)
{
	webkit_web_view_reload_bypass_cache(web_view);
}
gboolean
download_request_cb(WebKitWebView *view, WebKitDownload *o, gpointer v)
{
	char *buf = getcwd(NULL, 0);
	const char *uri = webkit_download_get_uri(o);
	spawn(DOWNLOAD(uri,
		webkit_download_get_suggested_filename(o),
		webkit_web_view_get_uri(web_view),
		buf));
	free(buf);
	return FALSE;
}
gboolean
mime_type_policy_decision_requested_cb(WebKitWebView* view, WebKitWebFrame* frame, WebKitNetworkRequest* request, const char* mime_type, WebKitWebPolicyDecision* decision, gpointer data)
{
	if (!webkit_web_view_can_show_mime_type(view, mime_type))
	{
		webkit_web_policy_decision_download(decision);
		return TRUE;
	}
	return FALSE;
}
gboolean
new_window_policy_decision_requested_cb(WebKitWebView *view, WebKitWebFrame *frame, WebKitNetworkRequest *req, WebKitWebNavigationAction *nav, WebKitWebPolicyDecision *decision, gpointer data)
{
	if (webkit_web_navigation_action_get_reason(nav) == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED)
	{
		webkit_web_policy_decision_ignore(decision);
		spawn(NEWWINDOW(webkit_network_request_get_uri(req)));
		return TRUE;
	}
	return FALSE;
}
void
open_new_window(const char *uri)
{
	spawn(NEWWINDOW(uri));
}
WebKitWebView*
create_web_view_cb(WebKitWebView *web_view, WebKitWebFrame *frame, gpointer user_data)
{
	open_new_window(gtk_entry_get_text(GTK_ENTRY(uri_entry)));
	return NULL;
}
void
key_action(const char *action)
{
	if (!strcmp(action, "go-home")) go_home_cb(NULL, NULL);
	else if (!strcmp(action, "go-back")) webkit_web_view_go_back_or_forward(web_view, -1);
	else if (!strcmp(action, "go-forward")) webkit_web_view_go_back_or_forward(web_view, 1);
	else if (!strcmp(action, "zoom-in")) webkit_web_view_zoom_in(web_view);
	else if (!strcmp(action, "zoom-out")) webkit_web_view_zoom_out(web_view);
	else if (!strcmp(action, "zoom-reset")) webkit_web_view_set_zoom_level(web_view, 1.0);
	else if (!strcmp(action, "toggle-source")) toggle_source_mode();
	else if (!strcmp(action, "reload-nocache")) webkit_web_view_reload_bypass_cache(web_view);
	else if (!strcmp(action, "run-scriptfile")) jsf(SCRIPTFILE);
	else if (!strcmp(action, "focus-navbar")) { focus_uri_entry(); select_uri_entry(); }
	else if (!strcmp(action, "new-window")) open_new_window(HOMEPAGE);
	else if (!strcmp(action, "print-page")) webkit_web_frame_print(webkit_web_view_get_main_frame(web_view));
	else if (!strcmp(action, "find-text")) focus_uri_entry_search();
	else if (!strcmp(action, "bookmark-page")) focus_uri_entry_bookmark();
	else fprintf(stderr, "unknown action: %s\n", action);
}
gboolean
keypress_cb(GtkWidget* widget, GdkEventKey *ev, gpointer data)
{
	guint m = (ev->state & ~(GDK_MOD2_MASK));
	guint k = gdk_keyval_to_lower(ev->keyval);
	int i = 0; while (keys[i].action)
	{
		if (keys[i].mod == m && keys[i].key == k)
		{
			key_action(keys[i].action);
			break;
		}
		i++;
	}
	return FALSE;
}
WebKitWebView*
inspector_create_cb (WebKitWebInspector* web_ins, WebKitWebView* page, gpointer data)
{
	GtkWidget *inspector_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(inspector_window), "Meme Inspector");
	gtk_window_set_default_size(GTK_WINDOW(inspector_window), WIDTH, HEIGHT);
	gtk_widget_show(inspector_window);

	GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(inspector_window), scrolled_window);
	gtk_widget_show(scrolled_window);

	GtkWidget* new_web_view = webkit_web_view_new();
	gtk_container_add(GTK_CONTAINER(scrolled_window), new_web_view);

	return WEBKIT_WEB_VIEW(new_web_view);
}
void add_cookie(SoupCookie *c)
{
	if (!COOKIEFILE) return;
	int lock = open(COOKIEFILE, 0);
	if (!flock(lock, LOCK_EX))
	{
		SoupDate *e;
		SoupCookieJar *j = soup_cookie_jar_text_new(COOKIEFILE, FALSE);
		c = soup_cookie_copy(c);
		if (c->expires == NULL && SESSIONTIME)
		{
			e = soup_date_new_from_time_t(time(NULL) + SESSIONTIME);
			soup_cookie_set_expires(c, e);
		}
		soup_cookie_jar_add_cookie(j, c);
		g_object_unref(j);
		flock(lock, LOCK_UN);
		close(lock);
	}
	else fprintf(stderr, "could not read: %s\n", COOKIEFILE);
}
void
got_headers_cb(SoupMessage *msg, gpointer v)
{
	GSList *l, *p;
	for(p = l = soup_cookies_from_response(msg); p; p = g_slist_next(p))
	{
		SoupCookie *c = (SoupCookie *)p->data;
		add_cookie(c);
	}
	soup_cookies_free(l);
}
void
request_start_cb(SoupSession *s, SoupMessage *msg, gpointer v)
{
	SoupMessageHeaders *h = msg->request_headers;
	soup_message_headers_remove(h, "Cookie");
	SoupURI *uri = soup_message_get_uri(msg);
	SoupCookieJar *j = soup_cookie_jar_text_new(COOKIEFILE, TRUE);
	const char *c = soup_cookie_jar_get_cookies(j, uri, TRUE);
	g_object_unref(j);
	if (c) soup_message_headers_append(h, "Cookie", c);
	g_signal_connect_after(G_OBJECT(msg), "got-headers", G_CALLBACK(got_headers_cb), NULL);
}
void
window_object_cleared_cb(WebKitWebView *web_view, WebKitWebFrame *frame, gpointer context, gpointer window_object, gpointer user_data)
{
	jsf_frame(ONLOADFILE, frame);
}
void
print_requested_cb(WebKitWebView *web_view, GtkMenu *menu, gpointer user_data)
{
	webkit_web_frame_print(webkit_web_view_get_main_frame(web_view));
}
static gboolean
match_selected_cb(GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, gpointer v)
{
	GValue value = {0, };
	gtk_tree_model_get_value(model, iter, 0, &value);
	gtk_entry_set_text (GTK_ENTRY (uri_entry), g_value_get_string(&value));
	g_value_unset(&value);
	activate_uri_entry_cb(uri_entry, NULL);
	return FALSE;
}
static gboolean
uri_entry_match_cb(GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	GValue value = {0, };
	gtk_tree_model_get_value(GTK_TREE_MODEL(uri_model), iter, 0, &value);
	gboolean flag = !strchr("!/", key[0]) && strstr(g_value_get_string(&value), key) ? TRUE : FALSE;
	g_value_unset(&value);
	return flag;
}
static GtkWidget*
create_browser ()
{
	GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

	g_signal_connect(web_view, "notify::title", G_CALLBACK (notify_title_cb), web_view);
	g_signal_connect(web_view, "notify::load-status", G_CALLBACK (notify_load_status_cb), web_view);
	g_signal_connect(web_view, "notify::progress", G_CALLBACK (notify_progress_cb), web_view);
	g_signal_connect(web_view, "download-requested", G_CALLBACK(download_request_cb), web_view);
	g_signal_connect(web_view, "hovering-over-link", G_CALLBACK (link_hover_cb), web_view);
	g_signal_connect(web_view, "create-web-view", G_CALLBACK(create_web_view_cb), web_view);
	g_signal_connect(web_view, "window-object-cleared", G_CALLBACK(window_object_cleared_cb), web_view);
	g_signal_connect(web_view, "print-requested", G_CALLBACK(print_requested_cb), web_view);
	g_signal_connect(web_view, "mime-type-policy-decision-requested", G_CALLBACK(mime_type_policy_decision_requested_cb), web_view);
	g_signal_connect(web_view, "new-window-policy-decision-requested", G_CALLBACK(new_window_policy_decision_requested_cb), web_view);

	web_settings = webkit_web_view_get_settings(web_view);
	g_object_set(G_OBJECT(web_settings), "user-agent", USERAGENT, NULL);
	g_object_set(G_OBJECT(web_settings), "enable-developer-extras", TRUE, NULL);
	g_object_set(G_OBJECT(web_settings), "enable-spell-checking", TRUE, NULL);
	g_object_set(G_OBJECT(web_settings), "enable-plugins", flag_plugins, NULL);
	g_object_set(G_OBJECT(web_settings), "javascript-can-open-windows-automatically", FALSE, NULL);
	g_object_set(G_OBJECT(web_settings), "enable-html5-local-storage", TRUE, NULL);
	g_object_set(G_OBJECT(web_settings), "html5-local-storage-database-path", MEMEDIR, NULL);

	web_inspector = webkit_web_view_get_inspector(web_view);
	g_signal_connect (G_OBJECT (web_inspector), "inspect-web-view", G_CALLBACK (inspector_create_cb), NULL);

	return scrolled_window;
}
static GtkWidget*
create_toolbar ()
{
	GtkWidget* toolbar = gtk_toolbar_new ();

#if GTK_CHECK_VERSION(2,15,0)
	gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
#else
	gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);
#endif
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);

	GtkToolItem* item;

	// the back button
	item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
	g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_back_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	// The forward button
	item = gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_forward_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	// The reload button
	item = gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH);
	g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_reload_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	// The home button
	item = gtk_tool_button_new_from_stock (GTK_STOCK_HOME);
	g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (go_home_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	// The URL entry
	item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (item, TRUE);
	uri_entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (item), uri_entry);
	g_signal_connect (G_OBJECT (uri_entry), "activate", G_CALLBACK (activate_uri_entry_cb), NULL);
	g_signal_connect (G_OBJECT (uri_entry), "focus-in-event", G_CALLBACK (focus_in_uri_entry_cb), NULL);

	// bookmark completion
	uri_completion = gtk_entry_completion_new();
	gtk_entry_completion_set_text_column(uri_completion, 0);
	gtk_entry_set_completion(GTK_ENTRY(uri_entry), uri_completion);
	gtk_entry_completion_set_match_func (uri_completion, uri_entry_match_cb, NULL, NULL);
	g_signal_connect(G_OBJECT (uri_completion), "match-selected", G_CALLBACK (match_selected_cb), NULL);

	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	return toolbar;
}
static GtkWidget*
create_window ()
{
	GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), WIDTH, HEIGHT);
	gtk_widget_set_name (window, "Meme");

	g_signal_connect(window, "destroy", G_CALLBACK (destroy_cb), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(keypress_cb), NULL);

	return window;
}
int
main (int argc, char* argv[])
{
	sigchld(0);
	gtk_init (&argc, &argv);
	if (!g_thread_supported ())
		g_thread_init (NULL);

	int i;
	for(i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0' && argv[i][2] == '\0'; i++)
	{
		switch(argv[i][1]) {
		case 'p':
			flag_plugins = TRUE;
			break;
		}
	}

	SoupSession *soup = webkit_get_default_session();
	soup_session_remove_feature_by_type(soup, soup_cookie_get_type());
	soup_session_remove_feature_by_type(soup, soup_cookie_jar_get_type());
	g_signal_connect_after(G_OBJECT(soup), "request-started", G_CALLBACK(request_start_cb), NULL);
	g_object_set(G_OBJECT(soup), SOUP_SESSION_MAX_CONNS, 100, NULL);
	g_object_set(G_OBJECT(soup), SOUP_SESSION_MAX_CONNS_PER_HOST, 8, NULL);

	GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), create_toolbar (), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);

	main_window = create_window ();
	gtk_container_add (GTK_CONTAINER (main_window), vbox);

	gtk_widget_grab_focus (GTK_WIDGET (web_view));
	gtk_widget_show_all (main_window);

	webkit_web_view_load_uri(web_view, i < argc ? argv[i]: HOMEPAGE);

	gtk_main ();

	return 0;
}
