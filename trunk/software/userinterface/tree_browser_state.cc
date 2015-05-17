/* ========================================================================== */
/*                                                                            */
/*   tree_browser_state.cc                                                    */
/*   (c) 2010 Gideon Zweijtzer                                                */
/*                                                                            */
/* ========================================================================== */
#include "tree_browser.h"
#include "tree_browser_state.h"
#include "config_menu.h"
#include "context_menu.h"
#include "task_menu.h"
#include "filemanager.h"

/*****************************/
/* Tree Browser State Object */
/*****************************/
TreeBrowserState :: TreeBrowserState(Browsable *n, TreeBrowser *b, int lev)
	: children(0, NULL)
{
	browser = b;
	level = lev;
	node = n;

	first_item_on_screen = -1;
    selected_line = 0;

    cursor_pos = 0;
    under_cursor = NULL;

    refresh = true;
    initial_index = -1;
    previous = NULL;
    deeper = NULL;

    reload();
//    printf("Constructor tree browser state: Node = %p\n", node);
}

TreeBrowserState :: ~TreeBrowserState()
{
	cleanup();
	if(previous)
		delete previous;
}

void TreeBrowserState :: cleanup()
{
	for(int i=0;children.get_elements();i++) {
		delete children[i];
	}
	children.clear_list();
}

/*
 * State functions
 */
void TreeBrowserState :: do_refresh()
{
//	printf("RefreshIn.");
	refresh = false;
    if(!(browser->window)) {
        printf("INTERNAL ERROR: NO WINDOW TO DO REFRESH ON!!\n");
        return;
    }

    if(!under_cursor) {
        browser->reset_quick_seek();
        draw();
        if(initial_index >= 0) {
            move_to_index(initial_index);
        } else {
        	move_to_index(0);
        	for(int i=0;i<children.get_elements();i++) {
        		if (children[i]->isSelectable()) {
        			move_to_index(i);
        			break;
        		}
        	}
        }
    } else {
        draw(); // just draw.. we don't need to move anything
    }
//	printf("RefreshOut.");
}

void TreeBrowserState :: draw()
{
	if(!browser->window)
		return;
		
//	printf("Draw. First=%d. Selected_line=%d. Number of el=%d\n", first_item_on_screen, selected_line, children.get_elements());
//	printf("Window = %p. WindowBase: %p\n", browser->window, browser->window->get_pointer());
	// this functions initializes the screen
    browser->window->set_color(user_interface->color_fg);

    if(children.get_elements() == 0) {
		browser->window->clear();
    	browser->window->move_cursor(0, 0);
    	browser->window->output("\033[32m< No Items >");
    	under_cursor = NULL;
    	return;
    }

    int y = browser->window->get_size_y(); // how many can I show?
//    printf("TreeBrowserState::Draw: first=%d, y=%d, selected_line=%d\n", first_item_on_screen, y, selected_line);

    if(first_item_on_screen < 0)
        return;
    
    Browsable *t;
    for(int i=0;i<y;i++) {
    	t = children[i+first_item_on_screen];

        browser->window->move_cursor(0, i);
        if(t) {
        	if ((i + first_item_on_screen) == cursor_pos) {
        		browser->window->set_color(user_interface->color_sel);
        	} else if(t->isSelectable()) {
        		browser->window->set_color(user_interface->color_fg);
        	} else { // non selectable item
        		browser->window->set_color(12); // TODO
        	}
        	browser->window->output_line(t->getDisplayString());
        }
		else {
			browser->window->output_line("");
		}
    }

    if(selected_line < 0) {
        printf("error! %d", selected_line);
        selected_line = 0;
    }
    cursor_pos = first_item_on_screen + selected_line;
    under_cursor = children[cursor_pos];
}

void TreeBrowserState :: update_selected(void)
{
    if(!under_cursor)
        return;

    browser->window->move_cursor(0, selected_line);
    browser->window->set_color(user_interface->color_sel); // highlighted
    browser->window->output_line(under_cursor->getDisplayString());
}
    
void TreeBrowserState :: up(int num)
{
	int original = cursor_pos;
	int previous = cursor_pos;

	while(num--) {
		cursor_pos--;
		if (cursor_pos < 0) {
			cursor_pos = previous;
			break;
		}
		if (children[cursor_pos]->isSelectable()) {
			previous = cursor_pos; // store last selectable item
		} else {
			num++; // try to jump by moving one extra step
		}
	}

	under_cursor = children[cursor_pos];
	move_to_index(cursor_pos);
}

void TreeBrowserState :: down(int num)
{
	int original = cursor_pos;
	int previous = cursor_pos;

	while(num--) {
		cursor_pos++;
		if (cursor_pos >= children.get_elements()) {
			cursor_pos = previous;
			break;
		}
		if (children[cursor_pos]->isSelectable()) {
			previous = cursor_pos; // store last selectable item
		} else {
			num++; // try to jump by moving one extra step
		}
	}

	under_cursor = children[cursor_pos];
	move_to_index(cursor_pos);
}

void TreeBrowserState :: reload(void)
{
	cleanup();
	node->getSubItems(children);
	/*
	int child_count = node->children.get_elements();
	node->cleanup_children();
	node->fetch_children();
	child_count = node->children.get_elements();
	reselect();
	refresh = true;
*/
}

void TreeBrowserState :: into(void)
{
	if(!under_cursor)
		return;

	printf("Going deeper into = %s\n", under_cursor->getName());
    int child_count = under_cursor->getSubItems(children);
    if(child_count < 0)
        return;
        
    printf("%d children fetched.\n", child_count);
        
	deeper = new TreeBrowserState(under_cursor, browser, level+1);
    //user_interface->set_path(under_cursor);
    browser->state = deeper;
    deeper->previous = this;
}

bool TreeBrowserState :: into2(void)
{
	// return True if should show context menu instead
	if(!under_cursor)
		return(false);

	printf("Going deeper into = %s\n", under_cursor->getName());
    int child_count = under_cursor->getSubItems(children);

    if(child_count < 0)
        return(true);
        
    printf("%d children fetched.\n", child_count);

	deeper = new TreeBrowserState(under_cursor, browser, level+1);
    // user_interface->set_path(under_cursor);
    browser->state = deeper;
    deeper->previous = this;
	return(false);
}

void TreeBrowserState :: level_up(void)
{
    if(!previous)
        return;

    browser->state = previous;
    previous->refresh = true;
    previous = NULL; // unlink;
    delete this;
}


void TreeBrowserState :: move_to_index(int idx)
{
	cursor_pos = idx;

	int num_el = children.get_elements();
	if(num_el == 0) {
		first_item_on_screen = -1;
		draw();
		return;
	}

	if((first_item_on_screen + selected_line)==idx) // duh!
        return;

    // Try to determine the first item on the screen, by
    // stepping half of the screen size up.
	int y = (browser->window->get_size_y() / 2);
	if(idx < y) {
		first_item_on_screen = 0;
		selected_line = idx;
	    draw();
		return;
	}
	if(num_el <= browser->window->get_size_y()) {
		first_item_on_screen = 0;
		selected_line = idx;
	    draw();
		return;
	}
	first_item_on_screen = idx - y;

	if(first_item_on_screen + browser->window->get_size_y() >= num_el)
		first_item_on_screen = num_el - browser->window->get_size_y();

	selected_line = idx - first_item_on_screen;

    draw();    
}
