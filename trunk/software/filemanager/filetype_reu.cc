#include "filetype_reu.h"
#include "filemanager.h"
#include "c64.h"
#include "audio_select.h"
#include "menu.h"
#include "userinterface.h"

// tester instance
FactoryRegistrator<CachedTreeNode *, FileType *> tester_reu(Globals :: getFileTypeFactory(), FileTypeREU :: test_type);

// cart definition
extern uint8_t _binary_module_bin_start;
cart_def mod_cart  = { 0x00, (void *)0, 0x4000, 0x02 | CART_REU | CART_RAM };

/*********************************************************************/
/* REU File Browser Handling                                         */
/*********************************************************************/
#define REUFILE_LOAD      0x5201
#define REUFILE_PLAYMOD   0x5202

#define REU_TYPE_REU 0
#define REU_TYPE_MOD 1

FileTypeREU :: FileTypeREU(CachedTreeNode *node, int type)
{
    printf("Creating REU type from info: %s\n", node->get_name());
    this->node = node;
    this->type = type;
}

FileTypeREU :: ~FileTypeREU()
{
}

void FileTypeREU :: execute_st(void *obj, void *param)
{
	// fall through in original execute method
	((FileTypeREU *)obj)->execute((int)param);
}

int FileTypeREU :: fetch_context_items(IndexedList<Action *> &list)
{
    int count = 1;

    list.append(new Action("Load into REU", FileTypeREU :: execute_st, this, (void *)REUFILE_LOAD));
    uint32_t capabilities = getFpgaCapabilities();
    if ((type == REU_TYPE_MOD) && (capabilities & CAPAB_SAMPLER)) {
        list.append(new Action("Play MOD", FileTypeREU :: execute_st, this, (void *)REUFILE_PLAYMOD));
        count++;
    }
    return count;
}

FileType *FileTypeREU :: test_type(CachedTreeNode *obj)
{
	FileInfo *inf = obj->get_file_info();
    if(strcmp(inf->extension, "REU")==0)
        return new FileTypeREU(obj, REU_TYPE_REU);
    if(strcmp(inf->extension, "MOD")==0)
        return new FileTypeREU(obj, REU_TYPE_MOD);
    return NULL;
}

void FileTypeREU :: execute(int selection)
{
	printf("REU Select: %4x\n", selection);
	File *file;
	uint32_t bytes_read;
	bool progress;
	int sectors;
    int secs_per_step;
    int bytes_per_step;
    int total_bytes_read;
    int remain;
    
	static char buffer[36];
    uint8_t *dest;
    FileInfo *info = node->get_file_info();
    FileManager *fm = FileManager :: getFileManager();

    switch(selection) {
    case REUFILE_PLAYMOD:
        audio_configurator.clear_sampler_registers();
        // fallthrough
	case REUFILE_LOAD:
//		if(info->size > 0x100000)
//			res = user_interface->popup("This might take a while. Continue?", BUTTON_OK | BUTTON_CANCEL );
        sectors = (info->size >> 9);
        if(sectors >= 128)
            progress = true;
        secs_per_step = (sectors + 31) >> 5;
        bytes_per_step = secs_per_step << 9;
        remain = info->size;
        
		printf("REU Load.. %s\n", node->get_name());
		file = fm->fopen_node(node, FA_READ);
		if(file) {
            total_bytes_read = 0;
			// load file in REU memory
            if(progress) {
                user_interface->show_progress("Loading REU file..", 32);
                dest = (uint8_t *)(REU_MEMORY_BASE);
                while(remain >= 0) {
        			file->read(dest, bytes_per_step, &bytes_read);
                    total_bytes_read += bytes_read;
                    user_interface->update_progress(NULL, 1);
        			remain -= bytes_per_step;
        			dest += bytes_per_step;
        	    }
        	    user_interface->hide_progress();
        	} else {
    			file->read((void *)(REU_MEMORY_BASE), (REU_MAX_SIZE), &bytes_read);
                total_bytes_read += bytes_read;
    	    }
			fm->fclose(file);
			file = NULL;
            if (selection == REUFILE_LOAD) {
    			sprintf(buffer, "Bytes loaded: %d ($%8x)", total_bytes_read, total_bytes_read);
    			user_interface->popup(buffer, BUTTON_OK);
            } else {
                mod_cart.custom_addr = (void *)&_binary_module_bin_start;
                push_event(e_unfreeze, (void *)&mod_cart, 1);
                push_event(e_object_private_cmd, c64, C64_EVENT_MAX_REU);
                push_event(e_object_private_cmd, c64, C64_EVENT_AUDIO_ON);
                AUDIO_SELECT_LEFT   = 6;
                AUDIO_SELECT_RIGHT  = 7;
            }
		} else {
			printf("Error opening file.\n");
		}
        break;
	default:
		break;
    }
}
