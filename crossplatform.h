//
//  crossplatform.h
//  
//
//  Created by Gao Shan on 2017/10/19.
//

#ifndef crossplatform_h
#define crossplatform_h

#if defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#endif /* crossplatform_h */
