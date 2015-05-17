#include "editor.h"

/* Configuration */
char *colors[] = { "Black", "White", "Red", "Cyan", "Purple", "Green", "Blue", "Yellow",
                   "Orange", "Brown", "Pink", "Dark Grey", "Mid Grey", "Light Green", "Light Blue", "Light Grey" };
                          
#define CFG_USERIF_BACKGROUND 0x01
#define CFG_USERIF_BORDER     0x02
#define CFG_USERIF_FOREGROUND 0x03
#define CFG_USERIF_SELECTED   0x04
#define CFG_USERIF_WORDWRAP   0x05

struct t_cfg_definition user_if_config[] = {
    { CFG_USERIF_BACKGROUND, CFG_TYPE_ENUM,   "Background color",     "%s", colors,  0, 15, 0 },
    { CFG_USERIF_BORDER,     CFG_TYPE_ENUM,   "Border color",         "%s", colors,  0, 15, 0 },
    { CFG_USERIF_FOREGROUND, CFG_TYPE_ENUM,   "Foreground color",     "%s", colors,  0, 15, 15 },
    { CFG_USERIF_SELECTED,   CFG_TYPE_ENUM,   "Selected Item color",  "%s", colors,  0, 15, 1 },
//    { CFG_USERIF_WORDWRAP,   CFG_TYPE_ENUM,   "Wordwrap text viewer", "%s", en_dis,  0,  1, 1 },
    { CFG_TYPE_END,           CFG_TYPE_END,    "", "", NULL, 0, 0, 0 }         
};

const char *c_button_names[NUM_BUTTONS] = { " Ok ", " Yes ", " No ", " All ", " Cancel " };
const char c_button_keys[NUM_BUTTONS] = { 'o', 'y', 'n', 'c', 'a' };
const int c_button_widths[NUM_BUTTONS] = { 4, 5, 4, 5, 8 };

/* COMPILER BUG: static data members are not declared, and do not appear in the linker */

void poll_user_interface(Event &e)
{
	user_interface->handle_event(e);
}

UserInterface :: UserInterface()
{
    initialized = false;
    poll_list.append(&poll_user_interface);
    focus = -1;
    state = ui_idle;
    current_path = NULL;
    host = NULL;
    keyboard = NULL;
    screen = NULL;

    register_store(0x47454E2E, "User Interface Settings", user_if_config);
    effectuate_settings();
}

UserInterface :: ~UserInterface()
{
//	if(host->has_stopped())
//		printf("WARNING: Host is still frozen!!\n");

	printf("Destructing user interface..\n");
	poll_list.remove(&poll_user_interface);
    do {
    	ui_objects[focus]->deinit();
    	delete ui_objects[focus--];
    } while(focus>=0);

//    delete browser_path;
    printf(" bye UI!\n");
}

void UserInterface :: effectuate_settings(void)
{
    color_border = cfg->get_value(CFG_USERIF_BORDER);
    color_fg     = cfg->get_value(CFG_USERIF_FOREGROUND);
    color_bg     = cfg->get_value(CFG_USERIF_BACKGROUND);
    color_sel    = cfg->get_value(CFG_USERIF_SELECTED);

    if(host && host->is_accessible())
        host->set_colors(color_bg, color_border);

    push_event(e_refresh_browser);
}
    
void UserInterface :: init(GenericHost *h, Keyboard *k)
{
    host = h;
    keyboard = k;
    screen = NULL;
    initialized = true;
}

void UserInterface :: set_screen(Screen *s)
{
    screen = s;
}

