//  crossplatform.h
//  Created by Gao Shan

#ifndef crossplatform_h
    #define crossplatform_h
    #ifdef _MSC_VER
        #define strcasecmp _stricmp
        #define strncasecmp _strnicmp
        #if _MSC_VER < 1900
            #define inline __inline
        #endif
    #endif
#endif
