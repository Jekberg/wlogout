#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <gtk/gtk.h>
#include "jsmn.h"
#include "config.h" /* Generated by meson */
#ifdef LAYERSHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#ifdef LAYERSHELL
static const int exclusive_level = -1;
#endif

#ifdef LAYERSHELL
static gboolean protocol = TRUE;
#else
static gboolean protocol = FALSE;
#endif

typedef struct
{
    char *label;
    char *action;
    char *text;
    float yalign;
    float xalign;
    guint bind;
    gboolean circular;
} button;

enum
{
    MARGIN_TOP,
    MARGIN_BOTTOM,
    MARGIN_LEFT,
    MARGIN_RIGHT,

    /*
     * The total number of margin values. This must be defined last in the
     * enum.
     */
    MARGIN_MAX
};

enum
{
    SPACING_ROW,
    SPACING_COLUMN,

    /*
     * The total number of spacing values. This mas be defined last in the
     * enum.
     */
    SPACING_MAX,
};

static const char *margin_names[MARGIN_MAX] = {[MARGIN_TOP] = "top margin",
                                               [MARGIN_BOTTOM] =
                                                   "bottom margin",
                                               [MARGIN_LEFT] = "left margin",
                                               [MARGIN_RIGHT] = "right margin"};

static const char *space_names[MARGIN_MAX] = {
    [SPACING_ROW] = "row spacing", [SPACING_COLUMN] = "column spacing"};

#define DEFAULT_SIZE 100

static char *command = NULL;
static char *layout_path = NULL;
static char *css_path = NULL;
static button buttons[DEFAULT_SIZE];
static GtkWidget *gtk_window = NULL;
static int num_buttons = 0;
static int draw = 0;
static int num_of_monitors = 0;
static GtkWindow **window = NULL;
static int buttons_per_row = 3;
static int primary_monitor = -1;
static int margin[MARGIN_MAX] = {230, 230, 230, 230};
static int space[SPACING_MAX] = {0, 0};
static gboolean show_bind = FALSE;
static gboolean no_span = FALSE;
static gboolean layershell = FALSE;

static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"layout", required_argument, NULL, 'l'},
    {"version", no_argument, NULL, 'v'},
    {"css", required_argument, NULL, 'C'},
    {"margin", required_argument, NULL, 'm'},
    {"margin-top", required_argument, NULL, 'T'},
    {"margin-bottom", required_argument, NULL, 'B'},
    {"margin-left", required_argument, NULL, 'L'},
    {"margin-right", required_argument, NULL, 'R'},
    {"buttons-per-row", required_argument, NULL, 'b'},
    {"column-spacing", required_argument, NULL, 'c'},
    {"row-spacing", required_argument, NULL, 'r'},
    {"protocol", required_argument, NULL, 'p'},
    {"show-binds", no_argument, NULL, 's'},
    {"no-span", no_argument, NULL, 'n'},
    {"primary-monitor", required_argument, NULL, 'P'},
    {0, 0, 0, 0}};

static const char *help =
    "Usage: wlogout [options...]\n"
    "\n"
    "   -h, --help                      Show help message and stop\n"
    "   -l, --layout </path/to/layout>  Specify a layout file\n"
    "   -v, --version                   Show version number and stop\n"
    "   -C, --css </path/to/css>        Specify a css file\n"
    "   -b, --buttons-per-row <1-x>     Set the number of buttons per row\n"
    "   -c  --column-spacing <0-x>      Set space between buttons columns\n"
    "   -r  --row-spacing <0-x>         Set space between buttons rows\n"
    "   -m, --margin <0-x>              Set margin around buttons\n"
    "   -L, --margin-left <0-x>         Set margin for left of buttons\n"
    "   -R, --margin-right <0-x>        Set margin for right of buttons\n"
    "   -T, --margin-top <0-x>          Set margin for top of buttons\n"
    "   -B, --margin-bottom <0-x>       Set margin for bottom of buttons\n"
    "   -p, --protocol <protocol>       Use layer-shell or xdg protocol\n"
    "   -s, --show-binds                Show the keybinds on their "
    "corresponding button\n"
    "   -n, --no-span                   Stops from spanning across "
    "multiple monitors\n"
    "   -P, --primary-monitor <0-x>     Set the primary monitor\n";