void UserInterface :: handle_event(Event &e)
{
/*
    if(!initialized)
        return;
*/
    static BYTE button_prev;
    int ret, i;

    switch(state) {
        case ui_idle:
        	if ((e.type == e_button_press)||(e.type == e_freeze)) {
                host->freeze();
                host->set_colors(color_bg, color_border);
                screen = new Screen(host->get_screen(), host->get_color_map(), 40, 25);
                set_screen_title();
                for(i=0;i<=focus;i++) {  // build up
                    //printf("Going to (re)init objects %d.\n", i);
                    ui_objects[i]->init(screen, keyboard);
                }
                if(e.param)
                    state = ui_host_permanent;
                else
                    state = ui_host_owned;

                push_event(e_enter_menu, 0);
            }
            else if((e.type == e_invalidate) && (focus >= 0)) { // even if we are inactive, the tree browser should execute this!!
            	ui_objects[0]->poll(0, e);
            }
            break;

        case ui_host_owned:
        	if ((e.type == e_button_press)||(e.type == e_unfreeze)) {
                for(i=focus;i>=0;i--) {  // tear down
                    ui_objects[i]->deinit();
                }
                delete screen;
                host->unfreeze(e); // e.param, (cart_def *)e.object
                state = ui_idle;
                break;
            }
        // fall through from host_owned:
        case ui_host_permanent:
            ret = 0;
            do {
                ret = ui_objects[focus]->poll(ret, e); // param pass chain
                if(!ret) // return value of 0 keeps us in the same state
                    break;
                printf("Object level %d returned %d.\n", focus, ret);
                ui_objects[focus]->deinit();
                if(focus) {
                    focus--;
                    //printf("Restored focus to level %d.\n", focus);
                }
                else {
                    delete screen;
                    host->unfreeze((Event &)c_empty_event);
                    state = ui_idle;
                    break;
                }
            } while(1);    
            break;
        default:
            break;
    }            

    BYTE buttons = ITU_IRQ_ACTIVE & ITU_BUTTONS;
    if((buttons & ~button_prev) & ITU_BUTTON1) {
        if(state == ui_idle) { // this is nasty, but at least we know that it's executed FIRST
            // and that it has no effect when no copper exists.
            push_event(e_copper_capture, 0);
        }
        push_event(e_button_press, 0);
    }
    button_prev = buttons;
}

bool UserInterface :: is_available(void)
{
    return (state != ui_idle);
}

int UserInterface :: activate_uiobject(UIObject *obj)
{
    if(focus < (MAX_UI_OBJECTS-1)) {
        focus++;
        ui_objects[focus] = obj;
        return 0;
    }

    return -1;
}

void UserInterface :: set_screen_title()
{
    static char title[48];
    // precondition: screen is cleared.  // \020 = alpha \021 = beta
    sprintf(title, "\033[37m    **** 1541 Ultimate %s (%b) ****\033[0m\n", APPL_VERSION, ITU_FPGA_VERSION);
    screen->move_cursor(0,0);
    screen->output(title);
    screen->move_cursor(0,1);
    for(int i=0;i<40;i++)
        screen->output('\002');
    screen->move_cursor(0,24);
	screen->no_scroll();
    for(int i=0;i<40;i++)
        screen->output('\002');
    screen->move_cursor(32,24);
	screen->output("\033[37mF3=Help\033[0m");
}
    
/* Blocking variants of our simple objects follow: */
int  UserInterface :: popup(char *msg, BYTE flags)
{
	Event e(e_nop, 0, 0);
    UIPopup *pop = new UIPopup(msg, flags);
    pop->init(screen, keyboard);
    int ret;
    do {
        ret = pop->poll(0, e);
    } while(!ret);
    pop->deinit();
    return ret;
}
    
int UserInterface :: string_box(char *msg, char *buffer, int maxlen)
{
	Event e(e_nop, 0, 0);
    UIStringBox *box = new UIStringBox(msg, buffer, maxlen);
    box->init(screen, keyboard);
    int ret;
    do {
        ret = box->poll(0, e);
    } while(!ret);
    box->deinit();
    return ret;
}

void UserInterface :: show_progress(char *msg, int steps)
{
    status_box = new UIStatusBox(msg, steps);
    status_box->init(screen);
}

