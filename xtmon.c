#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>


#define MAX_TITLE_LENGTH 256
#define MAX_NUM_WINDOWS 256

#define OUTPUT_FORMAT L"%s 0x%08x %ls\n"


xcb_connection_t *CONN;
xcb_window_t ROOT;
xcb_window_t XTMON;

xcb_atom_t _NET_CLIENT_LIST;
xcb_atom_t _NET_ACTIVE_WINDOW;
xcb_atom_t WM_NAME;
xcb_atom_t _NET_WM_NAME;

xcb_atom_t UTF8_STRING;
xcb_atom_t COMPOUND_TEXT;
xcb_atom_t STRING;


/**
 * When we're asked to terminate, send an X event that'll get picked up by the
 * event loop.
 */
void signal_handler(const int sig_num) {
	if (sig_num == SIGINT || sig_num == SIGHUP || sig_num == SIGTERM) {
		xcb_client_message_event_t event;
		memset(&event, 0, sizeof(xcb_client_message_event_t));

		event.response_type = XCB_CLIENT_MESSAGE;
		event.format = 32;
		event.window = XTMON;

		xcb_send_event(
			CONN, false, XTMON, XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
		xcb_flush(CONN);
	}
}

/**
 * Subscribe to a window's property change events.
 *
 * @param[in] window The window to subscribe to.
 */
void subscribe(const xcb_window_t window) {
	xcb_change_window_attributes(
		CONN, window, XCB_CW_EVENT_MASK,
		&(xcb_event_mask_t){XCB_EVENT_MASK_PROPERTY_CHANGE});
}

/**
 * Get the atom by name.
 *
 * @param[in] atom_name The string name of the atom.
 *
 * @return The xcb_atom_t.
 */
xcb_atom_t get_atom(const char atom_name[]) {
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
		CONN, 1, strlen(atom_name), atom_name);

	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
		CONN, cookie, NULL);

	xcb_atom_t atom = reply->atom;

	free(reply);

	return atom;
}

/**
 * Get the title of a window.
 *
 * @param[in] window The window to get the title for.
 * @param[out] title Will be populated with the window title.
 *
 * @return Whether the window title was successfully retrieved.
 */
bool get_window_title(const xcb_window_t window, wchar_t title[]) {
	xcb_get_property_cookie_t cookie;
	xcb_get_property_reply_t *reply;

	char *new_title = NULL;
	size_t title_length;

	xcb_atom_t atoms[] = {_NET_WM_NAME, WM_NAME};
	for (size_t i = 0; i < sizeof(atoms) / sizeof(xcb_atom_t); i++) {
		cookie = xcb_get_property(
			CONN, 0, window, atoms[i], XCB_ATOM_ANY, 0,
			(sizeof(wchar_t) * MAX_TITLE_LENGTH) / 4);

		reply = xcb_get_property_reply(CONN, cookie, NULL);

		if (!reply) {
			return false;
		}

		new_title = xcb_get_property_value(reply);
		title_length = xcb_get_property_value_length(reply);

		if (title_length > 0 && new_title != NULL)
			break;
	}

	if (reply->type == STRING || reply->type == UTF8_STRING) {
		// We don't need to do anything special here
	} else if (reply->type == COMPOUND_TEXT) {
		// TODO: Extract title from compound text
		// https://www.x.org/releases/X11R7.6/doc/xorg-docs/specs/CTEXT/ctext.html

		// See relevant bug report:
		// https://github.com/baskerville/xtitle/issues/21

		new_title = "Error: COMPOUND TEXT Encoded Title";
		title_length = strlen(new_title);
	} else {
		new_title = "Error: Unknown Title Encoding";
		title_length = strlen(new_title);
	}

	title_length = mbsnrtowcs(
		title, (const char **)&new_title, title_length, MAX_TITLE_LENGTH, NULL);

	// It appears that sometimes the title returned is not null terminated, so
	// we manually terminate it here.
	title[title_length] = '\0';

	free(reply);

	return true;
}