static gboolean parse_numarg(const char *str, int *nump)
{
    /*
     * sscanf() will successfully parse an integer from a string if there are
     * trailing non-nummeric characters. We can check that the entire string
     * looks like a number if there is no extra trailing characters not parsed
     * by "%d". So we expect sscanf to parse exactly 1 argument from the
     * format.
     */
    char garbage;
    return sscanf(str, "%d%c", nump, &garbage) == 1;
}

static gboolean process_args(int argc, char *argv[])
{

    while (TRUE)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hl:vc:m:b:T:R:L:B:r:c:p:C:sP:n",
                        long_options, &option_index);
        if (c == -1)
        {
            break;
        }
        switch (c)
        {
        case 'm':
            int m;
            if (!parse_numarg(optarg, &m))
            {
                g_print("margin %s is not a number\n", optarg);
                return TRUE;
            }

            for (int i = 0; i < MARGIN_MAX; i++)
                margin[i] = m;
            break;
        case 'L':
            if (!parse_numarg(optarg, &margin[MARGIN_LEFT]))
            {
                g_print("%s %s is not a number\n", margin_names[MARGIN_LEFT],
                        optarg);
                return TRUE;
            }
            break;
        case 'T':
            if (!parse_numarg(optarg, &margin[MARGIN_TOP]))
            {
                g_print("%s %s is not a number\n", margin_names[MARGIN_TOP],
                        optarg);
                return TRUE;
            }
            break;
        case 'B':
            if (!parse_numarg(optarg, &margin[MARGIN_BOTTOM]))
            {
                g_print("%s %s is not a number\n", margin_names[MARGIN_BOTTOM],
                        optarg);
                return TRUE;
            }
            break;
        case 'R':
            if (!parse_numarg(optarg, &margin[MARGIN_RIGHT]))
            {
                g_print("%s %s is not a number\n", margin_names[MARGIN_RIGHT],
                        optarg);
                return TRUE;
            }
            break;
        case 'c':
            if (!parse_numarg(optarg, &space[SPACING_COLUMN]))
            {
                g_print("%s %s is not a number\n", space_names[SPACING_COLUMN],
                        optarg);
                return TRUE;
            }
            break;
        case 'r':
            if (!parse_numarg(optarg, &space[SPACING_ROW]))
            {
                g_print("%s %s is not a number\n", space_names[SPACING_ROW],
                        optarg);
                return TRUE;
            }
            break;
        case 'l':
            layout_path = g_strdup(optarg);
            break;
        case 'v':
            g_print("wlogout %s\n", PROJECT_VERSION);
            return TRUE;
        case 'C':
            css_path = g_strdup(optarg);
            break;
        case 'b':
            if (!parse_numarg(optarg, &buttons_per_row))
            {
                g_print("buttons per row %s is not a number\n", optarg);
                return TRUE;
            }
            break;
        case 'p':
            if (strcmp("layer-shell", optarg) == 0)
            {
                protocol = TRUE;
            }
            else if (strcmp("xdg", optarg) == 0)
            {
                protocol = FALSE;
            }
            else
            {
                g_print("%s is an invalid protocol\n", optarg);
                return TRUE;
            }
            break;
        case 's':
            show_bind = TRUE;
            break;
        case 'P':
            if (!parse_numarg(optarg, &primary_monitor))
            {
                g_print("pparimary monitor %s is not a number\n", optarg);
                return TRUE;
            }
            break;
        case 'n':
            no_span = TRUE;
            break;
        case '?':
        case 'h':
        default:
            g_print("%s\n", help);
            return TRUE;
        }
    }

    /*
     * Before resuming, validate that the arguments make sense. Otherwise we
     * will encounter failures during the run.
     */

    if (buttons_per_row < 1)
    {
        g_print("The number of buttons per row cannot be less than 1\n");
        return TRUE;
    }

    for (int i = 0; i < MARGIN_MAX; i++)
    {
        if (margin[i] < 0)
        {
            g_print("%s cannot be a negative number\n", margin_names[i]);
            return TRUE;
        }
    }

    for (int i = 0; i < SPACING_MAX; i++)
    {
        if (space[i] < 0)
        {
            g_print("%s cannot be a negative number\n", space_names[i]);
            return TRUE;
        }
    }

    return FALSE;
}