void UserInterface :: update_progress(char *msg, int steps)
{
    status_box->update(msg, steps);
}

void UserInterface :: hide_progress(void)
{
    status_box->deinit();
    delete status_box;
}

void UserInterface :: run_editor(char *text_buf)
{
	Event e(e_nop, 0, 0);
    Editor *edit = new Editor(text_buf);
    edit->init(screen, keyboard);
    int ret;
    do {
        ret = edit->poll(0, e);
    } while(!ret);
    edit->deinit();
}

/* User Interface Objects */
/* Popup */
UIPopup :: UIPopup(char *msg, BYTE btns) : message(msg)
{
    buttons = btns;
    btns_active = 0;
    active_button = 0; // we can change this
    button_start_x = 0;
    window = NULL;
    keyboard = NULL;
}    

void UIPopup :: init(Screen *screen, Keyboard *k)
{
    // first, determine how wide our popup needs to be
    int button_width = 0;
    int message_width = message.length();
    keyboard = k;

    BYTE b = buttons;
    for(int i=0;i<NUM_BUTTONS;i++) {
        if(b & 1) {
            btns_active ++;
            button_width += c_button_widths[i];
        }
        b >>= 1;
    }
        
    int window_width = message_width;
    if (button_width > message_width)
        window_width = button_width;

    int x1 = (screen->get_size_x() - window_width) / 2;
    int y1 = (screen->get_size_y() - 5) / 2;
    int x_m = (window_width - message_width) / 2;
    int x_b = (window_width - button_width) / 2;
    button_start_x = x_b;

    window = new Screen(screen, x1, y1, window_width+2, 5);
    window->draw_border();
    window->no_scroll();
    window->move_cursor(x_m, 0);
    window->output(message.c_str());

    active_button = 0; // we can change this
    draw_buttons();
}

void UIPopup :: draw_buttons()
{
    window->move_cursor(button_start_x, 2);
    int j=0;
    int b = buttons;
    for(int i=0;i<NUM_BUTTONS;i++) {
        if(b & 1) {
			window->reverse_mode((j == active_button)? 1 : 0);
        	window->output((char *)c_button_names[i]);
            button_key[j++] = c_button_keys[i];
        }
        b >>= 1;
    }
    
    keyboard->clear_buffer();
}

int UIPopup :: poll(int dummy, Event &e)
{
    BYTE c = keyboard->getch();
    
    for(int i=0;i<btns_active;i++) {
        if(c == button_key[i]) {
            return (1 << i);
        }
    }
    if((c == 0x0D)||(c == 0x20)) {
		for(int i=0,j=0;i<NUM_BUTTONS;i++) {
			if(buttons & (1 << i)) {
				if(active_button == j)
					return (1 << i);
				j++;
			}
		}
        return 0;
    }
    if(c == 0x1D) {
        active_button ++;
        if(active_button >= btns_active)
            active_button = btns_active-1;
        draw_buttons();
    } else if(c == 0x9D) {
        active_button --;
        if(active_button < 0)
            active_button = 0;
        draw_buttons();
    }
    return 0;
}    

void UIPopup :: deinit()
{
	delete window;
}


UIStringBox :: UIStringBox(char *msg, char *buf, int max) : message(msg)
{
    buffer = buf;
    max_len = max;
    window = NULL;
    keyboard = NULL;
    cur = len = 0;
}

void UIStringBox :: init(Screen *screen, Keyboard *keyb)
{
    int message_width = message.length();
    int window_width = message_width;
    if (max_len > message_width)
        window_width = max_len;
    window_width += 2; // compensate for border
    
    int x1 = (screen->get_size_x() - window_width) / 2;
    int y1 = (screen->get_size_y() - 5) / 2;
    int x_m = (window_width - message_width) / 2;

    keyboard = keyb;
    window = new Screen(screen, x1, y1, window_width, 5);
    window->draw_border();
    window->move_cursor(x_m, 0);
    window->output_line(message.c_str());

    window->move_cursor(0, 2);
    //scr = window->get_pointer();

/// Now prefill the box...
    len = 0;
    cur = 0;
        
/// Default to old string
    cur = strlen(buffer); // assume it is prefilled, set cursor at the end.
    if(cur > max_len) {
        buffer[cur]=0;
        cur = max_len;
    }
    len = cur;
/// Default to old string
    window->output_length(buffer, len);
    window->move_cursor(cur, 2);
    window->cursor_visible(1);
}

