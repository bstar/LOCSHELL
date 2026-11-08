#ifndef LOC_PROFILE_H
#define LOC_PROFILE_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The boot profile supplies MACHINE and optional PROFILE.TXT metadata;
 * launchers can override the display name through LOCSHELL_PROFILE.
 * Leave room for the RAM summary and close button in the 80-column header. */
#define LOC_TITLE_WIDTH 35
static const char *loc_shell_title(void) {
    static char title[LOC_TITLE_WIDTH+1];
    const char *profile,*end;
    char path[40],label[96];
    unsigned n;
    FILE *f;
    if(*title)return title;
    strcpy(title,"LOC SHELL 2.3");
    profile=getenv("LOCSHELL_PROFILE");
    if(!profile) {
        profile=getenv("MACHINE");
        if(profile) {
            /* MACHINE is a DOS directory key, not the display name. Never
             * interpret a supplied label as a path outside that directory. */
            for(n=0;n<9&&profile[n];n++) {
                unsigned char c=(unsigned char)profile[n];
                if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                     (c>='0'&&c<='9')||c=='_'))break;
            }
            if(n&&n<=8&&!profile[n]) {
                sprintf(path,"C:\\BOOT\\%s\\PROFILE.TXT",profile);
                f=fopen(path,"r");
                if(f) {
                    while(fgets(label,sizeof label,f))if(!strnicmp(label,"Machine:",8)) {
                        label[strcspn(label,"\r\n")]=0;
                        profile=label+8;break;
                    }
                    fclose(f);
                }
            }
        }
    }
    if(!profile)return title;
    while(*profile==' '||*profile=='\t')profile++;
    end=profile+strlen(profile);
    while(end>profile&&(end[-1]==' '||end[-1]=='\t'))end--;
    if(end==profile)return title;
    strcat(title," / ");
    n=strlen(title);
    while(profile<end&&n<LOC_TITLE_WIDTH) {
        unsigned char c=(unsigned char)*profile++;
        title[n++]=c<32||c==127?' ':c;
    }
    while(n&&title[n-1]==' ')n--;
    title[n]=0;
    return title;
}
#endif
