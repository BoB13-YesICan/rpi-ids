//송희 과제 때문에 잠시만 추가하겠습니다..

#include <stdio.h>

#ifdef linux
int JenkinsBuilderFileVersion[4] __attribute__((section ("FileVersion"))) = { 2,0,2,51 };
int JenkinsBuilderProductVersion[4] __attribute__((section ("ProductVersion"))) = FILE_VERSION;
#endif

int main(int nArgc, char** pszArgv)
{
    printf("FileVersion: %d.%d.%d.%d\n",
        JenkinsBuilderFileVersion[0],
        JenkinsBuilderFileVersion[1],
        JenkinsBuilderFileVersion[2],
        JenkinsBuilderFileVersion[3]
        );
    printf("ProductVersion: %d.%d.%d.%d\n",
        JenkinsBuilderProductVersion[0],
        JenkinsBuilderProductVersion[1],
        JenkinsBuilderProductVersion[2],
        JenkinsBuilderProductVersion[3]
        );
    return 0;
}