int UIStringBox :: poll(int dummy, Event &e)
{
    BYTE key;
    int i;

    key = keyboard->getch();
    if(!key)
        return 0;

    switch(key) {
    case 0x0D: // CR
        window->cursor_visible(0);
        if(!len)
            return -1; // cancel
        return 1; // done
    case 0x9D: // left
    	if (cur > 0) {
            cur--;
        	window->move_cursor(cur, 2);
        }                
        break;
    case 0x1D: // right
        if (cur < len) {
            cur++;
        	window->move_cursor(cur, 2);
        }
        break;
    case 0x14: // backspace
        if (cur > 0) {
            cur--;
            len--;
            for(i=cur;i<len;i++) {
                buffer[i] = buffer[i+1];
            } buffer[i] = 0;
//            window->move_cursor(0, 2);
            window->output_length(buffer, len);
            window->move_cursor(cur, 2);
        }
        break;
    case 0x93: // clear
        len = 0;
        cur = 0;
        window->move_cursor(0, 2);
        window->repeat(' ', max_len);
        window->move_cursor(cur, 2);
        break;
    case 0x94: // del
        if(cur < len) {
            len--;
            for(i=cur;i<len;i++) {
            	buffer[i] = buffer[i+1];
            } buffer[i] = 0x20;
            window->output_length(buffer, len);
            window->move_cursor(cur, 2);
        }
        break;
    case 0x13: // home
        cur = 0;
        window->move_cursor(cur, 2);
        break;        
    case 0x11: // down = end
        cur = len;
        window->move_cursor(cur, 2);
        break;
    case 0x03: // break
        return -1; // cancel
    default:
        if ((key < 32)||(key > 127)) {
            break;
        }
        if (len < max_len) {
            for(i=len; i>=cur; i--) { // insert if necessary
                buffer[i+1] = buffer[i];
            }
            buffer[cur] = key;
            cur++;
            len++;
            window->move_cursor(0, 2);
            window->output_length(buffer, len);
            window->move_cursor(cur, 2);
        }
        break;
    }
    return 0; // still busy
}

void UIStringBox :: deinit(void)
{
	if (window)
		delete window;
}

/* Status Box */
UIStatusBox :: UIStatusBox(char *msg, int steps) : message(msg)
{
    total_steps = steps;
    progress = 0;
    window = NULL;
}
    
void UIStatusBox :: init(Screen *screen)
{
    int window_width = 32;
    int message_width = message.length();
    int x1 = (screen->get_size_x() - window_width) / 2;
    int y1 = (screen->get_size_y() - 5) / 2;
    int x_m = (window_width - message_width) / 2;

    window = new Screen(screen, x1, y1, window_width, 5);
    window->draw_border();
    window->move_cursor(x_m, 0);
    window->output_line(message.c_str());
    window->move_cursor(0, 2);
}

void UIStatusBox :: deinit(void)
{
    delete window;
}

void UIStatusBox :: update(char *msg, int steps)
{
    static char percent[8];
    static char bar[40];
    progress += steps;

    if(msg) {
        message = msg;
        window->clear();
        window->output_line(message.c_str());
    }
    
    memset(bar, 32, 36);
    window->move_cursor(0, 2);
    window->reverse_mode(1);
    int terminate = (32 * progress) / total_steps;
    if(terminate > 32)
        terminate = 32;
    bar[terminate] = 0; // terminate
    window->output_line(bar);
}