static void abort_if(gboolean condition, const char *message)
{
    if (!condition)
        return;
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static gboolean get_layout_path()
{
    char *buf = malloc(DEFAULT_SIZE * sizeof(char));
    abort_if(!buf, error_nomem);

    char *tmp = getenv("XDG_CONFIG_HOME");
    char *config_path = g_strdup(tmp);
    if (!config_path)
    {
        config_path = getenv("HOME");
        int n = snprintf(buf, DEFAULT_SIZE, "%s/.config", config_path);
        if (n != 0)
        {
            free(buf);
            buf = malloc((DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n));
            abort_if(!buf, error_nomem);
            snprintf(buf, (DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n),
                     "%s/.config", config_path);
        }
        config_path = g_strdup(buf);
    }

    int n = snprintf(buf, DEFAULT_SIZE, "%s/wlogout/layout", config_path);
    if (n != 0)
    {
        free(buf);
        buf = malloc((DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n));
        abort_if(!buf, error_nomem);
        snprintf(buf, (DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n),
                 "%s/wlogout/layout", config_path);
    }
    free(config_path);

    if (layout_path)
    {
        free(buf);
        return FALSE;
    }
    else if (access(buf, F_OK) != -1)
    {
        layout_path = g_strdup(buf);
        free(buf);
        return FALSE;
    }
    else if (access("/etc/wlogout/layout", F_OK) != -1)
    {
        layout_path = "/etc/wlogout/layout";
        free(buf);
        return FALSE;
    }
    else if (access("/usr/local/etc/wlogout/layout", F_OK) != -1)
    {
        layout_path = "/usr/local/etc/wlogout/layout";
        free(buf);
        return FALSE;
    }
    else
    {
        free(buf);
        return TRUE;
    }
}

static gboolean get_css_path()
{
    char *buf = malloc(DEFAULT_SIZE * sizeof(char));
    abort_if(!buf, error_nomem);

    char *tmp = getenv("XDG_CONFIG_HOME");
    char *config_path = g_strdup(tmp);
    if (!config_path)
    {
        config_path = getenv("HOME");
        int n = snprintf(buf, DEFAULT_SIZE, "%s/.config", config_path);
        if (n != 0)
        {
            free(buf);
            buf = malloc((DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n));
            abort_if(!buf, error_nomem);
            snprintf(buf, (DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n),
                     "%s/.config", config_path);
        }
        config_path = g_strdup(buf);
    }

    int n = snprintf(buf, DEFAULT_SIZE, "%s/wlogout/style.css", config_path);
    if (n != 0)
    {
        free(buf);
        buf = malloc((DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n));
        abort_if(!buf, error_nomem);
        snprintf(buf, (DEFAULT_SIZE * sizeof(char)) + (sizeof(char) * n),
                 "%s/wlogout/style.css", config_path);
    }
    free(config_path);

    if (css_path)
    {
        free(buf);
        return FALSE;
    }
    else if (access(buf, F_OK) != -1)
    {
        css_path = g_strdup(buf);
        free(buf);
        return FALSE;
    }
    else if (access("/etc/wlogout/style.css", F_OK) != -1)
    {
        css_path = "/etc/wlogout/style.css";
        free(buf);
        return FALSE;
    }
    else if (access("/usr/local/etc/wlogout/style.css", F_OK) != -1)
    {
        css_path = "/usr/local/etc/wlogout/style.css";
        free(buf);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

static gboolean background_clicked(GtkWidget *widget, GdkEventButton event,
                                   gpointer user_data)
{
    for (int i = 0; i < num_of_monitors; i++)
    {
        if (i != primary_monitor)
        {
            gtk_widget_destroy(GTK_WIDGET(window[i]));
        }
    }
    gtk_main_quit();
    return TRUE;
}

static char *get_substring(char *s, int start, int end, char *buf)
{
    memcpy(s, &buf[start], (end - start));
    s[end - start] = '\0';
    return s;
}

static gboolean get_buttons(FILE *json)
{
    fseek(json, 0L, SEEK_END);
    int length = ftell(json);
    rewind(json);

    char *buffer = malloc(length);
    abort_if(!buffer, error_nomem);
    if (!buffer)
    {
        g_warning("Failed to allocate memory\n");
        return TRUE;
    }
    fread(buffer, 1, length, json);

    jsmn_parser p;
    jsmntok_t *tok = malloc(DEFAULT_SIZE * sizeof(jsmntok_t));
    abort_if(!tok, error_nomem);

    jsmn_init(&p);
    int numtok, i = 1;
    do
    {
        numtok = jsmn_parse(&p, buffer, length, tok, DEFAULT_SIZE * i);
        if (numtok == JSMN_ERROR_NOMEM)
        {
            i++;
            jsmntok_t *tmp =
                realloc(tok, ((DEFAULT_SIZE * i) * sizeof(jsmntok_t)));
            if (!tmp)
            {
                free(tok);
                return FALSE;
            }
            else if (tmp != tok)
            {
                tok = tmp;
            }
        }
        else
{
            break;
        }
    } while (TRUE);

    if (numtok < 0)
    {
        free(tok);
        g_warning("Failed to parse JSON data\n");
        return TRUE;
    }

    for (int i = 0; i < numtok; i++)
    {
        if (tok[i].type == JSMN_OBJECT)
        {
            num_buttons++;
            buttons[num_buttons - 1].yalign = 0.9;
            buttons[num_buttons - 1].xalign = 0.5;
            buttons[num_buttons - 1].circular = FALSE;
        }
        else if (tok[i].type == JSMN_STRING)
        {
            int length = tok[i].end - tok[i].start;
            char tmp[length + 1];
            get_substring(tmp, tok[i].start, tok[i].end, buffer);
            i++;
            length = tok[i].end - tok[i].start;

            if (strcmp(tmp, "label") == 0)
            {
                char buf[length + 1];
                get_substring(buf, tok[i].start, tok[i].end, buffer);
                buttons[num_buttons - 1].label =
                    malloc(sizeof(char) * (length + 1));
                abort_if(!buttons[num_buttons - 1].label, error_nomem);
                strcpy(buttons[num_buttons - 1].label, buf);
            }
            else if (strcmp(tmp, "action") == 0)
            {
                char buf[length + 1];
                get_substring(buf, tok[i].start, tok[i].end, buffer);
                buttons[num_buttons - 1].action =
                    malloc(sizeof(char) * length + 1);
                abort_if(!buttons[num_buttons - 1].action, error_nomem);
                strcpy(buttons[num_buttons - 1].action, buf);
            }
            else if (strcmp(tmp, "text") == 0)
            {
                char buf[length + 1];
                get_substring(buf, tok[i].start, tok[i].end, buffer);
                /* Add a small buffer to allocated memory so the keybind
                 * can easily be concatenated later if needed */
                int keybind_buffer = sizeof(guint) + (sizeof(char) * 2);
                buttons[num_buttons - 1].text =
                    malloc((sizeof(char) * (length + 1)) + keybind_buffer);
                abort_if(!buttons[num_buttons - 1].text, error_nomem);
                strcpy(buttons[num_buttons - 1].text, buf);
            }
            else if (strcmp(tmp, "keybind") == 0)
            {
                if (length != 1)
                {
                    fprintf(stderr, "Invalid keybind\n");
                }
                else
                {
                    buttons[num_buttons - 1].bind = buffer[tok[i].start];
                }
            }
            else if (strcmp(tmp, "height") == 0)
            {
                if (tok[i].type != JSMN_PRIMITIVE ||
                    !isdigit(buffer[tok[i].start]))
                {
                    fprintf(stderr, "Invalid height\n");
                }
                else
                {
                    buttons[num_buttons - 1].yalign = buffer[tok[i].start];
                }
            }
            else if (strcmp(tmp, "width") == 0)
            {
                if (tok[i].type != JSMN_PRIMITIVE ||
                    !isdigit(buffer[tok[i].start]))
                {
                    fprintf(stderr, "Invalid width\n");
                }
                else
                {
                    buttons[num_buttons - 1].xalign = buffer[tok[i].start];
                }
            }
            else if (strcmp(tmp, "circular") == 0)
            {
                if (tok[i].type != JSMN_PRIMITIVE)
                {
                    fprintf(stderr, "Invalid boolean\n");
                }
                else
                {
                    if (buffer[tok[i].start] == 't')
                    {
                        buttons[num_buttons - 1].circular = TRUE;
                    }
                    else
                    {
                        buttons[num_buttons - 1].circular = FALSE;
                    }
                }
            }
            else
            {
                g_warning("Invalid key %s\n", tmp);
                return TRUE;
            }
        }
        else
        {
            g_warning("Invalid JSON Data\n");
            return TRUE;
        }
    }

    free(tok);
    free(buffer);
    fclose(json);

    return FALSE;
}

static void execute(GtkWidget *widget, char *action)
{
    command = malloc(strlen(action) * sizeof(char) + 1);
    abort_if(!command, error_nomem);
    strcpy(command, action);
    gtk_widget_destroy(gtk_window);
    for (int i = 0; i < num_of_monitors; i++)
    {
        if (i != primary_monitor)
        {
            gtk_widget_destroy(GTK_WIDGET(window[i]));
        }
    }
    gtk_main_quit();
}

static gboolean check_key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        gtk_main_quit();
        return TRUE;
    }
    for (int i = 0; i < num_buttons; i++)
    {
        if (buttons[i].bind == event->keyval)
        {
            execute(NULL, buttons[i].action);
            return TRUE;
        }
    }
    return FALSE;
}

static void set_fullscreen(GtkWindow *win, int monitor, gboolean keyboard)
{
    if (!layershell && protocol)
    {
#ifdef LAYERSHELL
        g_warning("Falling back to xdg protocol");
#else
        g_warning("wlogout was compiled without layer-shell support\n"
                  "Falling back to xdg protocol");
#endif
    }

    if (protocol && layershell)
    {
#ifdef LAYERSHELL
        GdkMonitor *mon =
            gdk_display_get_monitor(gdk_display_get_default(), monitor);
        gtk_layer_init_for_window(win);
        gtk_layer_set_layer(win, GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_namespace(win, "logout_dialog");
        gtk_layer_set_exclusive_zone(win, exclusive_level);

        for (int j = 0; j < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; j++)
        {
            gtk_layer_set_anchor(win, j, TRUE);
        }
        gtk_layer_set_monitor(win, mon);
        gtk_layer_set_keyboard_interactivity(win, keyboard);
#endif
    }
    else
    {
        if (monitor < 0)
        {
            gtk_window_fullscreen(win);
        }
        else
        {
            gtk_window_fullscreen_on_monitor(win, gdk_screen_get_default(),
                                             monitor);
        }
    }
}

static void get_monitor(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    /* For some reason gtk only returns the correct monitor after the window
     * has been drawn twice */
    if (draw != 2)
    {
        draw++;
        return;
    }
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *active_monitor = gdk_display_get_monitor_at_window(
        display, gtk_widget_get_window(gtk_window));
    num_of_monitors = gdk_display_get_n_monitors(display);
    window = malloc(num_of_monitors * sizeof(GtkWindow *));
    abort_if(!window, error_nomem);
    GtkWidget **box = malloc(num_of_monitors * sizeof(GtkWidget *));
    abort_if(!box, error_nomem);
    GdkMonitor **monitors = malloc(num_of_monitors * sizeof(GdkMonitor *));
    abort_if(!monitors, error_nomem);

    for (int i = 0; i < num_of_monitors; i++)
    {
        window[i] = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
        box[i] = gtk_event_box_new();
        monitors[i] = gdk_display_get_monitor(display, i);
        if (monitors[i] == active_monitor)
        {
            primary_monitor = i;
        }
    }

    for (int i = 0; i < num_of_monitors; i++)
    {
        if (i != primary_monitor)
        {
            set_fullscreen(window[i], i, FALSE);
        }
    }

    for (int i = 0; i < num_of_monitors; i++)
    {
        if (i != primary_monitor)
        {
            // add event box to exit when clicking the background
            gtk_container_add(GTK_CONTAINER(window[i]), box[i]);
            g_signal_connect(box[i], "button-press-event",
                             G_CALLBACK(background_clicked), NULL);
            gtk_widget_show_all(GTK_WIDGET(window[i]));
        }
    }
    g_signal_handlers_disconnect_by_func(gtk_window, G_CALLBACK(get_monitor),
                                         NULL);
}

static void load_buttons(GtkContainer *container)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(container, grid);

    gtk_grid_set_row_spacing(GTK_GRID(grid), space[SPACING_ROW]);
    gtk_grid_set_column_spacing(GTK_GRID(grid), space[SPACING_COLUMN]);

    gtk_widget_set_margin_top(grid, margin[MARGIN_TOP]);
    gtk_widget_set_margin_bottom(grid, margin[MARGIN_BOTTOM]);
    gtk_widget_set_margin_start(grid, margin[MARGIN_LEFT]);
    gtk_widget_set_margin_end(grid, margin[MARGIN_RIGHT]);

    int num_col = 0;
    if ((num_buttons % buttons_per_row) == 0)
    {
        num_col = (num_buttons / buttons_per_row);
    }
    else
    {
        num_col = (num_buttons / buttons_per_row) + 1;
    }

    GtkWidget *but[buttons_per_row][num_col];

    int count = 0;
    for (int i = 0; i < buttons_per_row; i++)
    {
        for (int j = 0; j < num_col; j++)
        {
            if (buttons[count].text && show_bind)
            {
                strcat(buttons[count].text, "[");
                strcat(buttons[count].text, (char *)&buttons[count].bind);
                strcat(buttons[count].text, "]");
            }
            but[i][j] = gtk_button_new_with_label(buttons[count].text);
            gtk_widget_set_name(but[i][j], buttons[count].label);
            gtk_label_set_yalign(
                GTK_LABEL(gtk_bin_get_child(GTK_BIN(but[i][j]))),
                buttons[count].yalign);
            gtk_label_set_xalign(
                GTK_LABEL(gtk_bin_get_child(GTK_BIN(but[i][j]))),
                buttons[count].xalign);
            if (buttons[count].circular)
            {
                gtk_style_context_add_class(
                    gtk_widget_get_style_context(but[i][j]), "circular");
            }
            g_signal_connect(but[i][j], "clicked", G_CALLBACK(execute),
                             buttons[count].action);
            gtk_widget_set_hexpand(but[i][j], TRUE);
            gtk_widget_set_vexpand(but[i][j], TRUE);
            gtk_grid_attach(GTK_GRID(grid), but[i][j], i, j, 1, 1);
            count++;
        }
    }
}

static void load_css()
{
    GtkCssProvider *css = gtk_css_provider_new();
    GError *error = NULL;
    gtk_css_provider_load_from_path(css, css_path, &error);
    if (error)
    {
        g_warning("%s", error->message);
        g_clear_error(&error);
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

int main(int argc, char *argv[])
{
    g_set_prgname("wlogout");
    gtk_init(&argc, &argv);
    if (process_args(argc, argv))
    {
        return 0;
    }

    if (get_layout_path())
    {
        g_warning("Failed to find a layout\n");
        return 1;
    }

    if (get_css_path())
    {
        g_warning("Failed to find css file\n");
    }

    FILE *inptr = fopen(layout_path, "r");
    if (!inptr)
    {
        g_warning("Failed to open %s\n", layout_path);
        return 2;
    }
    if (get_buttons(inptr))
    {
        fclose(inptr);
        return 3;
    }

#ifdef LAYERSHELL
    layershell = gtk_layer_is_supported();
#endif

    GtkWindow *active_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    set_fullscreen(active_window, primary_monitor, TRUE);

    gtk_window = GTK_WIDGET(active_window);
    g_signal_connect(gtk_window, "key_press_event", G_CALLBACK(check_key),
                     NULL);
    if (!no_span)
    {
        /* The compositor will only tell us what monitor wlogouts on after
         * after gtk_main() is called and its been drawn */
        g_signal_connect_after(gtk_window, "draw", G_CALLBACK(get_monitor),
                               NULL);
    }

    GtkWidget *active_box = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(gtk_window), active_box);
    g_signal_connect(active_box, "button-press-event",
                     G_CALLBACK(background_clicked), NULL);

    load_buttons(GTK_CONTAINER(active_box));
    load_css();
    gtk_widget_show_all(gtk_window);

    gtk_main();

    system(command);

    for (int i = 0; i < num_buttons; i++)
    {
        free(buttons[i].label);
        free(buttons[i].action);
        free(buttons[i].text);
    }
    free(command);
}
