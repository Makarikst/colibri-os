#ifndef FS_H
#define FS_H

#include "../utils/utils.h"

#define FS_MAX_OBJECTS 32
#define FS_NAME_LEN    32
#define FS_DATA_LEN    4096

#define OBJ_FREE         0
#define OBJ_FILE         1
#define OBJ_DIR          2
#define OBJ_APP_TERMINAL 10
#define OBJ_APP_TRASH    11
#define OBJ_APP_NANO     12
#define OBJ_APP_FILER    13
#define OBJ_LINK         20
#define ROOT_INDEX       0

typedef struct {
    char name[FS_NAME_LEN];
    char data[FS_DATA_LEN];
    int  type;
    int  parent;
    int  size;
    int  target_id;
} FsObject;

void fs_init(void);

int  fs_find_in(int parent, const char* name);
int  fs_create_in(int parent_idx, const char* name, int type);
void fs_delete_by_index(int idx);

int  parse_path(const char* path, int start_idx);
int  split_path(const char* path, int start_idx, int* parent_idx, char* last);
void strip_quotes(const char* in, char* out);

void tree_recursive(int dir_idx, int depth);
void find_recursive(int dir_idx, const char* name, int* found);

FsObject* fs_get(int idx);
int       fs_get_current_dir(void);
void      fs_set_current_dir(int idx);

#endif