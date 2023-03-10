#include "App.h"
using namespace std;
#ifdef __HuaweiLite__
 int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{ 
        Application * app = new Application();
        app->Init();
        app->Start();
        return 0;
}