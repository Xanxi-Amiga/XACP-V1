/*
 * ZZDoom640 - ZZDoom 640x480 standalone.
 * CLI : ZZDoom640 DOOM.WAD [-nomusic][-camdtest][-camddebug]
 * WB  : double-click icon, tooltypes:
 *         WAD=DOOM.WAD       (required)
 *         NOMUSIC            (optional)
 *         CAMDPORT=out.1     (optional)
 * ASCII only.
 */
#include <exec/types.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <string.h>

extern const unsigned char zzdoom_blob[];
extern const unsigned int  zzdoom_blob_size;
extern int zzdoom_main(int argc, char **argv);

struct Library *IconBase = NULL;

int main(int argc, char **argv)
{
    static const char *args[32];
    int n = 0, i;
    static char wad_path[256];
    static char port_buf[64];
    int nomusic = 0;
    char *camdport = NULL;

    wad_path[0] = 0;

    if(argc == 0){
        /* Workbench launch */
        struct WBStartup *wbs = (struct WBStartup *)argv;
        struct WBArg    *wba  = wbs->sm_ArgList;
        struct DiskObject *dobj = NULL;
        BPTR olddir = 0;
        char *tt;

        IconBase = OpenLibrary("icon.library", 37L);
        if(!IconBase) return 10;

        if(wba->wa_Lock)
            olddir = CurrentDir(wba->wa_Lock);

        dobj = GetDiskObject(wba->wa_Name);
        if(dobj){
            tt = FindToolType(dobj->do_ToolTypes, "WAD");
            if(tt){ strncpy(wad_path, tt, 255); wad_path[255]=0; }

            tt = FindToolType(dobj->do_ToolTypes, "NOMUSIC");
            if(tt) nomusic = 1;

            tt = FindToolType(dobj->do_ToolTypes, "CAMDPORT");
            if(tt){ strncpy(port_buf, tt, 63); port_buf[63]=0; camdport=port_buf; }

            FreeDiskObject(dobj);
        }

        if(olddir) CurrentDir(olddir);
        CloseLibrary(IconBase); IconBase = NULL;

        if(!wad_path[0]){
            /* Show error in Workbench context - no CLI available */
            struct EasyStruct es = {
                sizeof(struct EasyStruct), 0,
                "ZZDoom640",
                "Tooltype WAD= is missing.\nExample: WAD=DOOM.WAD",
                "OK"
            };
            EasyRequest(NULL, &es, NULL, TAG_DONE);
            return 10;
        }

    } else {
        /* CLI launch */
        if(argc < 2){
            Printf("Usage: ZZDoom640 <WAD> [-nomusic][-camdtest][-camddebug]\n");
            Printf("  WB tooltypes: WAD=DOOM.WAD  NOMUSIC  CAMDPORT=out.1\n");
            return 10;
        }
        strncpy(wad_path, argv[1], 255);
        wad_path[255] = 0;
    }

    /* Build argv for zzdoom_main */
    args[n++] = "ZZDoom640";
    args[n++] = wad_path;
    args[n++] = "-640";
    if(!nomusic)  args[n++] = "-camd";
    if(camdport){ args[n++] = "-camdport"; args[n++] = camdport; }
    if(argc > 2)
        for(i=2; i<argc && n<30; i++) args[n++] = argv[i];
    args[n] = NULL;

    return zzdoom_main(n, (char**)args);
}
