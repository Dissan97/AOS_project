#define ON 0x01             // 0000 0001
#define OFF 0x02            // 0000 0010 
#define REC_ON 0x04         // 0000 0100
#define REC_OFF 0x08        // 0000 1000
#define ADD_PATH 0x10       // 0001 0000
#define REMOVE_PATH 0x20    // 0010 0000


#define CHANGE_STATE (134)
#define CHANGE_PATH (174)

int change_state(const char *, int);
int change_path(const char *, const char *, int op);