/**
 * Get an array of windows managed by the window manager.
 *
 * @param[out] windows The array to populate with windows.
 *
 * @return The number of windows managed.
 */
size_t get_managed_windows(xcb_window_t windows[]) {
	xcb_get_property_cookie_t cookie = xcb_get_property(
		CONN, 0, ROOT,
		_NET_CLIENT_LIST, XCB_ATOM_WINDOW, 0,
		(sizeof(xcb_window_t) * MAX_NUM_WINDOWS) / 4);

	xcb_get_property_reply_t *reply = xcb_get_property_reply(
		CONN, cookie, NULL);

	size_t num_windows =
		xcb_get_property_value_length(reply) / sizeof(xcb_window_t);

	memcpy(
		windows, xcb_get_property_value(reply),
		sizeof(xcb_window_t) * num_windows);

	free(reply);

	return num_windows;
}

/**
 * Get the currently focused window.
 *
 * @return The currently focused window.
 */
xcb_window_t get_focused_window() {
	xcb_get_property_cookie_t cookie = xcb_get_property(
		CONN, 0, ROOT, _NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 0, 1);

	xcb_get_property_reply_t *reply = xcb_get_property_reply(
		CONN, cookie, NULL);

	xcb_window_t focused_window;
	memcpy(&focused_window, xcb_get_property_value(reply), sizeof(xcb_window_t));

	free(reply);

	return focused_window;
}

/**
 * Check if a given window is in an array.
 *
 * @param[in] window The window to check for.
 * @param[in] windows The array to look in for the window.
 * @param[in] num_windows The number of windows in the array
 *
 * @return A boolean value indicating whether the value is within the array.
 */
bool window_in_array(
	const xcb_window_t window, const xcb_window_t windows[],
	const size_t num_windows
) {
	for (size_t i = 0; i < num_windows; i++) {
		if (window == windows[i])
			return true;
	}

	return false;
}

/**
 * Update the list of currently managed windows.
 *
 * @param windows[in,out] The array of windows to update.
 * @param num_windows[in,out] The number of windows in the array.
 * @param changed_window[out] The window that was added or removed.
 */
int8_t update_managed_windows(
	xcb_window_t windows[], size_t *num_windows,
	xcb_window_t *changed_window
) {
	xcb_window_t new_windows[MAX_NUM_WINDOWS];
	size_t num_new_windows = get_managed_windows(new_windows);

	// Check for new windows
	for (size_t i = 0; i < num_new_windows; i++) {
		if (!window_in_array(new_windows[i], windows, *num_windows)) {
			// Save the added window id
			memcpy(changed_window, &new_windows[i], sizeof(xcb_window_t));

			// Add the new window to the array
			windows[*num_windows] = new_windows[i];

			// Register the new window with the event system
			subscribe(new_windows[i]);

			*num_windows += 1;
			return 1;
		}
	}

	// Check for removed windows
	for (size_t i = 0; i < *num_windows; i++) {
		if (!window_in_array(windows[i], new_windows, num_new_windows)) {
			// Save the removed window id
			memcpy(changed_window, &windows[i], sizeof(xcb_window_t));

			// Remove the window from the array
			for (size_t j = i + 1; j < *num_windows; j++) {
				windows[j - 1] = windows[j];
			}

			*num_windows += -1;
			return -1;
		}
	}

	memset(changed_window, 0x00, sizeof(xcb_window_t));
	return 0;
}

/**
 * Set up all the required XCB globals.
 */
