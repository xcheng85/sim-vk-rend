#include <misc.h>

std::string getAssetPath()
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
return "";
#else
return "./../assets/";
#endif
}