bool setup() {
	// Connect to the X server
	CONN = xcb_connect(NULL, NULL);

	if (xcb_connection_has_error(CONN) > 0) {
		wprintf(L"error could not connect to X server");
		return false;
	}

	// Setup commonly used atoms
	_NET_CLIENT_LIST = get_atom("_NET_CLIENT_LIST");
	_NET_ACTIVE_WINDOW = get_atom("_NET_ACTIVE_WINDOW");
	WM_NAME = get_atom("WM_NAME");
	_NET_WM_NAME = get_atom("_NET_WM_NAME");

	UTF8_STRING = get_atom("UTF8_STRING");
	COMPOUND_TEXT = get_atom("COMPOUND_TEXT");
	STRING = get_atom("STRING");

	// Get the root window
	const xcb_setup_t *setup = xcb_get_setup(CONN);

	xcb_screen_iterator_t screens = xcb_setup_roots_iterator(setup);
	ROOT = screens.data->root;

	// Create our window (for event handling)
	XTMON = xcb_generate_id(CONN);
	xcb_create_window(
		CONN, XCB_COPY_FROM_PARENT, XTMON, ROOT, 0, 0, 100, 100, 0,
		XCB_WINDOW_CLASS_COPY_FROM_PARENT, screens.data->root_visual, 0, NULL);

	return true;
}

/**
 * Set up signal handling.
 */
void init_signal_handling() {
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));

	action.sa_handler = signal_handler;

	sigaddset(&action.sa_mask, SIGINT);
	sigaddset(&action.sa_mask, SIGHUP);
	sigaddset(&action.sa_mask, SIGTERM);

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
}


int main(const int argc, const char *argv[]) {
	setlocale(LC_ALL, "");
	init_signal_handling();

	if (!setup()) {
		// TODO: Write error message to stderr
		exit(1);
	}

	// Subscribe to events on the root window (_NET_CLIENT_LIST)
	subscribe(ROOT);

	xcb_window_t windows[MAX_NUM_WINDOWS];
	wchar_t title[MAX_TITLE_LENGTH + 1];
	xcb_window_t focused_window = get_focused_window();


	size_t num_windows = get_managed_windows(windows);

	for (size_t i = 0; i < num_windows; i++) {
		// Subscribe to property change events
		subscribe(windows[i]);

		if (get_window_title(windows[i], title)) {
			wprintf(OUTPUT_FORMAT, "initial_title", windows[i], title);
		}

		// Output the initially focused window
		if (windows[i] == focused_window) {
			wprintf(OUTPUT_FORMAT, "initial_focus", windows[i], title);
		}

		fflush(stdout);
	}


	int8_t delta = 0;
	xcb_window_t changed_window;

	xcb_generic_event_t *event;
	xcb_event_mask_t response_type;

	while (event = xcb_wait_for_event(CONN)) {
		response_type = event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK;

		if (response_type == XCB_PROPERTY_NOTIFY) {
			xcb_property_notify_event_t *property_event =
				(xcb_property_notify_event_t *)event;

			if (
				property_event->atom == _NET_WM_NAME
				&& get_window_title(property_event->window, title)
			) {
				wprintf(OUTPUT_FORMAT,
					"title_changed", property_event->window, title);
			} else if (
				property_event->window == ROOT
				&& property_event->atom == _NET_CLIENT_LIST
			) {
				delta = update_managed_windows(
					windows, &num_windows, &changed_window);

				if (delta == 1 && get_window_title(changed_window, title)) {
					wprintf(OUTPUT_FORMAT, "new_window", changed_window, title);
				} else if (delta == -1) {
					wprintf(OUTPUT_FORMAT, "removed_window", changed_window, L"");
				}

				if (num_windows >= MAX_NUM_WINDOWS) {
					wprintf(
						L"warning: at the window limit, things might be wonky "
						L"from here on out\n");
				}
			} else if (
				property_event->window == ROOT
				&& property_event->atom == _NET_ACTIVE_WINDOW
			) {
				focused_window = get_focused_window();

				if (
					focused_window > 0
					&& get_window_title(focused_window, title)
				) {
					
					wprintf(OUTPUT_FORMAT, "focus_changed", focused_window, title);
				} else {
					wprintf(OUTPUT_FORMAT, "focus_changed", 0, L"");
				}
			}

			fflush(stdout);
			free(event);
		} else if (response_type == XCB_CLIENT_MESSAGE) {
			xcb_client_message_event_t *client_event =
				(xcb_client_message_event_t *)event;

			if (client_event->window == XTMON) {
				free(event);
				break;
			}
		}
	}

	xcb_disconnect(CONN);
}
